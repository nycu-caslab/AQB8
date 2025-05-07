#define NUM_COLORS 20
#include <stdio.h>
#include "xxhash.h"

static unsigned int encode(unsigned int x) {
    unsigned int sign = (x & (1 << 31)) ? 1 : 0;
    unsigned int exponent = (x >> 23) & 0b11111111;
    unsigned int mantissa = x & ((1 << 23) - 1);

    unsigned int new_exponent = (exponent - (127 + 7)) & 0b11111;
    unsigned int new_mantissa = 0b10000000 | (mantissa >> 16);

    return (sign << 13) | (new_exponent << 8) | new_mantissa;
}

void encode_and_hash(int size,
                     unsigned int* x,
                     unsigned int* y,
                     unsigned int* z,
                     unsigned int* c) {
    for (int i = 0; i < size; i++) {
        unsigned int ex = encode(x[i]);
        unsigned int ey = encode(y[i]);
        unsigned int ez = encode(z[i]);
        unsigned long long encoded = ex;
        encoded = (encoded << 14) | ey;
        encoded = (encoded << 14) | ez;
        _Bool quant_type = (ex & (1 << 8)) ^ (ey & (1 << 8)) ^ (ez & (1 << 8));
        XXH32_hash_t hash = XXH32(&encoded, sizeof(encoded), 0);
        c[i] = hash % NUM_COLORS;
        if (quant_type)
            c[i] += NUM_COLORS;
    }
}
