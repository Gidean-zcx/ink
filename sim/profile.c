#include "profile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

sim_profile_t g_profile;

/* Every value the profile does not set falls back to a default; the defaults are recorded so the
 * run lists them (a silent, optimistic default is how a model quietly drifts from the device). */
static sim_profile_t *g_loading = &g_profile;

static void note_default(const cJSON *o, const char *k) {
    sim_profile_t *p = g_loading;
    const char *sect = (o && o->string) ? o->string : "?";
    size_t used = strlen(p->defaults_used);
    if (used + strlen(sect) + strlen(k) + 3 < sizeof p->defaults_used)
        snprintf(p->defaults_used + used, sizeof p->defaults_used - used, "%s%s.%s", used ? " " : "", sect, k);
    p->ndefaults++;
}

static double num(const cJSON *o, const char *k, double def) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    if (cJSON_IsNumber(v)) return v->valuedouble;
    note_default(o, k);
    return def;
}

/* optional value: no "default used" note when absent */
static double num_opt(const cJSON *o, const char *k, double def) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    return cJSON_IsNumber(v) ? v->valuedouble : def;
}

static bool flag(const cJSON *o, const char *k, bool def) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    if (cJSON_IsBool(v)) return cJSON_IsTrue(v);
    note_default(o, k);
    return def;
}

