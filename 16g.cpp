// 16g - tiny 16-op graphics/compute engine for Windows
// Build with MinGW-w64:
//   g++ 16g.cpp -o 16g.exe -std=c++17 -O2 -Wall -Wextra -static -static-libgcc -static-libstdc++ -mwindows -ladvapi32 -lshell32 -luser32 -lgdi32
// Named-pipe request: 5 bytes [OP][A high][A low][B high][B low]
// Named-pipe response: 4 bytes [status][result high][result low][flags]

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif
#include <windows.h>
#include <shellapi.h>
#include <sddl.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr int kLogicalWidth = 256;
constexpr int kLogicalHeight = 256;
constexpr int kScale = 2;
constexpr int kWindowSize = kLogicalWidth * kScale;
constexpr size_t kPixelCount = static_cast<size_t>(kLogicalWidth) * kLogicalHeight;
constexpr size_t kHistoryCapacity = 65536;
constexpr size_t kMaxFileBytes = 128u * 1024u * 1024u;
constexpr wchar_t kWindowClass[] = L"16g_VGA_Window_Class";
constexpr wchar_t kWindowTitle[] = L"16g VGA";
constexpr wchar_t kPipePath[] = L"\\\\.\\pipe\\16g";
constexpr wchar_t kPipeMutexName[] = L"Local\\16g_PipeServer_Mutex";

// Internal result categories (translated to the .md wire values in PipeClientWorker).
constexpr uint8_t kFlagNumeric = 0x01;
constexpr uint8_t kFlagVga = 0x02;
constexpr uint8_t kFlagShfl = 0x04;
constexpr uint8_t kFlagColor = 0x08;

// Internal status bits: 1=DIV/MOD by zero, 2=missing SHFL history, 4=invalid opcode.
// Pipe status encoding follows the project specification: 0=success, 1=warning,
// 0x80=invalid request/OP, 0x81=internal processing error.

struct Instruction {
    uint8_t op = 0;
    uint16_t a = 0;
    uint16_t b = 0;
};

struct ExecutionResult {
    uint8_t status = 0;
    uint8_t flags = 0;
    uint16_t value = 0;
    bool printNumericResult = false;
};

std::atomic<bool> g_running{true};
std::atomic<bool> g_hexOutput{false};
std::atomic<HWND> g_window{nullptr};
std::mutex g_outputMutex;

bool IsValidHandle(HANDLE h) {
    return h != nullptr && h != INVALID_HANDLE_VALUE;
}

bool WriteStandard(bool toStderr, const std::string& text) {
    std::lock_guard<std::mutex> lock(g_outputMutex);
    HANDLE h = GetStdHandle(toStderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    if (!IsValidHandle(h)) return false;

    size_t done = 0;
    while (done < text.size()) {
        DWORD chunk = static_cast<DWORD>(std::min<size_t>(text.size() - done, 1u << 20));
        DWORD written = 0;
        if (!WriteFile(h, text.data() + done, chunk, &written, nullptr) || written == 0) {
            return false;
        }
        done += written;
    }
    return true;
}

void PrintError(const std::string& text, bool popupIfNoStderr = false) {
    const std::string line = text + "\r\n";
    if (!WriteStandard(true, line) && popupIfNoStderr) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (wlen > 0) {
            std::vector<wchar_t> wide(static_cast<size_t>(wlen));
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), wlen);
            MessageBoxW(nullptr, wide.data(), L"16g", MB_OK | MB_ICONERROR);
        } else {
            MessageBoxA(nullptr, text.c_str(), "16g", MB_OK | MB_ICONERROR);
        }
    }
}

std::string FormatValue(uint16_t value) {
    std::ostringstream out;
    if (g_hexOutput.load()) {
        out << std::uppercase << std::hex << std::setfill('0') << std::setw(4)
            << static_cast<unsigned int>(value);
    } else {
        for (int bit = 15; bit >= 0; --bit) out << ((value >> bit) & 1u);
    }
    out << "\r\n";
    return out.str();
}

