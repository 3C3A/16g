# Full-screen RGB565 color counter
# State:
#   history[1] = RGB565 color
#   history[2] = pixel index (0..65535)
#
# Each execution draws one pixel.
# After the last pixel, increment the color.

# X = pixel & 255
SHFL 2, 0
AND 0, 255

# Y = pixel >> 8
SHFL 3, 0
SHR 0, 8

# Y coordinate in the high byte
SHFL 1, 0
SHL 0, 8

# Pack coordinate: (Y << 8) | X
SHFL 1, 3
OR 0, 0

# Draw the pixel using the current RGB565 color
SHFL 1, 5
VGA 0, 0

# Next pixel = pixel + 1
SHFL 6, 0
ADD 0, 1

# Check whether the current pixel was 65535
SHFL 7, 0
CMP 0, 65535

# Next color = color + comparison result
# CMP returns 1 only when pixel == 65535
SHFL 7, 1
ADD 0, 0

# Save the next pixel as the second newest result
SHFL 3, 0
PASS 0, 0

# Save the next color as the newest result
SHFL 2, 0
PASS 0, 0