static const char *str(const cJSON *o, const char *k, const char *def) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    if (cJSON_IsString(v)) return v->valuestring;
    note_default(o, k);
    return def;
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
    g_loading = p;
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

    p->icache_penalty = num(cpu, "icache_penalty", 0.0);
    p->dcache_bytes = (uint32_t)(num(cpu, "dcache_kib", 0) * 1024);
    p->dcache_line = (uint32_t)num(cpu, "cache_line_bytes", 64);
    if (p->dcache_line < 16) p->dcache_line = 16;

    const cJSON *mem = obj(root, "memory");
    p->internal_heap = (size_t)(num(mem, "internal_heap_kib", 80) * 1024);
    {
        const cJSON *ir = mem ? cJSON_GetObjectItemCaseSensitive(mem, "internal_regions_kib") : NULL;
        p->n_internal_regions = 0;
        size_t sum = 0;
        if (cJSON_IsArray(ir)) {
            const cJSON *e;
            cJSON_ArrayForEach(e, ir) {
                if (!cJSON_IsNumber(e) || p->n_internal_regions >= 8) continue;
                p->internal_regions[p->n_internal_regions++] = (size_t)(e->valuedouble * 1024);
                sum += (size_t)(e->valuedouble * 1024);
            }
        }
        if (!p->n_internal_regions) {
            p->internal_regions[p->n_internal_regions++] = p->internal_heap;
            sum = p->internal_heap;
        }
        p->internal_heap = sum;
        p->heap_region_overhead = (size_t)num(mem, "heap_region_overhead_bytes", 1536);
    }
    p->dma_heap = (size_t)(num(mem, "dma_heap_kib", 32) * 1024);
    p->external_heap = (size_t)(num(mem, "external_heap_kib", 7680) * 1024);
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
    p->power_up_us = (uint32_t)num(d, "power_up_us", 140000);
    p->power_down_us = (uint32_t)num(d, "power_down_us", 2000);
    p->touch_pause_us = (uint32_t)num(d, "touch_pause_during_power_us", 0);
    p->lcd_frame_us = p->panel.tech == HAL_PANEL_LCD ? (uint32_t)num(d, "lcd_frame_us", 16667) : 16667;
    p->line_count = (int)num(d, "line_count", 688);
    p->line_ns = (uint32_t)(num(d, "line_time_us", 17) * 1000);
    p->line_queue_lines = (int)num(d, "line_queue_lines", 127);
    p->prefill_lines = (int)num(d, "prefill_lines", 192);
    /* defaults: upstream S31 C feeder (epdiy_riscv_ve_stub.c) on the ISS fitted model, 1216 px */
    p->line_prep_ns = (uint32_t)(num(d, "line_prep_us", 33.4) * 1000);
    p->line_skip_ns = (uint32_t)(num(d, "line_skip_us", 0.5) * 1000);
    p->phase_min_us = (uint32_t)num(d, "phase_min_us", 12000);
    p->frame_overhead_us = (uint32_t)num(d, "frame_overhead_us", 150);
    p->bounce_lines = (int)num(d, "bounce_lines", 8);
    p->lcd_isr_iram_safe = flag(d, "lcd_isr_iram_safe", false);
    p->gc16_full_phases = (int)num(d, "gc16_full_phases", 48);
    p->clear_frames = (int)num(d, "clear_frames", 69);
    p->gc16_full_clear_frames = (int)num(d, "gc16_full_clear_frames", 0);
    p->lcd_isr_duty = num(d, "lcd_isr_duty", 0.10);
    {
        const char *dp = str(d, "diff_pass", "full_width");
        p->diff_pass = !strcmp(dp, "none") ? 0 : !strcmp(dp, "area") ? 1 : !strcmp(dp, "full_screen") ? 3 : 2;
        const char *fd = str(d, "feeders", NULL);
        bool none = p->panel.tech == HAL_PANEL_LCD ||
                    (cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(d, "prep_cpu_duty")) && num(d, "prep_cpu_duty", 0) <= 0);
        p->feeders_cpu = fd ? !strcmp(fd, "cpu") : !none;
        p->feeder_dynamic = !strcmp(str(d, "feeder_claim", "interleave"), "dynamic");
        p->feeder_area_columns = !strcmp(str(d, "feeder_columns", "full"), "area");
        p->feeder_block = !strcmp(str(d, "feeder_wait", "spin"), "block");
        p->display_driver = !strcmp(str(d, "driver", "epdiy"), "pqbn") ? 1 : 0;
        if (p->panel.tech != HAL_PANEL_EPD || !p->feeders_cpu) p->display_driver = 0;
        if (p->display_driver == 1) {
            const cJSON *q = obj(d, "pqbn");
            p->pqbn.plan_budget = (int)num(q, "plan_budget", 8000);
            p->pqbn.auto_motion = flag(q, "auto_motion", true);
            p->pqbn.full_swing = num(q, "full_swing_pushes", 7.6);
            p->pqbn.stiction = num(q, "stiction", 0.3);
            p->pqbn.stiction_tau = num(q, "stiction_tau_phases", 120);
            p->pqbn.mobility_sd = num(q, "mobility_sd", 0.04);
            const cJSON *c = obj(q, "costs_cycles");
            p->pqbn.row_idle = num(c, "row_idle", 43);
            p->pqbn.row_band = num(c, "row_band", 1836);
            p->pqbn.row_tile = num(c, "row_tile", 215);
            p->pqbn.plan_base = num(c, "plan_base", 267375);
            p->pqbn.plan_px = num(c, "plan_px", 248);
            p->pqbn.plan_motion_tile = num(c, "plan_motion_tile", 2125);
            p->pqbn.plan_exit_tile = num(c, "plan_exit_tile", 9305);
            p->pqbn.plan_idle = num(c, "plan_idle", 110000);
            p->panel.async_updates = true;
        }
    }
    if (p->line_count < p->panel.height) p->line_count = p->panel.height;
    if (p->bounce_lines < 1) p->bounce_lines = 1;
    if (p->prefill_lines < 2 * p->bounce_lines) p->prefill_lines = 2 * p->bounce_lines; /* both bounce buffers */
    if (p->line_queue_lines < 1) p->line_queue_lines = 1;
    p->panel_buffers_external = (size_t)(num(d, "driver_external_kib", 0) * 1024);
    p->panel_buffers_internal = (size_t)(num(d, "driver_internal_kib", 0) * 1024);
    p->driver_lut_bytes = (size_t)(num(d, "driver_lut_kib", 16) * 1024);
    static const char *WF[HAL_WF_COUNT] = {"GC16", "GL16", "DU", "A2"};
    const cJSON *wfs = obj(d, "waveforms");
    for (int i = 0; i < HAL_WF_COUNT; i++) {
        const cJSON *w = wfs ? obj(wfs, WF[i]) : NULL;
        if (!w) continue;
        p->wf[i].supported = true;
        p->wf[i].phases = (int)num(w, "phases", 20);
        p->wf[i].flashing = flag(w, "flashing", false);
        p->wf[i].ghost = num(w, "ghost", 0);
        p->wf[i].line_prep_ns = (uint32_t)(num_opt(w, "line_prep_us", 0) * 1000);
        p->wf[i].phase_min_us = (uint32_t)num_opt(w, "phase_min_us", 0);
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
    p->touch_poll_ms = (uint32_t)num(in, "touch_poll_ms", 10);
    p->touch_detect_us = (uint32_t)num(in, "touch_detect_us", 20000);
    p->touch_monitor_after_ms = (uint32_t)num(in, "touch_monitor_after_ms", 0); /* 0 = no monitor mode modelled */
    p->touch_monitor_period_ms = (uint32_t)num(in, "touch_monitor_period_ms", 0);
    p->i2c_background_idle_poll_ms = (uint32_t)num(in, "i2c_background_idle_poll_ms", p->i2c_background_poll_ms);
    const cJSON *keys = cJSON_GetObjectItemCaseSensitive(in, "keys");
    if (cJSON_IsArray(keys)) {
        const cJSON *k;
        cJSON_ArrayForEach(k, keys) if (cJSON_IsString(k)) p->keys_mask |= 1u << key_from(k->valuestring);
    }
    p->key_debounce_us = (uint32_t)num(in, "key_debounce_us", 20000);
    const cJSON *ks = cJSON_GetObjectItemCaseSensitive(in, "key_sources");
    if (cJSON_IsObject(ks)) {
        const cJSON *k;
        cJSON_ArrayForEach(k, ks) {
            if (!cJSON_IsString(k)) continue;
            int key = key_from(k->string);
            if (key <= 0 || key >= HAL_KEY_COUNT) continue;
            p->key_source[key] = !strcmp(k->valuestring, "expander") ? 1 : !strcmp(k->valuestring, "touch") ? 2 : 0;
        }
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
    p->assets_partition_bytes = (size_t)(num(fl, "assets_partition_kib", 0) * 1024);
    p->flash_read_mbps = num(fl, "read_mbps", 20);
    p->flash_op_overhead_us = (uint32_t)num(fl, "op_overhead_us", 30);
    p->flash_write_chunk = (uint32_t)num(fl, "write_chunk_bytes", 8192);
    p->flash_spi_cmd_bytes = (uint32_t)num(fl, "spi_cmd_bytes", 64);
    p->flash_spi_cmd_ns = (uint32_t)(num(fl, "spi_cmd_us", 1.5) * 1000);
    p->prog_first_us = (uint32_t)num(fl, "program_first_us", 30);
    if (p->flash_write_chunk < 256) p->flash_write_chunk = 256;
    if (p->flash_spi_cmd_bytes < 1) p->flash_spi_cmd_bytes = 64;

    const cJSON *sd = obj(root, "sd");
    p->sd_read_mbps = num(sd, "read_mbps", 8);
    p->sd_write_mbps = num(sd, "write_mbps", 3);
    p->sd_op_latency_us = (uint32_t)num(sd, "op_latency_us", 1500);
    p->sd_write_busy_spike_ms = (uint32_t)num(sd, "write_busy_spike_ms", 0);
    p->sd_spike_every_kib = (uint32_t)num(sd, "spike_every_kib", 4096);
    p->sd_capacity_gib = num(sd, "capacity_gib", 8);

    const cJSON *net = obj(root, "network");
    const char *links[2] = {"wifi", "lte"};
    profile_link_t *dst[2] = {&p->wifi, &p->lte};
    for (int i = 0; i < 2; i++) {
        const cJSON *l = net ? obj(net, links[i]) : NULL;
        profile_link_t *L = dst[i];
        L->present = l != NULL;
        L->rtt_p50_ms = num(l, "rtt_ms_p50", i ? 80 : 8);
        L->rtt_p99_ms = num(l, "rtt_ms_p99", i ? 400 : 60);
        L->mbps = num(l, "mbps", i ? 1.2 : 5);
        double cap = num(l, "uart_cap_mbps", 0);
        if (cap > 0 && cap < L->mbps) L->mbps = cap;
        L->loss = num(l, "loss", i ? 0.01 : 0.002);
        L->resume_after_s = num(l, "idle_resume_after_s", i ? 10 : 1);
        L->resume_extra_ms = num(l, "idle_resume_extra_ms", i ? 300 : 300);
        const cJSON *we = cJSON_GetObjectItemCaseSensitive(l, "wake_extra_ms");
        if (cJSON_IsArray(we) && cJSON_GetArraySize(we) == 2) L->resume_extra_ms = cJSON_GetArrayItem(we, 1)->valuedouble;
        L->tail_s = num(l, "tail_s", i ? 10 : 1);
        const cJSON *oph = cJSON_GetObjectItemCaseSensitive(l, "outages_per_hour");
        const cJSON *os = cJSON_GetObjectItemCaseSensitive(l, "outage_s");
        L->outages_per_hour = cJSON_IsArray(oph) ? cJSON_GetArrayItem(oph, 1)->valuedouble : num(l, "outages_per_hour", 0);
        L->outage_min_s = cJSON_IsArray(os) ? cJSON_GetArrayItem(os, 0)->valuedouble : 2;
        L->outage_max_s = cJSON_IsArray(os) ? cJSON_GetArrayItem(os, 1)->valuedouble : 5;
    }
    snprintf(p->default_link, sizeof p->default_link, "%s", str(net, "default_link", "lte"));
    p->tls_internal_bytes = (size_t)(num(net, "tls_internal_kib", 6) * 1024);
    p->tls_external_bytes = (size_t)(num(net, "tls_external_kib", 40) * 1024);
    p->ws_stack = (uint32_t)num(net, "ws_task_stack", 4096);
    p->sys_evt_stack = (uint32_t)num(net, "event_task_stack", 2560);
    p->dead_detect_ms = (uint32_t)num(net, "dead_detect_ms", 5000);
    p->uart_nic_dma_bytes = (size_t)(num(net, "uart_nic_dma_kib", 0) * 1024);
    p->uart_nic_stack = (uint32_t)num(net, "uart_nic_task_stack", 4096);
    p->ws_stack_external = flag(net, "ws_task_stack_external", false);
    p->uart_nic_stack_external = flag(net, "uart_nic_task_stack_external", false);

    const cJSON *au = obj(root, "audio");
    p->audio_dma_desc = (int)num(au, "dma_desc", 4);
    p->audio_dma_frames = (int)num(au, "dma_frames", 240);
    p->audio_slot_bytes = (int)num(au, "slot_bytes", 8);
    p->audio_start_us = (uint32_t)(num(au, "start_ms", 30) * 1000);
    p->audio_stop_us = (uint32_t)(num(au, "stop_ms", 5) * 1000);
    p->audio_task_stack = (uint32_t)num(au, "task_stack", 4096);
    p->audio_task_prio = (int)num(au, "task_prio", 8);
    p->opus_enc_bytes = (size_t)(num(au, "opus_enc_kib", 30) * 1024);
    p->opus_dec_bytes = (size_t)(num(au, "opus_dec_kib", 27) * 1024);
    if (p->audio_dma_desc < 2) p->audio_dma_desc = 2;
    if (p->audio_dma_frames < 16) p->audio_dma_frames = 16;

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
    p->idle_mhz = (int)num(pw, "idle_mhz", p->mhz);
    if (p->idle_mhz <= 0 || p->idle_mhz > p->mhz) p->idle_mhz = p->mhz;
    p->ma_base_awake_idle = num(pw, "base_awake_idle_ma", p->ma_base_awake);
    p->ma_standby_modem = num(pw, "standby_modem_ma", 2.5);
    p->sleep_enter_us = (uint32_t)(num(pw, "sleep_enter_ms", 200) * 1000);
    p->sleep_exit_us = (uint32_t)(num(pw, "sleep_exit_ms", 30) * 1000);
    p->sleep_wake_us = (uint32_t)num(pw, "sleep_wake_us", 2000);
    p->sd_remount_us = (uint32_t)(num(pw, "sd_remount_ms", 100) * 1000);

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

static void (*g_cost_warn)(const char *msg);
void profile_set_warn(void (*fn)(const char *msg)) { g_cost_warn = fn; }

double profile_cost(const char *name) {
    for (int i = 0; i < g_profile.ncosts; i++)
        if (!strcmp(g_profile.costs[i].name, name)) return g_profile.costs[i].cycles;
    /* a missing class would silently make that work free: say so once per class */
    static const char *warned[32];
    static int nwarned;
    for (int i = 0; i < nwarned; i++)
        if (warned[i] == name || !strcmp(warned[i], name)) return 0;
    if (nwarned < 32) warned[nwarned++] = name;
    if (g_cost_warn) {
        char msg[160];
        snprintf(msg, sizeof msg, "profile has no cost class '%s': that work is charged 0 cycles", name);
        g_cost_warn(msg);
    }
    return 0;
}
