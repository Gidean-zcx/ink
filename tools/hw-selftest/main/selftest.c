/*
 * Hardware self-test for the Metalio E-Ink4 Plus (ESP32-S31), run from ota_1 with nobody at the device.
 * One JSON object a line on the USB console, then HWTEST_DONE; after that it only idles and says all
 * results again every few seconds (the computer may reattach late), never sleeping.
 *
 * Safety first: the TPS65185's WAKEUP, PWRUP and VCOM_CTRL (TCA9555 P00-P02) are driven low and read
 * back; if that cannot be proven, SAFETY_FAIL and nothing else runs. Otherwise everything only reads:
 * the panel is not driven, the card is only detected, NVS, flash and eFuses are not written, Wi-Fi only
 * scans and reports a count, Bluetooth and the front light are not touched, the charger, gauge, clock,
 * accelerometer and touch chips are only read, except that the accelerometer's CTRL1 is switched on
 * for a moment (the vendor driver's value, CTRL1 = 0x57) and written back to what it was (read back to prove
 * it). No light sleep and nothing else that drops USB (run 3 lost USB that way). The modem gets the read-only questions in kAsk and
 * nothing else; its raw answer is logged as hex with long digit runs masked. No MAC, serial number,
 * IMEI, ICCID or network name is printed. Each test has its own time limit and the run has one too.
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/sdmmc_host.h"
#include "driver/uart.h"
#include "esp_chip_info.h"
#include "esp_cpu.h"
#include "esp_event.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_psram.h"
#include "esp_rom_crc.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdmmc_cmd.h"

#include "bq27220/bq27220.h"
#include "cx25601n/cx25601n.h"
#include "ft6336u/ft6336u.h"
#include "ink/hal_i2c.h"
#include "metalio_board.h"
#include "metalio_epd_power.h"
#include "pcf8563/pcf8563.h"
#include "s31_board.h"
#include "sc7a20h/sc7a20h.h"
#include "tca9555/tca9555.h"
#include "tps65185/tps65185.h"

#define SLOT_CAP 1536      /* one test's lines */
#define LINE_CAP 384       /* one line */
#define SAID_CAP (24 * 1024)
#define TOTAL_MS 170000    /* the whole run; the computer waits 240 s */
#define WORKER_STACK 8192
#define PMIC_PINS (EPD_PWR_VCOM_CTRL | EPD_PWR_PWRUP | EPD_PWR_WAKEUP)
enum { TCA_IN0 = 0, TCA_IN1 = 1, TCA_OUT0 = 2, TCA_OUT1 = 3, TCA_POL0 = 4, TCA_POL1 = 5, TCA_CFG0 = 6, TCA_CFG1 = 7 };

/* Not in s31_board.h: the modem's SRDY (input, upstream config.h), the card pins (tools/hw-bench). */
#define GPIO_MODEM_SRDY 3
#define CARD_CLK 24
#define CARD_CMD 25
#define CARD_D0 20

typedef struct slot slot_t;
typedef struct {
    const char *name;
    void (*fn)(slot_t *s);
    int timeout_ms;
} test_t;

struct slot {
    const test_t *t;
    char buf[SLOT_CAP];
    size_t n;
    bool ok;   /* the test's own verdict */
    bool done; /* set by the worker */
    bool gone; /* the runner gave up on it */
};

static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static char *g_said;
static size_t g_said_n;

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

