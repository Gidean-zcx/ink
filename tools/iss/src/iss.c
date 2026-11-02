/* iss core: memory map, caches, branch predictor, RV32 decode/execute and the
 * in-order timing scoreboard. See iss.h for the model. */
#include <fenv.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "iss.h"

const char *const iss_class_name[IC_COUNT] = {
    "alu",     "load",    "load_sub", "store",  "branch", "jal",    "jalr",    "mul",
    "div",     "csr",     "fence",    "amo",    "pushpop", "fp_load", "fp_store", "fp_add",
    "fp_mul",  "fp_fma",  "fp_div",   "fp_sqrt", "fp_misc", "system"};
const char *const iss_region_name[REG_COUNT] = {"sram", "flash", "psram"};

static ticks_t ticks(double cycles) {
    if (cycles <= 0) return 0;
    return (ticks_t)llround(cycles * ISS_TPC);
}

static void fail(iss_t *m, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void fail(iss_t *m, const char *fmt, ...) {
    if (!m->halted) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(m->error, sizeof m->error, fmt, ap);
        va_end(ap);
        m->halted = true;
        m->exit_code = -1;
    }
}

/* sign-extended immediates without shifting negative values */
static int32_t imm_s(uint32_t inst) { return (int32_t)((inst & 0xfe000000u) | ((inst >> 7) & 0x1fu) << 20) >> 20; }

/* ------------------------------------------------------------------ caches */

static int cache_init(iss_cache_t *c, const iss_cache_cfg_t *cfg) {
    unsigned size = (unsigned)cfg->size, line = (unsigned)cfg->line, ways = (unsigned)cfg->ways;
    if (!line || (line & (line - 1)) || !ways || size < line * ways || size % (line * ways)) return -1;
    c->ways = ways;
    c->sets = size / (line * ways);
    c->line_shift = 0;
    while ((1u << c->line_shift) < line) c->line_shift++;
    c->lines = calloc((size_t)c->sets * ways, sizeof *c->lines);
    c->clock = 0;
    return c->lines ? 0 : -1;
}

static inline int region_of(const iss_t *m, uint32_t a) {
    if (a - m->base[REG_SRAM] < m->size[REG_SRAM]) return REG_SRAM;
    if (a - m->base[REG_FLASH] < m->size[REG_FLASH]) return REG_FLASH;
    if (a - m->base[REG_PSRAM] < m->size[REG_PSRAM]) return REG_PSRAM;
    return -1;
}

/* One cache lookup. Returns the penalty in ticks (miss fill + dirty victim
 * write-back + hit extra) and updates the statistics. */
static ticks_t cache_access(iss_t *m, iss_cache_t *c, uint32_t a, int reg, bool write, bool icache) {
    uint64_t line = (uint64_t)a >> c->line_shift;
    iss_line_t *w = c->lines + (size_t)(line % c->sets) * c->ways;
    ticks_t hit = icache ? 0 : m->T_dchit;
    c->clock++;
    if (icache)
        m->st.ic_access[reg]++;
    else
        m->st.dc_access[reg]++;
    for (unsigned i = 0; i < c->ways; i++) {
        if (w[i].valid && w[i].tag == line) {
            w[i].lru = c->clock;
            if (write) w[i].dirty = 1;
            return hit;
        }
    }
    if (icache)
        m->st.ic_miss[reg]++;
    else
        m->st.dc_miss[reg]++;
    unsigned v = 0;
    for (unsigned i = 0; i < c->ways; i++) {
        if (!w[i].valid) {
            v = i;
            break;
        }
        if (w[i].lru < w[v].lru) v = i;
    }
    ticks_t pen = 0;
    if (w[v].valid && w[v].dirty) {
        int vr = region_of(m, (uint32_t)(w[v].tag << c->line_shift));
        if (vr >= 0) m->st.dc_writeback[vr]++;
        pen += m->T_pwb;
    }
    pen += reg == REG_FLASH ? m->T_ffill : m->T_pfill;
    w[v].tag = line;
    w[v].lru = c->clock;
    w[v].valid = 1;
    w[v].dirty = write;
    return pen + hit;
}

/* Write back (and optionally invalidate) the whole D-cache; returns the time
 * the write-backs would take. */
static ticks_t dcache_flush(iss_t *m, bool wb, bool inv) {
    ticks_t pen = 0;
    iss_cache_t *c = &m->dc;
    for (size_t i = 0; i < (size_t)c->sets * c->ways; i++) {
        iss_line_t *l = &c->lines[i];
        if (wb && l->valid && l->dirty) {
            int vr = region_of(m, (uint32_t)(l->tag << c->line_shift));
            if (vr >= 0) m->st.dc_writeback[vr]++;
            pen += m->T_pwb;
            l->dirty = 0;
        }
        if (inv) l->valid = l->dirty = 0;
    }
    return pen;
}

/* ------------------------------------------------------------------ memory */

static inline uint8_t *hostp(iss_t *m, uint32_t a, unsigned n, int *reg) {
    int r = region_of(m, a);
    if (r < 0 || a - m->base[r] + n > m->size[r]) return NULL;
    *reg = r;
    return m->mem[r] + (a - m->base[r]);
}

/* Data-side timing of one access (already known to be inside region reg). */
static ticks_t dmem_time(iss_t *m, uint32_t a, unsigned n, bool write, int reg) {
    ticks_t pen = 0;
    m->st.mem_access[reg]++;
    if (n > 1 && (a & 3) + n > 4) {
        m->st.misaligned++;
        pen += m->T_mis;
    }
    if (reg == REG_SRAM) {
        pen += m->T_sram;
    } else if (m->cfg.dcache_enable) {
        pen += cache_access(m, &m->dc, a, reg, write, false);
        if (((a + n - 1) >> m->dc.line_shift) != (a >> m->dc.line_shift))
            pen += cache_access(m, &m->dc, a + n - 1, reg, write, false);
    }
    m->st.stall_mem[reg] += pen;
    return pen;
}

static uint32_t rd_le(const uint8_t *p, unsigned n) {
    uint32_t v = 0;
    for (unsigned i = 0; i < n; i++) v |= (uint32_t)p[i] << (8 * i);
    return v;
}

