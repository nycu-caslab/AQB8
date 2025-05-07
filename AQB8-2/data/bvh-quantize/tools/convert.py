import numpy as np
import sys

if len(sys.argv) != 2:
    print(f'usage {sys.argv[0]} NUM')
    exit(1)

num = np.floor(float(sys.argv[1]))
b = np.float32(num).view(np.uint32)

sign = 1 if b & (1 << 31) else 0
exponent = (b >> 23) & 0b011111111
mantissa = b & ((1 << 23) - 1)

low_exponent = (exponent - (127 + 7)) & 0b11111
low_mantissa = mantissa >> 16
low = (low_exponent << 7) | low_mantissa

high = (low - 1) if sign else (low + 1)
high_exponent = high >> 7
high_mantissa = high & 0b1111111

low_mantissa |= 0b10000000
high_mantissa |= 0b10000000

print(sign)
print(f'{low_exponent}')
print(f'{low_mantissa}')
print(f'{high_exponent}')
print(f'{high_mantissa}')
