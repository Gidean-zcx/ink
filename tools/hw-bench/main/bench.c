/*
 * Measures the device and says so on the console, one "BENCH {json}" line per result, ending with
 * BENCH_DONE. Runs from ota_1 with nobody at the device. It only reads, apart from:
 *  - the last 256 KiB of ota_1, erased and written for the flash timing, left erased;
 *  - the RTC's timer, for a few seconds, put back as it was;
 *  - the panel, showing test pictures through the factory driver.
 * It asks the modem only questions. It never opens NVS and prints no MAC, IMEI, ICCID or Wi-Fi name.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_partition.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ink/sys/sha256.h"
#ifdef INK_BENCH_EPD
#include "metalio_i2c1.h"
#include "metalio_epd.h"
#include "epdiy.h"
#include "epd_highlevel.h"
#if __has_include("bench_images.h")
#include "bench_images.h"
#endif
#endif

/* What has been said, to say again at the end: the USB link can die when the panel starts (second run,
 * 2026-11-06), and the results should not die with it. Each line also gets time to leave. */
#define SAID_CAP (32 * 1024)
static char *g_said;
static size_t g_said_n;

__attribute__((format(printf, 1, 2))) static void R(const char *fmt, ...) {
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n >= (int) sizeof line) n = sizeof line - 1;
    printf("BENCH {%s}\n", line);
    fflush(stdout);
    if (g_said && g_said_n + (size_t) n + 1 < SAID_CAP) {
        memcpy(g_said + g_said_n, line, (size_t) n);
        g_said_n += (size_t) n;
        g_said[g_said_n++] = '\n';
    }
    vTaskDelay(pdMS_TO_TICKS(20));
}
#define SDA 0
#define SCL 1
#define TEST_REGION (256 * 1024)
#define PIN_ITERATIONS 3000 /* as in system/policy/members.c */

/* system/util/sha256.c runs under the OSAL elsewhere */
void osal_cost_units(const char *what, uint64_t units) { (void) what; (void) units; }
void osal_preempt_point(void) {}

/* The factory driver expects this from its card code, which is not built here. */
void MetalioSd_ReleaseHold(void) {
    for (int pin = 20; pin <= 25; pin++) gpio_hold_dis(pin);
}

static int64_t now_us(void) { return esp_timer_get_time(); }

