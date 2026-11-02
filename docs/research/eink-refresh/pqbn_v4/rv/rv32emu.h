// Minimal RV32IM interpreter for counting what a kernel costs on a small in-order RISC-V core.
// Counts: instructions, loads/stores (+ bytes per registered memory region), taken branches, mul/div.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RV_MEM (48u << 20)            // 48 MB flat memory
#define RV_MAXREG 16

typedef struct { uint32_t lo, hi; const char *name; uint64_t rd, wr; } rv_region;

typedef struct {
    uint32_t x[32], pc;
    uint8_t *m;
    uint64_t instr, loads, stores, br_taken, br_not, jumps, muldiv, ld_use;
    rv_region reg[RV_MAXREG]; int nreg;
    int last_load_rd;                 // for a simple load-use stall estimate
    uint32_t *pch;                    // optional per-instruction-address execution histogram (index (pc-0x1000)/4)
} rv_cpu;

static inline void rv_add_region(rv_cpu *c, uint32_t lo, uint32_t hi, const char *name) {
    rv_region *r = &c->reg[c->nreg++]; r->lo = lo; r->hi = hi; r->name = name; r->rd = r->wr = 0;
}
static inline void rv_count(rv_cpu *c, uint32_t a, int n, int wr) {
    for (int i = 0; i < c->nreg; i++)
        if (a >= c->reg[i].lo && a < c->reg[i].hi) { if (wr) c->reg[i].wr += n; else c->reg[i].rd += n; return; }
}

static inline int32_t sx(uint32_t v, int bits) { return (int32_t)(v << (32 - bits)) >> (32 - bits); }

