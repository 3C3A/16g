# Blue Hour Ocean — Rolling Horizon
# Warm evening sky, a moving stepped horizon, textured blue water, and a circular sun.
# The water texture and horizon phase advance after each complete screen scan.
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
MUL 0, 3
SHFL 4, 0
MUL 0, 2
SHFL 2, 1
ADD 0, 0
SHFL 1, 0
AND 0, 31
SHFL 1, 0
ADD 0, 112
SHFL 6, 1
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
SHFL 12, 0
SHR 0, 1
SHFL 0, 1
SUB 240, 0
SHFL 0, 1
BRIGHT 64800, 0
SHFL 1, 4
AND 0, 0
SHFL 17, 0
MUL 0, 7
SHFL 17, 0
MUL 0, 3
SHFL 20, 0
MUL 0, 5
SHFL 3, 2
ADD 0, 0
SHFL 1, 2
ADD 0, 0
SHFL 1, 0
AND 0, 95
SHFL 1, 0
ADD 0, 160
SHFL 0, 1
BRIGHT 1023, 0
SHFL 1, 14
AND 0, 0
SHFL 10, 1
OR 0, 0
SHFL 27, 0
SUB 0, 128
SHFL 1, 1
MUL 0, 0
SHFL 28, 0
SUB 0, 76
SHFL 1, 1
MUL 0, 0
SHFL 3, 1
ADD 0, 0
SHFL 1, 0
CMP 0, 626
SHFL 1, 0
CMP 0, 0
SHFL 0, 1
SUB 2, 0
SHFL 1, 0
MUL 0, 255
SHFL 0, 1
BRIGHT 65504, 0
SHFL 1, 25
AND 0, 0
SHFL 12, 1
OR 0, 0
SHFL 38, 0
SHL 0, 8
SHFL 1, 40
OR 0, 0
SHFL 1, 3
VGA 0, 0  # write one RGB565 pixel
SHFL 43, 0
ADD 0, 1
SHFL 44, 0
CMP 0, 65535
SHFL 44, 1
ADD 0, 0
SHFL 1, 0
AND 0, 255
SHFL 4, 0
PASS 0, 0
SHFL 2, 0
PASS 0, 0
