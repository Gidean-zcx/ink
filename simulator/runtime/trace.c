/*
 * Chrome trace-event JSON writer (open in https://ui.perfetto.dev or
 * chrome://tracing). Process 1 = CPU cores (one row per core, slices = task
 * runs); process 2 = device tracks (display, flash, input, i2c, marks...).
 * ISR-safe; everything is a no-op unless sim_trace_open() was called.
 */
#include <stdio.h>
#include <string.h>

#include "sim.h"

#define MAX_TRACKS 32

static FILE *TF;
static bool first = true;
static const char *tracks[MAX_TRACKS];
static int ntracks;
static uint64_t run_start[VTK_MAX_CORES];
static const vtk_task_t *run_task[VTK_MAX_CORES];

static void emit_raw(const char *s) {
    if (!TF) return;
    if (!first) fputs(",\n", TF);
    first = false;
    fputs(s, TF);
}

static void json_escape(char *dst, size_t n, const char *src) {
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j + 2 < n; i++) {
        char c = src[i];
        if (c == '"' || c == '\\') {
            dst[j++] = '\\';
            dst[j++] = c;
        } else if ((unsigned char)c < 0x20) {
            dst[j++] = ' ';
        } else {
            dst[j++] = c;
        }
    }
    dst[j] = 0;
}

static int track_id(const char *name) {
    for (int i = 0; i < ntracks; i++)
        if (!strcmp(tracks[i], name)) return i + 1;
    if (ntracks >= MAX_TRACKS) return MAX_TRACKS;
    tracks[ntracks] = name;
    int id = ++ntracks;
    char buf[200], esc[100];
    json_escape(esc, sizeof esc, name);
    snprintf(buf, sizeof buf, "{\"ph\":\"M\",\"pid\":2,\"tid\":%d,\"name\":\"thread_name\",\"args\":{\"name\":\"%s\"}}",
             id, esc);
    emit_raw(buf);
    return id;
}

void sim_trace_open(const char *path) {
    TF = fopen(path, "wb");
    if (!TF) {
        fprintf(stderr, "cannot write trace %s\n", path);
        return;
    }
    fputs("[\n", TF);
    first = true;
    char buf[200];
    emit_raw("{\"ph\":\"M\",\"pid\":1,\"name\":\"process_name\",\"args\":{\"name\":\"CPU cores\"}}");
    emit_raw("{\"ph\":\"M\",\"pid\":2,\"name\":\"process_name\",\"args\":{\"name\":\"device\"}}");
    for (int c = 0; c < g_profile.cores; c++) {
        snprintf(buf, sizeof buf, "{\"ph\":\"M\",\"pid\":1,\"tid\":%d,\"name\":\"thread_name\",\"args\":{\"name\":\"core %d\"}}",
                 c, c);
        emit_raw(buf);
    }
}

void sim_trace_close(void) {
    if (!TF) return;
    /* close open slices */
    for (int c = 0; c < VTK_MAX_CORES; c++)
        if (run_task[c]) sim_trace_switch_hook(c, run_task[c], false, vtk_now());
    fputs("\n]\n", TF);
    fclose(TF);
    TF = NULL;
}

bool sim_trace_enabled(void) { return TF != NULL; }

/* Writing the trace is host work (vtk_host_call): it must not count as the calling task's stack. */
typedef struct {
    const char *track, *name, *args_json;
    uint64_t t0_ns, t1_ns;
    double value;
} event_t;

static void span_on_host(void *p) {
    const event_t *e = p;
    int tid = track_id(e->track);
    char buf[700], esc[160];
    json_escape(esc, sizeof esc, e->name);
    snprintf(buf, sizeof buf, "{\"ph\":\"X\",\"pid\":2,\"tid\":%d,\"name\":\"%s\",\"ts\":%.3f,\"dur\":%.3f%s%s%s}", tid, esc,
             e->t0_ns / 1000.0, (e->t1_ns > e->t0_ns ? e->t1_ns - e->t0_ns : 0) / 1000.0, e->args_json ? ",\"args\":" : "",
             e->args_json ? e->args_json : "", "");
    emit_raw(buf);
}

void sim_trace_span(const char *track, const char *name, uint64_t t0_ns, uint64_t t1_ns, const char *args_json) {
    if (!TF) return;
    event_t e = {.track = track, .name = name, .args_json = args_json, .t0_ns = t0_ns, .t1_ns = t1_ns};
    vtk_host_call(span_on_host, &e);
}

static void instant_on_host(void *p) {
    const event_t *e = p;
    int tid = track_id(e->track);
    char buf[700], esc[160];
    json_escape(esc, sizeof esc, e->name);
    snprintf(buf, sizeof buf, "{\"ph\":\"i\",\"s\":\"t\",\"pid\":2,\"tid\":%d,\"name\":\"%s\",\"ts\":%.3f%s%s}", tid, esc,
             e->t0_ns / 1000.0, e->args_json ? ",\"args\":" : "", e->args_json ? e->args_json : "");
    emit_raw(buf);
}

void sim_trace_instant(const char *track, const char *name, uint64_t t_ns, const char *args_json) {
    if (!TF) return;
    event_t e = {.track = track, .name = name, .args_json = args_json, .t0_ns = t_ns};
    vtk_host_call(instant_on_host, &e);
}

static void counter_on_host(void *p) {
    const event_t *e = p;
    char buf[300], esc[100];
    json_escape(esc, sizeof esc, e->name);
    snprintf(buf, sizeof buf, "{\"ph\":\"C\",\"pid\":2,\"name\":\"%s\",\"ts\":%.3f,\"args\":{\"value\":%.3f}}", esc,
             e->t0_ns / 1000.0, e->value);
    emit_raw(buf);
}

void sim_trace_counter(const char *name, uint64_t t_ns, double value) {
    if (!TF) return;
    event_t e = {.name = name, .t0_ns = t_ns, .value = value};
    vtk_host_call(counter_on_host, &e);
}

void sim_trace_switch_hook(int core, const vtk_task_t *t, bool running, uint64_t t_ns) {
    if (!TF || core < 0 || core >= VTK_MAX_CORES) return;
    if (running) {
        run_start[core] = t_ns;
        run_task[core] = t;
        return;
    }
    if (run_task[core] != t) return;
    run_task[core] = NULL;
    char buf[300], esc[64];
    json_escape(esc, sizeof esc, vtk_task_name(t));
    snprintf(buf, sizeof buf, "{\"ph\":\"X\",\"pid\":1,\"tid\":%d,\"name\":\"%s\",\"ts\":%.3f,\"dur\":%.3f}", core, esc,
             run_start[core] / 1000.0, (t_ns - run_start[core]) / 1000.0);
    emit_raw(buf);
}
