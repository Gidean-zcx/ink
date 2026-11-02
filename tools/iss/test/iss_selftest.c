/* iss self-test: hand-encoded RV32 programs with known results and exact
 * cycle counts. Needs no cross compiler and no network (CTest: iss-selftest).
 * Each case builds a program in simulated memory, runs it under a known
 * timing configuration and checks registers, exit code, cycles and cache
 * statistics against values derived by hand from the model in iss.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iss.h"

/* ---------------------------------------------------------- tiny assembler */
enum { ZERO = 0, RA = 1, SP = 2, A0 = 10, A1, A2, A3, A4, A5, A6, A7 };

static uint32_t R(uint32_t f7, int rs2, int rs1, uint32_t f3, int rd, uint32_t op) {
    return f7 << 25 | (uint32_t)rs2 << 20 | (uint32_t)rs1 << 15 | f3 << 12 | (uint32_t)rd << 7 | op;
}
static uint32_t I(int32_t imm, int rs1, uint32_t f3, int rd, uint32_t op) {
    return ((uint32_t)imm & 0xfffu) << 20 | (uint32_t)rs1 << 15 | f3 << 12 | (uint32_t)rd << 7 | op;
}
static uint32_t S(int32_t imm, int rs2, int rs1, uint32_t f3) {
    uint32_t u = (uint32_t)imm;
    return ((u >> 5) & 0x7fu) << 25 | (uint32_t)rs2 << 20 | (uint32_t)rs1 << 15 | f3 << 12 | (u & 0x1fu) << 7 | 0x23u;
}
static uint32_t B(int32_t imm, int rs2, int rs1, uint32_t f3) {
    uint32_t u = (uint32_t)imm;
    return ((u >> 12) & 1u) << 31 | ((u >> 5) & 0x3fu) << 25 | (uint32_t)rs2 << 20 | (uint32_t)rs1 << 15 | f3 << 12 |
           ((u >> 1) & 0xfu) << 8 | ((u >> 11) & 1u) << 7 | 0x63u;
}
static uint32_t LUI(int rd, uint32_t imm20) { return imm20 << 12 | (uint32_t)rd << 7 | 0x37u; }
#define ADDI(rd, rs, imm) I((imm), (rs), 0, (rd), 0x13)
#define LW(rd, rs, imm) I((imm), (rs), 2, (rd), 0x03)
#define LBU(rd, rs, imm) I((imm), (rs), 4, (rd), 0x03)
#define SW(rs2, rs1, imm) S((imm), (rs2), (rs1), 2)
#define ADD(rd, a, b) R(0, (b), (a), 0, (rd), 0x33)
#define MUL(rd, a, b) R(1, (b), (a), 0, (rd), 0x33)
#define DIV(rd, a, b) R(1, (b), (a), 4, (rd), 0x33)
#define REM(rd, a, b) R(1, (b), (a), 6, (rd), 0x33)
#define BNE(a, b, off) B((off), (b), (a), 1)
#define ECALL 0x00000073u

typedef struct {
    uint8_t *p;
    uint32_t base, at;
} prog_t;

static void emit32(prog_t *g, uint32_t w) {
    memcpy(g->p + g->at, &w, 4);
    g->at += 4;
}
static void emit16(prog_t *g, uint16_t h) {
    memcpy(g->p + g->at, &h, 2);
    g->at += 2;
}
static void exit_with(prog_t *g, int reg) { /* a0 = reg; a7 = 93; ecall */
    emit32(g, ADDI(A0, reg, 0));
    emit32(g, ADDI(A7, ZERO, 93));
    emit32(g, ECALL);
}

static int failures;
#define CHECK(cond, ...)                                                                                               \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("FAIL %s:%d: ", __func__, __LINE__);                                                                \
            printf(__VA_ARGS__);                                                                                       \
            printf("\n");                                                                                              \
            failures++;                                                                                                \
        }                                                                                                              \
    } while (0)

static void setup(iss_t *m, iss_cfg_t *c, prog_t *g, int region) {
    if (iss_init(m, c)) {
        printf("iss_init failed\n");
        exit(2);
    }
    m->console = NULL;
    g->p = m->mem[region];
    g->base = m->base[region];
    g->at = 0;
    m->pc = g->base;
}

static double cycles(const iss_t *m) { return (double)m->st.cycles / ISS_TPC; }

/* 1. counted loop, static prediction: 2 + 100*3 + 99*taken_penalty + exit(2) */
static void t_loop_nobp(void) {
    iss_cfg_t c;
    iss_cfg_default(&c);
    c.bp_enable = 0;
    c.br_taken_nobp = 2;
    iss_t m;
    prog_t g;
    setup(&m, &c, &g, REG_SRAM);
    emit32(&g, ADDI(A0, ZERO, 0));
    emit32(&g, ADDI(A1, ZERO, 100));
    emit32(&g, ADDI(A0, A0, 3)); /* loop: */
    emit32(&g, ADDI(A1, A1, -1));
    emit32(&g, BNE(A1, ZERO, -8));
    exit_with(&g, A0);
    int rc = iss_run(&m, 100000);
    CHECK(rc == 300, "exit %d, want 300", rc);
    CHECK(cycles(&m) == 2 + 300 + 99 * 2 + 2, "cycles %.2f, want %d", cycles(&m), 2 + 300 + 99 * 2 + 2);
    iss_free(&m);
}

