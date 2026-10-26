#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
16gasm.py - 16g machine-code assembler / disassembler
Python 3.x, standard library only.

Binary format:
    OP(1 byte) INPUT1(2 bytes, big-endian)
    INPUT2(2 bytes, big-endian)
    0x26 separates records; no trailing separator.

Usage:
    py 16gasm.py program.asm
    py 16gasm.py program.asm -o program.16g
    py 16gasm.py --disassemble program.16g
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


SEPARATOR = 0x26
MAX_U16 = 0xFFFF

OPCODES = {
    "PASS":   0x0,
    "ADD":    0x1,
    "SUB":    0x2,
    "MUL":    0x3,
    "DIV":    0x4,
    "MOD":    0x5,
    "VGA":    0x6,
    "SHFL":   0x7,
    "AND":    0x8,
    "OR":     0x9,
    "XOR":    0xA,
    "SHL":    0xB,
    "SHR":    0xC,
    "BLEND":  0xD,
    "BRIGHT": 0xE,
    "CMP":    0xF,
}

NAMES_BY_OPCODE = {
    value: name for name, value in OPCODES.items()
}


class AssemblyError(Exception):
    """Assembly error with optional source line number."""

    def __init__(self, message: str, line: int | None = None):
        super().__init__(message)
        self.line = line

    def __str__(self) -> str:
        if self.line is not None:
            return f"Line {self.line}: {super().__str__()}"
        return super().__str__()


def remove_comment(line: str) -> str:
    """Remove #, ; or // comments."""
    positions = []

    for marker in ("#", ";", "//"):
        index = line.find(marker)
        if index >= 0:
            positions.append(index)

    if positions:
        line = line[:min(positions)]

    return line.strip()


def parse_number(token: str, line_number: int) -> int:
    """
    Supported forms:
        123       decimal
        0x007B    hexadecimal
        0b1111011 binary
        0o173     octal

    Values must be unsigned 16-bit integers.
    """
    token = token.strip().replace("_", "")

    if not token:
        raise AssemblyError("Missing numeric operand.", line_number)

    try:
        if token.lower().startswith(("0x", "0b", "0o")):
            value = int(token, 0)
        elif re.fullmatch(r"[0-9]+", token):
            # Decimal with leading zeros is allowed.
            value = int(token, 10)
        else:
            raise ValueError("Unsupported numeric format")
    except ValueError:
        raise AssemblyError(
            f"Invalid number: {token!r}. "
            "Use decimal, 0x hexadecimal, 0b binary or 0o octal.",
            line_number,
        )

    if not 0 <= value <= MAX_U16:
        raise AssemblyError(
            f"Value {token!r} is outside the 16-bit range "
            "(0..65535).",
            line_number,
        )

    return value


def parse_operands(text: str, line_number: int) -> tuple[int, int]:
    """Parse exactly two operands, comma-separated or whitespace-separated."""
    text = text.strip()

    if not text:
        raise AssemblyError(
            "Two operands are required: INPUT1, INPUT2.",
            line_number,
        )

    if "," in text:
        parts = text.split(",")
        if len(parts) != 2 or not parts[0].strip() or not parts[1].strip():
            raise AssemblyError(
                "Expected exactly two operands, for example: "
                "ADD 10, 3",
                line_number,
            )
    else:
        parts = text.split()
        if len(parts) != 2:
            raise AssemblyError(
                "Expected exactly two operands, for example: "
                "ADD 10 3",
                line_number,
            )

    a = parse_number(parts[0], line_number)
    b = parse_number(parts[1], line_number)
    return a, b


def encode_record(
    opcode: int,
    input1: int,
    input2: int,
    line_number: int,
) -> bytes:
    """Encode one 5-byte machine instruction."""
    record = bytes((
        opcode,
        (input1 >> 8) & 0xFF,
        input1 & 0xFF,
        (input2 >> 8) & 0xFF,
        input2 & 0xFF,
    ))

    # 0x26 is used as the record separator.
    # It cannot also appear inside a record in the current file format.
    for offset, byte in enumerate(record):
        if byte == SEPARATOR:
            field_names = {
                1: "INPUT1 high byte",
                2: "INPUT1 low byte",
                3: "INPUT2 high byte",
                4: "INPUT2 low byte",
            }
            field = field_names.get(offset, "opcode")
            raise AssemblyError(
                f"{field} becomes 0x26, which conflicts with the "
                "record separator. The current 16g binary format "
                "cannot encode this value directly. "
                "Calculate the value at runtime using instructions instead.",
                line_number,
            )

    return record


