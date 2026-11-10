/*
 * Hardware self-test for the Metalio E-Ink4 Plus (ESP32-S31), run from ota_1 with nobody at the device.
 * One JSON object a line on the USB console, then HWTEST_DONE; after that it only idles and says all
 * results again every few seconds (the computer may reattach late), never sleeping.
 *
 * Safety first: the TPS65185's WAKEUP, PWRUP and VCOM_CTRL (TCA9555 P00-P02) are driven low and read
 * back; if that cannot be proven, SAFETY_FAIL and nothing else runs. Otherwise everything only reads:
 * the panel is not driven, the card is only detected, NVS, flash and eFuses are not written, Wi-Fi only
 * scans and reports a count, Bluetooth and the front light are not touched, the charger, gauge, clock,
 * accelerometer and touch chips are only read. The modem gets the read-only questions in kAsk and
 * nothing else; its raw answer is logged as hex with long digit runs masked. No MAC, serial number,
 * IMEI, ICCID or network name is printed. Each test has its own time limit and the run has one too.
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
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
#include "esp_psram.h"
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
#define TOTAL_MS 90000     /* the whole run; the computer waits 120 s */
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
    s->ok = st == HAL_I2C_OK;
    emit(s, "\"ok\":%s,\"status\":\"%s\",\"part\":\"%02x\",\"charge_state\":%d,\"vbus_state\":%d,\"charge_current_mA\":%d",
         tf(s->ok), hal_i2c_status_name(st), part, (int)chg, vbus, code * CX25601N_ICHG_STEP_MA);
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

/* Scan only and say how many: no connection, no names, no addresses, nothing kept in NVS. */
static void t_wifi(slot_t *s) {
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("phy", ESP_LOG_ERROR);
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = 0;
    esp_err_t e = esp_wifi_init(&cfg);
    if (e == ESP_OK) e = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (e == ESP_OK) e = esp_wifi_set_mode(WIFI_MODE_STA);
    if (e == ESP_OK) e = esp_wifi_start();
    uint16_t found = 0;
    int64_t t = now_ms();
    if (e == ESP_OK) e = esp_wifi_scan_start(NULL, true);
    if (e == ESP_OK) e = esp_wifi_scan_get_ap_num(&found);
    int64_t ms = now_ms() - t;
    esp_wifi_clear_ap_list();
    esp_wifi_stop();
    esp_wifi_deinit();
    s->ok = e == ESP_OK;
    emit(s, "\"ok\":%s,\"err\":%d,\"scan_ms\":%lld,\"networks\":%u", tf(s->ok), e, (long long)ms, (unsigned)found);
}

/* ------------------------------------------------------------------ the runner */

static const test_t kSafety = {"safety", t_safety, 3000};
static const test_t kTests[] = {
    {"sys", t_sys, 2000},         {"heap", t_heap, 2000},     {"psram", t_psram, 3000},      {"flash", t_flash, 2000},
    {"timing", t_timing, 3000},   {"gpio", t_gpio, 2000},     {"i2c_scan", t_i2c_scan, 5000}, {"expander", t_expander, 2000},
    {"touch", t_touch, 2000},     {"gauge", t_gauge, 2000},   {"charger", t_charger, 2000},  {"rtc", t_rtc, 2000},
    {"accel", t_accel, 2000},     {"pmic", t_pmic, 2000},     {"card", t_card, 4000},        {"modem", t_modem, 10000},
    {"wifi", t_wifi, 15000},
};
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
