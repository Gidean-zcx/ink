/* Timing-model configuration: keys, defaults and "key = value" parsing. */
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "iss.h"

#define K(k, f, h) {k, offsetof(iss_cfg_t, f), h}
const iss_cfg_key_t iss_cfg_keys[] = {
    K("cpu.mhz", mhz, "core clock in MHz (reporting only)"),
    K("lat.alu", lat_alu, "ALU result latency"),
    K("lat.load_word", lat_load_word, "LW result latency; 2 = one load-use bubble"),
    K("lat.load_sub", lat_load_sub, "LB/LBU/LH/LHU result latency"),
    K("lat.mul", lat_mul, "MUL/MULH* result latency (pipelined)"),
    K("lat.csr_read", lat_csr_read, "CSR read result latency"),
    K("lat.fp_add", lat_fp_add, "FADD/FSUB result latency"),
    K("lat.fp_mul", lat_fp_mul, "FMUL result latency"),
    K("lat.fp_fma", lat_fp_fma, "FMADD/FMSUB/FNMADD/FNMSUB result latency"),
    K("lat.fp_misc", lat_fp_misc, "FP convert/compare/move/sign-inject/min-max/class latency"),
    K("lat.fp_load", lat_fp_load, "FLW result latency"),
    K("occ.div", occ_div, "DIV/DIVU/REM/REMU: blocking cycles"),
    K("occ.fp_div", occ_fp_div, "FDIV.S: blocking cycles"),
    K("occ.fp_sqrt", occ_fp_sqrt, "FSQRT.S: blocking cycles"),
    K("occ.csr_write", occ_csr_write, "CSR write: pipeline flush cycles"),
    K("occ.fence_i", occ_fence_i, "FENCE.I cycles"),
    K("occ.amo", occ_amo, "AMO/LR/SC occupancy cycles (plus memory penalties)"),
    K("mem.misaligned_extra", misaligned_extra, "extra cycles for a data access crossing a 4-byte boundary"),
    K("bp.enable", bp_enable, "1 = BHT/BTB/RAS predictor, 0 = static not-taken"),
    K("bp.bht_entries", bht_entries, "2-bit counters, indexed by pc"),
    K("bp.btb_entries", btb_entries, "direct-mapped branch target buffer entries"),
    K("bp.ras_entries", ras_entries, "return address stack depth"),
    K("br.taken_correct", br_taken_correct, "extra cycles: correctly predicted taken branch/jump"),
    K("br.mispredict", br_mispredict, "extra cycles: mispredicted direction or target"),
    K("br.jal_nobtb", br_jal_nobtb, "extra cycles: JAL whose target is not in the BTB"),
    K("br.taken_nobp", br_taken_nobp, "bp.enable=0: extra cycles for a taken branch"),
    K("br.jalr_nobp", br_jalr_nobp, "bp.enable=0: extra cycles for JALR"),
    K("mem.sram_kib", sram_kib, "internal SRAM size (KiB) at 0x2F000000"),
    K("mem.flash_kib", flash_kib, "flash window (KiB) at 0x40000000"),
    K("mem.psram_kib", psram_kib, "PSRAM size (KiB) at 0x50000000"),
    K("mem.sram_extra", sram_extra, "extra cycles per internal SRAM data access"),
    K("dcache.hit_extra", dcache_hit_extra, "extra cycles per D-cache hit (vs. internal SRAM)"),
    K("icache.enable", icache_enable, "0 = ideal instruction fetch"),
    K("icache.size", ic.size, "I-cache bytes (per core)"),
    K("icache.line", ic.line, "I-cache line bytes"),
    K("icache.ways", ic.ways, "I-cache associativity"),
    K("dcache.enable", dcache_enable, "0 = ideal external memory"),
    K("dcache.size", dc.size, "D-cache bytes (shared)"),
    K("dcache.line", dc.line, "D-cache line bytes"),
    K("dcache.ways", dc.ways, "D-cache associativity"),
    K("flash.fill", flash_fill, "cycles to fill one cache line from flash"),
    K("psram.fill", psram_fill, "cycles to fill one cache line from PSRAM"),
    K("psram.writeback", psram_writeback, "cycles to write one dirty line back to PSRAM"),
};
const size_t iss_cfg_nkeys = sizeof iss_cfg_keys / sizeof iss_cfg_keys[0];