void PrintDiagnostics(const ExecutionResult& result) {
    if (result.status & 0x01) {
        WriteStandard(true, "16g: DIV/MOD by zero; result forced to zero.\r\n");
    }
    if (result.status & 0x02) {
        WriteStandard(true, "16g: SHFL history index is unavailable; that input was replaced with zero.\r\n");
    }
    if (result.status & 0x04) {
        WriteStandard(true, "16g: invalid opcode/request.\r\n");
    }
    if (result.status & 0x08) {
        WriteStandard(true, "16g: internal processing error.\r\n");
    }
}

std::string TrimAscii(std::string s) {
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && isSpace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && isSpace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

std::string UpperAscii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                    static_cast<int>(text.size()), nullptr, 0);
    if (needed <= 0) {
        needed = MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (needed <= 0) throw std::runtime_error("cannot convert file path to Unicode");
        std::wstring out(static_cast<size_t>(needed), L'\0');
        MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), out.data(), needed);
        return out;
    }
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), out.data(), needed);
    return out;
}

bool EndsWith16g(std::wstring path) {
    if (path.size() < 4) return false;
    std::wstring ext = path.substr(path.size() - 4);
    for (wchar_t& c : ext) c = static_cast<wchar_t>(towlower(c));
    return ext == L".16g";
}

uint32_t Color565ToRgb(uint16_t c) {
    const uint32_t r5 = (c >> 11) & 0x1Fu;
    const uint32_t g6 = (c >> 5) & 0x3Fu;
    const uint32_t b5 = c & 0x1Fu;
    const uint32_t r = (r5 * 255u) / 31u;
    const uint32_t g = (g6 * 255u) / 63u;
    const uint32_t b = (b5 * 255u) / 31u;
    return (r << 16) | (g << 8) | b; // DIB's little-endian bytes are B,G,R,0.
}

class Machine {
public:
    Machine() { ClearNoLock(); }

