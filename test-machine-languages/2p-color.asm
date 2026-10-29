# RGB565: 2 pixels per color
# State at loop end:
#   history[1] = current color
#   history[2] = next pixel index
#
# Each loop draws 2 pixels, increments color,
# advances the pixel index by 2, then saves state.

# ----- First pixel: P -----

# X = P & 255
SHFL 2, 0
AND 0, 255

# Y = P >> 8
SHFL 3, 0
SHR 0, 8

# Pack YX coordinate
SHFL 1, 0
SHL 0, 8

SHFL 1, 3
OR 0, 0

# Draw with current color
SHFL 1, 5
VGA 0, 0

# ----- Second pixel: P + 1 -----

SHFL 6, 0
ADD 0, 1

# X = (P + 1) & 255
SHFL 1, 0
AND 0, 255

# Y = (P + 1) >> 8
SHFL 2, 0
SHR 0, 8

# Pack YX coordinate
SHFL 1, 0
SHL 0, 8

SHFL 1, 3
OR 0, 0

# Draw with the same color
SHFL 1, 10
VGA 0, 0

# ----- Advance state -----

# Next pixel index = P + 2
SHFL 11, 0
ADD 0, 2

# Next color = current color + 1
SHFL 11, 0
ADD 0, 1

# Save next pixel index
SHFL 2, 0
PASS 0, 0

# Save next color
SHFL 2, 0
PASS 0, 0