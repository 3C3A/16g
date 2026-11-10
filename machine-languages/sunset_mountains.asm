# Sunset Mountain Range — Animated Terrain
# Jagged mountain silhouette with a warm gradient sky and a golden ridge contour.
# Terrain profile changes slightly after every complete 256x256 scan.
# Each file execution draws one pixel; use 16g.exe -loop <file>.16g

SHFL 2, 0
PASS 0, 0  # restore pixel counter
SHFL 2, 0
PASS 0, 0  # restore animation phase
SHFL 2, 0
AND 0, 255  # X = counter low byte
SHFL 3, 0
SHR 0, 8  # Y = counter high byte
SHFL 2, 0
MUL 0, 5
SHFL 3, 0
SHR 0, 2
SHFL 2, 1
XOR 0, 0
SHFL 1, 6
ADD 0, 0
SHFL 1, 0
AND 0, 63
SHFL 1, 0
ADD 0, 88
SHFL 7, 1
CMP 0, 0
SHFL 1, 0
CMP 0, 1
SHFL 1, 0
CMP 0, 0
SHFL 1, 0
SUB 0, 1
SHFL 1, 0
MUL 0, 65535
SHFL 0, 1
SUB 65535, 0
SHFL 13, 0
SHR 0, 1
SHFL 0, 1
SUB 255, 0
SHFL 0, 1
BRIGHT 64800, 0
SHFL 1, 4
AND 0, 0
SHFL 4, 0
ADD 0, 100
SHFL 0, 1
BRIGHT 8591, 0
SHFL 1, 8
AND 0, 0
SHFL 4, 1
OR 0, 0
SHFL 21, 15
CMP 0, 0
SHFL 1, 0
AND 0, 1
SHFL 1, 0
MUL 0, 255
SHFL 0, 1
BRIGHT 65504, 0
SHFL 5, 1
OR 0, 0
SHFL 26, 0
SHL 0, 8
SHFL 1, 28
OR 0, 0
SHFL 1, 3
VGA 0, 0  # write one RGB565 pixel
SHFL 31, 0
ADD 0, 1
SHFL 32, 0
CMP 0, 65535
SHFL 32, 1
ADD 0, 0
SHFL 1, 0
AND 0, 255
SHFL 4, 0
PASS 0, 0
SHFL 2, 0
PASS 0, 0