/* 2. same loop with BHT/BTB: one mispredict entering (BTB miss), one leaving */
static void t_loop_bp(void) {
    iss_cfg_t c;
    iss_cfg_default(&c);
    c.br_mispredict = 3;
    c.br_taken_correct = 0;
    iss_t m;
    prog_t g;
    setup(&m, &c, &g, REG_SRAM);
    emit32(&g, ADDI(A0, ZERO, 0));
    emit32(&g, ADDI(A1, ZERO, 100));
    emit32(&g, ADDI(A0, A0, 3));
    emit32(&g, ADDI(A1, A1, -1));
    emit32(&g, BNE(A1, ZERO, -8));
    exit_with(&g, A0);
    iss_run(&m, 100000);
    CHECK(m.st.br_cond == 100 && m.st.br_taken == 99, "branches %llu/%llu", (unsigned long long)m.st.br_cond,
          (unsigned long long)m.st.br_taken);
    CHECK(m.st.br_mispred == 2, "mispredicts %llu, want 2", (unsigned long long)m.st.br_mispred);
    CHECK(cycles(&m) == 2 + 300 + 2 * 3 + 2, "cycles %.2f, want %d", cycles(&m), 2 + 300 + 6 + 2);
    iss_free(&m);
}

/* 3. load-use: lw then dependent addi -> lat.load_word - 1 bubbles; byte load uses lat.load_sub */
static void t_load_use(void) {
    for (int lw = 2; lw <= 3; lw++) {
        iss_cfg_t c;
        iss_cfg_default(&c);
        c.lat_load_word = lw;
        c.lat_load_sub = 3;
        iss_t m;
        prog_t g;
        setup(&m, &c, &g, REG_SRAM);
        emit32(&g, LUI(A0, 0x2F010));  /* a0 = 0x2F010000 (SRAM data) */
        emit32(&g, ADDI(A1, ZERO, 42));
        emit32(&g, SW(A1, A0, 0));
        emit32(&g, LW(A2, A0, 0));
        emit32(&g, ADDI(A3, A2, 1));   /* stalls lw-1 */
        emit32(&g, LBU(A4, A0, 0));
        emit32(&g, ADD(A3, A3, A4));   /* stalls 2 */
        exit_with(&g, A3);
        int rc = iss_run(&m, 1000);
        CHECK(rc == 85, "exit %d, want 85", rc);
        double want = 7 + (lw - 1) + 2 + 2;
        CHECK(cycles(&m) == want, "lw=%d: cycles %.2f, want %.0f", lw, cycles(&m), want);
        CHECK(m.st.stall_data[IC_LOAD] == (ticks_t)(lw - 1) * ISS_TPC, "load stall attribution");
        iss_free(&m);
    }
}

/* 4. M extension edge cases and multiply/divide timing */
static void t_muldiv(void) {
    iss_cfg_t c;
    iss_cfg_default(&c);
    c.lat_mul = 2;
    c.occ_div = 34;
    iss_t m;
    prog_t g;
    setup(&m, &c, &g, REG_SRAM);
    emit32(&g, ADDI(A1, ZERO, -7));
    emit32(&g, ADDI(A2, ZERO, 3));
    emit32(&g, MUL(A3, A1, A2));  /* -21 */
    emit32(&g, ADD(A4, A3, A3));  /* -42, mul-use stall 1 */
    emit32(&g, DIV(A5, A1, A2));  /* -2 (truncating), 34 cycles */
    emit32(&g, REM(A6, A1, A2));  /* -1, 34 cycles */
    emit32(&g, DIV(A0, A1, ZERO)); /* x/0 = -1, 34 cycles */
    emit32(&g, ADD(A0, A0, A4));  /* -43 */
    emit32(&g, ADD(A0, A0, A5));  /* -45 */
    emit32(&g, ADD(A0, A0, A6));  /* -46 */
    emit32(&g, ADDI(A7, ZERO, 93));
    emit32(&g, ECALL);
    int rc = iss_run(&m, 1000);
    CHECK(rc == -46, "exit %d, want -46", rc);
    double want = 3 + (1 + 1) + 34 * 3 + 3 + 1;
    CHECK(cycles(&m) == want, "cycles %.2f, want %.0f", cycles(&m), want);
    iss_free(&m);
}

