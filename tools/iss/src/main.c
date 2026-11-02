/* iss command line: load timing config, run an ELF, write a JSON report. */
#include <stdlib.h>
#include <string.h>

#include "iss.h"

static void usage(void) {
    fprintf(stderr,
            "usage: iss [options] program.elf\n"
            "  -c FILE        timing config (key = value), may repeat\n"
            "  -s KEY=VALUE   override one timing parameter, may repeat\n"
            "  -o FILE        write the JSON report to FILE\n"
            "  -n N           instruction limit (default 20000000000)\n"
            "  -p N           print the top-N functions by cycles to stderr\n"
            "  -q             discard the program's console output\n"
            "  --keys         list timing parameters with defaults\n"
            "  --dump-config  print the effective configuration and exit\n");
}

static double cyc(ticks_t t) { return (double)t / ISS_TPC; }

static void json_str(FILE *o, const char *s) {
    fputc('"', o);
    for (; *s; s++) {
        if (*s == '"' || *s == '\\')
            fprintf(o, "\\%c", *s);
        else if ((unsigned char)*s < 0x20)
            fprintf(o, "\\u%04x", *s);
        else
            fputc(*s, o);
    }
    fputc('"', o);
}

static void json_stats(FILE *o, const iss_stats_t *s, const char *ind) {
    fprintf(o, "{\n%s  \"cycles\": %.17g,\n%s  \"instret\": %llu,\n%s  \"cpi\": %.6f,\n", ind, cyc(s->cycles), ind,
            (unsigned long long)s->instret, ind, s->instret ? cyc(s->cycles) / (double)s->instret : 0.0);
    fprintf(o, "%s  \"class\": {", ind);
    for (int i = 0, first = 1; i < IC_COUNT; i++) {
        if (!s->cls[i]) continue;
        fprintf(o, "%s\"%s\": %llu", first ? "" : ", ", iss_class_name[i], (unsigned long long)s->cls[i]);
        first = 0;
    }
    fprintf(o, "},\n%s  \"stall\": {\"fetch\": %.17g, \"ctrl\": %.17g, \"occupancy\": %.17g, \"data\": {", ind,
            cyc(s->stall_fetch), cyc(s->stall_ctrl), cyc(s->stall_occ));
    for (int i = 0, first = 1; i < IC_COUNT; i++) {
        if (!s->stall_data[i]) continue;
        fprintf(o, "%s\"%s\": %.17g", first ? "" : ", ", iss_class_name[i], cyc(s->stall_data[i]));
        first = 0;
    }
    fprintf(o, "}, \"mem\": {");
    for (int r = 0; r < REG_COUNT; r++)
        fprintf(o, "%s\"%s\": %.17g", r ? ", " : "", iss_region_name[r], cyc(s->stall_mem[r]));
    fprintf(o, "}},\n");
    fprintf(o,
            "%s  \"branch\": {\"cond\": %llu, \"taken\": %llu, \"mispredict\": %llu, \"jal\": %llu, \"jal_miss\": %llu, "
            "\"jalr\": %llu, \"jalr_miss\": %llu, \"ret\": %llu, \"ret_miss\": %llu},\n",
            ind, (unsigned long long)s->br_cond, (unsigned long long)s->br_taken, (unsigned long long)s->br_mispred,
            (unsigned long long)s->jal, (unsigned long long)s->jal_miss, (unsigned long long)s->jalr,
            (unsigned long long)s->jalr_miss, (unsigned long long)s->ret, (unsigned long long)s->ret_miss);
    fprintf(o, "%s  \"icache\": {", ind);
    for (int r = REG_FLASH; r < REG_COUNT; r++)
        fprintf(o, "%s\"%s\": {\"access\": %llu, \"miss\": %llu}", r > REG_FLASH ? ", " : "", iss_region_name[r],
                (unsigned long long)s->ic_access[r], (unsigned long long)s->ic_miss[r]);
    fprintf(o, "},\n%s  \"dcache\": {", ind);
    for (int r = REG_FLASH; r < REG_COUNT; r++)
        fprintf(o, "%s\"%s\": {\"access\": %llu, \"miss\": %llu, \"writeback\": %llu}", r > REG_FLASH ? ", " : "",
                iss_region_name[r], (unsigned long long)s->dc_access[r], (unsigned long long)s->dc_miss[r],
                (unsigned long long)s->dc_writeback[r]);
    fprintf(o, "},\n%s  \"mem_access\": {", ind);
    for (int r = 0; r < REG_COUNT; r++)
        fprintf(o, "%s\"%s\": %llu", r ? ", " : "", iss_region_name[r], (unsigned long long)s->mem_access[r]);
    fprintf(o, "},\n%s  \"misaligned\": %llu\n%s}", ind, (unsigned long long)s->misaligned, ind);
}