static void heap(const char *when) {
    R("\"test\":\"heap\",\"when\":\"%s\",\"internal_free\":%u,\"internal_largest\":%u,\"internal_min\":%u,"
      "\"dma_free\":%u,\"dma_largest\":%u,\"psram_free\":%u,\"psram_largest\":%u",
      when, (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
      (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
      (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
      (unsigned) heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
      (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
      (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
      (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
}

static void sys_info(void) {
    esp_chip_info_t c;
    esp_chip_info(&c);
    uint32_t flash = 0, jedec = 0;
    esp_flash_get_size(NULL, &flash);
    esp_flash_read_id(NULL, &jedec);
    R("\"test\":\"sys\",\"idf\":\"%s\",\"revision\":%d,\"cores\":%d,\"cpu_mhz\":%d,\"flash_bytes\":%u,\"flash_id\":\"%06x\","
      "\"psram_bytes\":%u,\"reset_reason\":%d",
      esp_get_idf_version(), c.revision, c.cores, CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, (unsigned) flash, (unsigned) jedec,
      (unsigned) esp_psram_get_size(), (int) esp_reset_reason());
    heap("boot");
}

static void copy_speed(const char *name, uint32_t dst_caps, uint32_t src_caps, size_t len) {
    uint8_t *d = heap_caps_malloc(len, dst_caps), *s = heap_caps_malloc(len, src_caps);
    if (d && s) {
        memset(s, 0x5a, len);
        int64_t t = now_us();
        for (int i = 0; i < 8; i++) memcpy(d, s, len);
        double mbps = 8.0 * len / (now_us() - t);
        R("\"test\":\"memcpy\",\"path\":\"%s\",\"MBps\":%.1f", name, mbps);
    }
    heap_caps_free(d);
    heap_caps_free(s);
}

static void flash_speed(void) {
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    if (!p || p->size < TEST_REGION) {
        R("\"test\":\"flash\",\"error\":\"no ota_1\"");
        return;
    }
    uint32_t base = p->address + p->size - TEST_REGION; /* the only place this firmware writes */
    static uint8_t page[4096];
    memset(page, 0xa5, sizeof page);
    int64_t emin = INT64_MAX, emax = 0, esum = 0, wsum = 0, wmax = 0, rsum = 0;
    int n = 16, err = 0;
    for (int i = 0; i < n; i++) {
        uint32_t a = base + i * 4096u;
        err |= esp_flash_erase_region(NULL, a, 4096); /* a blank sector erases almost at once: not what is measured */
        int64_t t = now_us();
        err |= esp_flash_write(NULL, page, a, sizeof page);
        int64_t w = now_us() - t;
        t = now_us();
        err |= esp_flash_erase_region(NULL, a, 4096);
        int64_t e = now_us() - t;
        err |= esp_flash_write(NULL, page, a, sizeof page);
        t = now_us();
        err |= esp_flash_read(NULL, page, a, sizeof page);
        rsum += now_us() - t;
        emin = e < emin ? e : emin;
        emax = e > emax ? e : emax;
        esum += e;
        wsum += w;
        wmax = w > wmax ? w : wmax;
    }
    int64_t t = now_us();
    err |= esp_flash_erase_region(NULL, base, TEST_REGION);
    R("\"test\":\"flash\",\"err\":%d,\"erase_4k_us_min\":%lld,\"erase_4k_us_avg\":%lld,\"erase_4k_us_max\":%lld,"
      "\"write_4k_us_avg\":%lld,\"write_4k_us_max\":%lld,\"read_4k_us_avg\":%lld,\"erase_256k_us\":%lld",
      err, emin, esum / n, emax, wsum / n, wmax, rsum / n, now_us() - t);
}

static void crypto_speed(void) {
    static uint8_t data[64 * 1024];
    uint8_t out[32], salt[16] = {1, 2, 3};
    int64_t t = now_us();
    ink_sha256(data, sizeof data, out);
    double mbps = (double) sizeof data / (now_us() - t);
    t = now_us();
    ink_pbkdf2_sha256("1234", 4, salt, sizeof salt, PIN_ITERATIONS, out);
    R("\"test\":\"crypto\",\"sha256_MBps\":%.2f,\"pbkdf2_iterations\":%d,\"pbkdf2_ms\":%.1f", mbps, PIN_ITERATIONS,
      (now_us() - t) / 1000.0);
}

/* ---------------------------------------------------------------- the I2C bus */
#ifndef INK_BENCH_EPD
static i2c_master_bus_handle_t g_bus;
static i2c_master_bus_handle_t bus(void) {
    if (!g_bus) {
        i2c_master_bus_config_t c = {.i2c_port = I2C_NUM_1, .sda_io_num = SDA, .scl_io_num = SCL, .clk_source = I2C_CLK_SRC_DEFAULT,
                                     .glitch_ignore_cnt = 7, .flags.enable_internal_pullup = true};
        if (i2c_new_master_bus(&c, &g_bus) != ESP_OK) g_bus = NULL;
    }
    return g_bus;
}
#else
static i2c_master_bus_handle_t bus(void) { /* the factory driver's bus: it owns port 1 */
    static i2c_master_bus_handle_t b;
    if (!b && metalio_i2c1_init(SDA, SCL) == ESP_OK) i2c_master_get_bus_handle(I2C_NUM_1, &b);
    return b;
}
#endif

static int rd(uint8_t addr, uint8_t reg, uint8_t *out, size_t n) {
    i2c_master_dev_handle_t d;
    i2c_device_config_t c = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = addr, .scl_speed_hz = 100000};
    if (i2c_master_bus_add_device(bus(), &c, &d) != ESP_OK) return -1;
    int e = i2c_master_transmit_receive(d, &reg, 1, out, n, 100);
    i2c_master_bus_rm_device(d);
    return e == ESP_OK ? 0 : -1;
}

static int wr(uint8_t addr, uint8_t reg, uint8_t v) {
    i2c_master_dev_handle_t d;
    i2c_device_config_t c = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = addr, .scl_speed_hz = 100000};
    if (i2c_master_bus_add_device(bus(), &c, &d) != ESP_OK) return -1;
    uint8_t b[2] = {reg, v};
    int e = i2c_master_transmit(d, b, 2, 100);
    i2c_master_bus_rm_device(d);
    return e == ESP_OK ? 0 : -1;
}

static void i2c_scan(void) {
    char found[160] = "";
    int n = 0;
    for (int a = 0x08; a < 0x78; a++)
        if (bus() && i2c_master_probe(bus(), a, 30) == ESP_OK) {
            size_t l = strlen(found);
            snprintf(found + l, sizeof found - l, "%s\"%02x\"", n++ ? "," : "", a);
        }
    R("\"test\":\"i2c\",\"addresses\":[%s]", found);
    uint8_t b[4] = {0};
    if (!rd(0x38, 0xA3, b, 1)) R("\"test\":\"chip\",\"name\":\"ft6336u\",\"chip_id\":\"%02x\"", b[0]);
    if (!rd(0x68, 0x10, b, 1)) R("\"test\":\"chip\",\"name\":\"tps65185\",\"version\":\"%02x\"", b[0]);
    if (!rd(0x19, 0x0F, b, 1) || !rd(0x18, 0x0F, b, 1)) R("\"test\":\"chip\",\"name\":\"sc7a20h\",\"who_am_i\":\"%02x\"", b[0]);
    uint8_t s[7];
    if (!rd(0x51, 0x02, s, 7)) R("\"test\":\"chip\",\"name\":\"pcf8563\",\"clock_valid\":%s", s[0] & 0x80 ? "false" : "true");
    uint8_t v[2], c[2], t[2], soc[2];
    if (!rd(0x55, 0x08, v, 2) && !rd(0x55, 0x0C, c, 2) && !rd(0x55, 0x06, t, 2) && !rd(0x55, 0x2C, soc, 2))
        R("\"test\":\"battery\",\"mV\":%u,\"mA\":%d,\"celsius\":%.1f,\"percent\":%u", v[0] | v[1] << 8,
          (int16_t) (c[0] | c[1] << 8), (t[0] | t[1] << 8) / 10.0 - 273.15, soc[0] | soc[1] << 8);
    uint8_t st;
    if (!rd(0x6B, 0x0B, &st, 1)) R("\"test\":\"chip\",\"name\":\"cx25601n\",\"status\":\"%02x\"", st);
    uint8_t in[2];
    if (!rd(0x20, 0x00, in, 2)) R("\"test\":\"chip\",\"name\":\"tca9555\",\"inputs\":\"%02x%02x\"", in[1], in[0]);
}

/* Does the RTC's interrupt reach the IO expander (P16)? The timer is put back afterwards. */
static void rtc_line(void) {
    uint8_t ctrl2 = 0, tc = 0, tv = 0, in[2] = {0};
    if (rd(0x51, 0x01, &ctrl2, 1) || rd(0x51, 0x0E, &tc, 1) || rd(0x51, 0x0F, &tv, 1)) {
        R("\"test\":\"rtc_line\",\"error\":\"no rtc\"");
        return;
    }
    rd(0x20, 0x01, in, 1);
    int before = (in[0] >> 6) & 1, fired = -1, seen = -1;
    wr(0x51, 0x0E, 0x00);
    wr(0x51, 0x0F, 2);                         /* 2 s */
    wr(0x51, 0x01, (ctrl2 & ~0x04) | 0x01);    /* TIE on, TF cleared */
    wr(0x51, 0x0E, 0x82);                      /* enable, 1 Hz */
    int64_t t = now_us();
    while (now_us() - t < 5000000) {
        rd(0x20, 0x01, in, 1);
        uint8_t c = 0;
        rd(0x51, 0x01, &c, 1);
        if (c & 0x04 && fired < 0) fired = (int) ((now_us() - t) / 1000);
        if (!((in[0] >> 6) & 1) != !before && seen < 0) seen = (int) ((now_us() - t) / 1000);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    wr(0x51, 0x0E, tc);
    wr(0x51, 0x0F, tv);
    wr(0x51, 0x01, ctrl2 & ~0x04);
    R("\"test\":\"rtc_line\",\"p16_before\":%d,\"timer_fired_ms\":%d,\"p16_changed_ms\":%d", before, fired, seen);
}

static void wifi_cost(void) {
    size_t before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = 0; /* nothing is written to NVS */
    int e = esp_wifi_init(&cfg);
    if (e == ESP_OK) e = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (e == ESP_OK) e = esp_wifi_set_mode(WIFI_MODE_STA);
    int64_t t = now_us();
    if (e == ESP_OK) e = esp_wifi_start();
    int64_t start_ms = (now_us() - t) / 1000;
    size_t after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    uint16_t found = 0;
    int best = -127;
    t = now_us();
    if (e == ESP_OK && esp_wifi_scan_start(NULL, true) == ESP_OK) {
        esp_wifi_scan_get_ap_num(&found);
        wifi_ap_record_t rec;
        uint16_t one = 1;
        if (found && esp_wifi_scan_get_ap_records(&one, &rec) == ESP_OK) best = rec.rssi;
    }
    R("\"test\":\"wifi\",\"err\":%d,\"internal_cost\":%d,\"start_ms\":%lld,\"scan_ms\":%lld,\"networks\":%u,\"best_rssi\":%d",
      e, (int) before - (int) after, start_ms, (now_us() - t) / 1000, found, best);
    heap("wifi_on");
    esp_wifi_stop();
    esp_wifi_deinit();
    heap("wifi_off");
}

/* Questions only: no setting of the modem is touched and no identity is asked for. */
static void modem(void) {
    static const char *const ask[] = {"AT", "ATI", "AT+CGMR", "AT+CPIN?", "AT+CSQ", "AT+CREG?", "AT+CEREG?", "AT+COPS?", "AT+CIREG?"};
    uart_config_t c = {.baud_rate = 115200, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE,
                       .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT};
    if (uart_driver_install(1, 1024, 0, 0, NULL, 0) != ESP_OK || uart_param_config(1, &c) != ESP_OK ||
        uart_set_pin(1, 58, 59, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        R("\"test\":\"modem\",\"error\":\"uart\"");
        return;
    }
    /* the factory board wakes the modem's UART with a ready line */
    gpio_set_direction(60, GPIO_MODE_OUTPUT);
    gpio_set_level(60, 1);
    gpio_set_direction(3, GPIO_MODE_INPUT);
    vTaskDelay(pdMS_TO_TICKS(100));
    R("\"test\":\"modem_lines\",\"srdy\":%d", gpio_get_level(3));
    char buf[192];
    for (size_t i = 0; i < sizeof ask / sizeof *ask; i++) {
        uart_flush_input(1);
        uart_write_bytes(1, ask[i], strlen(ask[i]));
        uart_write_bytes(1, "\r\n", 2);
        int n = uart_read_bytes(1, buf, sizeof buf - 1, pdMS_TO_TICKS(i ? 800 : 1500));
        buf[n > 0 ? n : 0] = 0;
        for (char *p = buf; *p; p++) if (*p == '\r' || *p == '\n' || *p == '"' || *p == '\\') *p = ' ';
        R("\"test\":\"modem\",\"ask\":\"%s\",\"answer\":\"%s\"", ask[i] + (ask[i][2] == '+' ? 3 : 0), buf);
        if (!i && n <= 0) break;
    }
    uart_driver_delete(1);
}

/* Reads sectors, no file system, nothing written, no names seen. */
void MetalioSd_ReleaseHold(void);

static void card(void) {
    MetalioSd_ReleaseHold();
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = 24; slot.cmd = 25; slot.d0 = 20; slot.d1 = 21; slot.d2 = 22; slot.d3 = 23;
    sdmmc_card_t c;
    int e = sdmmc_host_init();
    if (e == ESP_OK) e = sdmmc_host_init_slot(host.slot, &slot);
    if (e == ESP_OK) e = sdmmc_card_init(&host, &c);
    if (e != ESP_OK) {
        R("\"test\":\"card\",\"err\":%d", e);
        sdmmc_host_deinit();
        return;
    }
    size_t len = 2 * 1024 * 1024;
    uint8_t *b = heap_caps_malloc(32 * 1024, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    int64_t t = now_us();
    size_t got = 0;
    while (b && got < len && sdmmc_read_sectors(&c, b, got / 512, 64) == ESP_OK) got += 32 * 1024;
    R("\"test\":\"card\",\"err\":0,\"size_mib\":%llu,\"read_kBps\":%.0f,\"max_khz\":%d",
      (unsigned long long) c.csd.capacity * c.csd.sector_size / 1048576, got * 1000.0 / (now_us() - t), c.max_freq_khz);
    heap_caps_free(b);
    sdmmc_host_deinit();
}

#ifdef INK_BENCH_EPD
static int64_t show(EpdiyHighlevelState *hl, const char *what, enum EpdDrawMode mode, int temp) {
    int64_t t = now_us();
    int e = epd_hl_update_screen(hl, mode, temp);
    int64_t ms = (now_us() - t) / 1000;
    R("\"test\":\"panel\",\"what\":\"%s\",\"err\":%d,\"ms\":%lld", what, e, ms);
    return ms;
}

static void ramp(uint8_t *fb) {
    int w = epd_rotated_display_width(), h = epd_rotated_display_height();
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) epd_draw_pixel(x, y, (uint8_t) (((y * 16 / h) & 15) * 17), fb);
}

/* Which way are touch and gravity? The person holds the device upright and taps numbered dots.
 * Reads the two chips only; the accelerometer goes back to what it was doing. */
static void touch_and_gravity(void) {
    uint8_t was = 0, a[6], t[5];
    bool accel = !rd(0x19, 0x20, &was, 1) && !wr(0x19, 0x20, 0x57);
    bool down = false;
    int64_t end = now_us() + 40 * 1000000, next_gravity = 0;
    while (now_us() < end) {
        if (!rd(0x38, 0x02, t, 5)) {
            bool pressed = (t[0] & 0x0f) != 0;
            if (pressed && !down) R("\"test\":\"touch\",\"x\":%d,\"y\":%d", (t[1] & 0x0f) << 8 | t[2], (t[3] & 0x0f) << 8 | t[4]);
            down = pressed;
        }
        if (accel && now_us() > next_gravity && !rd(0x19, 0x28 | 0x80, a, 6)) {
            R("\"test\":\"gravity\",\"x\":%d,\"y\":%d,\"z\":%d", (int16_t) (a[1] << 8 | a[0]) >> 4, (int16_t) (a[3] << 8 | a[2]) >> 4,
              (int16_t) (a[5] << 8 | a[4]) >> 4);
            next_gravity = now_us() + 5000000;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (accel) wr(0x19, 0x20, was);
}

static void panel(void) {
    size_t before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    int64_t t = now_us();
    bool ok = MetalioEpd_Init();
    R("\"test\":\"panel_init\",\"ok\":%s,\"ms\":%lld,\"internal_cost\":%d", ok ? "true" : "false", (now_us() - t) / 1000,
      (int) before - (int) heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    heap("panel_on");
    if (!ok) return;
    EpdiyHighlevelState *hl = MetalioEpd_Hl();
    epd_set_rotation(EPD_ROT_INVERTED_PORTRAIT); /* the way the device is held: the first run was upside down */
    uint8_t *fb = epd_hl_get_framebuffer(hl);
    int temp = 25;
    epd_poweron();
    temp = (int) epd_ambient_temperature();
    R("\"test\":\"panel_temp\",\"celsius\":%d", temp);
    t = now_us();
    epd_fullclear(hl, temp);
    R("\"test\":\"panel\",\"what\":\"full_clear\",\"err\":0,\"ms\":%lld", (now_us() - t) / 1000);
    ramp(fb);
    show(hl, "gray_ramp_gc16", MODE_GC16, temp);
    vTaskDelay(pdMS_TO_TICKS(5000));
    /* a small area changing, as a button press would */
    EpdRect box = {.x = 60, .y = 60, .width = 120, .height = 120};
    for (int i = 0; i < 4; i++) {
        epd_fill_rect(box, i & 1 ? 0xff : 0x00, fb);
        show(hl, "button_du", MODE_DU, temp);
    }
#ifdef INK_BENCH_IMAGES
    for (int i = 0; i < INK_BENCH_IMAGES; i++) {
        const uint8_t *img = BENCH_IMAGES[i].data;
        for (int y = 0; y < 1216; y++)
            for (int x = 0; x < 684; x++) {
                uint8_t v = img[(y * 684 + x) / 2];
                epd_draw_pixel(x, y, (uint8_t) (((x + y * 684) & 1 ? v & 15 : v >> 4) * 17), fb);
            }
        char what[32];
        snprintf(what, sizeof what, "image_%s", BENCH_IMAGES[i].name);
        /* Settings comes after the home screen without a flash, as a page turn does: how much really stays behind? */
        show(hl, what, strcmp(BENCH_IMAGES[i].name, "Settings") ? MODE_GC16 : MODE_GL16, temp);
        if (i == 0) touch_and_gravity(); /* the first picture has the dots */
        else vTaskDelay(pdMS_TO_TICKS(20000)); /* long enough to look at */
    }
#endif
    ramp(fb); /* what stays on the panel for a look afterwards */
    show(hl, "gray_ramp_final", MODE_GC16, temp);
    epd_poweroff();
    heap("panel_done");
}
#endif

static void battery_again(const char *when) {
    uint8_t c[2];
    if (!rd(0x55, 0x0C, c, 2)) R("\"test\":\"battery_load\",\"when\":\"%s\",\"mA\":%d", when, (int16_t) (c[0] | c[1] << 8));
}

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(500));
    g_said = heap_caps_malloc(SAID_CAP, MALLOC_CAP_SPIRAM);
    printf("BENCH_START\n");
    sys_info();
    copy_speed("internal_to_internal", MALLOC_CAP_INTERNAL, MALLOC_CAP_INTERNAL, 16 * 1024);
    copy_speed("psram_to_psram", MALLOC_CAP_SPIRAM, MALLOC_CAP_SPIRAM, 256 * 1024);
    copy_speed("psram_to_internal", MALLOC_CAP_INTERNAL, MALLOC_CAP_SPIRAM, 16 * 1024);
    crypto_speed();
    flash_speed();
    i2c_scan();
    rtc_line();
    battery_again("idle");
    card();
    modem();
    wifi_cost();
    battery_again("after_wifi");
#ifdef INK_BENCH_EPD
    panel();
    battery_again("after_panel");
#else
    R("\"test\":\"panel_init\",\"ok\":false,\"error\":\"built without the panel driver\"");
#endif
    heap("end");
    for (;;) { /* until the computer has it: the panel is idle now, restoring may interrupt this at any point */
        printf("BENCH_REPLAY\n");
        for (size_t at = 0; at < g_said_n;) {
            const char *end = memchr(g_said + at, '\n', g_said_n - at);
            int n = (int) (end - (g_said + at));
            printf("BENCH {%.*s}\n", n, g_said + at);
            at += (size_t) n + 1;
        }
        printf("BENCH_DONE\n");
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(8000));
    }
}