/* 5. RV32C: c.li / c.addi / c.mv / c.add and a compressed loop */
static void t_compressed(void) {
    iss_cfg_t c;
    iss_cfg_default(&c);
    c.bp_enable = 0;
    c.br_taken_nobp = 1;
    iss_t m;
    prog_t g;
    setup(&m, &c, &g, REG_SRAM);
    emit16(&g, 0x4515);          /* c.li a0, 5 */
    emit16(&g, 0x4591);          /* c.li a1, 4 */
    emit16(&g, 0x0509);          /* loop: c.addi a0, 2 */
    emit16(&g, 0x15fd);          /* c.addi a1, -1 */
    emit16(&g, 0xfdf5);          /* c.bnez a1, -4 */
    emit16(&g, 0x862a);          /* c.mv a2, a0 */
    emit16(&g, 0x9532);          /* c.add a0, a2 */
    emit32(&g, ADDI(A7, ZERO, 93));
    emit32(&g, ECALL);
    int rc = iss_run(&m, 1000);
    CHECK(rc == 26, "exit %d, want 26", rc);
    CHECK(cycles(&m) == 2 + 4 * 3 + 3 * 1 + 2 + 1, "cycles %.2f, want %d", cycles(&m), 2 + 12 + 3 + 2 + 1);
    iss_free(&m);
}

/* 6. D-cache: PSRAM miss/hit, then a dirty eviction (2-way set) */
static void t_dcache(void) {
    iss_cfg_t c;
    iss_cfg_default(&c);
    c.psram_fill = 100;
    c.psram_writeback = 50;
    c.lat_load_word = 1; /* isolate memory penalties from load-use stalls */
    iss_t m;
    prog_t g;
    setup(&m, &c, &g, REG_SRAM);
    uint32_t way = (uint32_t)(c.dc.size / c.dc.ways); /* 32 KiB: same set */
    emit32(&g, LUI(A0, 0x50000));          /* PSRAM base */
    emit32(&g, LW(A1, A0, 0));             /* miss (100) */
    emit32(&g, LW(A2, A0, 4));             /* hit */
    emit32(&g, SW(A2, A0, 8));             /* hit, line now dirty */
    emit32(&g, LUI(A3, 0x50000 + (way >> 12)));
    emit32(&g, LW(A4, A3, 0));             /* miss (100), way 2 */
    emit32(&g, LUI(A5, 0x50000 + (2 * way >> 12)));
    emit32(&g, LW(A6, A5, 0));             /* miss (100) evicts dirty line 0 (+50) */
    exit_with(&g, ZERO);
    iss_run(&m, 1000);
    CHECK(m.st.dc_access[REG_PSRAM] == 5 && m.st.dc_miss[REG_PSRAM] == 3, "dcache %llu/%llu",
          (unsigned long long)m.st.dc_access[REG_PSRAM], (unsigned long long)m.st.dc_miss[REG_PSRAM]);
    CHECK(m.st.dc_writeback[REG_PSRAM] == 1, "writebacks %llu", (unsigned long long)m.st.dc_writeback[REG_PSRAM]);
    double want = 8 + 2 + 3 * 100 + 50;
    CHECK(cycles(&m) == want, "cycles %.2f, want %.0f", cycles(&m), want);
    iss_free(&m);
}

/* 7. I-cache: a loop in flash misses once per new line, then hits */
static void t_icache(void) {
    iss_cfg_t c;
    iss_cfg_default(&c);
    c.flash_fill = 1000;
    c.bp_enable = 0;
    c.br_taken_nobp = 2;
    iss_t m;
    prog_t g;
    setup(&m, &c, &g, REG_FLASH);
    emit32(&g, ADDI(A0, ZERO, 0));
    emit32(&g, ADDI(A1, ZERO, 10));
    emit32(&g, ADDI(A0, A0, 1));
    emit32(&g, ADDI(A1, A1, -1));
    emit32(&g, BNE(A1, ZERO, -8));
    exit_with(&g, A0); /* 9 instructions = 36 bytes: one 64-byte line */
    int rc = iss_run(&m, 1000);
    CHECK(rc == 10, "exit %d", rc);
    CHECK(m.st.ic_miss[REG_FLASH] == 1, "icache misses %llu", (unsigned long long)m.st.ic_miss[REG_FLASH]);
    CHECK(cycles(&m) == 1000 + 2 + 30 + 9 * 2 + 2, "cycles %.2f, want %d", cycles(&m), 1000 + 2 + 30 + 18 + 2);
    iss_free(&m);
}

/* 8. fractional penalties stay exact in fixed point */
static void t_fractional(void) {
    iss_cfg_t c;
    iss_cfg_default(&c);
    c.bp_enable = 0;
    c.br_taken_nobp = 0.5;
    iss_t m;
    prog_t g;
    setup(&m, &c, &g, REG_SRAM);
    emit32(&g, ADDI(A1, ZERO, 9));
    emit32(&g, ADDI(A1, A1, -1));
    emit32(&g, BNE(A1, ZERO, -4));
    exit_with(&g, ZERO);
    iss_run(&m, 1000);
    CHECK(cycles(&m) == 1 + 18 + 8 * 0.5 + 2, "cycles %.3f, want %.3f", cycles(&m), 1 + 18 + 4.0 + 2);
    iss_free(&m);
}

int main(void) {
    t_loop_nobp();
    t_loop_bp();
    t_load_use();
    t_muldiv();
    t_compressed();
    t_dcache();
    t_icache();
    t_fractional();
    if (failures) {
        printf("iss-selftest: %d failure(s)\n", failures);
        return 1;
    }
    printf("iss-selftest: all passed\n");
    return 0;
}