    ExecutionResult Execute(const Instruction& ins) {
        ExecutionResult result{};
        if (ins.op > 0x0F) {
            result.status = 0x04;
            result.flags = 0;
            return result;
        }

        bool pixelChanged = false;
        RECT dirty{};
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // SHFL overwrites an earlier unused SHFL setting. SHFL's own fields
            // are never replaced by a preceding pending setting.
            if (ins.op == 0x07) {
                pendingA_ = ins.a;
                pendingB_ = ins.b;
                result.flags = kFlagShfl;
                return result;
            }

            uint16_t a = ins.a;
            uint16_t b = ins.b;
            bool historyError = false;
            if (pendingA_ != 0) {
                if (!ReadHistoryNoLock(pendingA_, a)) {
                    a = 0;
                    historyError = true;
                }
            }
            if (pendingB_ != 0) {
                if (!ReadHistoryNoLock(pendingB_, b)) {
                    b = 0;
                    historyError = true;
                }
            }
            pendingA_ = 0;
            pendingB_ = 0;

            uint16_t out = 0;
            bool shouldSaveHistory = false;
            switch (ins.op) {
            case 0x00: // PASS
                out = a;
                shouldSaveHistory = true;
                result.printNumericResult = true;
                result.flags = kFlagNumeric;
                break;
            case 0x01: // ADD
                out = static_cast<uint16_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
                shouldSaveHistory = true;
                result.printNumericResult = true;
                result.flags = kFlagNumeric;
                break;
            case 0x02: // SUB, modulo 2^16
                out = static_cast<uint16_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
                shouldSaveHistory = true;
                result.printNumericResult = true;
                result.flags = kFlagNumeric;
                break;
            case 0x03: // MUL, low 16 bits
                out = static_cast<uint16_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
                shouldSaveHistory = true;
                result.printNumericResult = true;
                result.flags = kFlagNumeric;
                break;
            case 0x04: // DIV
                if (b == 0) {
                    out = 0;
                    result.status |= 0x01;
                } else {
                    out = static_cast<uint16_t>(a / b);
                }
                shouldSaveHistory = true;
                result.printNumericResult = true;
                result.flags = kFlagNumeric;
                break;
            case 0x05: // MOD
                if (b == 0) {
                    out = 0;
                    result.status |= 0x01;
                } else {
                    out = static_cast<uint16_t>(a % b);
                }
                shouldSaveHistory = true;
                result.printNumericResult = true;
                result.flags = kFlagNumeric;
                break;
            case 0x06: { // VGA: A=(Y<<8)|X, B=RGB565
                const uint8_t y = static_cast<uint8_t>((a >> 8) & 0xFFu);
                const uint8_t x = static_cast<uint8_t>(a & 0xFFu);
                framebuffer_[static_cast<size_t>(y) * kLogicalWidth + x] = b;
                const uint32_t rgb = Color565ToRgb(b);
                const int px = static_cast<int>(x) * kScale;
                const int py = static_cast<int>(y) * kScale;
                for (int dy = 0; dy < kScale; ++dy) {
                    for (int dx = 0; dx < kScale; ++dx) {
                        image_[static_cast<size_t>(py + dy) * kWindowSize + (px + dx)] = rgb;
                    }
                }
                dirty.left = px;
                dirty.top = py;
                dirty.right = px + kScale;
                dirty.bottom = py + kScale;
                pixelChanged = true;
                result.flags = kFlagVga;
                break;
            }
            case 0x08: // AND
                out = static_cast<uint16_t>(a & b);
                shouldSaveHistory = true;
                result.printNumericResult = true;
                result.flags = kFlagNumeric;
                break;
            case 0x09: // OR
                out = static_cast<uint16_t>(a | b);
                shouldSaveHistory = true;
                result.printNumericResult = true;
                result.flags = kFlagNumeric;
                break;
            case 0x0A: // XOR
                out = static_cast<uint16_t>(a ^ b);
                shouldSaveHistory = true;
                result.printNumericResult = true;
                result.flags = kFlagNumeric;
                break;
            case 0x0B: // SHL
                out = (b >= 16) ? 0 : static_cast<uint16_t>((static_cast<uint32_t>(a) << b) & 0xFFFFu);
                shouldSaveHistory = true;
                result.printNumericResult = true;
                result.flags = kFlagNumeric;
                break;
            case 0x0C: // SHR, logical
                out = (b >= 16) ? 0 : static_cast<uint16_t>(a >> b);
                shouldSaveHistory = true;
                result.printNumericResult = true;
                result.flags = kFlagNumeric;
                break;
            case 0x0D: { // BLEND RGB565 channels separately
                const unsigned r = ((((a >> 11) & 31u) + ((b >> 11) & 31u)) / 2u);
                const unsigned g = ((((a >> 5) & 63u) + ((b >> 5) & 63u)) / 2u);
                const unsigned blue = (((a & 31u) + (b & 31u)) / 2u);
                out = static_cast<uint16_t>((r << 11) | (g << 5) | blue);
                shouldSaveHistory = true;
                result.flags = kFlagColor;
                break;
            }
            case 0x0E: { // BRIGHT, factor 0..255
                const uint32_t factor = std::min<uint32_t>(b, 255u);
                const uint32_t r = (((a >> 11) & 31u) * factor) / 255u;
                const uint32_t g = (((a >> 5) & 63u) * factor) / 255u;
                const uint32_t blue = ((a & 31u) * factor) / 255u;
                out = static_cast<uint16_t>((r << 11) | (g << 5) | blue);
                shouldSaveHistory = true;
                result.flags = kFlagColor;
                break;
            }
            case 0x0F: // CMP: less=0, equal=1, greater=2
                out = (a < b) ? 0 : ((a == b) ? 1 : 2);
                shouldSaveHistory = true;
                result.printNumericResult = true;
                result.flags = kFlagNumeric;
                break;
            default:
                result.status = 0x04;
                result.flags = 0;
                return result;
            }

            if (shouldSaveHistory) PushHistoryNoLock(out);
            if (historyError) result.status |= 0x02;
            result.value = out;
        }

