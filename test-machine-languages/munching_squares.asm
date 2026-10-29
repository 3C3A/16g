# 16g Munching Squares: animated grayscale fractal
# Per-pixel intensity = ((X XOR Y) + T) mod 256
# C = 0..65535 scans the whole 256x256 screen.
# X = C & 255, Y = C >> 8.
# T changes once when C wraps after a complete screen pass.
# State is retained in the two newest history entries:
#   newest = T (0..255), second newest = C (0..65535)
# Run with: 16g.exe -loop munching_squares_loop.16g >nul 2>nul

# Restore C; then restore T. First launch initializes missing history to zero.
SHFL 2, 0
PASS 0, 0
SHFL 2, 0
PASS 0, 0

# X = C & 255
SHFL 2, 0
AND 0, 255

# Y = C >> 8
SHFL 3, 0
SHR 0, 8

# K = X XOR Y
SHFL 2, 1
XOR 0, 0

# I = (K + T) & 255
# At this point history order is: K, Y, X, T, C, ...
SHFL 1, 4
ADD 0, 0
SHFL 1, 0
AND 0, 255

# RGB565 grayscale: white scaled by I/255
SHFL 0, 1
BRIGHT 65535, 0

# Coordinate = (Y << 8) | X
# Before SHL, history is: gray, I, sum, K, Y, X, T, C, ...
SHFL 5, 0
SHL 0, 8
SHFL 1, 7
OR 0, 0

# Draw (X,Y) with computed grayscale
# Before VGA, history is: coord, Yhigh, gray, I, sum, K, Y, X, T, C, ...
SHFL 1, 3
VGA 0, 0

# Cnext = C + 1 (16-bit wrap)
SHFL 10, 0
ADD 0, 1

# Wrap = 1 iff old C == 65535; otherwise 0
SHFL 11, 0
CMP 0, 65535

# Tnext = (T + Wrap) & 255
# Before ADD, history is: Wrap, Cnext, coord, Yhigh, gray, I, sum, K, Y, X, T, C...
SHFL 1, 11
ADD 0, 0
SHFL 1, 0
AND 0, 255

# Re-publish state in required order: newest Tnext, second newest Cnext
SHFL 4, 0
PASS 0, 0
SHFL 2, 0
PASS 0, 0