/* Defaults = the "analog" baseline: a classic 5-stage in-order RV32 core with
 * the published SiFive E51/U54 pipeline figures (PolarFire SoC MSS TRM) and
 * the ESP32-S31 memory system from ESP-IDF. The calibrated values live in
 * tools/iss/timing/esp32s31.cfg. */
void iss_cfg_default(iss_cfg_t *c) {
    memset(c, 0, sizeof *c);
    c->mhz = 320;
    c->lat_alu = 1;
    c->lat_load_word = 2;
    c->lat_load_sub = 3;
    c->lat_mul = 2;
    c->lat_csr_read = 3;
    c->lat_fp_add = 4;
    c->lat_fp_mul = 4;
    c->lat_fp_fma = 5;
    c->lat_fp_misc = 2;
    c->lat_fp_load = 2;
    c->occ_div = 34;
    c->occ_fp_div = 20;
    c->occ_fp_sqrt = 20;
    c->occ_csr_write = 5;
    c->occ_fence_i = 5;
    c->occ_amo = 3;
    c->misaligned_extra = 1;
    c->bp_enable = 1;
    c->bht_entries = 512;
    c->btb_entries = 28;
    c->ras_entries = 6;
    c->br_taken_correct = 0;
    c->br_mispredict = 3;
    c->br_jal_nobtb = 3;
    c->br_taken_nobp = 3;
    c->br_jalr_nobp = 3;
    c->sram_kib = 512;
    c->flash_kib = 16384;
    c->psram_kib = 8192;
    c->sram_extra = 0;
    c->dcache_hit_extra = 0;
    c->icache_enable = 1;
    c->ic.size = 16384;
    c->ic.line = 64;
    c->ic.ways = 2;
    c->dcache_enable = 1;
    c->dc.size = 65536;
    c->dc.line = 64;
    c->dc.ways = 2;
    c->flash_fill = 1150;
    c->psram_fill = 96;
    c->psram_writeback = 85;
}

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

static int set_kv(iss_cfg_t *c, const char *key, const char *val) {
    for (size_t i = 0; i < iss_cfg_nkeys; i++) {
        if (strcmp(iss_cfg_keys[i].key, key) != 0) continue;
        char *end;
        errno = 0;
        double v = strtod(val, &end);
        if (errno || end == val || *trim(end)) {
            fprintf(stderr, "iss: bad value for %s: '%s'\n", key, val);
            return -1;
        }
        *(double *)((char *)c + iss_cfg_keys[i].off) = v;
        return 0;
    }
    fprintf(stderr, "iss: unknown config key '%s'\n", key);
    return -1;
}

int iss_cfg_set(iss_cfg_t *c, const char *assignment) {
    char buf[256];
    snprintf(buf, sizeof buf, "%s", assignment);
    char *eq = strchr(buf, '=');
    if (!eq) {
        fprintf(stderr, "iss: expected key=value, got '%s'\n", assignment);
        return -1;
    }
    *eq = 0;
    return set_kv(c, trim(buf), trim(eq + 1));
}

int iss_cfg_load(iss_cfg_t *c, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "iss: cannot open config %s\n", path);
        return -1;
    }
    char line[512];
    int lineno = 0, rc = 0;
    while (fgets(line, sizeof line, f)) {
        lineno++;
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        char *s = trim(line);
        if (!*s) continue;
        char *eq = strchr(s, '=');
        if (!eq) {
            fprintf(stderr, "iss: %s:%d: expected key = value\n", path, lineno);
            rc = -1;
            continue;
        }
        *eq = 0;
        if (set_kv(c, trim(s), trim(eq + 1))) {
            fprintf(stderr, "iss: %s:%d\n", path, lineno);
            rc = -1;
        }
    }
    fclose(f);
    return rc;
}
