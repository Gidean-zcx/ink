/*
 * iss — a small deterministic RV32IMAFC(+Zba/Zbb/Zbs/Zcb/Zcmp) instruction-set
 * simulator with a configurable in-order timing model.
 *
 * Purpose: measure target cycles per unit of work for the simulator's cost
 * classes (profiles/<device>.json "costs_cycles"). It is a performance model, not a
 * full system emulator: no privilege modes, no interrupts, no MMU, traps stop
 * the run.
 *
 * Timing model (single issue, in order):
 *   - every instruction issues no earlier than the previous one's issue time
 *     plus its occupancy (1 for pipelined ops, N for blocking units);
 *   - a scoreboard holds, for every register, the time its value becomes
 *     available (issue + result latency + memory penalty) -> RAW stalls;
 *   - the I-side accesses an instruction cache each time fetch enters a new
 *     line (flash/PSRAM only; internal SRAM is not cached);
 *   - the D-side: internal SRAM has a fixed latency, flash/PSRAM go through a
 *     set-associative write-back, write-allocate data cache (blocking misses);
 *   - control flow: optional BHT (2-bit) + BTB + RAS predictor, penalties for
 *     correctly predicted taken transfers and for mispredictions.
 * Time is kept in fixed point (ISS_TPC ticks per cycle) so that fitted
 * penalties may be fractional while the run stays bit-exact deterministic.
 */
#ifndef ISS_H
#define ISS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define ISS_TPC 256u /* ticks per cycle */
typedef uint64_t ticks_t;

/* Memory regions (addresses follow the ESP32-S31 map in ESP-IDF soc.h). */
enum { REG_SRAM, REG_FLASH, REG_PSRAM, REG_COUNT };
#define ISS_SRAM_BASE 0x2F000000u
#define ISS_FLASH_BASE 0x40000000u
#define ISS_PSRAM_BASE 0x50000000u

/* Instruction classes (for statistics and stall attribution). */
enum {
    IC_ALU,
    IC_LOAD,     /* LW */
    IC_LOAD_SUB, /* LB/LBU/LH/LHU */
    IC_STORE,
    IC_BRANCH,
    IC_JAL,
    IC_JALR,
    IC_MUL,
    IC_DIV,
    IC_CSR,
    IC_FENCE,
    IC_AMO,
    IC_PUSHPOP, /* Zcmp push/pop/popret */
    IC_FP_LOAD,
    IC_FP_STORE,
    IC_FP_ADD,
    IC_FP_MUL,
    IC_FP_FMA,
    IC_FP_DIV,
    IC_FP_SQRT,
    IC_FP_MISC, /* convert, compare, move, sign-inject, min/max, class */
    IC_SYSTEM,  /* ecall/ebreak/wfi */
    IC_COUNT
};
extern const char *const iss_class_name[IC_COUNT];
extern const char *const iss_region_name[REG_COUNT];

typedef struct {
    double size, line, ways; /* bytes, bytes, associativity */
} iss_cache_cfg_t;

/* All timing values are in CPU cycles (fractions allowed). Defaults: see
 * iss_cfg_default(); fitted values: tools/iss/timing/esp32s31.cfg. */
typedef struct {
    double mhz;
    /* result latency: cycles from issue until a dependent instruction may issue */
    double lat_alu, lat_load_word, lat_load_sub, lat_mul, lat_csr_read;
    double lat_fp_add, lat_fp_mul, lat_fp_fma, lat_fp_misc, lat_fp_load;
    /* occupancy of blocking operations (pipeline held for this many cycles) */
    double occ_div, occ_fp_div, occ_fp_sqrt, occ_csr_write, occ_fence_i, occ_amo;
    double misaligned_extra; /* extra cycles for a data access crossing a 4-byte boundary */
    /* control flow */
    double bp_enable, bht_entries, btb_entries, ras_entries;
    double br_taken_correct; /* correctly predicted taken branch / jump (BTB or RAS hit) */
    double br_mispredict;    /* wrong direction or wrong/unknown target, resolved in EX */
    double br_jal_nobtb;     /* JAL without BTB hit (target computed in decode) */
    double br_taken_nobp;    /* bp_enable=0: taken conditional branch */
    double br_jalr_nobp;     /* bp_enable=0: JALR */
    /* memory system */
    double sram_kib, flash_kib, psram_kib;
    double sram_extra;      /* extra cycles per internal SRAM data access */
    double dcache_hit_extra; /* extra cycles per D-cache hit (shared cache vs. SRAM) */
    double icache_enable, dcache_enable;
    iss_cache_cfg_t ic, dc;
    double flash_fill;     /* cycles to fill one cache line from flash (XIP) */
    double psram_fill;     /* cycles to fill one cache line from PSRAM */
    double psram_writeback; /* cycles to write one dirty line back to PSRAM */
} iss_cfg_t;

typedef struct {
    const char *key;
    size_t off;
    const char *help;
} iss_cfg_key_t;
extern const iss_cfg_key_t iss_cfg_keys[];
extern const size_t iss_cfg_nkeys;

void iss_cfg_default(iss_cfg_t *c);
/* "key = value" lines, '#' comments. Returns 0 on success. */
int iss_cfg_load(iss_cfg_t *c, const char *path);
/* single "key=value" assignment. Returns 0 on success. */
int iss_cfg_set(iss_cfg_t *c, const char *assignment);

