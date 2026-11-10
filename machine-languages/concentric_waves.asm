# 16g Animated Concentric Waves
# Intensity = (((X-128)^2 + (Y-128)^2 + 4*T) mod 256). Concentric RGB565 grayscale rings move outward as T advances.
# One pixel per -loop iteration; framebuffer and history persist.
# Run: 16g.exe -loop <file.16g> >nul 2>nul

SHFL 2, 0
PASS 0, 0
SHFL 2, 0
PASS 0, 0
SHFL 2, 0
AND 0, 255
SHFL 3, 0
SHR 0, 8
SHFL 2, 0
SUB 0, 128
SHFL 1, 1
MUL 0, 0
SHFL 3, 0
SUB 0, 128
SHFL 1, 1
MUL 0, 0
SHFL 3, 1
ADD 0, 0
SHFL 8, 0
MUL 0, 4
SHFL 2, 1
ADD 0, 0
SHFL 1, 0
AND 0, 255
SHFL 0, 1
BRIGHT 65535, 0
SHFL 10, 0
SHL 0, 8
SHFL 1, 12
OR 0, 0
SHFL 1, 3
VGA 0, 0
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