        if (pixelChanged) {
            HWND hwnd = g_window.load();
            if (hwnd) InvalidateRect(hwnd, &dirty, FALSE);
        }
        return result;
    }

    void Clear() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ClearNoLock();
        }
        HWND hwnd = g_window.load();
        if (hwnd) InvalidateRect(hwnd, nullptr, FALSE);
    }

    void Paint(HDC dc, const BITMAPINFO* bmi) {
        std::lock_guard<std::mutex> lock(mutex_);
        StretchDIBits(dc, 0, 0, kWindowSize, kWindowSize,
                      0, 0, kWindowSize, kWindowSize,
                      image_.data(), bmi, DIB_RGB_COLORS, SRCCOPY);
    }

private:
    void ClearNoLock() {
        framebuffer_.fill(0);
        image_.fill(0);
        history_.fill(0);
        historyHead_ = kHistoryCapacity - 1;
        historyCount_ = 0;
        pendingA_ = 0;
        pendingB_ = 0;
    }

    void PushHistoryNoLock(uint16_t value) {
        historyHead_ = (historyHead_ + 1) % kHistoryCapacity;
        history_[historyHead_] = value;
        if (historyCount_ < kHistoryCapacity) ++historyCount_;
    }

    bool ReadHistoryNoLock(uint16_t index, uint16_t& value) const {
        if (index == 0 || index > historyCount_) return false;
        const size_t slot = (historyHead_ + kHistoryCapacity - (static_cast<size_t>(index) - 1)) % kHistoryCapacity;
        value = history_[slot];
        return true;
    }

    std::mutex mutex_;
    std::array<uint16_t, kPixelCount> framebuffer_{};
    std::array<uint32_t, static_cast<size_t>(kWindowSize) * kWindowSize> image_{};
    std::array<uint16_t, kHistoryCapacity> history_{};
    size_t historyHead_ = kHistoryCapacity - 1;
    size_t historyCount_ = 0;
    uint16_t pendingA_ = 0;
    uint16_t pendingB_ = 0;
};

Machine g_machine;

BITMAPINFO MakeBitmapInfo() {
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = kWindowSize;
    bmi.bmiHeader.biHeight = -kWindowSize; // top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    return bmi;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        const BITMAPINFO bmi = MakeBitmapInfo();
        g_machine.Paint(dc, &bmi);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        g_running.store(false);
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_window.store(nullptr);
        g_running.store(false);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

bool CreateVgaWindow(HINSTANCE instance) {
    SetProcessDPIAware();
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    RECT rect{0, 0, kWindowSize, kWindowSize};
    if (!AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE)) return false;
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    HWND hwnd = CreateWindowExW(0, kWindowClass, kWindowTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, instance, nullptr);
    if (!hwnd) return false;
    g_window.store(hwnd);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return true;
}

bool ReadWholeFile(const std::wstring& path, std::vector<uint8_t>& bytes) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
                              FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot open .16g file");
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 ||
        static_cast<unsigned long long>(size.QuadPart) > kMaxFileBytes) {
        CloseHandle(file);
        throw std::runtime_error(".16g file is too large or has an invalid size");
    }
    bytes.resize(static_cast<size_t>(size.QuadPart));
    size_t offset = 0;
    while (offset < bytes.size()) {
        const DWORD want = static_cast<DWORD>(std::min<size_t>(bytes.size() - offset, 1u << 20));
        DWORD got = 0;
        if (!ReadFile(file, bytes.data() + offset, want, &got, nullptr) || got == 0) {
            CloseHandle(file);
            throw std::runtime_error("failed while reading .16g file");
        }
        offset += got;
    }
    CloseHandle(file);
    return true;
}

