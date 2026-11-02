/*
 * Metrics registry, warnings and CI budget evaluation for the simulator.
 * All functions are ISR-safe (no OSAL/vtk calls), see sim.h.
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "sim.h"

typedef enum { M_COUNTER, M_GAUGE, M_MAX, M_DIST } mkind_t;

typedef struct {
    char *name;
    mkind_t kind;
    double value;
    double *samples;
    size_t n, cap;
} metric_t;

static metric_t *M;
static size_t nM, capM;

#define MAX_WARN_KEEP 200
static char *W[MAX_WARN_KEEP];
static size_t nW, nW_total;

static metric_t *find(const char *name, mkind_t kind) {
    for (size_t i = 0; i < nM; i++)
        if (!strcmp(M[i].name, name)) return &M[i];
    if (nM == capM) {
        capM = capM ? capM * 2 : 64;
        M = realloc(M, capM * sizeof *M);
        if (!M) abort();
    }
    metric_t *m = &M[nM++];
    memset(m, 0, sizeof *m);
    m->name = strdup(name);
    m->kind = kind;
    if (kind == M_MAX) m->value = -INFINITY;
    return m;
}

void sim_metric_sample(const char *name, double v) {
    metric_t *m = find(name, M_DIST);
    if (m->n == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 32;
        m->samples = realloc(m->samples, m->cap * sizeof(double));
        if (!m->samples) abort();
    }
    m->samples[m->n++] = v;
}

void sim_metric_add(const char *name, double v) { find(name, M_COUNTER)->value += v; }
void sim_metric_set(const char *name, double v) { find(name, M_GAUGE)->value = v; }
void sim_metric_max(const char *name, double v) {
    metric_t *m = find(name, M_MAX);
    if (v > m->value) m->value = v;
}

static void stamp(char *buf, size_t n) {
    uint64_t t = vtk_now();
    snprintf(buf, n, "%llu.%03llu", (unsigned long long)(t / 1000000000ull),
             (unsigned long long)(t / 1000000ull % 1000ull));
}

void sim_warn(const char *fmt, ...) {
    char msg[600], ts[32];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    stamp(ts, sizeof ts);
    if (!g_sim.quiet) fprintf(stderr, "[%10ss] WARN %s\n", ts, msg);
    nW_total++;
    sim_metric_add("warnings.count", 1);
    if (nW < MAX_WARN_KEEP) {
        char line[660];
        snprintf(line, sizeof line, "%ss %s", ts, msg);
        W[nW++] = strdup(line);
    }
}

void sim_note(const char *fmt, ...) {
    if (g_sim.quiet) return;
    char msg[600], ts[32];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    stamp(ts, sizeof ts);
    fprintf(stderr, "[%10ss] %s\n", ts, msg);
}

static int cmp_d(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static double pct(const double *sorted, size_t n, double p) {
    if (!n) return NAN;
    /* nearest-rank: pessimistic for small sample counts */
    size_t rank = (size_t)ceil(p / 100.0 * (double)n);
    if (rank < 1) rank = 1;
    if (rank > n) rank = n;
    return sorted[rank - 1];
}

typedef struct {
    double count, mean, min, p50, p95, p99, max;
} dstat_t;

static dstat_t dist_stats(metric_t *m) {
    dstat_t s = {0};
    s.count = (double)m->n;
    if (!m->n) return s;
    qsort(m->samples, m->n, sizeof(double), cmp_d);
    double sum = 0;
    for (size_t i = 0; i < m->n; i++) sum += m->samples[i];
    s.mean = sum / (double)m->n;
    s.min = m->samples[0];
    s.max = m->samples[m->n - 1];
    s.p50 = pct(m->samples, m->n, 50);
    s.p95 = pct(m->samples, m->n, 95);
    s.p99 = pct(m->samples, m->n, 99);
    return s;
}

/* Resolve "a.b.c" or "a.b.c.p95" to a number. Returns 0 if found. */
static int resolve(const char *key, double *out) {
    for (size_t i = 0; i < nM; i++)
        if (!strcmp(M[i].name, key) && M[i].kind != M_DIST) {
            *out = M[i].value;
            return 0;
        }
    const char *dot = strrchr(key, '.');
    if (!dot) return -1;
    size_t base = (size_t)(dot - key);
    const char *stat = dot + 1;
    for (size_t i = 0; i < nM; i++) {
        if (M[i].kind != M_DIST || strlen(M[i].name) != base || strncmp(M[i].name, key, base)) continue;
        dstat_t s = dist_stats(&M[i]);
        if (!strcmp(stat, "count")) *out = s.count;
        else if (!s.count) return -1;
        else if (!strcmp(stat, "mean")) *out = s.mean;
        else if (!strcmp(stat, "min")) *out = s.min;
        else if (!strcmp(stat, "p50")) *out = s.p50;
        else if (!strcmp(stat, "p95")) *out = s.p95;
        else if (!strcmp(stat, "p99")) *out = s.p99;
        else if (!strcmp(stat, "max")) *out = s.max;
        else return -1;
        return 0;
    }
    return -1;
}

static int cmp_name(const void *a, const void *b) {
    return strcmp(((const metric_t *)a)->name, ((const metric_t *)b)->name);
}

