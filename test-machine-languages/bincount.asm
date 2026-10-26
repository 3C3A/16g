# Binary Counting Visualization
# X = number (0..255)
# Y = bit index (0..7)
# White = bit 1, Black = bit 0
#
# One pixel is drawn per -loop iteration.
# 2048 iterations fill the 256 x 8 pattern.

# Recover the counter from the latest result.
# The first iteration starts from zero if empty history reads as zero.
SHFL 1, 0
PASS 0, 0

# Keep the counter in the range 0..2047.
SHFL 1, 0
MOD 0, 2048

# Y = counter % 8
SHFL 1, 0
MOD 0, 8

# X = counter / 8
SHFL 2, 0
DIV 0, 8

# Value = (X >> Y)
SHFL 1, 2
SHR 0, 0

# Bit = Value & 1
SHFL 1, 0
AND 0, 1

# Color = Bit * 65535
SHFL 1, 0
MUL 0, 65535

# Pack the pixel coordinate: (Y << 8) | X
SHFL 5, 0
SHL 0, 8

SHFL 1, 5
OR 0, 0

# Draw using packed YX coordinate and RGB565 color
SHFL 1, 3
VGA 0, 0

# Advance the counter for the next iteration
SHFL 8, 0
ADD 0, 1