std::vector<Instruction> Load16gFile(const std::wstring& path) {
    std::vector<uint8_t> data;
    ReadWholeFile(path, data);
    if (data.empty()) throw std::runtime_error(".16g file is empty");

    std::vector<Instruction> instructions;
    size_t offset = 0;
    while (offset < data.size()) {
        if (data.size() - offset < 5) throw std::runtime_error("truncated .16g instruction record");
        if (data[offset] > 0x0F) throw std::runtime_error(".16g contains an invalid opcode byte");
        Instruction ins{};
        ins.op = data[offset];
        ins.a = static_cast<uint16_t>((static_cast<uint16_t>(data[offset + 1]) << 8) | data[offset + 2]);
        ins.b = static_cast<uint16_t>((static_cast<uint16_t>(data[offset + 3]) << 8) | data[offset + 4]);
        instructions.push_back(ins);
        offset += 5;
        if (offset == data.size()) break;
        if (data[offset] != 0x26) throw std::runtime_error("missing 0x26 (&) command separator in .16g file");
        ++offset;
        if (offset == data.size()) throw std::runtime_error(".16g file ends with a command separator");
    }
    if (instructions.empty()) throw std::runtime_error(".16g file contains no instructions");
    return instructions;
}

unsigned int ParseUnsigned(const std::string& s, unsigned int base, size_t exactLength, unsigned int maxValue) {
    if (s.size() != exactLength) throw std::runtime_error("field has the wrong number of digits");
    unsigned int value = 0;
    for (char c : s) {
        unsigned int digit = 0;
        if (c >= '0' && c <= '9') digit = static_cast<unsigned int>(c - '0');
        else if (base == 16 && c >= 'a' && c <= 'f') digit = static_cast<unsigned int>(c - 'a' + 10);
        else if (base == 16 && c >= 'A' && c <= 'F') digit = static_cast<unsigned int>(c - 'A' + 10);
        else throw std::runtime_error("field contains an invalid digit");
        if (digit >= base) throw std::runtime_error("field contains an invalid digit");
        value = value * base + digit;
        if (value > maxValue) throw std::runtime_error("field is outside its allowed range");
    }
    return value;
}

Instruction ParseTextInstruction(const std::vector<std::string>& fields, bool hex) {
    if (fields.size() != 3) throw std::runtime_error("expected exactly three fields: OP INPUT1 INPUT2");
    Instruction ins{};
    if (hex) {
        ins.op = static_cast<uint8_t>(ParseUnsigned(fields[0], 16, 2, 0x0F));
        ins.a = static_cast<uint16_t>(ParseUnsigned(fields[1], 16, 4, 0xFFFF));
        ins.b = static_cast<uint16_t>(ParseUnsigned(fields[2], 16, 4, 0xFFFF));
    } else {
        ins.op = static_cast<uint8_t>(ParseUnsigned(fields[0], 2, 4, 0x0F));
        ins.a = static_cast<uint16_t>(ParseUnsigned(fields[1], 2, 16, 0xFFFF));
        ins.b = static_cast<uint16_t>(ParseUnsigned(fields[2], 2, 16, 0xFFFF));
    }
    return ins;
}

std::vector<std::string> SplitFields(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> fields;
    std::string field;
    while (in >> field) fields.push_back(field);
    return fields;
}

void ExecuteAndReport(const Instruction& ins, bool printToStdout, bool printErrors) {
    const ExecutionResult result = g_machine.Execute(ins);
    if (printToStdout && result.printNumericResult) WriteStandard(false, FormatValue(result.value));
    if (printErrors && result.status != 0) PrintDiagnostics(result);
}

void RunInstructions(const std::vector<Instruction>& instructions, bool loop, bool printToStdout, bool printErrors) {
    do {
        for (const Instruction& ins : instructions) {
            if (!g_running.load()) return;
            ExecuteAndReport(ins, printToStdout, printErrors);
        }
        if (loop) Sleep(0); // yield to the window system without resetting state
    } while (loop && g_running.load());
}

bool ReadExact(HANDLE pipe, uint8_t* buffer, size_t bytes) {
    size_t offset = 0;
    while (offset < bytes) {
        DWORD got = 0;
        const DWORD wanted = static_cast<DWORD>(std::min<size_t>(bytes - offset, 0x7FFFFFFFu));
        if (!ReadFile(pipe, buffer + offset, wanted, &got, nullptr) || got == 0) return false;
        offset += got;
    }
    return true;
}

