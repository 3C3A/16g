# 16g Pixel Matrix Rain
# A green terminal-style pixel rain. Glyph blocks are 4x8 pixels; each column advances at a different phase.
# One pixel per -loop iteration; state and framebuffer persist.
# Run: 16g.exe -loop <filename>.16g >nul 2>nul

SHFL 2, 0
PASS 0, 0
SHFL 2, 0
PASS 0, 0

# X = C & 255; Y = C >> 8
SHFL 2, 0
AND 0, 255
SHFL 3, 0
SHR 0, 8

# Coarse 4x8 glyph cells; each column gets its own falling phase
SHFL 2, 0
SHR 0, 2
SHFL 2, 0
SHR 0, 3
SHFL 2, 0
MUL 0, 73
SHFL 2, 0
MUL 0, 151
SHFL 4, 0
ADD 0, 1
SHFL 8, 1
MUL 0, 0
SHFL 4, 3
ADD 0, 0
SHFL 1, 2
ADD 0, 0
SHFL 1, 0
AND 0, 15
SHFL 1, 0
CMP 0, 0
SHFL 0, 1
SUB 2, 0
SHFL 14, 0
MUL 0, 5
SHFL 13, 1
ADD 0, 0
SHFL 1, 0
AND 0, 255
SHFL 4, 1
MUL 0, 0
SHFL 0, 1
BRIGHT 2016, 0

# Coordinate = (Y << 8) | X; draw this pixel
SHFL 17, 0
SHL 0, 8
SHFL 1, 19
OR 0, 0
SHFL 1, 3
VGA 0, 0

# Advance C; increment T only when C wraps after 65536 pixels
SHFL 22, 0
ADD 0, 1
SHFL 23, 0
CMP 0, 65535
SHFL 23, 1
ADD 0, 0
SHFL 1, 0
AND 0, 255
SHFL 4, 0
PASS 0, 0
SHFL 2, 0
PASS 0, 0
