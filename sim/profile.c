#include "profile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

sim_profile_t g_profile;

static double num(const cJSON *o, const char *k, double def) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    return cJSON_IsNumber(v) ? v->valuedouble : def;
}

static bool flag(const cJSON *o, const char *k, bool def) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    if (cJSON_IsBool(v)) return cJSON_IsTrue(v);
    return def;
}

static const char *str(const cJSON *o, const char *k, const char *def) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    return cJSON_IsString(v) ? v->valuestring : def;
}

static const cJSON *obj(const cJSON *o, const char *k) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    return cJSON_IsObject(v) ? v : NULL;
}

static hal_key_t key_from(const char *s) {
    static const struct { const char *n; hal_key_t k; } M[] = {
        {"ai", HAL_KEY_AI},         {"power", HAL_KEY_POWER}, {"vol_up", HAL_KEY_VOL_UP},
        {"vol_down", HAL_KEY_VOL_DOWN}, {"home", HAL_KEY_HOME}, {"prev", HAL_KEY_PREV},
        {"next", HAL_KEY_NEXT},     {"back", HAL_KEY_BACK},
    };
    for (size_t i = 0; i < sizeof M / sizeof M[0]; i++)
        if (!strcmp(M[i].n, s)) return M[i].k;
    return HAL_KEY_NONE;
}

static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    if (buf) buf[n] = 0;
    fclose(f);
    return buf;
}

