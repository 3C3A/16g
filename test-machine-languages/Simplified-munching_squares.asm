# Moving Munching Squares
# One pixel per -loop iteration.
# Every full-screen scan changes the XOR phase.

# Restore state from result history:
# history[1] = phase
# history[2] = pixel position
SHFL 2, 0
PASS 0, 0

SHFL 2, 0
PASS 0, 0

# X = position & 255
SHFL 2, 0
AND 0, 255

# Y = position >> 8
SHFL 3, 0
SHR 0, 8

# K = X XOR Y
SHFL 2, 1
XOR 0, 0

# K = K XOR phase
SHFL 1, 4
XOR 0, 0

# CMP(85, K): result is 2 when K < 85
SHFL 0, 1
CMP 85, 0

# Convert comparison result to 1 if K < 85, otherwise 0
SHFL 1, 0
CMP 0, 2

# Color: 0xFFFF if true, otherwise 0x0000
SHFL 0, 1
SUB 0, 0

# Draw at the current packed YX coordinate
SHFL 9, 1
VGA 0, 0

# Advance pixel position: 0x0000 -> 0xFFFF
SHFL 9, 0
ADD 0, 1

# Carry = 1 only when the previous position was 0xFFFF
SHFL 10, 0
CMP 0, 65535

# Increment phase only when the screen scan wraps
SHFL 10, 1
ADD 0, 0

SHFL 1, 0
AND 0, 255

# Arrange persistent state for the next -loop iteration
SHFL 4, 0
PASS 0, 0

SHFL 2, 0
PASS 0, 0