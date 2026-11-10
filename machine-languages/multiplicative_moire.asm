# 16g Animated Multiplicative Moire
# Intensity = ((X*Y + 5*T) mod 256). The modular product creates a changing cross-hatched moire field.
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
SHFL 2, 1
MUL 0, 0
SHFL 4, 0
MUL 0, 5
SHFL 2, 1
ADD 0, 0
SHFL 1, 0
AND 0, 255
SHFL 0, 1
BRIGHT 65535, 0
SHFL 6, 0
SHL 0, 8
SHFL 1, 8
OR 0, 0
SHFL 1, 3
VGA 0, 0
SHFL 11, 0
ADD 0, 1
SHFL 12, 0
CMP 0, 65535
SHFL 12, 1
ADD 0, 0
SHFL 1, 0
AND 0, 255
SHFL 4, 0
PASS 0, 0
SHFL 2, 0
PASS 0, 0