int profile_load(const char *path, sim_profile_t *p, char *err, size_t errlen) {
    memset(p, 0, sizeof *p);
    char *text = slurp(path);
    if (!text) {
        snprintf(err, errlen, "cannot read %s", path);
        return -1;
    }
    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root) {
        snprintf(err, errlen, "invalid JSON in %s near: %.40s", path, cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "?");
        return -1;
    }

    snprintf(p->id, sizeof p->id, "%s", str(root, "id", "unknown"));
    snprintf(p->name, sizeof p->name, "%s", str(root, "name", p->id));
    const char *cls = str(root, "class", "mcu");
    p->device_class = !strcmp(cls, "soc") ? HAL_CLASS_SOC : !strcmp(cls, "desktop") ? HAL_CLASS_DESKTOP : HAL_CLASS_MCU;

    const cJSON *cpu = obj(root, "cpu");
    p->cores = (int)num(cpu, "cores", 2);
    p->mhz = (int)num(cpu, "mhz", 320);
    p->coremark_per_mhz_core = num(cpu, "coremark_per_mhz_per_core", 3.43);
    p->host_to_target = num(cpu, "host_to_target_default", 40.0);

    const cJSON *kern = obj(root, "kernel");
    p->sched_overhead_ns = (uint64_t)num(kern, "sched_overhead_ns", 1500);
    p->ctx_switch_ns = (uint64_t)num(kern, "ctx_switch_ns", 3000);
    p->queue_op_ns = (uint64_t)num(kern, "queue_op_ns", 1500);
    p->long_burst_warn_ns = (uint64_t)(num(kern, "long_burst_warn_ms", 20) * 1e6);
    p->host_stack_ratio = num(kern, "host_stack_ratio", 2.0);
    p->tcb_bytes = (uint32_t)num(kern, "tcb_bytes", 360);
    p->sync_obj_bytes = (uint32_t)num(kern, "sync_obj_bytes", 96);
    p->tick_hz = (int)num(kern, "tick_hz", 100);
    p->now_cost_ns = (uint64_t)num(kern, "now_cost_ns", 500);
    p->main_stack_bytes = (uint32_t)num(kern, "main_stack_bytes", 8192);

    const cJSON *mem = obj(root, "memory");
    p->internal_heap = (size_t)(num(mem, "internal_heap_kib", 80) * 1024);
    p->dma_heap = (size_t)(num(mem, "dma_heap_kib", 32) * 1024);
    p->external_heap = (size_t)(num(mem, "external_heap_kib", 15872) * 1024);
    p->always_internal_max = (size_t)num(mem, "always_internal_max_bytes", 16);

    const cJSON *d = obj(root, "display");
    p->panel.tech = !strcmp(str(d, "tech", "epd"), "lcd") ? HAL_PANEL_LCD : HAL_PANEL_EPD;
    p->panel.width = (int)num(d, "width", 1216);
    p->panel.height = (int)num(d, "height", 684);
    p->panel.dpi = (int)num(d, "dpi", 297);
    p->panel.gray_levels = (int)num(d, "gray_levels", 16);
    p->panel.color = flag(d, "color", false);
    p->panel.rotation_deg = (int)num(d, "rotation_deg", 0);
    p->panel.full_frame_scan = flag(d, "full_frame_scan", true);
    p->frame_time_us = (uint32_t)num(d, "frame_time_us", 16000);
    p->power_up_us = (uint32_t)num(d, "power_up_us", 10000);
    p->power_down_us = (uint32_t)num(d, "power_down_us", 2000);
    p->touch_pause_us = (uint32_t)num(d, "touch_pause_during_power_us", 0);
    p->lcd_frame_us = (uint32_t)num(d, "lcd_frame_us", 16667);
    p->keep_power_ms = (uint32_t)num(d, "keep_power_ms", 0);
    p->prep_cpu_duty = num(d, "prep_cpu_duty", 0.0);
    p->submit_copy_mbps = num(d, "submit_copy_mbps", 0.0);
    p->panel_buffers_external = (size_t)(num(d, "driver_external_kib", 0) * 1024);
    p->panel_buffers_internal = (size_t)(num(d, "driver_internal_kib", 0) * 1024);
    static const char *WF[HAL_WF_COUNT] = {"GC16", "GL16", "DU", "A2"};
    const cJSON *wfs = obj(d, "waveforms");
    for (int i = 0; i < HAL_WF_COUNT; i++) {
        const cJSON *w = wfs ? obj(wfs, WF[i]) : NULL;
        if (!w) continue;
        p->wf[i].supported = true;
        p->wf[i].phases = (int)num(w, "phases", 20);
        p->wf[i].flashing = flag(w, "flashing", false);
        p->wf[i].ghost = num(w, "ghost", 0);
        p->panel.waveforms |= 1u << i;
    }
    if (p->panel.tech == HAL_PANEL_LCD) p->panel.waveforms = 0;

    const cJSON *in = obj(root, "input");
    p->has_touch = flag(in, "touch", true);
    p->touch_points = (int)num(in, "touch_points", 2);
    p->i2c_khz = (int)num(in, "i2c_khz", 100);
    p->touch_read_bytes = (int)num(in, "touch_read_bytes", 9);
    p->i2c_background_poll_ms = (uint32_t)num(in, "i2c_background_poll_ms", 1000);
    p->i2c_background_bytes = (int)num(in, "i2c_background_bytes", 24);
    p->touch_read_us = (uint32_t)num(in, "touch_read_us", 1200);
    p->touch_poll_ms = (uint32_t)num(in, "touch_poll_ms", 10);
    p->touch_detect_us = (uint32_t)num(in, "touch_detect_us", 20000);
    const cJSON *keys = cJSON_GetObjectItemCaseSensitive(in, "keys");
    if (cJSON_IsArray(keys)) {
        const cJSON *k;
        cJSON_ArrayForEach(k, keys) if (cJSON_IsString(k)) p->keys_mask |= 1u << key_from(k->valuestring);
    }

    const cJSON *fl = obj(root, "flash");
    p->flash_user_bytes = (size_t)(num(fl, "user_partition_kib", 1408) * 1024);
    p->flash_sector = (uint32_t)num(fl, "sector_bytes", 4096);
    p->flash_page = (uint32_t)num(fl, "page_bytes", 256);
    const cJSON *er = obj(fl, "erase_sector_us");
    p->erase_us_typ = (uint32_t)num(er, "typ", 45000);
    p->erase_us_max = (uint32_t)num(er, "max", 400000);
    const cJSON *pr = obj(fl, "program_page_us");
    p->prog_us_typ = (uint32_t)num(pr, "typ", 700);
    p->prog_us_max = (uint32_t)num(pr, "max", 3000);
    const char *pess = str(fl, "timing", "tail");
    p->flash_pessimism = !strcmp(pess, "typ") ? 0 : !strcmp(pess, "max") ? 1 : 2;
    p->flash_stalls_cpu = flag(fl, "stalls_cpu", true);
    p->flash_write_chunk = (uint32_t)num(fl, "write_chunk_bytes", 8192);
    p->flash_read_mbps = num(fl, "read_mbps", 20);
    p->flash_op_overhead_us = (uint32_t)num(fl, "op_overhead_us", 30);

    const cJSON *sd = obj(root, "sd");
    p->sd_read_mbps = num(sd, "read_mbps", 8);
    p->sd_write_mbps = num(sd, "write_mbps", 3);
    p->sd_op_latency_us = (uint32_t)num(sd, "op_latency_us", 1500);
    p->sd_write_busy_spike_ms = (uint32_t)num(sd, "write_busy_spike_ms", 0);
    p->sd_spike_every_kib = (uint32_t)num(sd, "spike_every_kib", 4096);
    p->sd_capacity_gib = num(sd, "capacity_gib", 8);

    const cJSON *pw = obj(root, "power");
    p->batt_mah = num(pw, "battery_mah", 1500);
    p->ma_base_awake = num(pw, "base_awake_ma", 25);
    p->ma_per_core_busy = num(pw, "per_core_busy_ma", 20);
    p->ma_display_rails = num(pw, "display_rails_ma", 8);
    p->ma_display_refresh = num(pw, "display_refresh_ma", 40);
    p->ma_lte_idle = num(pw, "lte_idle_ma", 8);
    p->ma_lte_active = num(pw, "lte_active_ma", 180);
    p->ma_wifi_idle = num(pw, "wifi_idle_ma", 15);
    p->ma_wifi_active = num(pw, "wifi_active_ma", 110);
    p->ma_audio_out = num(pw, "audio_out_ma", 35);
    p->ma_audio_in = num(pw, "audio_in_ma", 12);
    p->ma_light_full = num(pw, "frontlight_full_ma", 45);
    p->ma_sleep = num(pw, "sleep_ma", 1.5);

    const cJSON *f = obj(root, "features");
    p->info.profile_id = p->id;
    p->info.display_name = p->name;
    p->info.device_class = p->device_class;
    p->info.cpu_cores = p->cores;
    p->info.cpu_mhz = p->mhz;
    p->info.internal_heap_bytes = (uint32_t)p->internal_heap;
    p->info.dma_heap_bytes = (uint32_t)p->dma_heap;
    p->info.external_heap_bytes = (uint32_t)(p->external_heap > 0xffffffffu ? 0xffffffffu : p->external_heap);
    p->info.has_modem = flag(f, "modem", false);
    p->info.has_wifi = flag(f, "wifi", true);
    p->info.has_audio_out = flag(f, "audio_out", true);
    p->info.has_audio_in = flag(f, "audio_in", true);
    p->info.has_frontlight = flag(f, "frontlight", false);
    p->info.has_haptics = flag(f, "haptics", false);
    p->info.has_rtc = flag(f, "rtc", true);

    const cJSON *costs = obj(root, "costs_cycles");
    if (costs) {
        const cJSON *c;
        cJSON_ArrayForEach(c, costs) {
            if (!cJSON_IsNumber(c) || p->ncosts >= PROFILE_MAX_COSTS) continue;
            snprintf(p->costs[p->ncosts].name, sizeof p->costs[0].name, "%s", c->string);
            p->costs[p->ncosts].cycles = c->valuedouble;
            p->ncosts++;
        }
    }
    cJSON_Delete(root);
    return 0;
}

double profile_cost(const char *name) {
    for (int i = 0; i < g_profile.ncosts; i++)
        if (!strcmp(g_profile.costs[i].name, name)) return g_profile.costs[i].cycles;
    return 0;
}