def assemble(source: str) -> tuple[bytes, int]:
    """Compile assembly source into a separator-delimited .16g byte stream."""
    records: list[bytes] = []

    for line_number, original_line in enumerate(source.splitlines(), 1):
        line = remove_comment(original_line)

        if not line:
            continue

        match = re.match(
            r"^([A-Za-z][A-Za-z0-9_]*)(?:\s+(.*))?$",
            line,
        )

        if not match:
            raise AssemblyError(
                "Invalid syntax. Expected: MNEMONIC INPUT1, INPUT2.",
                line_number,
            )

        mnemonic = match.group(1).upper()
        operand_text = match.group(2) or ""

        if mnemonic not in OPCODES:
            valid = ", ".join(OPCODES.keys())
            raise AssemblyError(
                f"Unknown instruction {mnemonic!r}. "
                f"Supported instructions: {valid}",
                line_number,
            )

        input1, input2 = parse_operands(
            operand_text,
            line_number,
        )

        record = encode_record(
            OPCODES[mnemonic],
            input1,
            input2,
            line_number,
        )

        records.append(record)

    if not records:
        raise AssemblyError("No instructions found in source file.")

    machine_code = bytes([SEPARATOR]).join(records)
    return machine_code, len(records)


def disassemble(data: bytes) -> str:
    """Decode a .16g file back into readable assembly."""
    if not data:
        raise AssemblyError("The input binary is empty.")

    chunks = data.split(bytes([SEPARATOR]))
    output: list[str] = []

    for index, chunk in enumerate(chunks, 1):
        if len(chunk) != 5:
            raise AssemblyError(
                f"Record {index} has {len(chunk)} bytes; "
                "exactly 5 bytes are required. "
                "The file may contain a separator collision or be corrupted."
            )

        opcode = chunk[0]

        if opcode not in NAMES_BY_OPCODE:
            raise AssemblyError(
                f"Record {index} has invalid opcode 0x{opcode:02X}."
            )

        input1 = (chunk[1] << 8) | chunk[2]
        input2 = (chunk[3] << 8) | chunk[4]
        mnemonic = NAMES_BY_OPCODE[opcode]

        extra = ""

        if mnemonic == "VGA":
            x = input1 & 0xFF
            y = (input1 >> 8) & 0xFF
            extra = f"    ; X={x}, Y={y}, RGB565=0x{input2:04X}"
        elif mnemonic == "SHFL":
            extra = (
                "    ; History index: 0=literal, 1=latest result"
            )

        output.append(
            f"{mnemonic:<7} 0x{input1:04X}, 0x{input2:04X}{extra}"
        )

    return "\n".join(output)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="16g machine-code compiler and disassembler."
    )

    parser.add_argument(
        "source",
        nargs="?",
        help="Input assembly source file.",
    )
    parser.add_argument(
        "-o",
        "--output",
        help="Output .16g binary filename.",
    )
    parser.add_argument(
        "--disassemble",
        metavar="FILE",
        help="Disassemble an existing .16g file.",
    )

    args = parser.parse_args()

    try:
        if args.disassemble:
            if args.source or args.output:
                parser.error(
                    "--disassemble cannot be combined with "
                    "a source file or -o."
                )

            input_path = Path(args.disassemble)
            data = input_path.read_bytes()
            print(disassemble(data))
            return 0

        if not args.source:
            parser.error(
                "Provide an assembly source file, or use --disassemble."
            )

        source_path = Path(args.source)
        source_text = source_path.read_text(encoding="utf-8-sig")

        machine_code, instruction_count = assemble(source_text)

        if args.output:
            output_path = Path(args.output)
        else:
            output_path = source_path.with_suffix(".16g")

        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_bytes(machine_code)

        print("16g compilation successful")
        print(f"Source       : {source_path}")
        print(f"Output       : {output_path}")
        print(f"Instructions : {instruction_count}")
        print(f"File size    : {len(machine_code)} bytes")

        return 0

    except (OSError, UnicodeError, AssemblyError) as exc:
        print(f"16gasm error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())