bool WriteExact(HANDLE pipe, const uint8_t* buffer, size_t bytes) {
    size_t offset = 0;
    while (offset < bytes) {
        DWORD written = 0;
        const DWORD wanted = static_cast<DWORD>(std::min<size_t>(bytes - offset, 0x7FFFFFFFu));
        if (!WriteFile(pipe, buffer + offset, wanted, &written, nullptr) || written == 0) return false;
        offset += written;
    }
    return true;
}

void PipeClientWorker(HANDLE pipe) {
    for (;;) {
        uint8_t request[5]{};
        if (!ReadExact(pipe, request, sizeof(request))) break;
        ExecutionResult result{};
        if (request[0] > 0x0F) {
            result.status = 0x04;
            result.flags = 0;
        } else {
            try {
                Instruction ins{};
                ins.op = request[0];
                ins.a = static_cast<uint16_t>((static_cast<uint16_t>(request[1]) << 8) | request[2]);
                ins.b = static_cast<uint16_t>((static_cast<uint16_t>(request[3]) << 8) | request[4]);
                result = g_machine.Execute(ins);
            } catch (...) {
                result.status = 0x08;
                result.flags = 0;
                result.value = 0;
            }
        }
        // The wire protocol has a single result-kind byte, not a bit mask:
        // 0=numeric, 1=color intermediate, 2=VGA changed, 3=SHFL accepted.
        uint8_t wireStatus = 0x00;
        if (result.status & 0x04) wireStatus = 0x80;
        else if (result.status & 0x08) wireStatus = 0x81;
        else if (result.status != 0) wireStatus = 0x01;

        uint8_t resultKind = 0x00; // also used as a neutral value if status is an error
        if (result.flags & kFlagNumeric) resultKind = 0x00;
        else if (result.flags & kFlagColor) resultKind = 0x01;
        else if (result.flags & kFlagVga) resultKind = 0x02;
        else if (result.flags & kFlagShfl) resultKind = 0x03;

        uint8_t response[4]{};
        response[0] = wireStatus;
        response[1] = static_cast<uint8_t>((result.value >> 8) & 0xFFu);
        response[2] = static_cast<uint8_t>(result.value & 0xFFu);
        response[3] = resultKind;
        if (result.status != 0) PrintDiagnostics(result);
        if (!WriteExact(pipe, response, sizeof(response))) break;
    }
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
}

bool MakeCurrentUserPipeSecurity(SECURITY_ATTRIBUTES& sa, PSECURITY_DESCRIPTOR& descriptor) {
    descriptor = nullptr;
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    DWORD bytesNeeded = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &bytesNeeded);
    if (bytesNeeded == 0) {
        CloseHandle(token);
        return false;
    }
    std::vector<uint8_t> tokenInfo(bytesNeeded);
    if (!GetTokenInformation(token, TokenUser, tokenInfo.data(), bytesNeeded, &bytesNeeded)) {
        CloseHandle(token);
        return false;
    }
    CloseHandle(token);

    const auto* user = reinterpret_cast<const TOKEN_USER*>(tokenInfo.data());
    LPWSTR sidText = nullptr;
    if (!ConvertSidToStringSidW(user->User.Sid, &sidText)) return false;
    const std::wstring sddl = std::wstring(L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;") + sidText + L")";
    LocalFree(sidText);
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr)) return false;

    sa = {};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = descriptor;
    sa.bInheritHandle = FALSE;
    return true;
}

