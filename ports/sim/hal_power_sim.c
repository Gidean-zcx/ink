/*
 * Power model: resource reference counts, front light, battery drain and an
 * energy estimate per scenario. Currents are ESTIMATES from the profile until
 * bench measurements exist (SP-08) — use the result to compare designs, not
 * to promise battery life.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_device.h"
#include "ink/hal_power.h"
#include "sim.h"

#define MAX_CONSUMERS 24

typedef struct {
    const char *name;
    bool on;
    double ma;
    uint64_t since, on_ns;
    double mah;
} consumer_t;

static struct {
    consumer_t c[MAX_CONSUMERS];
    int n;
    int refs[HAL_PWR_COUNT];
    double soc; /* state of charge 0..1 */
    bool charging;
    int light_cold, light_warm;
    double cpu_mah;
} PW;

static consumer_t *find(const char *name) {
    for (int i = 0; i < PW.n; i++)
        if (!strcmp(PW.c[i].name, name)) return &PW.c[i];
    if (PW.n >= MAX_CONSUMERS) return &PW.c[MAX_CONSUMERS - 1];
    consumer_t *c = &PW.c[PW.n++];
    memset(c, 0, sizeof *c);
    c->name = name;
    return c;
}

static double default_ma(const char *name) {
    if (!strcmp(name, "display.rails")) return g_profile.ma_display_rails;
    if (!strcmp(name, "modem")) return g_profile.ma_lte_idle;
    if (!strcmp(name, "wifi")) return g_profile.ma_wifi_idle;
    if (!strcmp(name, "audio.out")) return g_profile.ma_audio_out;
    if (!strcmp(name, "audio.in")) return g_profile.ma_audio_in;
    return 0;
}

static uint64_t now_ns(void) { return vtk_in_isr() ? vtk_isr_now() : vtk_now(); }

static void settle(consumer_t *c, uint64_t t) {
    if (c->on && t > c->since) {
        double h = (double)(t - c->since) / 3.6e12;
        c->mah += c->ma * h;
        c->on_ns += t - c->since;
    }
    c->since = t;
}

void sim_power_consumer(const char *name, bool on, double ma) {
    consumer_t *c = find(name);
    uint64_t t = now_ns();
    settle(c, t);
    c->on = on;
    c->ma = ma > 0 ? ma : default_ma(name);
}

static const char *RES_NAME[HAL_PWR_COUNT] = {"display", "audio.out", "audio.in", "modem", "wifi"};

void hal_power_request(hal_power_res_t res, bool on) {
    if (res >= HAL_PWR_COUNT) return;
    int before = PW.refs[res];
    PW.refs[res] += on ? 1 : -1;
    if (PW.refs[res] < 0) {
        sim_warn("hal_power_request: unbalanced release of %s", RES_NAME[res]);
        PW.refs[res] = 0;
    }
    if ((before == 0) != (PW.refs[res] == 0) && res != HAL_PWR_DISPLAY)
        sim_power_consumer(RES_NAME[res], PW.refs[res] > 0, 0);
}

void hal_light_set(int cold_permille, int warm_permille) {
    if (!g_profile.info.has_frontlight) return;
    PW.light_cold = cold_permille < 0 ? 0 : cold_permille > 1000 ? 1000 : cold_permille;
    PW.light_warm = warm_permille < 0 ? 0 : warm_permille > 1000 ? 1000 : warm_permille;
    double frac = (PW.light_cold + PW.light_warm) / 2000.0;
    sim_power_consumer("frontlight", frac > 0, g_profile.ma_light_full * frac);
}

bool hal_haptic_available(void) { return g_profile.info.has_haptics; }
void hal_haptic_pulse(int ms) {
    (void)ms;
    if (g_profile.info.has_haptics) sim_metric_add("haptics.pulses", 1);
}

static double total_mah(void) {
    uint64_t t = now_ns();
    double sum = 0;
    for (int i = 0; i < PW.n; i++) {
        settle(&PW.c[i], t);
        sum += PW.c[i].mah;
    }
    /* CPU: base while awake + per-core busy share (no light sleep modelled yet: pessimistic) */
    double h = (double)t / 3.6e12;
    double busy_h = 0;
    for (int c = 0; c < g_profile.cores; c++) busy_h += (double)vtk_core_busy_ns(c) / 3.6e12;
    PW.cpu_mah = g_profile.ma_base_awake * h + g_profile.ma_per_core_busy * busy_h;
    return sum + PW.cpu_mah;
}

void hal_power_battery(hal_battery_t *out) {
    double used = total_mah();
    double soc = PW.soc - (g_profile.batt_mah > 0 ? used / g_profile.batt_mah : 0);
    if (soc < 0) soc = 0;
    out->percent = (int)(soc * 100.0 + 0.5);
    out->millivolts = 3300 + (int)(soc * 900.0); /* crude linear OCV */
    out->charging = PW.charging;
    out->external_power = PW.charging;
}

void sim_power_set_battery(int percent, bool charging) {
    double used = total_mah();
    PW.soc = percent / 100.0 + (g_profile.batt_mah > 0 ? used / g_profile.batt_mah : 0);
    PW.charging = charging;
}

void sim_power_init(void) {
    PW.soc = 0.8;
    find("display.rails");
}

void sim_power_report(void) {
    double total = total_mah();
    double h = (double)vtk_now() / 3.6e12;
    char key[80];
    for (int i = 0; i < PW.n; i++) {
        snprintf(key, sizeof key, "power.mah.%s", PW.c[i].name);
        sim_metric_set(key, PW.c[i].mah);
    }
    sim_metric_set("power.mah.cpu", PW.cpu_mah);
    sim_metric_set("power.mah_total", total);
    sim_metric_set("power.avg_ma_estimate", h > 0 ? total / h : 0);
}

/* ------------------------------------------------------------ device */

void hal_device_get_info(hal_device_info_t *out) { *out = g_profile.info; }

const char *hal_device_reset_reason(void) { return g_sim.reset_reason ? g_sim.reset_reason : "poweron"; }

void hal_log_write(const char *line) {
    if (g_sim.quiet) return;
    uint64_t t = now_ns();
    fprintf(stdout, "[%6llu.%03llu] %s\n", (unsigned long long)(t / 1000000000ull),
            (unsigned long long)(t / 1000000ull % 1000ull), line);
}
