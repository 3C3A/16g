# 16g Animated Quadratic XOR
# Intensity = (((X*X) XOR (Y*Y) + 3*T) mod 256). Squared coordinates create a symmetric binary interference texture.
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
SHFL 2, 2
MUL 0, 0
SHFL 2, 2
MUL 0, 0
SHFL 2, 1
XOR 0, 0
SHFL 6, 0
MUL 0, 3
SHFL 2, 1
ADD 0, 0
SHFL 1, 0
AND 0, 255
SHFL 0, 1
BRIGHT 65535, 0
SHFL 8, 0
SHL 0, 8
SHFL 1, 10
OR 0, 0
SHFL 1, 3
VGA 0, 0
SHFL 13, 0
ADD 0, 1
SHFL 14, 0
CMP 0, 65535
SHFL 14, 1
ADD 0, 0
SHFL 1, 0
AND 0, 255
SHFL 4, 0
PASS 0, 0
SHFL 2, 0
PASS 0, 0