void PostCloseWindow() {
    HWND hwnd = g_window.load();
    if (hwnd) PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

void PipeServerWorker() {
    SetLastError(ERROR_SUCCESS);
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kPipeMutexName);
    const DWORD mutexError = GetLastError();
    if (!mutex) {
        PrintError("16g: cannot create the pipe-server mutex.", true);
        PostCloseWindow();
        return;
    }
    if (mutexError == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        PrintError("16g: another -pipe server is already running.", true);
        PostCloseWindow();
        return;
    }

    SECURITY_ATTRIBUTES sa{};
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!MakeCurrentUserPipeSecurity(sa, descriptor)) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        PrintError("16g: could not create the named-pipe access rules.", true);
        PostCloseWindow();
        return;
    }

    bool firstPipeInstance = true;
    while (g_running.load()) {
        const DWORD pipeOpenMode = PIPE_ACCESS_DUPLEX |
            (firstPipeInstance ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0);
        HANDLE pipe = CreateNamedPipeW(
            kPipePath,
            pipeOpenMode,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            4, 5, 0, &sa);
        if (pipe == INVALID_HANDLE_VALUE) {
            PrintError("16g: CreateNamedPipe failed.", true);
            PostCloseWindow();
            break;
        }

        firstPipeInstance = false;
        const BOOL connected = ConnectNamedPipe(pipe, nullptr);
        const DWORD connectError = connected ? ERROR_SUCCESS : GetLastError();
        if (!connected && connectError != ERROR_PIPE_CONNECTED) {
            CloseHandle(pipe);
            if (!g_running.load()) break;
            Sleep(20);
            continue;
        }
        std::thread(PipeClientWorker, pipe).detach();
    }

    LocalFree(descriptor);
    ReleaseMutex(mutex);
    CloseHandle(mutex);
}

void PrintHelp() {
    const char* help =
        "16g commands:\r\n"
        "  HELP                  show this help\r\n"
        "  CLEAR                 clear the VGA framebuffer only\r\n"
        "  RUN <file.16g>        execute a binary instruction file once\r\n"
        "  EXIT                  close 16g\r\n"
        "  <OP> <INPUT1> <INPUT2> execute one instruction\r\n\r\n"
        "OP map (binary / hex):\r\n"
        "  0000/00 PASS   0001/01 ADD    0010/02 SUB    0011/03 MUL\r\n"
        "  0100/04 DIV    0101/05 MOD    0110/06 VGA    0111/07 SHFL\r\n"
        "  1000/08 AND    1001/09 OR     1010/0A XOR    1011/0B SHL\r\n"
        "  1100/0C SHR    1101/0D BLEND  1110/0E BRIGHT 1111/0F CMP\r\n"
        "Default fields are 4/16/16-bit binary; -hex mode uses 2/4/4 hex digits.\r\n"
        "Pipe mode: 16g.exe -pipe  (\\\\.\\pipe\\16g)\r\n";
    WriteStandard(false, help);
}

void ConsoleWorker() {
    WriteStandard(false,
        "16g Mini GPU\r\n"
        "Resolution: 256x256 cells\r\n"
        "Cell scale: 2x2 pixels (512x512 display)\r\n"
        "Color format: RGB565\r\n"
        "Type HELP for commands.\r\n\r\n");
    std::string line;
    while (g_running.load()) {
        WriteStandard(false, "16g> ");
        if (!std::getline(std::cin, line)) {
            PostCloseWindow();
            return;
        }
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = TrimAscii(line);
        if (line.empty()) continue;

        const std::string upper = UpperAscii(line);
        if (upper == "HELP") {
            PrintHelp();
            continue;
        }
        if (upper == "CLEAR") {
            g_machine.Clear();
            continue;
        }
        if (upper == "EXIT" || upper == "QUIT") {
            PostCloseWindow();
            return;
        }
        if (upper.size() >= 3 && upper.substr(0, 3) == "RUN" &&
            (upper.size() == 3 || std::isspace(static_cast<unsigned char>(upper[3])))) {
            std::string pathText = TrimAscii(line.substr(3));
            if (pathText.size() >= 2 && pathText.front() == '"' && pathText.back() == '"') {
                pathText = pathText.substr(1, pathText.size() - 2);
            }
            if (pathText.empty()) {
                PrintError("Usage: RUN <file.16g>");
                continue;
            }
            try {
                const std::wstring path = Utf8ToWide(pathText);
                if (!EndsWith16g(path)) throw std::runtime_error("RUN accepts only .16g files");
                const auto instructions = Load16gFile(path);
                RunInstructions(instructions, false, true, true);
            } catch (const std::exception& e) {
                PrintError(std::string("16g: ") + e.what());
            }
            continue;
        }

        try {
            const auto fields = SplitFields(line);
            const Instruction ins = ParseTextInstruction(fields, g_hexOutput.load());
            ExecuteAndReport(ins, true, true);
        } catch (const std::exception& e) {
            PrintError(std::string("16g: ") + e.what());
        }
    }
}