static void json_report(FILE *o, iss_t *m, const char *elf) {
    fprintf(o, "{\n  \"elf\": ");
    json_str(o, elf);
    fprintf(o, ",\n  \"exit_code\": %d,\n  \"error\": ", m->exit_code);
    if (m->error[0])
        json_str(o, m->error);
    else
        fprintf(o, "null");
    fprintf(o, ",\n  \"config\": {");
    for (size_t i = 0; i < iss_cfg_nkeys; i++)
        fprintf(o, "%s\n    \"%s\": %.17g", i ? "," : "", iss_cfg_keys[i].key,
                *(const double *)((const char *)&m->cfg + iss_cfg_keys[i].off));
    fprintf(o, "\n  },\n  \"total\": ");
    json_stats(o, &m->st, "  ");
    fprintf(o, ",\n  \"regions\": [");
    for (size_t i = 0; i < m->nregions; i++) {
        iss_region_t *r = &m->regions[i];
        fprintf(o, "%s\n    {\"name\": ", i ? "," : "");
        json_str(o, r->name);
        fprintf(o, ", \"units\": %llu, \"aux_units\": %llu, \"cycles\": %.17g, \"cycles_per_unit\": %.6f,\n     \"stats\": ",
                (unsigned long long)r->units, (unsigned long long)r->aux_units, cyc(r->d.cycles),
                r->units ? cyc(r->d.cycles) / (double)r->units : 0.0);
        json_stats(o, &r->d, "     ");
        fprintf(o, "}");
    }
    fprintf(o, "\n  ]\n}\n");
}

int main(int argc, char **argv) {
    iss_cfg_t cfg;
    iss_cfg_default(&cfg);
    const char *out = NULL, *elf = NULL;
    const char *sets[128];
    int nsets = 0, top = 0, quiet = 0, dump = 0;
    unsigned long long limit = 20000000000ull;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-c") && i + 1 < argc) {
            if (iss_cfg_load(&cfg, argv[++i])) return 2;
        } else if (!strcmp(a, "-s") && i + 1 < argc) {
            if (nsets < 128) sets[nsets++] = argv[++i];
        } else if (!strcmp(a, "-o") && i + 1 < argc) {
            out = argv[++i];
        } else if (!strcmp(a, "-n") && i + 1 < argc) {
            limit = strtoull(argv[++i], NULL, 0);
        } else if (!strcmp(a, "-p") && i + 1 < argc) {
            top = atoi(argv[++i]);
        } else if (!strcmp(a, "-q")) {
            quiet = 1;
        } else if (!strcmp(a, "--keys")) {
            for (size_t k = 0; k < iss_cfg_nkeys; k++)
                printf("%-22s %10g  %s\n", iss_cfg_keys[k].key,
                       *(const double *)((const char *)&cfg + iss_cfg_keys[k].off), iss_cfg_keys[k].help);
            return 0;
        } else if (!strcmp(a, "--dump-config")) {
            dump = 1;
        } else if (a[0] == '-') {
            usage();
            return 2;
        } else {
            elf = a;
        }
    }
    for (int i = 0; i < nsets; i++)
        if (iss_cfg_set(&cfg, sets[i])) return 2;
    if (dump) {
        for (size_t k = 0; k < iss_cfg_nkeys; k++)
            printf("%s = %.17g\n", iss_cfg_keys[k].key, *(const double *)((const char *)&cfg + iss_cfg_keys[k].off));
        return 0;
    }
    if (!elf) {
        usage();
        return 2;
    }
    iss_t m;
    if (iss_init(&m, &cfg)) return 2;
    if (quiet) m.console = NULL;
    m.profile = top > 0;
    if (iss_load_elf(&m, elf)) return 2;
    int rc = iss_run(&m, limit);
    if (m.console) fflush(m.console);
    if (m.error[0]) fprintf(stderr, "iss: %s\n", m.error);
    if (out) {
        FILE *o = fopen(out, "w");
        if (!o) {
            fprintf(stderr, "iss: cannot write %s\n", out);
            return 2;
        }
        json_report(o, &m, elf);
        fclose(o);
    }
    if (top > 0) iss_print_profile(&m, stderr, top);
    fprintf(stderr, "iss: exit %d, %llu instructions, %.0f cycles, CPI %.3f\n", rc,
            (unsigned long long)m.st.instret, cyc(m.st.cycles),
            m.st.instret ? cyc(m.st.cycles) / (double)m.st.instret : 0.0);
    iss_free(&m);
    return rc < 0 ? 3 : rc;
}
