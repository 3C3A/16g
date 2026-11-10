# 16g Retro Tile Mosaic
# A 16x16 pixel checker mosaic in deep blue and bright cyan. The checker phase flips every 16 frames.
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

# Tile coordinates and a slowly changing one-bit phase
SHFL 2, 0
SHR 0, 4
SHFL 2, 0
SHR 0, 4
SHFL 5, 0
SHR 0, 4
SHFL 3, 2
XOR 0, 0
SHFL 1, 2
XOR 0, 0
SHFL 1, 0
AND 0, 1
SHFL 1, 0
MUL 0, 2016
SHFL 1, 0
ADD 0, 31

# Coordinate = (Y << 8) | X; draw this pixel
SHFL 9, 0
SHL 0, 8
SHFL 1, 11
OR 0, 0
SHFL 1, 3
VGA 0, 0

# Advance C; increment T only when C wraps after 65536 pixels
SHFL 14, 0
ADD 0, 1
SHFL 15, 0
CMP 0, 65535
SHFL 15, 1
ADD 0, 0
SHFL 1, 0
AND 0, 255
SHFL 4, 0
PASS 0, 0
SHFL 2, 0
PASS 0, 0