static void wr_le(uint8_t *p, uint32_t v, unsigned n) {
    for (unsigned i = 0; i < n; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static uint32_t load(iss_t *m, uint32_t a, unsigned n, ticks_t *pen) {
    int reg;
    uint8_t *p = hostp(m, a, n, &reg);
    if (!p) {
        fail(m, "load access fault at 0x%08x (pc 0x%08x)", a, m->pc);
        return 0;
    }
    *pen += dmem_time(m, a, n, false, reg);
    return rd_le(p, n);
}

static void store(iss_t *m, uint32_t a, uint32_t v, unsigned n, ticks_t *pen) {
    int reg;
    uint8_t *p = hostp(m, a, n, &reg);
    if (!p || reg == REG_FLASH) {
        fail(m, "store access fault at 0x%08x (pc 0x%08x)", a, m->pc);
        return;
    }
    if (m->resv_valid && (a & ~3u) == m->resv_addr) m->resv_valid = false;
    *pen += dmem_time(m, a, n, true, reg);
    wr_le(p, v, n);
}

/* ---------------------------------------------------------- branch predictor */

static unsigned bp_idx(uint32_t pc, unsigned n) { return (pc >> 1) % n; }

/* Conditional branch: returns the penalty in ticks and trains the predictor. */
static ticks_t bp_branch(iss_t *m, uint32_t pc, bool taken, uint32_t target) {
    m->st.br_cond++;
    if (taken) m->st.br_taken++;
    if (!m->cfg.bp_enable) return taken ? m->T_br_nobp : 0;
    uint8_t *ctr = &m->bht[bp_idx(pc, (unsigned)m->cfg.bht_entries)];
    iss_btb_t *b = &m->btb[bp_idx(pc, (unsigned)m->cfg.btb_entries)];
    bool btb_hit = b->valid && b->pc_tag == pc;
    bool pred_taken = *ctr >= 2 && btb_hit; /* fetch can only redirect with a target */
    ticks_t pen;
    if (taken) {
        if (pred_taken && b->target == target) {
            pen = m->T_br_ok;
        } else {
            pen = m->T_br_bad;
            m->st.br_mispred++;
        }
        b->valid = 1;
        b->pc_tag = pc;
        b->target = target;
        if (*ctr < 3) (*ctr)++;
    } else {
        if (pred_taken) {
            pen = m->T_br_bad;
            m->st.br_mispred++;
        } else {
            pen = 0;
        }
        if (*ctr > 0) (*ctr)--;
    }
    return pen;
}

static void ras_push(iss_t *m, uint32_t ret) {
    unsigned n = (unsigned)m->cfg.ras_entries;
    if (!n) return;
    m->ras[m->ras_top] = ret;
    m->ras_top = (m->ras_top + 1) % n;
    if (m->ras_n < n) m->ras_n++;
}

static bool ras_pop(iss_t *m, uint32_t *ret) {
    unsigned n = (unsigned)m->cfg.ras_entries;
    if (!n || !m->ras_n) return false;
    m->ras_top = (m->ras_top + n - 1) % n;
    m->ras_n--;
    *ret = m->ras[m->ras_top];
    return true;
}

static bool is_link(unsigned r) { return r == 1 || r == 5; }

/* JAL / JALR (also used for cm.popret). */
static ticks_t bp_jump(iss_t *m, uint32_t pc, uint32_t target, uint32_t link, unsigned rd, unsigned rs1, bool indirect) {
    bool call = is_link(rd), ret = indirect && rd == 0 && is_link(rs1);
    ticks_t pen;
    if (!indirect)
        m->st.jal++;
    else if (ret)
        m->st.ret++;
    else
        m->st.jalr++;
    if (!m->cfg.bp_enable) {
        pen = indirect ? m->T_jalr_nobp : m->T_jal_nobtb;
        if (!indirect) m->st.jal_miss++;
        else if (ret) m->st.ret_miss++;
        else m->st.jalr_miss++;
        return pen;
    }
    if (ret) {
        uint32_t pred;
        if (ras_pop(m, &pred) && pred == target) {
            pen = m->T_br_ok;
        } else {
            pen = m->T_br_bad;
            m->st.ret_miss++;
        }
    } else {
        iss_btb_t *b = &m->btb[bp_idx(pc, (unsigned)m->cfg.btb_entries)];
        if (b->valid && b->pc_tag == pc && b->target == target) {
            pen = m->T_br_ok;
        } else {
            pen = indirect ? m->T_br_bad : m->T_jal_nobtb;
            if (indirect)
                m->st.jalr_miss++;
            else
                m->st.jal_miss++;
            b->valid = 1;
            b->pc_tag = pc;
            b->target = target;
        }
    }
    if (call) ras_push(m, link);
    return pen;
}

/* ------------------------------------------------------------------- fetch */

static ticks_t fetch_time(iss_t *m, uint32_t pc, unsigned len, int reg) {
    if (reg == REG_SRAM || !m->cfg.icache_enable) {
        m->fetch_line = 0;
        return 0;
    }
    ticks_t pen = 0;
    uint64_t l0 = pc >> m->ic.line_shift, l1 = (pc + len - 1) >> m->ic.line_shift;
    if (l0 + 1 != m->fetch_line) pen += cache_access(m, &m->ic, pc, reg, false, true);
    if (l1 != l0) pen += cache_access(m, &m->ic, pc + len - 1, reg, false, true);
    m->fetch_line = l1 + 1;
    return pen;
}

/* --------------------------------------------------------------- RV32C */

static uint32_t enc_r(uint32_t f7, unsigned rs2, unsigned rs1, uint32_t f3, unsigned rd, uint32_t op) {
    return f7 << 25 | rs2 << 20 | rs1 << 15 | f3 << 12 | rd << 7 | op;
}
static uint32_t enc_i(int32_t imm, unsigned rs1, uint32_t f3, unsigned rd, uint32_t op) {
    return ((uint32_t)imm & 0xfffu) << 20 | rs1 << 15 | f3 << 12 | rd << 7 | op;
}
static uint32_t enc_s(int32_t imm, unsigned rs2, unsigned rs1, uint32_t f3, uint32_t op) {
    uint32_t u = (uint32_t)imm;
    return ((u >> 5) & 0x7fu) << 25 | rs2 << 20 | rs1 << 15 | f3 << 12 | (u & 0x1fu) << 7 | op;
}
static uint32_t enc_b(int32_t imm, unsigned rs2, unsigned rs1, uint32_t f3) {
    uint32_t u = (uint32_t)imm;
    return ((u >> 12) & 1u) << 31 | ((u >> 5) & 0x3fu) << 25 | rs2 << 20 | rs1 << 15 | f3 << 12 |
           ((u >> 1) & 0xfu) << 8 | ((u >> 11) & 1u) << 7 | 0x63u;
}
static uint32_t enc_u(uint32_t imm, unsigned rd, uint32_t op) { return (imm & 0xfffff000u) | rd << 7 | op; }
static uint32_t enc_j(int32_t imm, unsigned rd) {
    uint32_t u = (uint32_t)imm;
    return ((u >> 20) & 1u) << 31 | ((u >> 1) & 0x3ffu) << 21 | ((u >> 11) & 1u) << 20 | ((u >> 12) & 0xffu) << 12 |
           rd << 7 | 0x6fu;
}

static int32_t sext6(uint32_t c) { return (int32_t)((((c >> 12) & 1u) << 5 | ((c >> 2) & 31u)) << 26) >> 26; }
static int32_t cj_imm(uint32_t c) {
    uint32_t u = ((c >> 12) & 1u) << 11 | ((c >> 11) & 1u) << 4 | ((c >> 9) & 3u) << 8 | ((c >> 8) & 1u) << 10 |
                 ((c >> 7) & 1u) << 6 | ((c >> 6) & 1u) << 7 | ((c >> 3) & 7u) << 1 | ((c >> 2) & 1u) << 5;
    return (int32_t)(u << 20) >> 20;
}
static int32_t cb_imm(uint32_t c) {
    uint32_t u = ((c >> 12) & 1u) << 8 | ((c >> 10) & 3u) << 3 | ((c >> 5) & 3u) << 6 | ((c >> 3) & 3u) << 1 |
                 ((c >> 2) & 1u) << 5;
    return (int32_t)(u << 23) >> 23;
}

/* Expands a 16-bit instruction to its 32-bit equivalent. Returns 0 for an
 * illegal encoding; sets *zcmp for the Zcmp space (handled separately). */
static uint32_t expand_c(uint32_t c, int *zcmp) {
    unsigned f3 = (c >> 13) & 7, rd = (c >> 7) & 31, rs2 = (c >> 2) & 31;
    unsigned rdp = 8 + ((c >> 2) & 7), rs1p = 8 + ((c >> 7) & 7);
    *zcmp = 0;
    switch (c & 3) {
    case 0:
        switch (f3) {
        case 0: {
            int32_t imm = (int32_t)(((c >> 6) & 1u) << 2 | ((c >> 5) & 1u) << 3 | ((c >> 11) & 3u) << 4 | ((c >> 7) & 0xfu) << 6);
            return imm ? enc_i(imm, 2, 0, rdp, 0x13) : 0; /* c.addi4spn */
        }
        case 2:
        case 3: {
            int32_t imm = (int32_t)(((c >> 10) & 7u) << 3 | ((c >> 6) & 1u) << 2 | ((c >> 5) & 1u) << 6);
            return enc_i(imm, rs1p, 2, rdp, f3 == 2 ? 0x03 : 0x07); /* c.lw / c.flw */
        }
        case 4: /* Zcb loads/stores */
            switch ((c >> 10) & 0x3f) {
            case 0x20:
                return enc_i((int32_t)(((c >> 6) & 1u) | ((c >> 5) & 1u) << 1), rs1p, 4, rdp, 0x03); /* c.lbu */
            case 0x21:
                return enc_i((int32_t)(((c >> 5) & 1u) << 1), rs1p, ((c >> 6) & 1u) ? 1 : 5, rdp, 0x03); /* c.lh/lhu */
            case 0x22:
                return enc_s((int32_t)(((c >> 6) & 1u) | ((c >> 5) & 1u) << 1), rdp, rs1p, 0, 0x23); /* c.sb */
            case 0x23:
                if ((c >> 6) & 1u) return 0;
                return enc_s((int32_t)(((c >> 5) & 1u) << 1), rdp, rs1p, 1, 0x23); /* c.sh */
            }
            return 0;
        case 6:
        case 7: {
            int32_t imm = (int32_t)(((c >> 10) & 7u) << 3 | ((c >> 6) & 1u) << 2 | ((c >> 5) & 1u) << 6);
            return enc_s(imm, rdp, rs1p, 2, f3 == 6 ? 0x23 : 0x27); /* c.sw / c.fsw */
        }
        }
        return 0;
    case 1:
        switch (f3) {
        case 0:
            return enc_i(sext6(c), rd, 0, rd, 0x13); /* c.addi, c.nop */
        case 1:
            return enc_j(cj_imm(c), 1); /* c.jal */
        case 2:
            return enc_i(sext6(c), 0, 0, rd, 0x13); /* c.li */
        case 3:
            if (rd == 2) {
                uint32_t u = ((c >> 12) & 1u) << 9 | ((c >> 6) & 1u) << 4 | ((c >> 5) & 1u) << 6 | ((c >> 3) & 3u) << 7 |
                             ((c >> 2) & 1u) << 5;
                int32_t imm = (int32_t)(u << 22) >> 22;
                return imm ? enc_i(imm, 2, 0, 2, 0x13) : 0; /* c.addi16sp */
            } else {
                int32_t imm = (int32_t)((uint32_t)sext6(c) << 12);
                return imm ? enc_u((uint32_t)imm, rd, 0x37) : 0; /* c.lui */
            }
        case 4: {
            unsigned f2 = (c >> 10) & 3, r2 = 8 + ((c >> 2) & 7), f = (c >> 5) & 3;
            if (f2 <= 1) {
                if ((c >> 12) & 1u) return 0;
                return enc_i((int32_t)((f2 ? 0x400u : 0u) | ((c >> 2) & 31u)), rs1p, 5, rs1p, 0x13); /* c.srli/srai */
            }
            if (f2 == 2) return enc_i(sext6(c), rs1p, 7, rs1p, 0x13); /* c.andi */
            if (!((c >> 12) & 1u)) {
                static const uint32_t f3s[4] = {0, 4, 6, 7}; /* sub xor or and */
                return enc_r(f == 0 ? 0x20 : 0, r2, rs1p, f3s[f], rs1p, 0x33);
            }
            if (f == 2) return enc_r(1, r2, rs1p, 0, rs1p, 0x33); /* c.mul (Zcb) */
            if (f == 3) {
                switch ((c >> 2) & 7) {
                case 0: return enc_i(0xff, rs1p, 7, rs1p, 0x13);  /* c.zext.b */
                case 1: return enc_i(0x604, rs1p, 1, rs1p, 0x13); /* c.sext.b */
                case 2: return enc_r(0x04, 0, rs1p, 4, rs1p, 0x33); /* c.zext.h */
                case 3: return enc_i(0x605, rs1p, 1, rs1p, 0x13); /* c.sext.h */
                case 5: return enc_i(-1, rs1p, 4, rs1p, 0x13);    /* c.not */
                }
            }
            return 0;
        }
        case 5:
            return enc_j(cj_imm(c), 0); /* c.j */
        case 6:
        case 7:
            return enc_b(cb_imm(c), 0, rs1p, f3 == 6 ? 0 : 1); /* c.beqz / c.bnez */
        }
        return 0;
    case 2:
        switch (f3) {
        case 0:
            if ((c >> 12) & 1u) return 0;
            return enc_i((int32_t)rs2, rd, 1, rd, 0x13); /* c.slli */
        case 2:
        case 3: {
            if (f3 == 2 && rd == 0) return 0;
            int32_t imm = (int32_t)(((c >> 12) & 1u) << 5 | ((c >> 4) & 7u) << 2 | ((c >> 2) & 3u) << 6);
            return enc_i(imm, 2, 2, rd, f3 == 2 ? 0x03 : 0x07); /* c.lwsp / c.flwsp */
        }
        case 4:
            if (!((c >> 12) & 1u)) {
                if (rs2 == 0) return rd ? enc_i(0, rd, 0, 0, 0x67) : 0; /* c.jr */
                return enc_r(0, rs2, 0, 0, rd, 0x33);                  /* c.mv */
            }
            if (rs2 == 0) return rd ? enc_i(0, rd, 0, 1, 0x67) : 0x00100073u; /* c.jalr / c.ebreak */
            return enc_r(0, rs2, rd, 0, rd, 0x33);                           /* c.add */
        case 5:
            *zcmp = 1;
            return 0;
        case 6:
        case 7: {
            int32_t imm = (int32_t)(((c >> 9) & 0xfu) << 2 | ((c >> 7) & 3u) << 6);
            return enc_s(imm, rs2, 2, 2, f3 == 6 ? 0x23 : 0x27); /* c.swsp / c.fswsp */
        }
        }
        return 0;
    }
    return 0;
}

/* --------------------------------------------------------------- RV32F helpers */

#define FFLAG_NV 0x10u
#define FFLAG_DZ 0x08u
#define FFLAG_OF 0x04u
#define FFLAG_UF 0x02u
#define FFLAG_NX 0x01u

static float f32(uint32_t b) {
    float f;
    memcpy(&f, &b, 4);
    return f;
}
static uint32_t b32(float f) {
    uint32_t b;
    if (isnan(f)) return 0x7fc00000u; /* canonical NaN */
    memcpy(&b, &f, 4);
    return b;
}
static bool is_snan(uint32_t b) { return (b & 0x7fc00000u) == 0x7f800000u && (b & 0x003fffffu); }

static int fp_rm(iss_t *m, unsigned rm) { return rm == 7 ? (int)((m->fcsr >> 5) & 7) : (int)rm; }

static void fp_begin(int rm) {
    feclearexcept(FE_ALL_EXCEPT);
    switch (rm) {
    case 1: fesetround(FE_TOWARDZERO); break;
    case 2: fesetround(FE_DOWNWARD); break;
    case 3: fesetround(FE_UPWARD); break;
    default: fesetround(FE_TONEAREST); break; /* RMM approximated by RNE */
    }
}

static void fp_end(iss_t *m) {
    int e = fetestexcept(FE_ALL_EXCEPT);
    uint32_t fl = 0;
    if (e & FE_INVALID) fl |= FFLAG_NV;
    if (e & FE_DIVBYZERO) fl |= FFLAG_DZ;
    if (e & FE_OVERFLOW) fl |= FFLAG_OF;
    if (e & FE_UNDERFLOW) fl |= FFLAG_UF;
    if (e & FE_INEXACT) fl |= FFLAG_NX;
    m->fcsr |= fl;
    fesetround(FE_TONEAREST);
}

static double round_rm(double x, int rm) {
    switch (rm) {
    case 1: return trunc(x);
    case 2: return floor(x);
    case 3: return ceil(x);
    case 4: return round(x);
    default: return nearbyint(x);
    }
}

static uint32_t fcvt_w(iss_t *m, uint32_t fb, int rm, bool uns) {
    float f = f32(fb);
    if (isnan(f)) {
        m->fcsr |= FFLAG_NV;
        return uns ? 0xffffffffu : 0x7fffffffu;
    }
    double r = round_rm((double)f, rm);
    if (uns) {
        if (r < 0) {
            m->fcsr |= FFLAG_NV;
            return 0;
        }
        if (r > 4294967295.0) {
            m->fcsr |= FFLAG_NV;
            return 0xffffffffu;
        }
        if (r != (double)f) m->fcsr |= FFLAG_NX;
        return (uint32_t)r;
    }
    if (r > 2147483647.0) {
        m->fcsr |= FFLAG_NV;
        return 0x7fffffffu;
    }
    if (r < -2147483648.0) {
        m->fcsr |= FFLAG_NV;
        return 0x80000000u;
    }
    if (r != (double)f) m->fcsr |= FFLAG_NX;
    return (uint32_t)(int32_t)r;
}

static uint32_t fclass(uint32_t b) {
    uint32_t sign = b >> 31, exp = (b >> 23) & 0xff, man = b & 0x7fffff;
    if (exp == 0xff) {
        if (!man) return sign ? 1u << 0 : 1u << 7;
        return (man & 0x400000) ? 1u << 9 : 1u << 8;
    }
    if (exp == 0) {
        if (!man) return sign ? 1u << 3 : 1u << 4;
        return sign ? 1u << 2 : 1u << 5;
    }
    return sign ? 1u << 1 : 1u << 6;
}

static uint32_t fminmax(iss_t *m, uint32_t a, uint32_t b, bool max) {
    float fa = f32(a), fb = f32(b);
    if (is_snan(a) || is_snan(b)) m->fcsr |= FFLAG_NV;
    if (isnan(fa) && isnan(fb)) return 0x7fc00000u;
    if (isnan(fa)) return b;
    if (isnan(fb)) return a;
    if (fa == fb) { /* -0 < +0 */
        bool na = a >> 31, nb = b >> 31;
        if (max) return na && !nb ? b : a;
        return na ? a : b;
    }
    return (max ? fa > fb : fa < fb) ? a : b;
}

static uint32_t fcmp(iss_t *m, uint32_t a, uint32_t b, unsigned op) {
    float fa = f32(a), fb = f32(b);
    if (isnan(fa) || isnan(fb)) {
        if (op != 2 || is_snan(a) || is_snan(b)) m->fcsr |= FFLAG_NV; /* FLT/FLE signal on any NaN */
        return 0;
    }
    switch (op) {
    case 0: return fa <= fb; /* FLE */
    case 1: return fa < fb;  /* FLT */
    default: return fa == fb; /* FEQ */
    }
}

/* ----------------------------------------------------------------- Zbb helpers */

static uint32_t clz32(uint32_t v) { return v ? (uint32_t)__builtin_clz(v) : 32; }
static uint32_t ctz32(uint32_t v) { return v ? (uint32_t)__builtin_ctz(v) : 32; }
static uint32_t rol32(uint32_t v, unsigned s) { s &= 31; return s ? v << s | v >> (32 - s) : v; }
static uint32_t ror32(uint32_t v, unsigned s) { s &= 31; return s ? v >> s | v << (32 - s) : v; }
static uint32_t orcb(uint32_t v) {
    uint32_t r = 0;
    for (int i = 0; i < 4; i++)
        if ((v >> (8 * i)) & 0xff) r |= 0xffu << (8 * i);
    return r;
}

/* --------------------------------------------------------------- syscalls */

static void read_cstr(iss_t *m, uint32_t a, char *out, size_t cap) {
    size_t i = 0;
    int reg;
    for (; i + 1 < cap; i++) {
        uint8_t *p = hostp(m, a + (uint32_t)i, 1, &reg);
        if (!p || !*p) break;
        out[i] = (char)*p;
    }
    out[i] = 0;
}

static void stats_sub(iss_stats_t *d, const iss_stats_t *a, const iss_stats_t *b) {
    const uint64_t *pa = (const uint64_t *)a, *pb = (const uint64_t *)b;
    uint64_t *pd = (uint64_t *)d;
    for (size_t i = 0; i < sizeof *d / sizeof(uint64_t); i++) pd[i] = pa[i] - pb[i];
}

/* Returns true if the ecall was a free simulator service (no timing). */
static bool do_ecall(iss_t *m) {
    uint32_t n = m->x[17], a0 = m->x[10], a1 = m->x[11], a2 = m->x[12];
    switch (n) {
    case ISS_SYS_WRITE: {
        for (uint32_t i = 0; i < a2; i++) {
            int reg;
            uint8_t *p = hostp(m, a1 + i, 1, &reg);
            if (!p) break;
            if (m->console) fputc(*p, m->console);
        }
        m->x[10] = a2;
        (void)a0;
        return true;
    }
    case ISS_SYS_EXIT:
        m->halted = true;
        m->exit_code = (int)a0;
        return true;
    case ISS_SYS_BEGIN:
        if (m->in_region) {
            fail(m, "iss_begin while region active (pc 0x%08x)", m->pc);
            return true;
        }
        read_cstr(m, a0, m->region_name, sizeof m->region_name);
        m->st.cycles = m->t;
        m->region_start = m->st;
        m->in_region = true;
        return true;
    case ISS_SYS_END: {
        if (!m->in_region) {
            fail(m, "iss_end without iss_begin (pc 0x%08x)", m->pc);
            return true;
        }
        if (m->nregions == m->cap_regions) {
            m->cap_regions = m->cap_regions ? 2 * m->cap_regions : 64;
            m->regions = realloc(m->regions, m->cap_regions * sizeof *m->regions);
        }
        iss_region_t *r = &m->regions[m->nregions++];
        memset(r, 0, sizeof *r);
        snprintf(r->name, sizeof r->name, "%s", m->region_name);
        r->units = a0;
        r->aux_units = a1;
        m->st.cycles = m->t;
        stats_sub(&r->d, &m->st, &m->region_start);
        m->in_region = false;
        return true;
    }
    case ISS_SYS_CACHE: {
        ticks_t pen = 0;
        if (a0 & (ISS_CACHE_WB | ISS_CACHE_INV_D)) pen = dcache_flush(m, a0 & ISS_CACHE_WB, a0 & ISS_CACHE_INV_D);
        if (a0 & ISS_CACHE_INV_I) {
            for (size_t i = 0; i < (size_t)m->ic.sets * m->ic.ways; i++) m->ic.lines[i].valid = 0;
            m->fetch_line = 0;
        }
        if (a0 & ISS_CACHE_CHARGE) {
            m->t += pen;
            m->st.stall_mem[REG_PSRAM] += pen;
        }
        return true;
    }
    default:
        fail(m, "unknown ecall %u (pc 0x%08x)", n, m->pc);
        return true;
    }
}

/* -------------------------------------------------------------------- CSRs */

static bool csr_read(iss_t *m, unsigned csr, ticks_t now, uint32_t *v) {
    uint64_t cyc = now / ISS_TPC;
    switch (csr) {
    case 0x001: *v = m->fcsr & 0x1f; return true;
    case 0x002: *v = (m->fcsr >> 5) & 7; return true;
    case 0x003: *v = m->fcsr & 0xff; return true;
    case 0xC00: case 0xB00: case 0xC01: *v = (uint32_t)cyc; return true;
    case 0xC80: case 0xB80: case 0xC81: *v = (uint32_t)(cyc >> 32); return true;
    case 0xC02: case 0xB02: *v = (uint32_t)m->st.instret; return true;
    case 0xC82: case 0xB82: *v = (uint32_t)(m->st.instret >> 32); return true;
    case 0x300: *v = m->mstatus; return true;
    case 0x301: *v = 0x40001129u; return true; /* RV32 A C F I M */
    case 0x305: *v = m->mtvec; return true;
    case 0x340: *v = m->mscratch; return true;
    case 0x341: *v = m->mepc; return true;
    case 0x342: *v = m->mcause; return true;
    case 0x343: *v = m->mtval; return true;
    case 0x304: case 0x344: case 0xF11: case 0xF12: case 0xF13: case 0xF14: *v = 0; return true;
    }
    return false;
}

static bool csr_write(iss_t *m, unsigned csr, uint32_t v) {
    switch (csr) {
    case 0x001: m->fcsr = (m->fcsr & ~0x1fu) | (v & 0x1f); return true;
    case 0x002: m->fcsr = (m->fcsr & 0x1fu) | ((v & 7) << 5); return true;
    case 0x003: m->fcsr = v & 0xff; return true;
    case 0x300: m->mstatus = v; return true;
    case 0x305: m->mtvec = v; return true;
    case 0x340: m->mscratch = v; return true;
    case 0x341: m->mepc = v; return true;
    case 0x342: m->mcause = v; return true;
    case 0x343: m->mtval = v; return true;
    case 0x304: case 0x344: case 0xB00: case 0xB02: case 0xB80: case 0xB82: return true; /* ignored */
    }
    return false;
}

/* --------------------------------------------------------------- profiling */

static void prof_add(iss_t *m, uint32_t pc, ticks_t dt) {
    if (m->prof_n * 2 >= m->prof_cap) {
        size_t ncap = m->prof_cap ? m->prof_cap * 2 : 65536;
        struct iss_prof *np = calloc(ncap, sizeof *np);
        for (size_t i = 0; i < m->prof_cap; i++) {
            if (!m->prof[i].count) continue;
            size_t h = (m->prof[i].pc * 2654435761u) & (ncap - 1);
            while (np[h].count) h = (h + 1) & (ncap - 1);
            np[h] = m->prof[i];
        }
        free(m->prof);
        m->prof = np;
        m->prof_cap = ncap;
    }
    size_t h = (pc * 2654435761u) & (m->prof_cap - 1);
    while (m->prof[h].count && m->prof[h].pc != pc) h = (h + 1) & (m->prof_cap - 1);
    if (!m->prof[h].count) {
        m->prof[h].pc = pc;
        m->prof_n++;
    }
    m->prof[h].count++;
    m->prof[h].ticks += dt;
}

/* ------------------------------------------------------------------- core */

#define DX(r)                                                                                                          \
    do {                                                                                                               \
        unsigned r_ = (r);                                                                                             \
        if (r_ && m->xr[r_] > ready) {                                                                                 \
            ready = m->xr[r_];                                                                                         \
            prod = m->xprod[r_];                                                                                       \
        }                                                                                                              \
    } while (0)
#define DF(r)                                                                                                          \
    do {                                                                                                               \
        unsigned r_ = (r);                                                                                             \
        if (m->fr[r_] > ready) {                                                                                       \
            ready = m->fr[r_];                                                                                         \
            prod = m->fprod[r_];                                                                                       \
        }                                                                                                              \
    } while (0)
/* Issue: wait for operands; stalls are attributed to the producer class. */
#define ISSUE(c)                                                                                                       \
    do {                                                                                                               \
        cls = (c);                                                                                                     \
        if (ready > m->t) m->st.stall_data[prod] += ready - m->t;                                                      \
        issue = ready;                                                                                                 \
    } while (0)
#define WX(r, v)                                                                                                       \
    do {                                                                                                               \
        unsigned r_ = (r);                                                                                             \
        uint32_t v_ = (v);                                                                                             \
        if (r_) m->x[r_] = v_;                                                                                         \
    } while (0)
#define RX(r, lat)                                                                                                     \
    do {                                                                                                               \
        unsigned r_ = (r);                                                                                             \
        if (r_) {                                                                                                      \
            m->xr[r_] = issue + (lat) + mem;                                                                           \
            m->xprod[r_] = (uint8_t)cls;                                                                               \
        }                                                                                                              \
    } while (0)
#define RF(r, lat)                                                                                                     \
    do {                                                                                                               \
        unsigned r_ = (r);                                                                                             \
        m->fr[r_] = issue + (lat) + mem;                                                                               \
        m->fprod[r_] = (uint8_t)cls;                                                                                   \
    } while (0)

static unsigned sreg(unsigned s) { return s < 2 ? 8 + s : 16 + s; }

static uint32_t zcmp_list(unsigned rlist, unsigned *n) {
    uint32_t mask = 1u << 1;
    unsigned cnt = 1;
    if (rlist >= 5) mask |= 1u << 8, cnt++;
    if (rlist >= 6) mask |= 1u << 9, cnt++;
    unsigned last = rlist == 15 ? 27 : (rlist >= 7 ? 18 + rlist - 7 : 17);
    for (unsigned r = 18; r <= last; r++) mask |= 1u << r, cnt++;
    *n = cnt;
    return mask;
}

static const unsigned zcmp_order[13] = {27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 9, 8, 1};

/* Executes one instruction. */
static void step(iss_t *m) {
    uint32_t pc = m->pc;
    int freg;
    uint8_t *ip = hostp(m, pc, 2, &freg);
    if (!ip || (pc & 1)) {
        fail(m, "instruction fetch fault at 0x%08x", pc);
        return;
    }
    uint32_t inst = rd_le(ip, 2);
    unsigned len = 2;
    if ((inst & 3) == 3) {
        int r2;
        uint8_t *ip2 = hostp(m, pc + 2, 2, &r2);
        if (!ip2) {
            fail(m, "instruction fetch fault at 0x%08x", pc + 2);
            return;
        }
        inst |= rd_le(ip2, 2) << 16;
        len = 4;
    }
    ticks_t t0 = m->t;
    ticks_t fpen = fetch_time(m, pc, len, freg);
    m->t += fpen;
    m->st.stall_fetch += fpen;

    ticks_t ready = m->t, issue = m->t, mem = 0, ctrl = 0, occ = ISS_TPC;
    uint8_t prod = IC_ALU;
    int cls = IC_ALU;
    uint32_t npc = pc + len;
    bool free_op = false;

    if (len == 2) {
        int zcmp;
        uint32_t e = expand_c(inst, &zcmp);
        if (zcmp) {
            /* Zcmp: cm.push / cm.pop / cm.popret(z) / cm.mvsa01 / cm.mva01s */
            unsigned f5 = (inst >> 8) & 31;
            if (f5 == 0x18 || f5 == 0x1a || f5 == 0x1c || f5 == 0x1e) {
                unsigned n, rlist = (inst >> 4) & 15;
                if (rlist < 4) goto illegal;
                uint32_t mask = zcmp_list(rlist, &n);
                uint32_t adj = ((n * 4 + 15) / 16) * 16 + ((inst >> 2) & 3) * 16;
                uint32_t sp = m->x[2];
                DX(2);
                if (f5 == 0x18) { /* push: sources are the listed registers */
                    for (unsigned r = 1; r < 32; r++)
                        if (mask & (1u << r)) DX(r);
                    ISSUE(IC_PUSHPOP);
                    uint32_t a = sp - 4;
                    for (int i = 0; i < 13; i++) {
                        unsigned r = zcmp_order[i];
                        if (!(mask & (1u << r))) continue;
                        store(m, a, m->x[r], 4, &mem);
                        a -= 4;
                    }
                    m->x[2] = sp - adj;
                    RX(2, n * ISS_TPC);
                    occ = n * ISS_TPC;
                    m->st.stall_occ += occ - ISS_TPC;
                } else { /* pop family */
                    ISSUE(IC_PUSHPOP);
                    uint32_t a = sp + adj - 4;
                    unsigned k = 0;
                    for (int i = 0; i < 13; i++) {
                        unsigned r = zcmp_order[i];
                        if (!(mask & (1u << r))) continue;
                        ticks_t before = mem;
                        uint32_t v = load(m, a, 4, &mem);
                        m->x[r] = v;
                        m->xr[r] = issue + k * ISS_TPC + m->T_lw + mem;
                        m->xprod[r] = IC_PUSHPOP;
                        (void)before;
                        a -= 4;
                        k++;
                    }
                    m->x[2] = sp + adj;
                    RX(2, n * ISS_TPC);
                    occ = n * ISS_TPC;
                    m->st.stall_occ += occ - ISS_TPC;
                    if (f5 == 0x1c) {
                        m->x[10] = 0;
                        RX(10, n * ISS_TPC);
                    }
                    if (f5 != 0x1a) { /* popret: return through ra (wait for the reload) */
                        npc = m->x[1] & ~1u;
                        ticks_t need = m->xr[1] - mem; /* m->t = issue + occ + mem + ctrl */
                        if (need > issue + occ) {
                            m->st.stall_data[IC_PUSHPOP] += need - (issue + occ);
                            occ = need - issue;
                        }
                        ctrl = bp_jump(m, pc, npc, 0, 0, 1, true);
                    }
                }
                goto done;
            }
            if (((inst >> 10) & 7) == 3 && ((inst >> 5) & 3) == 1) { /* cm.mvsa01 */
                unsigned r1 = sreg((inst >> 7) & 7), r2 = sreg((inst >> 2) & 7);
                DX(10);
                DX(11);
                ISSUE(IC_ALU);
                uint32_t a0 = m->x[10], a1 = m->x[11];
                WX(r1, a0);
                WX(r2, a1);
                RX(r1, m->T_alu);
                RX(r2, m->T_alu);
                occ = 2 * ISS_TPC;
                goto done;
            }
            if (((inst >> 10) & 7) == 3 && ((inst >> 5) & 3) == 3) { /* cm.mva01s */
                unsigned r1 = sreg((inst >> 7) & 7), r2 = sreg((inst >> 2) & 7);
                DX(r1);
                DX(r2);
                ISSUE(IC_ALU);
                uint32_t v1 = m->x[r1], v2 = m->x[r2];
                m->x[10] = v1;
                m->x[11] = v2;
                RX(10, m->T_alu);
                RX(11, m->T_alu);
                occ = 2 * ISS_TPC;
                goto done;
            }
            goto illegal;
        }
        if (!e) goto illegal;
        inst = e;
    }

    {
        unsigned op = inst & 0x7f, rd = (inst >> 7) & 31, f3 = (inst >> 12) & 7, rs1 = (inst >> 15) & 31,
                 rs2 = (inst >> 20) & 31, f7 = inst >> 25;
        int32_t immi = (int32_t)inst >> 20;
        uint32_t a, b, v;
        switch (op) {
        case 0x37: /* LUI */
            ISSUE(IC_ALU);
            WX(rd, inst & 0xfffff000u);
            RX(rd, m->T_alu);
            break;
        case 0x17: /* AUIPC */
            ISSUE(IC_ALU);
            WX(rd, pc + (inst & 0xfffff000u));
            RX(rd, m->T_alu);
            break;
        case 0x6f: { /* JAL */
            int32_t imm = ((int32_t)(inst & 0x80000000u) >> 11) | (int32_t)(inst & 0xff000u) |
                          (int32_t)((inst >> 9) & 0x800u) | (int32_t)((inst >> 20) & 0x7feu);
            ISSUE(IC_JAL);
            npc = pc + (uint32_t)imm;
            WX(rd, pc + len);
            RX(rd, m->T_alu);
            ctrl = bp_jump(m, pc, npc, pc + len, rd, 0, false);
            break;
        }
        case 0x67: /* JALR */
            if (f3) goto illegal;
            DX(rs1);
            ISSUE(IC_JALR);
            npc = (m->x[rs1] + (uint32_t)immi) & ~1u;
            WX(rd, pc + len);
            RX(rd, m->T_alu);
            ctrl = bp_jump(m, pc, npc, pc + len, rd, rs1, true);
            break;
        case 0x63: { /* branches */
            int32_t imm = ((int32_t)(inst & 0x80000000u) >> 19) | (int32_t)((inst & 0x80u) << 4) |
                          (int32_t)((inst >> 20) & 0x7e0u) | (int32_t)((inst >> 7) & 0x1eu);
            DX(rs1);
            DX(rs2);
            ISSUE(IC_BRANCH);
            a = m->x[rs1];
            b = m->x[rs2];
            bool tk;
            switch (f3) {
            case 0: tk = a == b; break;
            case 1: tk = a != b; break;
            case 4: tk = (int32_t)a < (int32_t)b; break;
            case 5: tk = (int32_t)a >= (int32_t)b; break;
            case 6: tk = a < b; break;
            case 7: tk = a >= b; break;
            default: goto illegal;
            }
            uint32_t target = pc + (uint32_t)imm;
            if (tk) npc = target;
            ctrl = bp_branch(m, pc, tk, target);
            break;
        }
        case 0x03: { /* loads */
            DX(rs1);
            unsigned n = (f3 & 3) == 0 ? 1 : (f3 & 3) == 1 ? 2 : 4;
            if (f3 == 3 || f3 > 5) goto illegal;
            ISSUE(n == 4 ? IC_LOAD : IC_LOAD_SUB);
            a = m->x[rs1] + (uint32_t)immi;
            v = load(m, a, n, &mem);
            if (f3 == 0) v = (uint32_t)(int32_t)(int8_t)v;
            if (f3 == 1) v = (uint32_t)(int32_t)(int16_t)v;
            WX(rd, v);
            RX(rd, n == 4 ? m->T_lw : m->T_lsub);
            break;
        }
        case 0x23: { /* stores */
            int32_t imm = imm_s(inst);
            if (f3 > 2) goto illegal;
            DX(rs1);
            DX(rs2);
            ISSUE(IC_STORE);
            store(m, m->x[rs1] + (uint32_t)imm, m->x[rs2], 1u << f3, &mem);
            break;
        }
        case 0x13: { /* OP-IMM (+ Zbb/Zbs immediates) */
            DX(rs1);
            ISSUE(IC_ALU);
            a = m->x[rs1];
            unsigned sh = rs2, f12 = inst >> 20;
            switch (f3) {
            case 0: v = a + (uint32_t)immi; break;
            case 2: v = (int32_t)a < immi; break;
            case 3: v = a < (uint32_t)immi; break;
            case 4: v = a ^ (uint32_t)immi; break;
            case 6: v = a | (uint32_t)immi; break;
            case 7: v = a & (uint32_t)immi; break;
            case 1:
                if (f7 == 0x00) v = a << sh;
                else if (f12 == 0x600) v = clz32(a);
                else if (f12 == 0x601) v = ctz32(a);
                else if (f12 == 0x602) v = (uint32_t)__builtin_popcount(a);
                else if (f12 == 0x604) v = (uint32_t)(int32_t)(int8_t)a;
                else if (f12 == 0x605) v = (uint32_t)(int32_t)(int16_t)a;
                else if (f7 == 0x24) v = a & ~(1u << sh); /* bclri */
                else if (f7 == 0x34) v = a ^ (1u << sh);  /* binvi */
                else if (f7 == 0x14) v = a | (1u << sh);  /* bseti */
                else goto illegal;
                break;
            case 5:
                if (f7 == 0x00) v = a >> sh;
                else if (f7 == 0x20) v = (uint32_t)((int32_t)a >> sh);
                else if (f7 == 0x30) v = ror32(a, sh);       /* rori */
                else if (f12 == 0x287) v = orcb(a);          /* orc.b */
                else if (f12 == 0x698) v = __builtin_bswap32(a); /* rev8 */
                else if (f7 == 0x24) v = (a >> sh) & 1;      /* bexti */
                else goto illegal;
                break;
            default: goto illegal;
            }
            WX(rd, v);
            RX(rd, m->T_alu);
            break;
        }
        case 0x33: { /* OP: RV32I, M, Zba, Zbb, Zbs */
            DX(rs1);
            DX(rs2);
            a = m->x[rs1];
            b = m->x[rs2];
            if (f7 == 0x01) { /* M */
                if (f3 < 4) {
                    ISSUE(IC_MUL);
                    switch (f3) {
                    case 0: v = a * b; break;
                    case 1: v = (uint32_t)(((int64_t)(int32_t)a * (int64_t)(int32_t)b) >> 32); break;
                    case 2: v = (uint32_t)(((int64_t)(int32_t)a * (int64_t)(uint64_t)b) >> 32); break;
                    default: v = (uint32_t)(((uint64_t)a * (uint64_t)b) >> 32); break;
                    }
                    WX(rd, v);
                    RX(rd, m->T_mul);
                } else {
                    ISSUE(IC_DIV);
                    switch (f3) {
                    case 4:
                        v = b == 0 ? 0xffffffffu
                            : (a == 0x80000000u && b == 0xffffffffu) ? a
                                                                      : (uint32_t)((int32_t)a / (int32_t)b);
                        break;
                    case 5: v = b == 0 ? 0xffffffffu : a / b; break;
                    case 6:
                        v = b == 0 ? a : (a == 0x80000000u && b == 0xffffffffu) ? 0 : (uint32_t)((int32_t)a % (int32_t)b);
                        break;
                    default: v = b == 0 ? a : a % b; break;
                    }
                    WX(rd, v);
                    RX(rd, m->T_div);
                    occ = m->T_div;
                    m->st.stall_occ += occ - ISS_TPC;
                }
                break;
            }
            ISSUE(IC_ALU);
            switch (f7 << 3 | f3) {
            case 0x000: v = a + b; break;
            case 0x100: v = a - b; break;
            case 0x001: v = a << (b & 31); break;
            case 0x002: v = (int32_t)a < (int32_t)b; break;
            case 0x003: v = a < b; break;
            case 0x004: v = a ^ b; break;
            case 0x005: v = a >> (b & 31); break;
            case 0x105: v = (uint32_t)((int32_t)a >> (b & 31)); break;
            case 0x006: v = a | b; break;
            case 0x007: v = a & b; break;
            case 0x082: v = (a << 1) + b; break; /* sh1add */
            case 0x084: v = (a << 2) + b; break; /* sh2add */
            case 0x086: v = (a << 3) + b; break; /* sh3add */
            case 0x107: v = a & ~b; break;       /* andn */
            case 0x106: v = a | ~b; break;       /* orn */
            case 0x104: v = ~(a ^ b); break;     /* xnor */
            case 0x02e: v = (int32_t)a > (int32_t)b ? a : b; break; /* max */
            case 0x02f: v = a > b ? a : b; break;                   /* maxu */
            case 0x02c: v = (int32_t)a < (int32_t)b ? a : b; break; /* min */
            case 0x02d: v = a < b ? a : b; break;                   /* minu */
            case 0x181: v = rol32(a, b); break;                     /* rol */
            case 0x185: v = ror32(a, b); break;                     /* ror */
            case 0x024:                                             /* zext.h (pack rs2=0) */
                if (rs2) goto illegal;
                v = a & 0xffff;
                break;
            case 0x121: v = a & ~(1u << (b & 31)); break; /* bclr */
            case 0x125: v = (a >> (b & 31)) & 1; break;   /* bext */
            case 0x1a1: v = a ^ (1u << (b & 31)); break;  /* binv */
            case 0x0a1: v = a | (1u << (b & 31)); break;  /* bset */
            default: goto illegal;
            }
            WX(rd, v);
            RX(rd, m->T_alu);
            break;
        }
        case 0x0f: /* FENCE / FENCE.I */
            ISSUE(IC_FENCE);
            if (f3 == 1) {
                occ = m->T_fencei;
                m->st.stall_occ += occ - ISS_TPC;
                m->fetch_line = 0;
            }
            break;
        case 0x2f: { /* A extension */
            if (f3 != 2) goto illegal;
            unsigned f5 = inst >> 27;
            DX(rs1);
            DX(rs2);
            ISSUE(IC_AMO);
            a = m->x[rs1];
            if (a & 3) {
                fail(m, "misaligned AMO at 0x%08x (pc 0x%08x)", a, pc);
                return;
            }
            occ = m->T_amo;
            m->st.stall_occ += occ > ISS_TPC ? occ - ISS_TPC : 0;
            if (f5 == 0x02) { /* LR.W */
                v = load(m, a, 4, &mem);
                m->resv_valid = true;
                m->resv_addr = a;
                WX(rd, v);
                RX(rd, m->T_lw);
                break;
            }
            if (f5 == 0x03) { /* SC.W */
                if (m->resv_valid && m->resv_addr == a) {
                    store(m, a, m->x[rs2], 4, &mem);
                    WX(rd, 0);
                } else {
                    WX(rd, 1);
                }
                m->resv_valid = false;
                RX(rd, m->T_lw);
                break;
            }
            uint32_t old = load(m, a, 4, &mem);
            b = m->x[rs2];
            switch (f5) {
            case 0x01: v = b; break;
            case 0x00: v = old + b; break;
            case 0x04: v = old ^ b; break;
            case 0x0c: v = old & b; break;
            case 0x08: v = old | b; break;
            case 0x10: v = (int32_t)old < (int32_t)b ? old : b; break;
            case 0x14: v = (int32_t)old > (int32_t)b ? old : b; break;
            case 0x18: v = old < b ? old : b; break;
            case 0x1c: v = old > b ? old : b; break;
            default: goto illegal;
            }
            store(m, a, v, 4, &mem);
            WX(rd, old);
            RX(rd, m->T_lw);
            break;
        }
        case 0x73: { /* SYSTEM */
            if (f3 == 0) {
                if (inst == 0x00000073u) { /* ecall */
                    free_op = do_ecall(m);
                    cls = IC_SYSTEM;
                    if (m->halted) return;
                    break;
                }
                if (inst == 0x00100073u) {
                    fail(m, "ebreak at pc 0x%08x", pc);
                    return;
                }
                if (inst == 0x10500073u) { /* wfi */
                    ISSUE(IC_SYSTEM);
                    break;
                }
                goto illegal;
            }
            unsigned csr = inst >> 20;
            bool imm = f3 & 4;
            if (!imm) DX(rs1);
            ISSUE(IC_CSR);
            uint32_t old = 0, src = imm ? rs1 : m->x[rs1];
            if (!csr_read(m, csr, issue, &old)) {
                fail(m, "unsupported CSR 0x%03x (pc 0x%08x)", csr, pc);
                return;
            }
            bool wr = (f3 & 3) == 1 || rs1 != 0;
            if (wr) {
                uint32_t nv = (f3 & 3) == 1 ? src : (f3 & 3) == 2 ? old | src : old & ~src;
                if (!csr_write(m, csr, nv)) {
                    fail(m, "read-only CSR 0x%03x written (pc 0x%08x)", csr, pc);
                    return;
                }
                if (csr > 3) { /* FP CSRs are renamed in the FPU; others flush */
                    occ = m->T_csrw;
                    m->st.stall_occ += occ > ISS_TPC ? occ - ISS_TPC : 0;
                }
            }
            WX(rd, old);
            RX(rd, m->T_csr);
            break;
        }
        case 0x07: /* FLW */
            if (f3 != 2) goto illegal;
            DX(rs1);
            ISSUE(IC_FP_LOAD);
            m->f[rd] = load(m, m->x[rs1] + (uint32_t)immi, 4, &mem);
            RF(rd, m->T_fload);
            break;
        case 0x27: { /* FSW */
            int32_t imm = imm_s(inst);
            if (f3 != 2) goto illegal;
            DX(rs1);
            DF(rs2);
            ISSUE(IC_FP_STORE);
            store(m, m->x[rs1] + (uint32_t)imm, m->f[rs2], 4, &mem);
            break;
        }
        case 0x43:
        case 0x47:
        case 0x4b:
        case 0x4f: { /* FMADD family */
            unsigned rs3 = inst >> 27;
            if (((inst >> 25) & 3) != 0) goto illegal;
            DF(rs1);
            DF(rs2);
            DF(rs3);
            ISSUE(IC_FP_FMA);
            float x = f32(m->f[rs1]), y = f32(m->f[rs2]), z = f32(m->f[rs3]);
            if (op == 0x47 || op == 0x4f) z = -z;
            if (op == 0x4b || op == 0x4f) x = -x;
            fp_begin(fp_rm(m, f3));
            float r = fmaf(x, y, z);
            fp_end(m);
            m->f[rd] = b32(r);
            RF(rd, m->T_ffma);
            break;
        }
        case 0x53: { /* OP-FP */
            uint32_t fa = m->f[rs1], fb = m->f[rs2];
            int rm = fp_rm(m, f3);
            switch (f7) {
            case 0x00: case 0x04: case 0x08: case 0x0c: { /* fadd fsub fmul fdiv */
                DF(rs1);
                DF(rs2);
                ISSUE(f7 == 0x08 ? IC_FP_MUL : f7 == 0x0c ? IC_FP_DIV : IC_FP_ADD);
                fp_begin(rm);
                float x = f32(fa), y = f32(fb), r;
                switch (f7) {
                case 0x00: r = x + y; break;
                case 0x04: r = x - y; break;
                case 0x08: r = x * y; break;
                default: r = x / y; break;
                }
                fp_end(m);
                m->f[rd] = b32(r);
                if (f7 == 0x0c) {
                    occ = m->T_fdiv;
                    m->st.stall_occ += occ - ISS_TPC;
                    RF(rd, m->T_fdiv);
                } else {
                    RF(rd, f7 == 0x08 ? m->T_fmul : m->T_fadd);
                }
                break;
            }
            case 0x2c: /* fsqrt */
                DF(rs1);
                ISSUE(IC_FP_SQRT);
                fp_begin(rm);
                m->f[rd] = b32(sqrtf(f32(fa)));
                fp_end(m);
                occ = m->T_fsqrt;
                m->st.stall_occ += occ - ISS_TPC;
                RF(rd, m->T_fsqrt);
                break;
            case 0x10: /* fsgnj, fsgnjn, fsgnjx */
                DF(rs1);
                DF(rs2);
                ISSUE(IC_FP_MISC);
                if (f3 == 0) v = (fa & 0x7fffffffu) | (fb & 0x80000000u);
                else if (f3 == 1) v = (fa & 0x7fffffffu) | (~fb & 0x80000000u);
                else if (f3 == 2) v = fa ^ (fb & 0x80000000u);
                else goto illegal;
                m->f[rd] = v;
                RF(rd, m->T_fmisc);
                break;
            case 0x14: /* fmin, fmax */
                DF(rs1);
                DF(rs2);
                ISSUE(IC_FP_MISC);
                if (f3 > 1) goto illegal;
                m->f[rd] = fminmax(m, fa, fb, f3 == 1);
                RF(rd, m->T_fmisc);
                break;
            case 0x60: /* fcvt.w[u].s */
                DF(rs1);
                ISSUE(IC_FP_MISC);
                if (rs2 > 1) goto illegal;
                WX(rd, fcvt_w(m, fa, rm, rs2 == 1));
                RX(rd, m->T_fmisc);
                break;
            case 0x70: /* fmv.x.w, fclass */
                DF(rs1);
                ISSUE(IC_FP_MISC);
                if (rs2) goto illegal;
                if (f3 == 0) v = fa;
                else if (f3 == 1) v = fclass(fa);
                else goto illegal;
                WX(rd, v);
                RX(rd, m->T_fmisc);
                break;
            case 0x50: /* feq, flt, fle */
                DF(rs1);
                DF(rs2);
                ISSUE(IC_FP_MISC);
                if (f3 > 2) goto illegal;
                WX(rd, fcmp(m, fa, fb, f3));
                RX(rd, m->T_fmisc);
                break;
            case 0x68: { /* fcvt.s.w[u] */
                DX(rs1);
                ISSUE(IC_FP_MISC);
                if (rs2 > 1) goto illegal;
                fp_begin(rm);
                float r = rs2 ? (float)m->x[rs1] : (float)(int32_t)m->x[rs1];
                fp_end(m);
                m->f[rd] = b32(r);
                RF(rd, m->T_fmisc);
                break;
            }
            case 0x78: /* fmv.w.x */
                DX(rs1);
                ISSUE(IC_FP_MISC);
                if (rs2 || f3) goto illegal;
                m->f[rd] = m->x[rs1];
                RF(rd, m->T_fmisc);
                break;
            default:
                goto illegal;
            }
            break;
        }
        default:
            goto illegal;
        }
    }

done:
    if (m->halted) return;
    if (free_op) { /* simulator services are free and not counted */
        m->t = t0 + fpen;
        m->pc = npc;
        return;
    }
    m->t = issue + occ + mem + ctrl;
    m->st.stall_ctrl += ctrl;
    m->st.cls[cls]++;
    m->st.instret++;
    m->pc = npc;
    if (m->profile) prof_add(m, pc, m->t - t0);
    return;

illegal:
    fail(m, "illegal instruction 0x%08x at pc 0x%08x", inst, pc);
}

/* ------------------------------------------------------------- lifecycle */

int iss_init(iss_t *m, const iss_cfg_t *cfg) {
    memset(m, 0, sizeof *m);
    m->cfg = *cfg;
    const iss_cfg_t *c = &m->cfg;
    m->base[REG_SRAM] = ISS_SRAM_BASE;
    m->base[REG_FLASH] = ISS_FLASH_BASE;
    m->base[REG_PSRAM] = ISS_PSRAM_BASE;
    m->size[REG_SRAM] = (uint32_t)(c->sram_kib * 1024);
    m->size[REG_FLASH] = (uint32_t)(c->flash_kib * 1024);
    m->size[REG_PSRAM] = (uint32_t)(c->psram_kib * 1024);
    for (int r = 0; r < REG_COUNT; r++) {
        m->mem[r] = calloc(1, m->size[r] ? m->size[r] : 1);
        if (!m->mem[r]) return -1;
    }
    if (cache_init(&m->ic, &c->ic) || cache_init(&m->dc, &c->dc)) {
        fprintf(stderr, "iss: invalid cache geometry\n");
        return -1;
    }
    unsigned bht = (unsigned)c->bht_entries, btb = (unsigned)c->btb_entries, ras = (unsigned)c->ras_entries;
    m->bht = calloc(bht ? bht : 1, 1);
    for (unsigned i = 0; i < bht; i++) m->bht[i] = 1; /* weakly not taken */
    m->btb = calloc(btb ? btb : 1, sizeof *m->btb);
    m->ras = calloc(ras ? ras : 1, sizeof *m->ras);
    if (c->bp_enable && (!bht || !btb)) {
        fprintf(stderr, "iss: bp.enable needs bht/btb entries\n");
        return -1;
    }
    m->T_alu = ticks(c->lat_alu);
    m->T_lw = ticks(c->lat_load_word);
    m->T_lsub = ticks(c->lat_load_sub);
    m->T_mul = ticks(c->lat_mul);
    m->T_csr = ticks(c->lat_csr_read);
    m->T_fadd = ticks(c->lat_fp_add);
    m->T_fmul = ticks(c->lat_fp_mul);
    m->T_ffma = ticks(c->lat_fp_fma);
    m->T_fmisc = ticks(c->lat_fp_misc);
    m->T_fload = ticks(c->lat_fp_load);
    m->T_div = ticks(c->occ_div);
    m->T_fdiv = ticks(c->occ_fp_div);
    m->T_fsqrt = ticks(c->occ_fp_sqrt);
    m->T_csrw = ticks(c->occ_csr_write);
    m->T_fencei = ticks(c->occ_fence_i);
    m->T_amo = ticks(c->occ_amo);
    m->T_mis = ticks(c->misaligned_extra);
    m->T_br_ok = ticks(c->br_taken_correct);
    m->T_br_bad = ticks(c->br_mispredict);
    m->T_jal_nobtb = ticks(c->br_jal_nobtb);
    m->T_br_nobp = ticks(c->br_taken_nobp);
    m->T_jalr_nobp = ticks(c->br_jalr_nobp);
    m->T_sram = ticks(c->sram_extra);
    m->T_dchit = ticks(c->dcache_hit_extra);
    m->T_ffill = ticks(c->flash_fill);
    m->T_pfill = ticks(c->psram_fill);
    m->T_pwb = ticks(c->psram_writeback);
    /* occupancies shorter than one cycle make no sense in a single-issue pipe */
    if (m->T_div < ISS_TPC) m->T_div = ISS_TPC;
    if (m->T_fdiv < ISS_TPC) m->T_fdiv = ISS_TPC;
    if (m->T_fsqrt < ISS_TPC) m->T_fsqrt = ISS_TPC;
    if (m->T_csrw < ISS_TPC) m->T_csrw = ISS_TPC;
    if (m->T_fencei < ISS_TPC) m->T_fencei = ISS_TPC;
    if (m->T_amo < ISS_TPC) m->T_amo = ISS_TPC;
    m->console = stdout;
    return 0;
}

void iss_free(iss_t *m) {
    for (int r = 0; r < REG_COUNT; r++) free(m->mem[r]);
    free(m->ic.lines);
    free(m->dc.lines);
    free(m->bht);
    free(m->btb);
    free(m->ras);
    free(m->regions);
    free(m->prof);
    for (size_t i = 0; i < m->nsyms; i++) free(m->syms[i].name);
    free(m->syms);
}

int iss_run(iss_t *m, uint64_t max_instr) {
    while (!m->halted) {
        step(m);
        if (max_instr && m->st.instret >= max_instr && !m->halted) {
            snprintf(m->error, sizeof m->error, "instruction limit %llu reached at pc 0x%08x",
                     (unsigned long long)max_instr, m->pc);
            m->halted = true;
            m->exit_code = -2;
        }
    }
    m->st.cycles = m->t;
    return m->exit_code;
}
