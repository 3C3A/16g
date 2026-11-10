# Night City — Pixel Skyline
# A block-by-block city silhouette against a deep blue-violet sky.
# Warm window lights shift vertically once per completed screen scan.
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
SHR 0, 4
SHFL 1, 0
MUL 0, 37
SHFL 1, 0
AND 0, 63
SHFL 1, 0
ADD 0, 80
SHFL 5, 1
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
SHFL 11, 0
SHR 0, 1
SHFL 1, 0
ADD 0, 80
SHFL 0, 1
BRIGHT 18463, 0
SHFL 1, 4
AND 0, 0
BRIGHT 12684, 190
SHFL 1, 7
AND 0, 0
SHFL 3, 1
OR 0, 0
SHFL 19, 0
AND 0, 7
SHFL 1, 0
CMP 0, 3
SHFL 1, 0
AND 0, 1
SHFL 21, 23
ADD 0, 0
SHFL 1, 0
AND 0, 15
SHFL 1, 0
CMP 0, 5
SHFL 1, 0
AND 0, 1
SHFL 5, 1
AND 0, 0
SHFL 1, 17
AND 0, 0
SHFL 1, 0
MUL 0, 255
SHFL 0, 1
BRIGHT 65504, 0
SHFL 12, 1
OR 0, 0
SHFL 30, 0
SHL 0, 8
SHFL 1, 32
OR 0, 0
SHFL 1, 3
VGA 0, 0  # write one RGB565 pixel
SHFL 35, 0
ADD 0, 1
SHFL 36, 0
CMP 0, 65535
SHFL 36, 1
ADD 0, 0
SHFL 1, 0
AND 0, 255
SHFL 4, 0
PASS 0, 0
SHFL 2, 0
PASS 0, 0
