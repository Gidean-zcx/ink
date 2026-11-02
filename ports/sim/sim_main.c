/*
 * ink-sim: runs the portable ink system on a simulated device.
 *
 *   ink-sim --profile profiles/metalio-plus.json --script scenario.txt --out out/ \
 *           [--budget budgets.json] [--trace] [--frames] [--end 20s]
 *           [--cpu annotated|measured[:factor]] [--realtime[=speed]]
 *           [--flash-image user.bin] [--sd-dir dir] [--seed N] [--quiet]
 *
 * Exit status: 0 ok, 1 budget violated, 2 panic, 3 usage/config error.
 * Outputs in --out: metrics.json, final.png, snap-*.png, frames/ (with
 * --frames), trace.json (with --trace; open in https://ui.perfetto.dev).
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "sim.h"

sim_options_t g_sim;

static uint32_t rng_state = 0x9e3779b9u;

uint32_t sim_rand(void) {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng_state = x;
}

double sim_rand01(void) { return (double)(sim_rand() >> 8) / (double)(1u << 24); }

static void usage(void) {
    fprintf(stderr,
            "usage: ink-sim --profile P.json [--script S] [--out DIR] [--budget B.json] [--trace] [--frames]\n"
            "               [--end 20s] [--cpu annotated|measured[:factor]] [--realtime[=speed]]\n"
            "               [--flash-image F] [--sd-dir D] [--assets DIR]... [--content DIR] [--hub F.json]\n"
            "               [--no-random-outages] [--seed N] [--flash-timing typ|max|tail] [--quiet]\n");
}

static int parse_time(const char *s, uint64_t *ns) {
    char *end;
    double v = strtod(s, &end);
    if (end == s) return -1;
    double mul = 1e9;
    if (!strcmp(end, "ms")) mul = 1e6;
    else if (!strcmp(end, "us")) mul = 1e3;
    else if (!strcmp(end, "m")) mul = 60e9;
    else if (*end && strcmp(end, "s")) return -1;
    *ns = (uint64_t)(v * mul);
    return 0;
}

static int mkdir_p(const char *path) {
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *q = tmp + 1; *q; q++)
        if (*q == '/') {
            *q = 0;
            if (mkdir(tmp, 0755) && errno != EEXIST) return -1;
            *q = '/';
        }
    return (mkdir(tmp, 0755) && errno != EEXIST) ? -1 : 0;
}

static bool g_finalized;
static int finalize(const char *status) {
    if (g_finalized) return 0;
    g_finalized = true;
    sim_storage_shutdown();
    sim_osal_report();
    sim_display_report();
    sim_input_report();
    sim_storage_report();
    sim_net_report();
    sim_audio_report();
    sim_assets_report();
    sim_power_report();
    sim_trace_close();
    char path[600];
    snprintf(path, sizeof path, "%s/metrics.json", g_sim.out_dir);
    if (!g_sim.quiet) sim_metrics_print_summary();
    int v = sim_metrics_write(path, g_sim.budget_path, status);
    fprintf(stderr, "ink-sim: %s, %.3f s virtual, report %s%s\n", status, (double)vtk_now() / 1e9, path,
            v ? " — BUDGET VIOLATIONS" : "");
    return v;
}

void sim_fatal(const char *reason) {
    finalize(reason);
    fflush(NULL);
    exit(2);
}

static void boot_task(void *arg) {
    (void)arg;
    const char *prev = osal_mem_tag("platform");
    sim_i2c_init();
    sim_power_init();
    sim_storage_init();
    sim_display_init();
    sim_input_init();
    sim_net_init();
    sim_audio_init();
    osal_mem_tag(prev);
    sim_metric_sample("boot.platform_ready_ms", vtk_now() / 1e6);
    ink_main();
}

static void vtk_warn_sink(const char *msg) { sim_warn("%s", msg); }
static void pools_warn_sink(const char *msg) { sim_warn("%s", msg); }

static void sampler(void *arg) { /* ISR: periodic heap fragmentation sampling */
    (void)arg;
    pools_sample();
    vtk_event_after(SIM_MS(50), sampler, NULL);
}