static cJSON *eval_budgets(const char *budget_path, int *violations) {
    *violations = 0;
    cJSON *res = cJSON_CreateArray();
    if (!budget_path) return res;
    FILE *f = fopen(budget_path, "rb");
    if (!f) {
        sim_warn("cannot read budget file %s", budget_path);
        (*violations)++;
        return res;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *txt = malloc((size_t)n + 1);
    size_t got = fread(txt, 1, (size_t)n, f);
    txt[got] = 0;
    fclose(f);
    cJSON *b = cJSON_Parse(txt);
    free(txt);
    if (!b) {
        sim_warn("invalid budget JSON %s", budget_path);
        (*violations)++;
        return res;
    }
    cJSON *items = cJSON_GetObjectItemCaseSensitive(b, "budgets");
    if (!items) items = b;
    cJSON *it;
    cJSON_ArrayForEach(it, items) {
        if (!cJSON_IsObject(it) || it->string[0] == '_') continue;
        double v = 0;
        int found = resolve(it->string, &v) == 0;
        cJSON *mx = cJSON_GetObjectItemCaseSensitive(it, "max");
        cJSON *mn = cJSON_GetObjectItemCaseSensitive(it, "min");
        bool optional = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(it, "optional"));
        const char *why = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(it, "why"));
        bool ok = true;
        char reason[160] = "";
        if (!found) {
            ok = optional;
            snprintf(reason, sizeof reason, "metric not recorded");
        } else {
            if (cJSON_IsNumber(mx) && v > mx->valuedouble) {
                ok = false;
                snprintf(reason, sizeof reason, "%.3f > max %.3f", v, mx->valuedouble);
            }
            if (cJSON_IsNumber(mn) && v < mn->valuedouble) {
                ok = false;
                snprintf(reason, sizeof reason, "%.3f < min %.3f", v, mn->valuedouble);
            }
        }
        cJSON *r = cJSON_CreateObject();
        cJSON_AddStringToObject(r, "metric", it->string);
        if (found) cJSON_AddNumberToObject(r, "value", v);
        if (cJSON_IsNumber(mx)) cJSON_AddNumberToObject(r, "max", mx->valuedouble);
        if (cJSON_IsNumber(mn)) cJSON_AddNumberToObject(r, "min", mn->valuedouble);
        cJSON_AddBoolToObject(r, "ok", ok);
        if (reason[0]) cJSON_AddStringToObject(r, "detail", reason);
        if (why) cJSON_AddStringToObject(r, "why", why);
        cJSON_AddItemToArray(res, r);
        if (!ok) {
            (*violations)++;
            fprintf(stderr, "BUDGET FAIL %s: %s%s%s\n", it->string, reason, why ? " — " : "", why ? why : "");
        }
    }
    cJSON_Delete(b);
    return res;
}

int sim_metrics_write(const char *path, const char *budget_path, const char *status) {
    qsort(M, nM, sizeof *M, cmp_name);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", status);
    cJSON_AddStringToObject(root, "profile", g_profile.id);
    cJSON_AddStringToObject(root, "cpu_mode", g_sim.cpu_mode == VTK_CPU_MEASURED ? "measured" : "annotated");
    cJSON_AddNumberToObject(root, "virtual_time_s", (double)vtk_now() / 1e9);
    cJSON *ms = cJSON_AddObjectToObject(root, "metrics");
    for (size_t i = 0; i < nM; i++) {
        metric_t *m = &M[i];
        if (m->kind == M_DIST) {
            dstat_t s = dist_stats(m);
            cJSON *o = cJSON_AddObjectToObject(ms, m->name);
            cJSON_AddNumberToObject(o, "count", s.count);
            if (s.count) {
                cJSON_AddNumberToObject(o, "mean", s.mean);
                cJSON_AddNumberToObject(o, "min", s.min);
                cJSON_AddNumberToObject(o, "p50", s.p50);
                cJSON_AddNumberToObject(o, "p95", s.p95);
                cJSON_AddNumberToObject(o, "p99", s.p99);
                cJSON_AddNumberToObject(o, "max", s.max);
            }
        } else {
            cJSON_AddNumberToObject(ms, m->name, isfinite(m->value) ? m->value : 0);
        }
    }
    cJSON *wa = cJSON_AddArrayToObject(root, "warnings");
    for (size_t i = 0; i < nW; i++) cJSON_AddItemToArray(wa, cJSON_CreateString(W[i]));
    if (nW_total > nW) cJSON_AddNumberToObject(root, "warnings_truncated", (double)(nW_total - nW));
    int violations = 0;
    cJSON *br = eval_budgets(budget_path, &violations);
    cJSON_AddItemToObject(root, "budgets", br);
    cJSON_AddNumberToObject(root, "budget_violations", violations);
    char *txt = cJSON_Print(root);
    if (path) {
        FILE *f = fopen(path, "wb");
        if (f) {
            fputs(txt, f);
            fputc('\n', f);
            fclose(f);
        } else {
            fprintf(stderr, "cannot write %s\n", path);
        }
    }
    free(txt);
    cJSON_Delete(root);
    return violations;
}

void sim_metrics_print_summary(void) {
    qsort(M, nM, sizeof *M, cmp_name);
    fprintf(stderr, "---- metrics (%s, %.3f s virtual) ----\n", g_profile.id, (double)vtk_now() / 1e9);
    for (size_t i = 0; i < nM; i++) {
        metric_t *m = &M[i];
        if (!strncmp(m->name, "task.", 5) || !strncmp(m->name, "mem.tag.", 8)) continue; /* detailed: see JSON */
        if (m->kind == M_DIST) {
            dstat_t s = dist_stats(m);
            if (s.count)
                fprintf(stderr, "  %-44s n=%-5.0f p50=%-10.2f p95=%-10.2f max=%.2f\n", m->name, s.count, s.p50, s.p95,
                        s.max);
            else
                fprintf(stderr, "  %-44s n=0\n", m->name);
        } else {
            fprintf(stderr, "  %-44s %.2f\n", m->name, isfinite(m->value) ? m->value : 0);
        }
    }
}
