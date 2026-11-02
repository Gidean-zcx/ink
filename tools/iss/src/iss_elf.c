/* ELF32 little-endian RISC-V loader (PT_LOAD segments + .symtab) and the
 * per-function profile printer. Own minimal structs keep it portable. */
#include <stdlib.h>
#include <string.h>

#include "iss.h"

typedef struct {
    uint8_t ident[16];
    uint16_t type, machine;
    uint32_t version, entry, phoff, shoff, flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} ehdr_t;
typedef struct {
    uint32_t type, offset, vaddr, paddr, filesz, memsz, flags, align;
} phdr_t;
typedef struct {
    uint32_t name, type, flags, addr, offset, size, link, info, addralign, entsize;
} shdr_t;
typedef struct {
    uint32_t name, value, size;
    uint8_t info, other;
    uint16_t shndx;
} sym_t;

static char *dupstr(const char *s) {
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static int sym_cmp(const void *a, const void *b) {
    const struct iss_sym *x = a, *y = b;
    return x->addr < y->addr ? -1 : x->addr > y->addr;
}

int iss_load_elf(iss_t *m, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "iss: cannot open %s\n", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)n);
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(buf);
        return -1;
    }
    fclose(f);
    if (n < 52 || memcmp(buf, "\x7f" "ELF", 4) || buf[4] != 1 || buf[5] != 1 || u16(buf + 18) != 243) {
        fprintf(stderr, "iss: %s is not an ELF32 little-endian RISC-V file\n", path);
        free(buf);
        return -1;
    }
    ehdr_t eh = {.entry = u32(buf + 24), .phoff = u32(buf + 28), .shoff = u32(buf + 32), .phentsize = u16(buf + 42),
                 .phnum = u16(buf + 44), .shentsize = u16(buf + 46), .shnum = u16(buf + 48)};
    for (unsigned i = 0; i < eh.phnum; i++) {
        const uint8_t *p = buf + eh.phoff + (size_t)i * eh.phentsize;
        phdr_t ph = {u32(p), u32(p + 4), u32(p + 8), u32(p + 12), u32(p + 16), u32(p + 20), u32(p + 24), u32(p + 28)};
        if (ph.type != 1 || !ph.memsz) continue; /* PT_LOAD */
        uint32_t a = ph.paddr;
        int r = -1;
        for (int k = 0; k < REG_COUNT; k++)
            if (a - m->base[k] < m->size[k] && a - m->base[k] + ph.memsz <= m->size[k]) r = k;
        if (r < 0 || ph.offset + ph.filesz > (uint32_t)n) {
            fprintf(stderr, "iss: segment 0x%08x+0x%x does not fit the memory map\n", a, ph.memsz);
            free(buf);
            return -1;
        }
        uint8_t *dst = m->mem[r] + (a - m->base[r]);
        memcpy(dst, buf + ph.offset, ph.filesz);
        memset(dst + ph.filesz, 0, ph.memsz - ph.filesz);
    }
    m->pc = eh.entry;
    /* symbols (optional) */
    if (eh.shoff && eh.shnum) {
        for (unsigned i = 0; i < eh.shnum; i++) {
            const uint8_t *s = buf + eh.shoff + (size_t)i * eh.shentsize;
            shdr_t sh = {u32(s), u32(s + 4), u32(s + 8), u32(s + 12), u32(s + 16), u32(s + 20), u32(s + 24), 0, 0, 0};
            if (sh.type != 2 || sh.link >= eh.shnum) continue; /* SHT_SYMTAB */
            const uint8_t *ls = buf + eh.shoff + (size_t)sh.link * eh.shentsize;
            uint32_t stroff = u32(ls + 16);
            size_t cnt = sh.size / 16;
            m->syms = calloc(cnt ? cnt : 1, sizeof *m->syms);
            for (size_t k = 0; k < cnt; k++) {
                const uint8_t *e = buf + sh.offset + k * 16;
                sym_t sy = {u32(e), u32(e + 4), u32(e + 8), e[12], e[13], u16(e + 14)};
                if ((sy.info & 15) != 2 || !sy.value) continue; /* STT_FUNC */
                m->syms[m->nsyms].addr = sy.value;
                m->syms[m->nsyms].size = sy.size;
                m->syms[m->nsyms].name = dupstr((const char *)buf + stroff + sy.name);
                m->nsyms++;
            }
            qsort(m->syms, m->nsyms, sizeof *m->syms, sym_cmp);
            break;
        }
    }
    free(buf);
    return 0;
}

static const char *sym_name(const iss_t *m, uint32_t pc) {
    size_t lo = 0, hi = m->nsyms;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (m->syms[mid].addr <= pc)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (!lo) return "?";
    const struct iss_sym *s = &m->syms[lo - 1];
    return (!s->size || pc < s->addr + s->size) ? s->name : "?";
}

typedef struct {
    const char *name;
    uint64_t ticks, count;
} fn_t;

static int fn_cmp(const void *a, const void *b) {
    const fn_t *x = a, *y = b;
    return x->ticks < y->ticks ? 1 : x->ticks > y->ticks ? -1 : strcmp(x->name, y->name);
}

void iss_print_profile(iss_t *m, FILE *out, int top) {
    fn_t *fn = calloc(m->nsyms + 1, sizeof *fn);
    size_t nfn = 0;
    uint64_t total = 0;
    for (size_t i = 0; i < m->prof_cap; i++) {
        if (!m->prof[i].count) continue;
        const char *name = sym_name(m, m->prof[i].pc);
        total += m->prof[i].ticks;
        size_t k;
        for (k = 0; k < nfn; k++)
            if (fn[k].name == name) break;
        if (k == nfn) {
            fn[nfn].name = name;
            nfn++;
        }
        fn[k].ticks += m->prof[i].ticks;
        fn[k].count += m->prof[i].count;
    }
    qsort(fn, nfn, sizeof *fn, fn_cmp);
    fprintf(out, "%-32s %14s %7s %12s %6s\n", "function", "cycles", "%", "instr", "CPI");
    for (size_t k = 0; k < nfn && (int)k < top; k++)
        fprintf(out, "%-32s %14.0f %6.2f%% %12llu %6.2f\n", fn[k].name, (double)fn[k].ticks / ISS_TPC,
                total ? 100.0 * (double)fn[k].ticks / (double)total : 0.0, (unsigned long long)fn[k].count,
                fn[k].count ? (double)fn[k].ticks / ISS_TPC / (double)fn[k].count : 0.0);
    free(fn);
}