/* Printed and kept for the replays. */
static void say(const char *line) {
    printf("%s\n", line);
    fflush(stdout);
    size_t n = strlen(line);
    if (g_said && g_said_n + n + 1 < SAID_CAP) {
        memcpy(g_said + g_said_n, line, n);
        g_said_n += n;
        g_said[g_said_n++] = '\n';
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}

/* One result line into the test's slot: {"test":"<name>",<body>} */
__attribute__((format(printf, 2, 3))) static void emit(slot_t *s, const char *fmt, ...) {
    char body[LINE_CAP - 64];
    va_list ap;
    va_start(ap, fmt);
    int b = vsnprintf(body, sizeof body, fmt, ap);
    va_end(ap);
    if (b < 0) return;
    char line[LINE_CAP];
    int n = snprintf(line, sizeof line, "{\"test\":\"%.40s\",%s%s}", s->t->name, body,
                     b >= (int)sizeof body ? ",\"cut\":true" : "");
    if (n < 0) return;
    if (n >= (int)sizeof line) n = (int)sizeof line - 1;
    if (s->n + (size_t)n + 1 >= sizeof s->buf) return;
    memcpy(s->buf + s->n, line, (size_t)n);
    s->n += (size_t)n;
    s->buf[s->n++] = '\n';
}

static const char *tf(bool v) { return v ? "true" : "false"; }

/* ------------------------------------------------------------------ I2C through the port's HAL */

static hal_i2c_dev_t *dev(uint8_t addr, const char *name) { return hal_i2c_device(0, addr, name); }

/* Register read, three tries on a timeout (the shared bus has had one, docs/hardware phase0). */
static hal_i2c_status_t rd(hal_i2c_dev_t *d, uint8_t reg, uint8_t *out, size_t n) {
    hal_i2c_status_t st = HAL_I2C_ERR;
    for (int i = 0; i < 3 && d; i++) {
        st = hal_i2c_write_read(d, &reg, 1, out, n);
        if (st != HAL_I2C_TIMEOUT) break;
    }
    return st;
}

/* ------------------------------------------------------------------ safety: panel power off */

static bool g_safe;

static void t_safety(slot_t *s) {
    hal_i2c_dev_t *x = dev(TCA9555_ADDR_BASE, "io-expander");
    uint8_t out0 = 0, cfg0 = 0, pol0 = 0, in0 = 0, out0b = 0, cfg0b = 0;
    hal_i2c_status_t st = rd(x, TCA_OUT0, &out0, 1);
    if (st == HAL_I2C_OK) st = rd(x, TCA_CFG0, &cfg0, 1);
    if (st == HAL_I2C_OK) st = rd(x, TCA_POL0, &pol0, 1);
    if (st != HAL_I2C_OK) {
        emit(s, "\"ok\":false,\"step\":\"read\",\"error\":\"%s\"", hal_i2c_status_name(st));
        return;
    }
    /* Only the three PMIC pins change, and only if they are not already low outputs: level first, then
     * direction, so they never drive high on the way. The other pins keep what the factory set. */
    bool wrote = false;
    tca9555_t t = {.dev = x, .out = {out0, 0}, .dir = {cfg0, 0}};
    if (out0 & PMIC_PINS) {
        st = tca9555_set(&t, 0, PMIC_PINS, 0);
        wrote = true;
    }
    if (st == HAL_I2C_OK && (cfg0 & PMIC_PINS)) {
        st = tca9555_set_dir(&t, 0, (uint8_t)(cfg0 & ~PMIC_PINS));
        wrote = true;
    }
    if (wrote) vTaskDelay(pdMS_TO_TICKS(5));
    hal_i2c_status_t rb = rd(x, TCA_OUT0, &out0b, 1);
    if (rb == HAL_I2C_OK) rb = rd(x, TCA_CFG0, &cfg0b, 1);
    if (rb == HAL_I2C_OK) rb = rd(x, TCA_IN0, &in0, 1);
    uint8_t level = (uint8_t)(in0 ^ pol0);
    g_safe = st == HAL_I2C_OK && rb == HAL_I2C_OK && !(out0b & PMIC_PINS) && !(cfg0b & PMIC_PINS) && !(level & PMIC_PINS);
    s->ok = g_safe;
    emit(s,
         "\"ok\":%s,\"wrote\":%s,\"write\":\"%s\",\"readback\":\"%s\",\"out0_before\":\"%02x\",\"cfg0_before\":\"%02x\","
         "\"out0\":\"%02x\",\"cfg0\":\"%02x\",\"in0\":\"%02x\",\"pol0\":\"%02x\"",
         tf(g_safe), tf(wrote), hal_i2c_status_name(st), hal_i2c_status_name(rb), out0, cfg0, out0b, cfg0b, in0, pol0);
}

/* ------------------------------------------------------------------ the chip itself */

static void t_sys(slot_t *s) {
    esp_chip_info_t c;
    esp_chip_info(&c);
    const esp_partition_t *run = esp_ota_get_running_partition();
    int64_t t0 = esp_timer_get_time();
    uint32_t c0 = esp_cpu_get_cycle_count();
    vTaskDelay(pdMS_TO_TICKS(100));
    uint32_t c1 = esp_cpu_get_cycle_count();
    int64_t us = esp_timer_get_time() - t0;
    s->ok = true;
    emit(s, "\"ok\":true,\"idf\":\"%s\",\"revision\":%d,\"cores\":%d,\"cpu_mhz_config\":%d,\"cpu_mhz_measured\":%.1f,"
            "\"reset_reason\":%d,\"running_at\":\"0x%lx\"",
         esp_get_idf_version(), c.revision, c.cores, CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, (double)(uint32_t)(c1 - c0) / (double)us,
         (int)esp_reset_reason(), run ? (unsigned long)run->address : 0ul);
}

static void t_heap(slot_t *s) {
    s->ok = true;
    emit(s, "\"ok\":true,\"internal_free\":%u,\"internal_largest\":%u,\"internal_min\":%u,\"dma_free\":%u,\"psram_free\":%u",
         (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
         (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
         (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

/* A fresh 1 MiB block: written with a pattern, read back; nothing else in PSRAM is touched. */
static void t_psram(slot_t *s) {
    const size_t len = 1024 * 1024;
    size_t size = esp_psram_get_size();
    uint32_t *p = heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
    if (!p) {
        emit(s, "\"ok\":false,\"size\":%u,\"error\":\"no block\"", (unsigned)size);
        return;
    }
    size_t words = len / 4, bad = 0;
    uint32_t x = 0x12345678u;
    int64_t t = esp_timer_get_time();
    for (size_t i = 0; i < words; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        p[i] = x;
    }
    int64_t w = esp_timer_get_time() - t;
    x = 0x12345678u;
    t = esp_timer_get_time();
    for (size_t i = 0; i < words; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        bad += p[i] != x;
    }
    int64_t r = esp_timer_get_time() - t;
    heap_caps_free(p);
    s->ok = size >= 16u * 1024 * 1024 && bad == 0;
    emit(s, "\"ok\":%s,\"size\":%u,\"tested\":%u,\"bad_words\":%u,\"write_MBps\":%.1f,\"read_MBps\":%.1f", tf(s->ok),
         (unsigned)size, (unsigned)len, (unsigned)bad, (double)len / (double)(w ? w : 1), (double)len / (double)(r ? r : 1));
}

static void t_flash(slot_t *s) {
    uint32_t id = 0, size = 0;
    esp_err_t e = esp_flash_read_id(NULL, &id);
    if (e == ESP_OK) e = esp_flash_get_size(NULL, &size);
    s->ok = e == ESP_OK && size == 16u * 1024 * 1024;
    emit(s, "\"ok\":%s,\"err\":%d,\"jedec_id\":\"%06lx\",\"size\":%lu", tf(s->ok), e, (unsigned long)id, (unsigned long)size);
}

static volatile uint32_t g_sink;

static void t_timing(slot_t *s) {
    int64_t t = esp_timer_get_time();
    uint32_t v = 1;
    for (int i = 0; i < 1000000; i++) v = v * 1664525u + 1013904223u;
    g_sink = v;
    int64_t loop = esp_timer_get_time() - t;
    size_t len = 16 * 1024;
    uint8_t *a = heap_caps_malloc(len, MALLOC_CAP_INTERNAL), *b = heap_caps_malloc(len, MALLOC_CAP_INTERNAL);
    double mbps = 0;
    if (a && b) {
        memset(a, 0x5a, len);
        t = esp_timer_get_time();
        for (int i = 0; i < 64; i++) memcpy(b, a, len);
        int64_t us = esp_timer_get_time() - t;
        mbps = 64.0 * (double)len / (double)(us ? us : 1);
    }
    heap_caps_free(a);
    heap_caps_free(b);
    t = esp_timer_get_time();
    vTaskDelay(pdMS_TO_TICKS(100));
    int64_t slept = esp_timer_get_time() - t;
    s->ok = true;
    emit(s, "\"ok\":true,\"lcg_1M_us\":%lld,\"memcpy_internal_MBps\":%.1f,\"delay_100ms_us\":%lld", (long long)loop, mbps,
         (long long)slept);
}

static void t_gpio(slot_t *s) {
    gpio_config_t c = {.pin_bit_mask = (1ULL << S31_GPIO_EXP_INT) | (1ULL << S31_GPIO_TOUCH_INT), .mode = GPIO_MODE_INPUT,
                       .pull_up_en = GPIO_PULLUP_ENABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE, .intr_type = GPIO_INTR_DISABLE};
    esp_err_t e = gpio_config(&c);
    vTaskDelay(pdMS_TO_TICKS(5));
    s->ok = e == ESP_OK;
    emit(s, "\"ok\":%s,\"gpio2_exp_int\":%d,\"gpio5_touch_int\":%d", tf(s->ok), gpio_get_level(S31_GPIO_EXP_INT),
         gpio_get_level(S31_GPIO_TOUCH_INT));
}

/* ------------------------------------------------------------------ the I2C chips */

static const uint8_t kExpected[] = {SC7A20H_ADDR, TCA9555_ADDR_BASE, FT6336U_ADDR, PCF8563_ADDR, BQ27220_ADDR, CX25601N_ADDR};
/* May stay silent: the camera without its clock, the PMIC with WAKEUP low, the accelerometer's other address. */
static const uint8_t kMaybe[] = {0x3C, TPS65185_ADDR, SC7A20H_ADDR_ALT};

static bool in_list(uint8_t a, const uint8_t *l, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (l[i] == a) return true;
    return false;
}

static void hexlist(char *out, size_t cap, const uint8_t *l, size_t n) {
    size_t at = 0;
    out[0] = 0;
    for (size_t i = 0; i < n && at + 6 < cap; i++) at += (size_t)snprintf(out + at, cap - at, "%s\"%02x\"", i ? "," : "", l[i]);
}

static void t_i2c_scan(slot_t *s) {
    dev(TCA9555_ADDR_BASE, "io-expander"); /* the HAL makes the bus with its first device */
    i2c_master_bus_handle_t bus = NULL;
    if (i2c_master_get_bus_handle((i2c_port_num_t)S31_I2C_PORT, &bus) != ESP_OK || !bus) {
        emit(s, "\"ok\":false,\"error\":\"no bus\"");
        return;
    }
    uint8_t found[32], missing[8], extra[32];
    size_t nf = 0, nm = 0, ne = 0;
    for (uint8_t a = 0x08; a < 0x78; a++)
        if (i2c_master_probe(bus, a, 30) == ESP_OK && nf < sizeof found) found[nf++] = a;
    for (size_t i = 0; i < sizeof kExpected; i++)
        if (!in_list(kExpected[i], found, nf)) missing[nm++] = kExpected[i];
    for (size_t i = 0; i < nf; i++)
        if (!in_list(found[i], kExpected, sizeof kExpected) && !in_list(found[i], kMaybe, sizeof kMaybe) && ne < sizeof extra)
            extra[ne++] = found[i];
    char f[160], m[64], x[160];
    hexlist(f, sizeof f, found, nf);
    hexlist(m, sizeof m, missing, nm);
    hexlist(x, sizeof x, extra, ne);
    s->ok = nm == 0;
    emit(s, "\"ok\":%s,\"found\":[%s],\"missing\":[%s],\"unexpected\":[%s]", tf(s->ok), f, m, x);
}

static void t_expander(slot_t *s) {
    hal_i2c_dev_t *x = dev(TCA9555_ADDR_BASE, "io-expander");
    uint8_t r[8] = {0};
    hal_i2c_status_t st = HAL_I2C_OK;
    for (uint8_t reg = 0; reg < 8 && st == HAL_I2C_OK; reg++) st = rd(x, reg, &r[reg], 1);
    s->ok = st == HAL_I2C_OK;
    if (!s->ok) {
        emit(s, "\"ok\":false,\"error\":\"%s\"", hal_i2c_status_name(st));
        return;
    }
    uint8_t p1 = (uint8_t)(r[TCA_IN1] ^ r[TCA_POL1]), p0 = (uint8_t)(r[TCA_IN0] ^ r[TCA_POL0]);
    emit(s,
         "\"ok\":true,\"in0\":\"%02x\",\"in1\":\"%02x\",\"out0\":\"%02x\",\"out1\":\"%02x\",\"cfg0\":\"%02x\",\"cfg1\":\"%02x\","
         "\"pol0\":\"%02x\",\"pol1\":\"%02x\",\"screen_power_p05\":%d,\"pmic_pwr_good_p07\":%d,\"shared_reset_p12\":%d,"
         "\"shared_power_p14\":%d,\"rtc_int_p16\":%d,\"vol_up_pressed\":%s,\"vol_down_pressed\":%s,\"power_pressed\":%s",
         r[TCA_IN0], r[TCA_IN1], r[TCA_OUT0], r[TCA_OUT1], r[TCA_CFG0], r[TCA_CFG1], r[TCA_POL0], r[TCA_POL1], !!(p0 & EPD_PWR_SCREEN),
         !!(p0 & EPD_PWR_GOOD), !!(p1 & MB_P1_SHARED_RST), !!(p1 & MB_P1_SHARED_PWR), !!(p1 & MB_P1_RTC_INT),
         tf(!(p1 & MB_P1_VOL_UP)), tf(!(p1 & MB_P1_VOL_DOWN)), tf(!(p1 & MB_P1_POWER_KEY)));
}

static void t_touch(slot_t *s) {
    hal_i2c_dev_t *d = dev(FT6336U_ADDR, "touch");
    uint8_t id = 0, fw = 0;
    hal_i2c_status_t st = rd(d, 0xA3, &id, 1);
    if (st == HAL_I2C_OK) st = rd(d, 0xA6, &fw, 1);
    ft6336u_point_t pts[FT6336U_MAX_POINTS];
    int n = -1;
    if (st == HAL_I2C_OK && ft6336u_read(d, pts, FT6336U_MAX_POINTS, &n) != HAL_I2C_OK) n = -1;
    s->ok = st == HAL_I2C_OK && id == FT6336U_CHIP_ID && fw == 0x03;
    emit(s, "\"ok\":%s,\"status\":\"%s\",\"chip_id\":\"%02x\",\"fw\":\"%02x\",\"points\":%d", tf(s->ok), hal_i2c_status_name(st), id,
         fw, n);
}

static void t_gauge(slot_t *s) {
    hal_i2c_dev_t *d = dev(BQ27220_ADDR, "fuel-gauge");
    bq27220_reading_t r = {0};
    int temp = 0;
    hal_i2c_status_t st = bq27220_reading(d, &r);
    if (st == HAL_I2C_OK) st = bq27220_read(d, BQ27220_TEMPERATURE, &temp);
    s->ok = st == HAL_I2C_OK && r.mv > 3000 && r.mv < 4500 && r.soc >= 0 && r.soc <= 100;
    emit(s, "\"ok\":%s,\"status\":\"%s\",\"mV\":%d,\"mA\":%d,\"percent\":%d,\"celsius\":%.1f", tf(s->ok), hal_i2c_status_name(st),
         r.mv, r.ma, r.soc, temp / 10.0 - 273.15);
}

static void t_charger(slot_t *s) {
    hal_i2c_dev_t *d = dev(CX25601N_ADDR, "charger");
    uint8_t part = 0, lo = 0, hi = 0;
    cx25601n_chg_t chg = CX25601N_NOT_CHARGING;
    int vbus = -1;
    hal_i2c_status_t st = cx25601n_part_info(d, &part);
    if (st == HAL_I2C_OK) st = cx25601n_status(d, &chg, &vbus);
    if (st == HAL_I2C_OK) st = rd(d, 0x02, &lo, 1); /* ICHG[1:0] in bits 7:6 (cx25601n.c) */
    if (st == HAL_I2C_OK) st = rd(d, 0x03, &hi, 1); /* ICHG[5:2] in bits 3:0 */
    int code = ((hi & 0x0F) << 2) | (lo >> 6);
    /* Input current limit, as the vendor driver reads it (cx25601n.cc: REG_IINDPM_LO 0x06 bits 7:4, HI 0x07
     * bits 3:0, 20 mA steps), and STATUS1 (0x1E) raw; read only. */
    uint8_t ilo = 0, ihi = 0, st1 = 0;
    if (st == HAL_I2C_OK) st = rd(d, 0x06, &ilo, 1);
    if (st == HAL_I2C_OK) st = rd(d, 0x07, &ihi, 1);
    if (st == HAL_I2C_OK) st = rd(d, 0x1E, &st1, 1);
    int iindpm = (((ihi & 0x0F) << 4) | (ilo >> 4)) * 20;
    s->ok = st == HAL_I2C_OK;
    emit(s, "\"ok\":%s,\"status\":\"%s\",\"part\":\"%02x\",\"charge_state\":%d,\"vbus_state\":%d,\"charge_current_mA\":%d,"
            "\"iindpm_mA\":%d,\"reg06\":\"%02x\",\"reg07\":\"%02x\",\"status1\":\"%02x\"",
         tf(s->ok), hal_i2c_status_name(st), part, (int)chg, vbus, code * CX25601N_ICHG_STEP_MA, iindpm, ilo, ihi, st1);
}

static void t_rtc(slot_t *s) {
    hal_i2c_dev_t *d = dev(PCF8563_ADDR, "rtc");
    int64_t unix_s = 0;
    bool valid = false;
    uint8_t ctrl2 = 0;
    hal_i2c_status_t st = pcf8563_get(d, &unix_s, &valid);
    if (st == HAL_I2C_OK) st = rd(d, 0x01, &ctrl2, 1);
    int y = 0, m = 0, day = 0;
    pcf8563_civil_from_days(unix_s / 86400, &y, &m, &day);
    s->ok = st == HAL_I2C_OK && valid && y >= 2025;
    emit(s, "\"ok\":%s,\"status\":\"%s\",\"clock_valid\":%s,\"year\":%d,\"ctrl2\":\"%02x\"", tf(s->ok), hal_i2c_status_name(st),
         tf(valid), y, ctrl2);
}

static void t_accel(slot_t *s) {
    uint8_t addr = SC7A20H_ADDR, who = 0, ctrl1 = 0;
    hal_i2c_dev_t *d = dev(addr, "accel");
    hal_i2c_status_t st = rd(d, 0x0F, &who, 1);
    if (st != HAL_I2C_OK) {
        addr = SC7A20H_ADDR_ALT;
        d = dev(addr, "accel-alt");
        st = rd(d, 0x0F, &who, 1);
    }
    int x = 0, y = 0, z = 0;
    if (st == HAL_I2C_OK) st = rd(d, 0x20, &ctrl1, 1);
    if (st == HAL_I2C_OK) st = sc7a20h_read_mg(d, &x, &y, &z);
    s->ok = st == HAL_I2C_OK && who == 0x11;
    emit(s, "\"ok\":%s,\"status\":\"%s\",\"addr\":\"%02x\",\"who_am_i\":\"%02x\",\"ctrl1\":\"%02x\",\"x\":%d,\"y\":%d,\"z\":%d", tf(s->ok),
         hal_i2c_status_name(st), addr, who, ctrl1, x, y, z);
}

static void t_pmic(slot_t *s) {
    i2c_master_bus_handle_t bus = NULL;
    bool ack = i2c_master_get_bus_handle((i2c_port_num_t)S31_I2C_PORT, &bus) == ESP_OK && bus &&
               i2c_master_probe(bus, TPS65185_ADDR, 30) == ESP_OK;
    uint8_t rev = 0;
    hal_i2c_status_t st = ack ? tps65185_read(dev(TPS65185_ADDR, "pmic"), TPS_REG_REVID, &rev) : HAL_I2C_NACK;
    s->ok = true; /* asleep is expected: WAKEUP is low */
    emit(s, "\"ok\":true,\"answers\":%s,\"status\":\"%s\",\"revid\":\"%02x\"", tf(ack), hal_i2c_status_name(st), rev);
}

/* ------------------------------------------------------------------ card, modem, Wi-Fi */

/* Detect only: the card is brought up to read its size; no sector is read or written. */
static void t_card(slot_t *s) {
    for (int pin = 20; pin <= 25; pin++) gpio_hold_dis(pin);
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = CARD_CLK;
    slot.cmd = CARD_CMD;
    slot.d0 = CARD_D0;
    slot.d1 = CARD_D0 + 1;
    slot.d2 = CARD_D0 + 2;
    slot.d3 = CARD_D0 + 3;
    sdmmc_card_t c;
    memset(&c, 0, sizeof c);
    esp_err_t e = sdmmc_host_init();
    if (e == ESP_OK) e = sdmmc_host_init_slot(host.slot, &slot);
    if (e == ESP_OK) e = sdmmc_card_init(&host, &c);
    s->ok = true; /* no card is an answer too */
    if (e == ESP_OK)
        emit(s, "\"ok\":true,\"present\":true,\"size_mib\":%llu",
             (unsigned long long)c.csd.capacity * (unsigned long long)c.csd.sector_size / 1048576ull);
    else
        emit(s, "\"ok\":true,\"present\":false,\"err\":%d", e);
    sdmmc_host_deinit();
}

/* The only things ever sent to the modem: questions, none about its or the SIM's identity. */
static const char *const kAsk[] = {"AT", "ATI", "AT+CGMR", "AT+CPIN?", "AT+CSQ", "AT+CREG?", "AT+CEREG?"};
#define MODEM_UART UART_NUM_2
#define MODEM_KEEP 64

/* Digit runs of 10 or more could be an IMEI, ICCID, IMSI or phone number: masked with 'X'. */
static int mask_digits(uint8_t *b, int n) {
    int masked = 0;
    for (int i = 0; i < n;) {
        int j = i;
        while (j < n && b[j] >= '0' && b[j] <= '9') j++;
        if (j - i >= 10) {
            memset(b + i, 'X', (size_t)(j - i));
            masked += j - i;
        }
        i = j > i ? j : i + 1;
    }
    return masked;
}

/* Sends one question, keeps the first MODEM_KEEP bytes of whatever comes back within 800 ms. */
static int modem_ask(const char *q, uint8_t *keep, int *kept) {
    uart_flush_input(MODEM_UART);
    uart_write_bytes(MODEM_UART, q, strlen(q));
    uart_write_bytes(MODEM_UART, "\r\n", 2);
    uart_wait_tx_done(MODEM_UART, pdMS_TO_TICKS(100));
    int total = 0;
    *kept = 0;
    int64_t end = now_ms() + 800;
    uint8_t tmp[64];
    while (now_ms() < end) {
        int got = uart_read_bytes(MODEM_UART, tmp, sizeof tmp, pdMS_TO_TICKS(100));
        if (got <= 0) {
            if (total) break; /* quiet after an answer */
            continue;
        }
        int take = got < MODEM_KEEP - *kept ? got : MODEM_KEEP - *kept;
        memcpy(keep + *kept, tmp, (size_t)take);
        *kept += take;
        total += got;
    }
    return total;
}

static void modem_line(slot_t *s, const char *q, int baud, int total, uint8_t *keep, int kept) {
    int masked = mask_digits(keep, kept);
    char hex[2 * MODEM_KEEP + 1];
    for (int i = 0; i < kept; i++) snprintf(hex + 2 * i, 3, "%02x", keep[i]);
    hex[2 * kept] = 0;
    emit(s, "\"ok\":true,\"ask\":\"%s\",\"baud\":%d,\"bytes\":%d,\"masked\":%d,\"hex\":\"%s\"", q, baud, total, masked, hex);
}

static void t_modem(slot_t *s) {
    gpio_set_direction(GPIO_MODEM_SRDY, GPIO_MODE_INPUT);
    int srdy = gpio_get_level(GPIO_MODEM_SRDY);
    uart_config_t c = {.baud_rate = 115200, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE,
                       .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT};
    if (uart_driver_install(MODEM_UART, 2048, 0, 0, NULL, 0) != ESP_OK) {
        emit(s, "\"ok\":false,\"error\":\"uart\"");
        return;
    }
    if (uart_param_config(MODEM_UART, &c) != ESP_OK ||
        uart_set_pin(MODEM_UART, S31_UART2_TX, S31_UART2_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        emit(s, "\"ok\":false,\"error\":\"uart pins\"");
        uart_driver_delete(MODEM_UART);
        return;
    }
    emit(s, "\"ok\":true,\"srdy\":%d", srdy);
    /* "AT" once at 115200 and, if nothing came back, once at upstream's 2 Mbps link rate; no retries. */
    static const int kBaud[] = {115200, 2000000};
    uint8_t keep[MODEM_KEEP];
    int kept = 0, baud = 0;
    for (size_t i = 0; i < sizeof kBaud / sizeof kBaud[0] && !baud; i++) {
        uart_set_baudrate(MODEM_UART, (uint32_t)kBaud[i]);
        int total = modem_ask(kAsk[0], keep, &kept);
        modem_line(s, kAsk[0], kBaud[i], total, keep, kept);
        if (total > 0) baud = kBaud[i];
    }
    if (baud)
        for (size_t i = 1; i < sizeof kAsk / sizeof kAsk[0]; i++) {
            int total = modem_ask(kAsk[i], keep, &kept);
            modem_line(s, kAsk[i], baud, total, keep, kept);
        }
    uart_driver_delete(MODEM_UART);
    s->ok = baud != 0;
    emit(s, "\"ok\":%s,\"answered_at\":%d", tf(s->ok), baud);
}

/* Wi-Fi up for a scan: RAM only, nothing kept in NVS; never connects. */
static bool g_net_ready;
static esp_err_t wifi_up(void) {
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("phy", ESP_LOG_ERROR);
    if (!g_net_ready) { /* once: a second default STA netif would not be made */
        esp_netif_init();
        esp_event_loop_create_default();
        esp_netif_create_default_wifi_sta();
        g_net_ready = true;
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = 0;
    esp_err_t e = esp_wifi_init(&cfg);
    if (e == ESP_OK) e = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (e == ESP_OK) e = esp_wifi_set_mode(WIFI_MODE_STA);
    if (e == ESP_OK) e = esp_wifi_start();
    return e;
}

static void wifi_down(void) {
    esp_wifi_clear_ap_list();
    esp_wifi_stop();
    esp_wifi_deinit();
}

/* Scan only and say how many: no connection, no names, no addresses. */
static void t_wifi(slot_t *s) {
    esp_err_t e = wifi_up();
    uint16_t found = 0;
    int64_t t = now_ms();
    if (e == ESP_OK) e = esp_wifi_scan_start(NULL, true);
    if (e == ESP_OK) e = esp_wifi_scan_get_ap_num(&found);
    int64_t ms = now_ms() - t;
    wifi_down();
    s->ok = e == ESP_OK;
    emit(s, "\"ok\":%s,\"err\":%d,\"scan_ms\":%lld,\"networks\":%u", tf(s->ok), e, (long long)ms, (unsigned)found);
}

/* ------------------------------------------------------------------ round 2: each leaves nothing changed */

/* Accelerometer on for a moment: CTRL1 to 100 Hz, normal mode, X/Y/Z (sc7a20h_init's value), a few
 * samples, then CTRL1 back as it was and read back. Nothing else is written. If our change latched a
 * motion interrupt that pulls GPIO2 low, AOI1_SRC is read once to release it (a read). */
#define ACC_CTRL1 0x20
#define ACC_CTRL4 0x23
#define ACC_AOI1_SRC 0x31
#define ACC_ON 0x57
static void t_accel_axes(slot_t *s) {
    uint8_t addr = SC7A20H_ADDR, who = 0;
    hal_i2c_dev_t *d = dev(addr, "accel");
    if (rd(d, 0x0F, &who, 1) != HAL_I2C_OK) {
        addr = SC7A20H_ADDR_ALT;
        d = dev(addr, "accel-alt");
    }
    uint8_t was = 0, ctrl4 = 0, back = 0xFF;
    hal_i2c_status_t st = rd(d, ACC_CTRL1, &was, 1);
    if (st == HAL_I2C_OK) st = rd(d, ACC_CTRL4, &ctrl4, 1);
    if (st != HAL_I2C_OK) {
        emit(s, "\"ok\":false,\"step\":\"read\",\"error\":\"%s\"", hal_i2c_status_name(st));
        return;
    }
    int line_before = gpio_get_level(S31_GPIO_EXP_INT);
    uint8_t on[2] = {ACC_CTRL1, ACC_ON};
    hal_i2c_status_t w = hal_i2c_write(d, on, 2);
    long sx = 0, sy = 0, sz = 0;
    int n = 0;
    if (w == HAL_I2C_OK) {
        vTaskDelay(pdMS_TO_TICKS(60)); /* a few 10 ms output periods */
        for (int i = 0; i < 8; i++) {
            int x, y, z;
            if (sc7a20h_read_mg(d, &x, &y, &z) == HAL_I2C_OK) {
                sx += x;
                sy += y;
                sz += z;
                n++;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    /* back as it was, whatever happened above (a failed write may have landed) */
    uint8_t off[2] = {ACC_CTRL1, was};
    hal_i2c_status_t r = HAL_I2C_ERR;
    for (int i = 0; i < 3 && r != HAL_I2C_OK; i++) r = hal_i2c_write(d, off, 2);
    if (r == HAL_I2C_OK) r = rd(d, ACC_CTRL1, &back, 1);
    bool restored = r == HAL_I2C_OK && back == was;
    vTaskDelay(pdMS_TO_TICKS(20));
    int line_after = gpio_get_level(S31_GPIO_EXP_INT);
    bool released = false;
    if (line_before == 1 && line_after == 0) {
        uint8_t src = 0;
        released = rd(d, ACC_AOI1_SRC, &src, 1) == HAL_I2C_OK;
        line_after = gpio_get_level(S31_GPIO_EXP_INT);
    }
    static const int kMgPerDigit[4] = {1, 2, 4, 12}; /* 12-bit counts, by CTRL4 FS: 2, 4, 8, 16 g */
    int scale = kMgPerDigit[(ctrl4 >> 4) & 3];
    int x = n ? (int)(sx / n) * scale : 0, y = n ? (int)(sy / n) * scale : 0, z = n ? (int)(sz / n) * scale : 0;
    int g = (int)sqrt((double)x * x + (double)y * y + (double)z * z);
    int ax = abs(x), ay = abs(y), az = abs(z);
    char axis = az >= ax && az >= ay ? 'z' : (ay >= ax ? 'y' : 'x');
    int v = axis == 'z' ? z : (axis == 'y' ? y : x);
    s->ok = w == HAL_I2C_OK && n > 0 && restored && g > 700 && g < 1300;
    emit(s,
         "\"ok\":%s,\"addr\":\"%02x\",\"ctrl1_was\":\"%02x\",\"ctrl1_back\":\"%02x\",\"restored\":%s,\"ctrl4\":\"%02x\","
         "\"samples\":%d,\"x_mg\":%d,\"y_mg\":%d,\"z_mg\":%d,\"g_mg\":%d,\"down\":\"%c%c\",\"gpio2_before\":%d,"
         "\"gpio2_after\":%d,\"latch_released\":%s",
         tf(s->ok), addr, was, back, tf(restored), ctrl4, n, x, y, z, g, v < 0 ? '-' : '+', axis, line_before, line_after,
         tf(released));
}

/* Battery current by the gauge (+ = into the battery). With USB plugged in the charger adds its own
 * share, so the differences between the phases are what counts. */
static volatile bool g_spin, g_scanning;
static volatile int g_scanners;

static void spin(void *arg) {
    (void)arg;
    uint32_t v = 1;
    while (g_spin) v = v * 1664525u + 1013904223u;
    g_sink = v;
    vTaskDelete(NULL);
}

static void scanner(void *arg) {
    (void)arg;
    while (g_scanning) {
        esp_wifi_scan_start(NULL, true);
        esp_wifi_clear_ap_list();
    }
    g_scanners--;
    vTaskDelete(NULL);
}

/* 1 s to settle (the gauge updates about once a second), then 5 s of samples every 250 ms. */
static int sample_ma(hal_i2c_dev_t *d, int *n, int *avg_reg) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    long sum = 0;
    *n = 0;
    int64_t end = now_ms() + 5000;
    while (now_ms() < end) {
        int v;
        if (bq27220_read(d, BQ27220_CURRENT, &v) == HAL_I2C_OK) {
            sum += v;
            (*n)++;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    if (bq27220_read(d, BQ27220_AVERAGE_CURRENT, avg_reg) != HAL_I2C_OK) *avg_reg = 0;
    return *n ? (int)(sum / *n) : 0;
}

static void t_current(slot_t *s) {
    hal_i2c_dev_t *d = dev(BQ27220_ADDR, "fuel-gauge");
    int n[3], avg[3], ma[3];
    ma[0] = sample_ma(d, &n[0], &avg[0]); /* idle: this task sleeps between samples, the cores wait */
    g_spin = true;
    bool spun = xTaskCreatePinnedToCore(spin, "spin0", 2048, NULL, 2, NULL, 0) == pdPASS;
    spun = xTaskCreatePinnedToCore(spin, "spin1", 2048, NULL, 2, NULL, 1) == pdPASS && spun;
    ma[1] = sample_ma(d, &n[1], &avg[1]);
    g_spin = false;
    esp_err_t e = wifi_up();
    if (e == ESP_OK) {
        g_scanning = true;
        g_scanners = 1;
        if (xTaskCreate(scanner, "scan", 4096, NULL, 4, NULL) != pdPASS) g_scanners = 0;
    }
    ma[2] = sample_ma(d, &n[2], &avg[2]);
    g_scanning = false;
    for (int i = 0; i < 400 && g_scanners > 0; i++) vTaskDelay(pdMS_TO_TICKS(10)); /* the last scan ends */
    wifi_down();
    static const char *const kPhase[3] = {"idle", "two_cores_busy", "wifi_scanning"};
    for (int i = 0; i < 3; i++)
        emit(s, "\"ok\":%s,\"phase\":\"%s\",\"mA\":%d,\"samples\":%d,\"avg_current_reg_mA\":%d", tf(n[i] > 0), kPhase[i], ma[i],
             n[i], avg[i]);
    s->ok = n[0] > 0 && n[1] > 0 && n[2] > 0 && spun && e == ESP_OK;
    emit(s, "\"ok\":%s,\"busy_minus_idle_mA\":%d,\"wifi_minus_idle_mA\":%d,\"wifi_err\":%d,\"note\":\"+ is into the battery; "
            "USB charging adds to every phase\"",
         tf(s->ok), ma[1] - ma[0], ma[2] - ma[0], e);
}

/* The clock chip's countdown timer for 2 s with its interrupt on: does P16 (PCF8563 INT, active low, through the
 * expander) follow, and GPIO2? The vendor reads P16 low as an event to clear (periph/metalio_rtc.cc:33-38); every
 * self-test round so far read it 0 with no flag set. CTRL2, timer control and timer value are read first and
 * written back; the flags are cleared. */
#define RTC_CTRL2 0x01
#define RTC_TCTRL 0x0E
#define RTC_TVAL 0x0F
static int p16_level(hal_i2c_dev_t *x) {
    uint8_t in1 = 0;
    return rd(x, 0x01, &in1, 1) == HAL_I2C_OK ? (in1 >> 6) & 1 : -1;
}
static void t_rtc_int(slot_t *s) {
    hal_i2c_dev_t *r = dev(PCF8563_ADDR, "rtc"), *x = dev(TCA9555_ADDR_BASE, "expander");
    uint8_t c2 = 0, tc = 0, tv = 0;
    hal_i2c_status_t st = rd(r, RTC_CTRL2, &c2, 1);
    if (st == HAL_I2C_OK) st = rd(r, RTC_TCTRL, &tc, 1);
    if (st == HAL_I2C_OK) st = rd(r, RTC_TVAL, &tv, 1);
    if (st != HAL_I2C_OK) {
        emit(s, "\"ok\":false,\"step\":\"read\",\"error\":\"%s\"", hal_i2c_status_name(st));
        return;
    }
    int p16_idle = p16_level(x), g2_idle = gpio_get_level(S31_GPIO_EXP_INT);
    uint8_t set_val[2] = {RTC_TVAL, 2}, set_ctl[2] = {RTC_TCTRL, 0x82}, set_c2[2] = {RTC_CTRL2, 0x01}; /* 2 ticks of 1 Hz, TIE */
    hal_i2c_status_t w = hal_i2c_write(r, set_val, 2);
    if (w == HAL_I2C_OK) w = hal_i2c_write(r, set_ctl, 2);
    if (w == HAL_I2C_OK) w = hal_i2c_write(r, set_c2, 2);
    int fired_ms = -1, p16_fired = -1, g2_fired = -1;
    for (int ms = 0; w == HAL_I2C_OK && ms < 3500; ms += 100) {
        uint8_t now = 0;
        if (rd(r, RTC_CTRL2, &now, 1) == HAL_I2C_OK && (now & 0x04)) {
            fired_ms = ms;
            p16_fired = p16_level(x);
            g2_fired = gpio_get_level(S31_GPIO_EXP_INT);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    uint8_t back_c2[2] = {RTC_CTRL2, (uint8_t)(c2 & ~0x0Cu)}, back_ctl[2] = {RTC_TCTRL, tc}, back_val[2] = {RTC_TVAL, tv};
    hal_i2c_status_t b = HAL_I2C_ERR;
    for (int i = 0; i < 3 && b != HAL_I2C_OK; i++) {
        b = hal_i2c_write(r, back_c2, 2);
        if (b == HAL_I2C_OK) b = hal_i2c_write(r, back_ctl, 2);
        if (b == HAL_I2C_OK) b = hal_i2c_write(r, back_val, 2);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    uint8_t c2_after = 0xFF, tc_after = 0xFF;
    if (rd(r, RTC_CTRL2, &c2_after, 1) != HAL_I2C_OK || rd(r, RTC_TCTRL, &tc_after, 1) != HAL_I2C_OK) b = HAL_I2C_ERR;
    int p16_after = p16_level(x), g2_after = gpio_get_level(S31_GPIO_EXP_INT);
    bool restored = b == HAL_I2C_OK && tc_after == tc && (c2_after & 0x0F) == (c2 & 0x03);
    s->ok = w == HAL_I2C_OK && fired_ms >= 0 && restored;
    emit(s, "\"ok\":%s,\"ctrl2_was\":\"%02x\",\"tctrl_was\":\"%02x\",\"fired_ms\":%d,\"p16_idle\":%d,\"p16_fired\":%d,"
            "\"p16_after\":%d,\"gpio2_idle\":%d,\"gpio2_fired\":%d,\"gpio2_after\":%d,\"restored\":%s,\"ctrl2_after\":\"%02x\"",
         tf(s->ok), c2, tc, fired_ms, p16_idle, p16_fired, p16_after, g2_idle, g2_fired, g2_after, tf(restored), c2_after);
}

/* The accelerometer's motion interrupt through to GPIO2, set off by the chip's own self-test so no one has to move
 * the device: AOI1 set up as the vendor does (common/sensor/sc7a20h.cc:22-31), self-test 0 on for up to 300 ms, then
 * off and the latch read away. All nine registers are read first and written back. AOI1_SRC's IA bit tells the
 * accelerometer from the expander, which shares GPIO2. */
static const uint8_t kAccRegs[] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x30, 0x32, 0x33};
static const uint8_t kAccVendor[] = {0x57, 0x01, 0x40, 0x88, 0x08, 0x03, 0x2A, 0x10, 0x00};
static void t_accel_int(slot_t *s) {
    uint8_t addr = SC7A20H_ADDR, who = 0;
    hal_i2c_dev_t *d = dev(addr, "accel");
    if (rd(d, 0x0F, &who, 1) != HAL_I2C_OK) {
        addr = SC7A20H_ADDR_ALT;
        d = dev(addr, "accel-alt");
    }
    uint8_t saved[sizeof kAccRegs];
    for (size_t i = 0; i < sizeof kAccRegs; i++)
        if (rd(d, kAccRegs[i], &saved[i], 1) != HAL_I2C_OK) {
            emit(s, "\"ok\":false,\"step\":\"save\"");
            return;
        }
    hal_i2c_status_t w = HAL_I2C_OK;
    for (size_t i = 0; i < sizeof kAccRegs && w == HAL_I2C_OK; i++) {
        uint8_t b2[2] = {kAccRegs[i], kAccVendor[i]};
        w = hal_i2c_write(d, b2, 2);
    }
    uint8_t src = 0, src_on = 0;
    vTaskDelay(pdMS_TO_TICKS(100));
    (void)rd(d, ACC_AOI1_SRC, &src, 1);
    vTaskDelay(pdMS_TO_TICKS(30));
    int g2_quiet = gpio_get_level(S31_GPIO_EXP_INT);
    uint8_t st_on[2] = {ACC_CTRL4, 0x88 | 0x02};
    if (w == HAL_I2C_OK) w = hal_i2c_write(d, st_on, 2);
    int low_ms = -1;
    for (int ms = 0; w == HAL_I2C_OK && ms <= 300; ms += 10) {
        if (gpio_get_level(S31_GPIO_EXP_INT) == 0) {
            low_ms = ms;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    (void)rd(d, ACC_AOI1_SRC, &src_on, 1); /* after looking at the line: reading clears the latch */
    uint8_t st_off[2] = {ACC_CTRL4, 0x88};
    (void)hal_i2c_write(d, st_off, 2);
    vTaskDelay(pdMS_TO_TICKS(150));
    (void)rd(d, ACC_AOI1_SRC, &src, 1);
    vTaskDelay(pdMS_TO_TICKS(30));
    int g2_cleared = gpio_get_level(S31_GPIO_EXP_INT);
    hal_i2c_status_t b = HAL_I2C_OK;
    for (size_t i = 0; i < sizeof kAccRegs; i++) {
        uint8_t b2[2] = {kAccRegs[i], saved[i]};
        hal_i2c_status_t e = hal_i2c_write(d, b2, 2);
        if (e != HAL_I2C_OK) b = e;
    }
    (void)rd(d, ACC_AOI1_SRC, &src, 1);
    bool restored = b == HAL_I2C_OK;
    for (size_t i = 0; i < sizeof kAccRegs && restored; i++) {
        uint8_t v = 0;
        restored = rd(d, kAccRegs[i], &v, 1) == HAL_I2C_OK && v == saved[i];
    }
    vTaskDelay(pdMS_TO_TICKS(30));
    int g2_end = gpio_get_level(S31_GPIO_EXP_INT);
    s->ok = w == HAL_I2C_OK && restored;
    emit(s, "\"ok\":%s,\"gpio2_quiet\":%d,\"gpio2_low_after_ms\":%d,\"aoi1_src_on\":\"%02x\",\"ia\":%s,\"gpio2_after_clear\":%d,"
            "\"gpio2_end\":%d,\"restored\":%s",
         tf(s->ok), g2_quiet, low_ms, src_on, tf((src_on & 0x40) != 0), g2_cleared, g2_end, tf(restored));
}

/* 1 MiB of ota_0 (the factory firmware) read through the flash driver; nothing written. */
static void t_flash_read(slot_t *s) {
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    const size_t chunk = 64 * 1024, len = 1024 * 1024;
    uint8_t *buf = heap_caps_malloc(chunk, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!p || !buf) {
        heap_caps_free(buf);
        emit(s, "\"ok\":false,\"error\":\"%s\"", p ? "no buffer" : "no ota_0");
        return;
    }
    esp_err_t e = ESP_OK;
    uint8_t first = 0;
    int64_t t = esp_timer_get_time();
    for (size_t off = 0; off < len && e == ESP_OK; off += chunk) {
        e = esp_partition_read(p, off, buf, chunk);
        if (!off) first = buf[0];
    }
    int64_t us = esp_timer_get_time() - t;
    heap_caps_free(buf);
    s->ok = e == ESP_OK && first == 0xE9;
    emit(s, "\"ok\":%s,\"err\":%d,\"bytes\":%u,\"MBps\":%.1f,\"app_magic\":%s", tf(s->ok), e, (unsigned)len,
         (double)len / (double)(us ? us : 1), tf(first == 0xE9));
}

#if HWTEST_SLEEP
/* ------------------------------------------------------------------ light sleep with the vendor patch (sleep build)
 * Built with HWTEST_SLEEP=1 (tools/hw-selftest/build.sh): only safety, sys and this test run. The vendor's S31
 * light-sleep patch (ports/esp32s31/vendor/s31_sleep, MIT) wraps the sleep; USB Serial/JTAG is brought back the way
 * the vendor does after a key wake (main/boards/common/power_policy/power_hw.cc, ReviveUsjAfterSdRestore). Nothing
 * outside the chip is configured and the panel stays off.
 *
 * A fuse so that no one has to come: the RTC watchdog with an RTC reset, the same full reset esptool's watchdog reset
 * makes, which brought USB back in 0.1 s on this unit (docs/hardware/2026-11-10-selftest.md). It is armed before the
 * sleep; IDF leaves a watchdog the program armed alone (esp_hw_support/sleep_modes.c, "If WDT was enabled in the user
 * code, then do not change it here"). Progress goes into ota_1's last sector, which the runner writes back from the
 * backup: ARMED before the sleep, WOKE after it. A boot that finds ARMED or WOKE reports it after a full reset (USB
 * alive), or after a CPU-only reset (a panic: run 3 showed that does not bring USB back) marks RECOVER and makes the
 * full reset itself. */
#include "hal/wdt_hal.h"
#include "hal/usb_serial_jtag_ll.h"
#include "soc/rtc.h"
int ink_s31_sleep_init(void);
int ink_s31_light_sleep(void);
bool usb_serial_jtag_is_connected(void); /* esp_driver_usb_serial_jtag; declared like the vendor does */
#define MARK_MAGIC 0x534C5032u           /* "SLP2" */
enum { MARK_ARMED = 1, MARK_WOKE = 2, MARK_RECOVER = 3 };
typedef struct {
    uint32_t magic, state, detail, slept_ms, causes, crc;
} mark_t;
static mark_t g_prev;
static bool g_have_prev;
static const esp_partition_t *mark_part(void) {
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
}
static uint32_t mark_crc(const mark_t *m) { return esp_rom_crc32_le(0, (const uint8_t *)m, (uint32_t)offsetof(mark_t, crc)); }
static bool mark_read(mark_t *m) {
    const esp_partition_t *p = mark_part();
    return p && esp_partition_read(p, p->size - 4096, m, sizeof *m) == ESP_OK && m->magic == MARK_MAGIC && m->crc == mark_crc(m);
}
static void mark_write(uint32_t state, uint32_t detail, uint32_t slept_ms, uint32_t causes) {
    const esp_partition_t *p = mark_part();
    mark_t m = {.magic = MARK_MAGIC, .state = state, .detail = detail, .slept_ms = slept_ms, .causes = causes};
    m.crc = mark_crc(&m);
    if (p && esp_partition_erase_range(p, p->size - 4096, 4096) == ESP_OK) (void)esp_partition_write(p, p->size - 4096, &m, sizeof m);
}
static void mark_clear(void) {
    const esp_partition_t *p = mark_part();
    if (p) (void)esp_partition_erase_range(p, p->size - 4096, 4096);
}
static void fuse_arm(uint32_t ms) {
    wdt_hal_context_t c = RWDT_HAL_CONTEXT_DEFAULT();
    wdt_hal_init(&c, WDT_RWDT, 0, false);
    wdt_hal_write_protect_disable(&c);
    wdt_hal_config_stage(&c, WDT_STAGE0, (uint32_t)((uint64_t)ms * rtc_clk_slow_freq_get_hz() / 1000u), WDT_STAGE_ACTION_RESET_RTC);
    wdt_hal_enable(&c);
    wdt_hal_write_protect_enable(&c);
}
static void fuse_off(void) {
    wdt_hal_context_t c = RWDT_HAL_CONTEXT_DEFAULT();
    wdt_hal_write_protect_disable(&c);
    wdt_hal_disable(&c);
    wdt_hal_write_protect_enable(&c);
}
/* First thing in app_main, before the console is used. */
static void sleep_boot_check(void) {
    mark_t m;
    if (!mark_read(&m)) return;
    esp_reset_reason_t why = esp_reset_reason();
    if ((m.state == MARK_ARMED || m.state == MARK_WOKE) && why != ESP_RST_POWERON) {
        mark_write(MARK_RECOVER, m.state | ((uint32_t)why << 8), m.slept_ms, m.causes);
        fuse_arm(200); /* a full reset brings USB back; a CPU reset did not (run 3) */
        for (;;) vTaskDelay(pdMS_TO_TICKS(10));
    }
    g_prev = m;
    g_have_prev = true;
}
static void t_light_sleep(slot_t *s) {
    if (g_have_prev) {
        static const char *const kState[] = {"?", "armed: reset during the sleep, never woke", "woke: USB did not come back",
                                             "recover: a CPU reset after waking"};
        mark_clear();
        emit(s, "\"ok\":false,\"previous\":\"%s\",\"detail\":%lu,\"slept_ms\":%lu,\"causes\":\"%lx\",\"reset_reason_now\":%d",
             g_prev.state < 4 ? kState[g_prev.state] : "?", (unsigned long)g_prev.detail, (unsigned long)g_prev.slept_ms,
             (unsigned long)g_prev.causes, (int)esp_reset_reason());
        return;
    }
    int init = ink_s31_sleep_init();
    mark_write(MARK_ARMED, 0, 0, 0);
    fuse_arm(20000);
    esp_sleep_enable_timer_wakeup(2000000);
    printf("HWTEST_SLEEPING 2 s\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(300)); /* the console drains */
    int64_t t0 = esp_timer_get_time();
    int rc = ink_s31_light_sleep();
    int64_t slept = (esp_timer_get_time() - t0) / 1000;
    uint32_t causes = esp_sleep_get_wakeup_causes();
    fuse_arm(20000); /* a fresh 20 s for what follows the wake */
    mark_write(MARK_WOKE, (uint32_t)rc, (uint32_t)slept, causes);
    usb_serial_jtag_ll_enable_bus_clock(true);
    usb_serial_jtag_ll_phy_enable_pad(false);
    vTaskDelay(pdMS_TO_TICKS(150));
    usb_serial_jtag_ll_phy_set_defaults();
    usb_serial_jtag_ll_phy_enable_pad(true);
    int waited = 0;
    while (waited < 3000 && !usb_serial_jtag_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(20));
        waited += 20;
    }
    vTaskDelay(pdMS_TO_TICKS(2000)); /* the computer opens the port again */
    bool by_timer = (causes & BIT(ESP_SLEEP_WAKEUP_TIMER)) != 0, back = usb_serial_jtag_is_connected();
    s->ok = init == ESP_OK && rc == ESP_OK && by_timer && back;
    emit(s, "\"ok\":%s,\"init\":%d,\"rc\":%d,\"slept_ms\":%lld,\"causes\":\"%lx\",\"by_timer\":%s,\"usb_back_after_ms\":%d,"
            "\"usb_connected\":%s",
         tf(s->ok), init, rc, (long long)slept, (unsigned long)causes, tf(by_timer), waited, tf(back));
    if (back) { /* only then: otherwise the fuse fires and the next boot reports what this one could not say */
        fuse_off();
        mark_clear();
    }
}
#endif

/* ------------------------------------------------------------------ the runner */

static const test_t kSafety = {"safety", t_safety, 3000};
#if HWTEST_SLEEP
static const test_t kTests[] = {{"sys", t_sys, 2000}, {"light_sleep", t_light_sleep, 30000}};
#else
static const test_t kTests[] = {
    {"sys", t_sys, 2000},         {"heap", t_heap, 2000},     {"psram", t_psram, 3000},      {"flash", t_flash, 2000},
    {"timing", t_timing, 3000},   {"gpio", t_gpio, 2000},     {"i2c_scan", t_i2c_scan, 5000}, {"expander", t_expander, 2000},
    {"touch", t_touch, 2000},     {"gauge", t_gauge, 2000},   {"charger", t_charger, 2000},  {"rtc", t_rtc, 2000},
    {"accel", t_accel, 2000},     {"pmic", t_pmic, 2000},     {"card", t_card, 4000},        {"modem", t_modem, 10000},
    {"wifi", t_wifi, 15000},      {"accel_axes", t_accel_axes, 3000}, {"rtc_int", t_rtc_int, 6000}, {"accel_int", t_accel_int, 3000},
    {"flash_read", t_flash_read, 5000}, {"current", t_current, 40000},
};
#endif
#define N_TESTS (sizeof kTests / sizeof kTests[0])
_Static_assert(N_TESTS <= 24, "slot table");
static slot_t g_slots[N_TESTS + 1]; /* never reused: a test given up on may still write into its own */

static void worker(void *arg) {
    slot_t *s = arg;
    s->t->fn(s);
    taskENTER_CRITICAL(&g_mux);
    s->done = true;
    taskEXIT_CRITICAL(&g_mux);
    vTaskDelete(NULL);
}

/* Runs one test on its own task; true when it finished within its time. Its lines are said then. */
static bool run(slot_t *s, const test_t *t, int64_t deadline) {
    char line[160];
    s->t = t;
    int64_t left = deadline - now_ms();
    if (left <= 0) {
        snprintf(line, sizeof line, "{\"test\":\"%s\",\"ok\":false,\"error\":\"skipped: run out of time\"}", t->name);
        say(line);
        return false;
    }
    int64_t limit = t->timeout_ms < left ? t->timeout_ms : left;
    int64_t start = now_ms();
    if (xTaskCreate(worker, t->name, WORKER_STACK, s, 5, NULL) != pdPASS) {
        snprintf(line, sizeof line, "{\"test\":\"%s\",\"ok\":false,\"error\":\"no task\"}", t->name);
        say(line);
        return false;
    }
    bool done = false;
    while (!done && now_ms() - start < limit) {
        vTaskDelay(pdMS_TO_TICKS(10));
        taskENTER_CRITICAL(&g_mux);
        done = s->done;
        if (!done && now_ms() - start >= limit) s->gone = true;
        taskEXIT_CRITICAL(&g_mux);
    }
    if (!done) {
        snprintf(line, sizeof line, "{\"test\":\"%s\",\"ok\":false,\"error\":\"timeout\",\"limit_ms\":%lld}", t->name, (long long)limit);
        say(line);
        return false;
    }
    for (size_t at = 0; at < s->n;) {
        const char *end = memchr(s->buf + at, '\n', s->n - at);
        size_t n = end ? (size_t)(end - (s->buf + at)) : s->n - at;
        char one[LINE_CAP];
        snprintf(one, sizeof one, "%.*s", (int)n, s->buf + at);
        say(one);
        at += n + 1;
    }
    return s->ok;
}

void app_main(void) {
    esp_log_level_set("*", ESP_LOG_WARN);
    g_said = heap_caps_malloc(SAID_CAP, MALLOC_CAP_SPIRAM);
#if HWTEST_SLEEP
    sleep_boot_check();
#endif
    printf("HWTEST_START\n");
    int64_t start = now_ms();
    bool safe = run(&g_slots[N_TESTS], &kSafety, start + TOTAL_MS) && g_safe;
    unsigned failed = 0, timeouts = 0;
    if (!safe) {
        say("SAFETY_FAIL");
    } else {
        vTaskDelay(pdMS_TO_TICKS(2000)); /* time for the computer to open the port */
        for (size_t i = 0; i < N_TESTS; i++) {
            bool ok = run(&g_slots[i], &kTests[i], start + TOTAL_MS);
            failed += !ok;
            timeouts += !ok && !g_slots[i].done;
        }
    }
    char line[160];
    snprintf(line, sizeof line, "{\"test\":\"summary\",\"ok\":%s,\"safe\":%s,\"tests\":%u,\"failed\":%u,\"not_finished\":%u,\"ms\":%lld}",
             tf(safe && !failed), tf(safe), safe ? (unsigned)N_TESTS : 0u, failed, timeouts, (long long)(now_ms() - start));
    say(line);
    for (;;) { /* idle, awake, and say everything again for a computer that came late */
        printf("HWTEST_REPLAY\n");
        for (size_t at = 0; g_said && at < g_said_n;) {
            const char *end = memchr(g_said + at, '\n', g_said_n - at);
            int n = (int)(end - (g_said + at));
            printf("%.*s\n", n, g_said + at);
            at += (size_t)n + 1;
        }
        printf("HWTEST_DONE\n");
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
