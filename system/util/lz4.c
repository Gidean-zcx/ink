/*
 * LZ4 block format (see ink/sys/lz4.h). A sequence is a token (literal length
 * high nibble, match length - 4 low nibble; 15 = more bytes follow, each
 * adding up to 255), the literals, a 2-byte little-endian offset and the
 * match. The last sequence has literals only; the format asks that the last
 * 5 bytes are literals and no match starts within the last 12.
 */
#include <string.h>

#include "ink/sys/lz4.h"

int ink_lz4_decode(const uint8_t *src, size_t n, uint8_t *dst, size_t cap) {
    size_t ip = 0, op = 0;
    while (ip < n) {
        unsigned token = src[ip++];
        size_t lit = token >> 4;
        if (lit == 15) {
            unsigned b;
            do {
                if (ip >= n) return -1;
                b = src[ip++];
                lit += b;
            } while (b == 255);
        }
        if (lit > n - ip || lit > cap - op) return -1;
        memcpy(dst + op, src + ip, lit);
        ip += lit;
        op += lit;
        if (ip == n) break; /* the last sequence: literals only */
        if (n - ip < 2) return -1;
        size_t off = (size_t)src[ip] | (size_t)src[ip + 1] << 8;
        ip += 2;
        if (!off || off > op) return -1;
        size_t len = (token & 15) + 4;
        if ((token & 15) == 15) {
            unsigned b;
            do {
                if (ip >= n) return -1;
                b = src[ip++];
                len += b;
            } while (b == 255);
        }
        if (len > cap - op) return -1;
        for (size_t i = 0; i < len; i++, op++) dst[op] = dst[op - off]; /* may overlap: byte by byte */
    }
    return (int)op;
}

size_t ink_lz4_bound(size_t n) { return n + n / 255 + 16; }

static int put_len(uint8_t *dst, size_t *op, size_t cap, size_t v) { /* the bytes after a 15 nibble */
    for (; v >= 255; v -= 255) {
        if (*op >= cap) return -1;
        dst[(*op)++] = 255;
    }
    if (*op >= cap) return -1;
    dst[(*op)++] = (uint8_t)v;
    return 0;
}

static int emit(uint8_t *dst, size_t *op, size_t cap, const uint8_t *lit, size_t nlit, size_t off, size_t mlen) {
    if (*op >= cap) return -1;
    size_t t = *op;
    (*op)++;
    unsigned hi = nlit >= 15 ? 15 : (unsigned)nlit, lo = 0;
    if (nlit >= 15 && put_len(dst, op, cap, nlit - 15)) return -1;
    if (nlit > cap - *op) return -1;
    memcpy(dst + *op, lit, nlit);
    *op += nlit;
    if (mlen) {
        if (cap - *op < 2) return -1;
        dst[(*op)++] = (uint8_t)off;
        dst[(*op)++] = (uint8_t)(off >> 8);
        size_t m = mlen - 4;
        lo = m >= 15 ? 15 : (unsigned)m;
        if (m >= 15 && put_len(dst, op, cap, m - 15)) return -1;
    }
    dst[t] = (uint8_t)(hi << 4 | lo);
    return 0;
}

int ink_lz4_encode(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, void *work) {
    enum { HASH_BITS = 12 };
    uint32_t *table = work; /* INK_LZ4_WORK bytes: last position + 1 of each 4-byte hash (0 = none) */
    memset(table, 0, sizeof(uint32_t) << HASH_BITS);
    size_t ip = 0, anchor = 0, op = 0;
    size_t limit = n > 12 ? n - 12 : 0; /* no match starts in the last 12 bytes */
    while (ip < limit) {
        uint32_t seq;
        memcpy(&seq, src + ip, 4);
        uint32_t h = (seq * 2654435761u) >> (32 - HASH_BITS);
        size_t ref = table[h];
        table[h] = (uint32_t)ip + 1;
        if (ref && ip - (ref - 1) <= 0xFFFF && !memcmp(src + ref - 1, src + ip, 4)) {
            ref--;
            size_t len = 4;
            while (ip + len < n - 5 && src[ref + len] == src[ip + len]) len++; /* the last 5 stay literals */
            if (emit(dst, &op, cap, src + anchor, ip - anchor, ip - ref, len)) return -1;
            ip += len;
            anchor = ip;
        } else ip++;
    }
    if (emit(dst, &op, cap, src + anchor, n - anchor, 0, 0)) return -1;
    return (int)op;
}