int main(int argc, char **argv) {
    g_sim.out_dir = "sim-out";
    g_sim.cpu_mode = VTK_CPU_ANNOTATED;
    g_sim.realtime_speed = 1.0;
    g_sim.seed = 1;
    g_sim.wall_start_us = 1793577600ll * 1000000ll; /* 2026-11-02T00:00:00Z, deterministic */
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;
#define NEEDV()                                         \
    do {                                                \
        if (!v) {                                       \
            fprintf(stderr, "missing value for %s\n", a); \
            return 3;                                   \
        }                                               \
        i++;                                            \
    } while (0)
        if (!strcmp(a, "--profile")) { NEEDV(); g_sim.profile_path = v; }
        else if (!strcmp(a, "--script")) { NEEDV(); g_sim.script_path = v; }
        else if (!strcmp(a, "--out")) { NEEDV(); g_sim.out_dir = v; }
        else if (!strcmp(a, "--budget")) { NEEDV(); g_sim.budget_path = v; }
        else if (!strcmp(a, "--flash-image")) { NEEDV(); g_sim.flash_image = v; }
        else if (!strcmp(a, "--sd-dir")) { NEEDV(); g_sim.sd_dir = v; }
        else if (!strcmp(a, "--assets")) {
            NEEDV();
            if (g_sim.nasset_dirs < 4) g_sim.asset_dirs[g_sim.nasset_dirs++] = v;
        } else if (!strcmp(a, "--content")) { NEEDV(); g_sim.seed_content = v; }
        else if (!strcmp(a, "--hub")) { NEEDV(); g_sim.hub_script = v; }
        else if (!strcmp(a, "--no-random-outages")) g_sim.no_random_outages = true;
        else if (!strcmp(a, "--seed")) { NEEDV(); g_sim.seed = (uint32_t)strtoul(v, NULL, 0); }
        else if (!strcmp(a, "--flash-timing")) { NEEDV(); g_sim.flash_timing = v; }
        else if (!strcmp(a, "--reset-reason")) { NEEDV(); g_sim.reset_reason = v; }
        else if (!strcmp(a, "--trace")) g_sim.trace = true;
        else if (!strcmp(a, "--frames")) g_sim.frames = true;
        else if (!strcmp(a, "--quiet")) g_sim.quiet = true;
        else if (!strncmp(a, "--realtime", 10)) {
            g_sim.realtime = true;
            if (a[10] == '=') g_sim.realtime_speed = atof(a + 11);
        } else if (!strcmp(a, "--end")) {
            NEEDV();
            if (parse_time(v, &g_sim.end_ns)) { fprintf(stderr, "bad --end %s\n", v); return 3; }
        } else if (!strcmp(a, "--cpu")) {
            NEEDV();
            if (!strncmp(v, "measured", 8)) {
                g_sim.cpu_mode = VTK_CPU_MEASURED;
                if (v[8] == ':') g_sim.host_to_target = atof(v + 9);
            } else if (!strcmp(v, "annotated")) {
                g_sim.cpu_mode = VTK_CPU_ANNOTATED;
            } else { fprintf(stderr, "bad --cpu %s\n", v); return 3; }
        } else if (!strcmp(a, "-h") || !strcmp(a, "--help")) { usage(); return 0; }
        else { fprintf(stderr, "unknown option %s\n", a); usage(); return 3; }
    }
    if (!g_sim.profile_path) { usage(); return 3; }
    if (!g_sim.nasset_dirs) g_sim.asset_dirs[g_sim.nasset_dirs++] = "assets";
    rng_state = g_sim.seed ? g_sim.seed * 2654435761u : 0x9e3779b9u;

    char err[300];
    if (profile_load(g_sim.profile_path, &g_profile, err, sizeof err)) {
        fprintf(stderr, "profile: %s\n", err);
        return 3;
    }
    if (g_profile.ndefaults && !g_sim.quiet)
        fprintf(stderr, "ink-sim: profile leaves %d values at simulator defaults: %s\n", g_profile.ndefaults,
                g_profile.defaults_used);
    if (g_sim.flash_timing) {
        const char *ft = g_sim.flash_timing;
        if (!strcmp(ft, "typ")) g_profile.flash_pessimism = 0;
        else if (!strcmp(ft, "max")) g_profile.flash_pessimism = 1;
        else if (!strcmp(ft, "tail")) g_profile.flash_pessimism = 2;
        else { fprintf(stderr, "--flash-timing: typ, max or tail\n"); return 3; }
    }
    if (mkdir_p(g_sim.out_dir)) {
        fprintf(stderr, "cannot create %s\n", g_sim.out_dir);
        return 3;
    }
    if (sim_hub_load(g_sim.hub_script, err, sizeof err)) {
        fprintf(stderr, "hub: %s\n", err);
        return 3;
    }
    if (g_sim.script_path && sim_script_load(g_sim.script_path, err, sizeof err)) {
        fprintf(stderr, "script: %s\n", err);
        return 3;
    }
    if (!g_sim.end_ns && !sim_script_has_end()) {
        g_sim.end_ns = 30ull * 1000000000ull;
        if (!g_sim.quiet) fprintf(stderr, "ink-sim: no --end and no 'end' in the script: stopping at 30 s\n");
    }

    vtk_config_t cfg = {
        .ncores = g_profile.cores,
        .cpu_mode = g_sim.cpu_mode,
        .host_to_target = g_sim.host_to_target > 0 ? g_sim.host_to_target : g_profile.host_to_target,
        .sched_overhead_ns = g_profile.sched_overhead_ns,
        .ctx_switch_ns = g_profile.ctx_switch_ns,
        .long_burst_warn_ns = g_profile.long_burst_warn_ns,
        .host_stack_ratio = g_profile.host_stack_ratio,
        .end_time_ns = g_sim.end_ns ? g_sim.end_ns : VTK_TIME_INF,
        .tick_ns = g_profile.tick_hz > 0 ? 1000000000ull / (uint64_t)g_profile.tick_hz : 0,
        .realtime = g_sim.realtime,
        .realtime_speed = g_sim.realtime_speed,
    };
    vtk_init(&cfg);
    vtk_set_warn_sink(vtk_warn_sink);
    pools_init(g_profile.internal_regions, g_profile.n_internal_regions, g_profile.dma_heap, g_profile.external_heap,
               g_profile.heap_region_overhead);
    pools_set_policy(g_profile.always_internal_max);
    pools_set_warn(pools_warn_sink);
    if (g_sim.trace) {
        char path[600];
        snprintf(path, sizeof path, "%s/trace.json", g_sim.out_dir);
        sim_trace_open(path);
        vtk_set_switch_hook(sim_trace_switch_hook);
    }
    if (!g_sim.quiet)
        fprintf(stderr, "ink-sim: %s | %d cores @ %d MHz | heap %zu KiB internal + %zu KiB DMA + %zu KiB external | %s CPU model\n",
                g_profile.name, g_profile.cores, g_profile.mhz, g_profile.internal_heap / 1024, g_profile.dma_heap / 1024,
                g_profile.external_heap / 1024, g_sim.cpu_mode == VTK_CPU_MEASURED ? "measured" : "annotated");

    vtk_calibrate_stack();
    sim_osal_init();
    osal_task_cfg_t main_cfg = {.name = "main", .fn = boot_task, .stack_bytes = g_profile.main_stack_bytes,
                                .priority = 1, .core = 0};
    if (osal_task_create(&main_cfg, NULL) != OSAL_OK) {
        fprintf(stderr, "cannot create the main task\n");
        return 3;
    }
    if (g_sim.script_path) sim_script_start();
    vtk_event_at(SIM_MS(50), sampler, NULL);
    vtk_run();
    int v = finalize("ok");
    fflush(NULL);
    return v ? 1 : 0;
}
