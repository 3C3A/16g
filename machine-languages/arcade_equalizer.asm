# 16g Arcade Equalizer
# A 32-column arcade equalizer: 8-pixel-wide bars, blue background, and teal-green columns.
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

# One height value per 8-pixel-wide bar
SHFL 2, 0
SHR 0, 3
SHFL 1, 0
MUL 0, 13
SHFL 5, 0
MUL 0, 5
SHFL 2, 1
ADD 0, 0
SHFL 1, 0
AND 0, 63
SHFL 1, 0
SHL 0, 2
SHFL 1, 7
CMP 0, 0
SHFL 1, 0
MUL 0, 992
SHFL 1, 0
OR 0, 31

# Coordinate = (Y << 8) | X; draw this pixel
SHFL 10, 0
SHL 0, 8
SHFL 1, 12
OR 0, 0
SHFL 1, 3
VGA 0, 0

# Advance C; increment T only when C wraps after 65536 pixels
SHFL 15, 0
ADD 0, 1
SHFL 16, 0
CMP 0, 65535
SHFL 16, 1
ADD 0, 0
SHFL 1, 0
AND 0, 255
SHFL 4, 0
PASS 0, 0
SHFL 2, 0
PASS 0, 0