void ShowUsage() {
    WriteStandard(true,
        "Usage:\r\n"
        "  16g.exe                         interactive console + VGA\r\n"
        "  16g.exe -hex                    interactive hexadecimal mode\r\n"
        "  16g.exe <OP> <A> <B>            run one binary instruction\r\n"
        "  16g.exe -hex <OP> <A> <B>       run one hex instruction\r\n"
        "  16g.exe [ -hex ] <file.16g>     run a binary file\r\n"
        "  16g.exe [ -hex ] -loop <file.16g> repeat a file\r\n"
        "  16g.exe -pipe                   host the named pipe (\\\\.\\pipe\\16g)\r\n");
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc <= 0) {
        PrintError("16g: failed to parse the command line.", true);
        return 2;
    }

    bool hex = false;
    bool loop = false;
    bool pipeMode = false;
    std::vector<std::wstring> positional;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        std::wstring lower = arg;
        for (wchar_t& c : lower) c = static_cast<wchar_t>(towlower(c));
        if (lower == L"-hex") hex = true;
        else if (lower == L"-loop") loop = true;
        else if (lower == L"-pipe") pipeMode = true;
        else positional.push_back(arg);
    }
    LocalFree(argv);
    g_hexOutput.store(hex);

    enum class Mode { Interactive, Single, File, Pipe };
    Mode mode = Mode::Interactive;
    Instruction single{};
    std::vector<Instruction> program;
    try {
        if (pipeMode) {
            if (loop || !positional.empty()) throw std::runtime_error("-pipe cannot be combined with a command or file");
            mode = Mode::Pipe;
        } else if (loop) {
            if (positional.size() != 1 || !EndsWith16g(positional[0])) {
                throw std::runtime_error("-loop requires exactly one .16g file");
            }
            program = Load16gFile(positional[0]);
            mode = Mode::File;
        } else if (positional.empty()) {
            mode = Mode::Interactive;
        } else if (positional.size() == 1 && EndsWith16g(positional[0])) {
            program = Load16gFile(positional[0]);
            mode = Mode::File;
        } else if (positional.size() == 3) {
            std::vector<std::string> fields;
            fields.reserve(3);
            for (const auto& field : positional) {
                // Command-line fields must be ASCII binary/hex digits.
                std::string narrow;
                for (wchar_t c : field) {
                    if (c > 0x7F) throw std::runtime_error("instruction fields must use ASCII digits");
                    narrow.push_back(static_cast<char>(c));
                }
                fields.push_back(narrow);
            }
            single = ParseTextInstruction(fields, hex);
            mode = Mode::Single;
        } else {
            throw std::runtime_error("unrecognized command-line arguments");
        }
    } catch (const std::exception& e) {
        ShowUsage();
        PrintError(std::string("16g: ") + e.what(), true);
        return 2;
    }

    if (mode == Mode::Interactive) {
        if (!AllocConsole()) {
            PrintError("16g: could not allocate the interactive console.", true);
            return 3;
        }
        SetConsoleCP(CP_UTF8);
        SetConsoleOutputCP(CP_UTF8);
        (void)freopen("CONIN$", "r", stdin);
        (void)freopen("CONOUT$", "w", stdout);
        (void)freopen("CONOUT$", "w", stderr);
        std::ios::sync_with_stdio(true);
    }

    if (!CreateVgaWindow(instance)) {
        PrintError("16g: could not create the VGA window.", true);
        return 4;
    }

    switch (mode) {
    case Mode::Interactive:
        std::thread(ConsoleWorker).detach();
        break;
    case Mode::Single:
        std::thread([single]() {
            ExecuteAndReport(single, true, true);
        }).detach();
        break;
    case Mode::File: {
        std::thread([program = std::move(program), loop]() {
            RunInstructions(program, loop, true, true);
        }).detach();
        break;
    }
    case Mode::Pipe:
        std::thread(PipeServerWorker).detach();
        break;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    g_running.store(false);
    return static_cast<int>(msg.wParam);
}