// Run from entry with up to 4 args until it returns to the magic address.
static uint32_t rv_call(rv_cpu *c, uint32_t entry, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t sp) {
    const uint32_t MAGIC = 0xFFFFFFF0u;
    memset(c->x, 0, sizeof c->x);
    c->x[1] = MAGIC; c->x[2] = sp; c->x[10] = a0; c->x[11] = a1; c->x[12] = a2; c->x[13] = a3;
    c->pc = entry; c->last_load_rd = -1;
    uint8_t *M = c->m;
    for (;;) {
        if (c->pc == MAGIC) return c->x[10];
        if (c->pc + 4 > RV_MEM) { fprintf(stderr, "pc out of range %08x\n", c->pc); exit(1); }
        uint32_t in; memcpy(&in, M + c->pc, 4);
        c->instr++; if (c->pch) c->pch[(c->pc - 0x1000) >> 2]++;
        const uint32_t op = in & 0x7f, rd = (in >> 7) & 31, f3 = (in >> 12) & 7, rs1 = (in >> 15) & 31, rs2 = (in >> 20) & 31, f7 = in >> 25;
        if (c->last_load_rd > 0 && ((int)rs1 == c->last_load_rd || ((op == 0x33 || op == 0x23 || op == 0x63) && (int)rs2 == c->last_load_rd)))
            c->ld_use++;
        c->last_load_rd = -1;
        const uint32_t v1 = c->x[rs1], v2 = c->x[rs2];
        uint32_t npc = c->pc + 4, res = 0; int wr = 1;
        switch (op) {
        case 0x37: res = in & 0xfffff000u; break;
        case 0x17: res = c->pc + (in & 0xfffff000u); break;
        case 0x6f: { int32_t imm = sx(((in >> 31) << 20) | (((in >> 12) & 0xff) << 12) | (((in >> 20) & 1) << 11) | (((in >> 21) & 0x3ff) << 1), 21);
                     res = c->pc + 4; npc = c->pc + imm; c->jumps++; } break;
        case 0x67: res = c->pc + 4; npc = (v1 + (uint32_t)sx(in >> 20, 12)) & ~1u; c->jumps++; break;
        case 0x63: { int32_t imm = sx(((in >> 31) << 12) | (((in >> 7) & 1) << 11) | (((in >> 25) & 0x3f) << 5) | (((in >> 8) & 0xf) << 1), 13);
                     int t = 0;
                     switch (f3) { case 0: t = v1 == v2; break; case 1: t = v1 != v2; break; case 4: t = (int32_t)v1 < (int32_t)v2; break;
                                   case 5: t = (int32_t)v1 >= (int32_t)v2; break; case 6: t = v1 < v2; break; case 7: t = v1 >= v2; break;
                                   default: fprintf(stderr, "bad branch\n"); exit(1); }
                     if (t) { npc = c->pc + imm; c->br_taken++; } else c->br_not++;
                     wr = 0; } break;
        case 0x03: { uint32_t a = v1 + (uint32_t)sx(in >> 20, 12);
                     if (a >= RV_MEM) { fprintf(stderr, "load oob %08x pc %08x\n", a, c->pc); exit(1); }
                     switch (f3) { case 0: res = (uint32_t)(int8_t)M[a]; rv_count(c, a, 1, 0); break;
                                   case 1: { int16_t h; memcpy(&h, M + a, 2); res = (uint32_t)(int32_t)h; rv_count(c, a, 2, 0); } break;
                                   case 2: memcpy(&res, M + a, 4); rv_count(c, a, 4, 0); break;
                                   case 4: res = M[a]; rv_count(c, a, 1, 0); break;
                                   case 5: { uint16_t h; memcpy(&h, M + a, 2); res = h; rv_count(c, a, 2, 0); } break;
                                   default: fprintf(stderr, "bad load\n"); exit(1); }
                     c->loads++; c->last_load_rd = (int)rd; } break;
        case 0x23: { uint32_t a = v1 + (uint32_t)sx(((in >> 25) << 5) | ((in >> 7) & 31), 12);
                     if (a >= RV_MEM) { fprintf(stderr, "store oob %08x pc %08x\n", a, c->pc); exit(1); }
                     switch (f3) { case 0: M[a] = (uint8_t)v2; rv_count(c, a, 1, 1); break;
                                   case 1: { uint16_t h = (uint16_t)v2; memcpy(M + a, &h, 2); rv_count(c, a, 2, 1); } break;
                                   case 2: memcpy(M + a, &v2, 4); rv_count(c, a, 4, 1); break;
                                   default: fprintf(stderr, "bad store\n"); exit(1); }
                     c->stores++; wr = 0; } break;
        case 0x13: { int32_t imm = sx(in >> 20, 12); uint32_t sh = (in >> 20) & 31;
                     switch (f3) { case 0: res = v1 + imm; break; case 2: res = (int32_t)v1 < imm; break; case 3: res = v1 < (uint32_t)imm; break;
                                   case 4: res = v1 ^ imm; break; case 6: res = v1 | imm; break; case 7: res = v1 & imm; break;
                                   case 1: res = v1 << sh; break; case 5: res = (in >> 30) & 1 ? (uint32_t)((int32_t)v1 >> sh) : v1 >> sh; break; } } break;
        case 0x33:
            if (f7 == 1) {
                c->muldiv++;
                switch (f3) { case 0: res = v1 * v2; break; case 1: res = (uint32_t)(((int64_t)(int32_t)v1 * (int32_t)v2) >> 32); break;
                              case 2: res = (uint32_t)(((int64_t)(int32_t)v1 * (uint64_t)v2) >> 32); break;
                              case 3: res = (uint32_t)(((uint64_t)v1 * v2) >> 32); break;
                              case 4: res = v2 ? (uint32_t)((int32_t)v1 / (int32_t)v2) : 0xffffffffu; break;
                              case 5: res = v2 ? v1 / v2 : 0xffffffffu; break;
                              case 6: res = v2 ? (uint32_t)((int32_t)v1 % (int32_t)v2) : v1; break;
                              case 7: res = v2 ? v1 % v2 : v1; break; }
            } else {
                switch (f3) { case 0: res = f7 ? v1 - v2 : v1 + v2; break; case 1: res = v1 << (v2 & 31); break;
                              case 2: res = (int32_t)v1 < (int32_t)v2; break; case 3: res = v1 < v2; break; case 4: res = v1 ^ v2; break;
                              case 5: res = f7 ? (uint32_t)((int32_t)v1 >> (v2 & 31)) : v1 >> (v2 & 31); break;
                              case 6: res = v1 | v2; break; case 7: res = v1 & v2; break; }
            } break;
        case 0x0f: wr = 0; break;
        default: fprintf(stderr, "unsupported opcode %02x at %08x (inst %08x)\n", op, c->pc, in); exit(1);
        }
        if (wr && rd) c->x[rd] = res;
        c->pc = npc;
    }
}
