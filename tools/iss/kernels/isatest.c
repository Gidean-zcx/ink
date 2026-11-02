/* Differential ISA test: the same program is compiled for the host and for
 * the iss target; both must print the same checksum. Exercises RV32IM (incl.
 * MULH, MULHU, MULHSU, DIV, REM), Zba/Zbb/Zbs patterns (when enabled), sub-word and
 * misaligned memory accesses and single-precision float (add/sub/mul/div/
 * sqrt/fma/min/max/conversions/compares). Only operations with fully defined
 * results in C are used (no division by zero, no out-of-range conversions,
 * NaNs canonicalized). Build both sides with -ffp-contract=off. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t h = 2166136261u;
static void mix(uint32_t v) { h = (h ^ v) * 16777619u; }
static uint32_t fbits(float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return f != f ? 0x7fc00000u : u;
}

int main(void) {
    volatile uint32_t seed = 12345u; /* volatile: keep the compiler from folding everything */
    uint32_t x = seed;
    for (int i = 0; i < 3000; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        uint32_t a = x, b = (x * 2654435761u) | 1u, c = x >> (i & 31);
        if (i % 7 == 0) b = 0xffffffffu - (uint32_t)i;
        int32_t sa = (int32_t)a, sb = (int32_t)b;
        mix(a + b);
        mix(a - b);
        mix(a * b);
        mix((uint32_t)(((int64_t)sa * sb) >> 32));
        mix((uint32_t)(((uint64_t)a * b) >> 32));
        mix((uint32_t)(((int64_t)sa * (int64_t)(uint64_t)b) >> 32));
        mix(a / b);
        mix(a % b);
        if (!(sa == INT32_MIN && sb == -1)) {
            mix((uint32_t)(sa / sb));
            mix((uint32_t)(sa % sb));
        }
        mix(a << (b & 31));
        mix(a >> (b & 31));
        mix((uint32_t)(sa >> (b & 31)));
        mix((uint32_t)(sa < sb));
        mix((uint32_t)(a < b));
        mix((uint32_t)__builtin_clz(a | 1u));
        mix((uint32_t)__builtin_ctz(a | 0x80000000u));
        mix((uint32_t)__builtin_popcount(a));
        mix(__builtin_bswap32(a));
        mix((a << (c & 31)) | (a >> ((32 - (c & 31)) & 31)));
        mix(a & ~b);
        mix(a | ~b);
        mix(~(a ^ b));
        mix(sa < sb ? a : b);
        mix(a > b ? a : b);
        mix((uint32_t)(int32_t)(int8_t)a);
        mix((uint32_t)(int32_t)(int16_t)a);
        mix(a & 0xffffu);
        mix(a & ~(1u << (b & 31)));
        mix((a >> (b & 31)) & 1u);
        mix(a ^ (1u << (b & 31)));
        mix(a | (1u << (b & 31)));
        mix((a << 1) + b);
        mix((a << 2) + b);
        mix((a << 3) + b);
        /* memory: sub-word and misaligned */
        uint8_t buf[16];
        memset(buf, (int)(a & 0xff), sizeof buf);
        memcpy(buf, &a, 4);
        memcpy(buf + 5, &b, 4);
        uint32_t u;
        memcpy(&u, buf + 3, 4);
        mix(u);
        int16_t hh;
        memcpy(&hh, buf + 1, 2);
        mix((uint32_t)(int32_t)hh);
        mix((uint32_t)(int32_t)(int8_t)buf[6]);
        /* single precision */
        float fa = (float)(int32_t)a / 65536.0f, fb = (float)(b >> 8) / 1024.0f + 0.5f;
        mix(fbits(fa + fb));
        mix(fbits(fa - fb));
        mix(fbits(fa * fb));
        mix(fbits(fa / fb));
        mix(fbits(sqrtf(fb)));
        mix(fbits(fmaf(fa, fb, 1.25f)));
        mix(fbits(fminf(fa, fb)));
        mix(fbits(fmaxf(fa, fb)));
        mix((uint32_t)(int32_t)fa);
        mix((uint32_t)fb);
        mix(fbits((float)sa));
        mix(fbits((float)a));
        mix((uint32_t)(fa < fb));
        mix((uint32_t)(fa <= fb));
        mix((uint32_t)(fa == fb));
        mix(fbits(-fa));
        mix(fbits(fabsf(fa)));
        mix(fbits(copysignf(fb, fa)));
        mix((uint32_t)lrintf(fa));
        mix((uint32_t)(int32_t)roundf(fb * 3.3f));
        mix((uint32_t)(int32_t)floorf(fa));
    }
    printf("isatest %08lx\n", (unsigned long)h);
    return 0;
}