/* Statistics: all fields are 64-bit counters so regions can subtract snapshots. */
typedef struct {
    uint64_t instret;
    ticks_t cycles;
    uint64_t cls[IC_COUNT];
    ticks_t stall_fetch;             /* I-cache miss penalties */
    ticks_t stall_data[IC_COUNT];    /* RAW stalls, by producer class */
    ticks_t stall_mem[REG_COUNT];    /* D-side extra latency + miss penalties, by region */
    ticks_t stall_ctrl;              /* branch / jump penalties */
    ticks_t stall_occ;               /* extra occupancy (div, fdiv, csr write, push/pop ...) */
    uint64_t br_cond, br_taken, br_mispred, jal, jal_miss, jalr, jalr_miss, ret, ret_miss;
    uint64_t ic_access[REG_COUNT], ic_miss[REG_COUNT];
    uint64_t dc_access[REG_COUNT], dc_miss[REG_COUNT], dc_writeback[REG_COUNT];
    uint64_t mem_access[REG_COUNT]; /* data accesses by region */
    uint64_t misaligned;
} iss_stats_t;

typedef struct {
    char name[64];
    uint64_t units, aux_units; /* aux: optional second unit count (e.g. pixels of the glyphs) */
    iss_stats_t d;             /* delta over the region */
} iss_region_t;

typedef struct {
    uint64_t tag; /* full line address */
    uint64_t lru;
    uint8_t valid, dirty;
} iss_line_t;

typedef struct {
    unsigned sets, ways, line_shift;
    iss_line_t *lines;
    uint64_t clock;
} iss_cache_t;

typedef struct {
    uint32_t pc_tag, target;
    uint8_t valid;
} iss_btb_t;

struct iss_prof { /* per-pc profile entry (open-addressing hash) */
    uint32_t pc;
    uint64_t ticks, count;
};

struct iss_sym { /* function symbol from .symtab */
    uint32_t addr, size;
    char *name;
};

typedef struct iss {
    /* architectural state */
    uint32_t x[32], f[32], pc, fcsr;
    uint32_t mstatus, mtvec, mscratch, mepc, mcause, mtval;
    uint32_t resv_addr;
    bool resv_valid;
    /* memory */
    uint8_t *mem[REG_COUNT];
    uint32_t base[REG_COUNT], size[REG_COUNT];
    /* timing state */
    iss_cfg_t cfg;
    ticks_t t;                 /* issue time of the next instruction */
    ticks_t xr[32], fr[32];    /* register ready times */
    uint8_t xprod[32], fprod[32]; /* producer class of the pending value */
    uint64_t fetch_line;       /* last I-side line touched (+1, 0 = none) */
    iss_cache_t ic, dc;
    uint8_t *bht;
    iss_btb_t *btb;
    uint32_t *ras;
    unsigned ras_top, ras_n;
    /* precomputed ticks */
    ticks_t T_alu, T_lw, T_lsub, T_mul, T_csr, T_fadd, T_fmul, T_ffma, T_fmisc, T_fload;
    ticks_t T_div, T_fdiv, T_fsqrt, T_csrw, T_fencei, T_amo, T_mis;
    ticks_t T_br_ok, T_br_bad, T_jal_nobtb, T_br_nobp, T_jalr_nobp;
    ticks_t T_sram, T_dchit, T_ffill, T_pfill, T_pwb;
    /* statistics */
    iss_stats_t st;
    iss_region_t *regions;
    size_t nregions, cap_regions;
    bool in_region;
    iss_stats_t region_start;
    char region_name[64];
    /* run control */
    bool halted;
    int exit_code;
    char error[256];
    FILE *console;
    FILE *input;               /* -i file for ISS_SYS_INPUT (NULL = none) */
    /* optional profile: cycles per pc */
    bool profile;
    struct iss_prof *prof;
    size_t prof_cap, prof_n;
    /* symbols */
    struct iss_sym *syms;
    size_t nsyms;
} iss_t;

int iss_init(iss_t *m, const iss_cfg_t *cfg);
void iss_free(iss_t *m);
/* Loads PT_LOAD segments, sets pc = entry, reads .symtab. Returns 0 on success. */
int iss_load_elf(iss_t *m, const char *path);
/* Runs until exit or max_instr instructions. Returns the program exit code,
 * or a negative value on simulator error (see m->error). */
int iss_run(iss_t *m, uint64_t max_instr);
void iss_print_profile(iss_t *m, FILE *out, int top);

/* Magic ecall numbers (a7) understood by the simulator. */
#define ISS_SYS_WRITE 64
#define ISS_SYS_EXIT 93
#define ISS_SYS_BEGIN 0x100 /* a0 = const char *name */
#define ISS_SYS_END 0x101   /* a0 = units, a1 = auxiliary units */
#define ISS_SYS_CACHE 0x102 /* a0 = ISS_CACHE_* flags */
#define ISS_SYS_INPUT 0x103 /* a0 = dst, a1 = file offset, a2 = length: copy from the -i file into memory;
                               free and not through the caches (write back + invalidate the D-cache first) */
#define ISS_CACHE_WB 1u        /* write back dirty D-cache lines */
#define ISS_CACHE_INV_D 2u     /* invalidate D-cache */
#define ISS_CACHE_INV_I 4u     /* invalidate I-cache, forget fetch line */
#define ISS_CACHE_CHARGE 8u    /* charge the write-back time to the program */

#endif
