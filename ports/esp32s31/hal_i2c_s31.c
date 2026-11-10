/*
 * hal_i2c on ESP-IDF i2c_master. HAL bus 0 is the board's shared I2C1 (s31_board.h). The bus is
 * created with the first device; i2c_master serialises transfers on one bus itself.
 */
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ink/hal_i2c.h"
#include "s31_board.h"
#include "s31_hal_map.h"

#define MAX_DEVS 16

struct hal_i2c_dev {
    int bus;
    uint8_t addr;
    i2c_master_dev_handle_t h;
};

static struct hal_i2c_dev g_devs[MAX_DEVS];
static int g_n;
static i2c_master_bus_handle_t g_bus;
static SemaphoreHandle_t g_lock;
static StaticSemaphore_t g_lock_buf;
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

static void lock(void) {
    taskENTER_CRITICAL(&g_mux);
    if (!g_lock) g_lock = xSemaphoreCreateMutexStatic(&g_lock_buf);
    taskEXIT_CRITICAL(&g_mux);
    xSemaphoreTake(g_lock, portMAX_DELAY);
}

hal_i2c_dev_t *hal_i2c_device(int bus, uint8_t addr7, const char *name) {
    (void)name;
    if (bus != 0) return NULL;
    hal_i2c_dev_t *d = NULL;
    lock();
    for (int i = 0; i < g_n && !d; i++)
        if (g_devs[i].bus == bus && g_devs[i].addr == addr7) d = &g_devs[i];
    if (!d && !g_bus) {
        i2c_master_bus_config_t c = {.i2c_port = S31_I2C_PORT, .sda_io_num = S31_I2C_SDA, .scl_io_num = S31_I2C_SCL,
                                     .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7,
                                     .flags.enable_internal_pullup = true};
        if (i2c_new_master_bus(&c, &g_bus) != ESP_OK) g_bus = NULL;
    }
    if (!d && g_bus && g_n < MAX_DEVS) {
        i2c_device_config_t c = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = addr7, .scl_speed_hz = S31_I2C_HZ};
        struct hal_i2c_dev *n = &g_devs[g_n];
        if (i2c_master_bus_add_device(g_bus, &c, &n->h) == ESP_OK) {
            n->bus = bus;
            n->addr = addr7;
            d = n;
            g_n++;
        }
    }
    xSemaphoreGive(g_lock);
    return d;
}

hal_i2c_status_t hal_i2c_write(hal_i2c_dev_t *dev, const uint8_t *data, size_t len) {
    if (!dev) return HAL_I2C_ERR;
    /* No data: an address-only probe, as the simulator's bus does (tps65185_probe); i2c_master_transmit refuses
     * 0 bytes, so on the device the panel PMIC was never found (trial smoke 3, 2026-11-10). NACK is NOT_FOUND. */
    if (!len) {
        esp_err_t e = i2c_master_probe(g_bus, dev->addr, S31_I2C_TIMEOUT_MS);
        return e == ESP_ERR_NOT_FOUND ? HAL_I2C_NACK : s31_i2c_status(e);
    }
    return s31_i2c_status(i2c_master_transmit(dev->h, data, len, S31_I2C_TIMEOUT_MS));
}

hal_i2c_status_t hal_i2c_write_read(hal_i2c_dev_t *dev, const uint8_t *wdata, size_t wlen, uint8_t *rdata,
                                    size_t rlen) {
    if (!dev || (!wlen && !rlen)) return HAL_I2C_ERR;
    if (!rlen) return hal_i2c_write(dev, wdata, wlen);
    if (!wlen) return s31_i2c_status(i2c_master_receive(dev->h, rdata, rlen, S31_I2C_TIMEOUT_MS));
    return s31_i2c_status(i2c_master_transmit_receive(dev->h, wdata, wlen, rdata, rlen, S31_I2C_TIMEOUT_MS));
}

const char *hal_i2c_status_name(hal_i2c_status_t s) {
    switch (s) {
    case HAL_I2C_OK: return "ok";
    case HAL_I2C_NACK: return "nack";
    case HAL_I2C_TIMEOUT: return "timeout";
    case HAL_I2C_ERR: return "error";
    }
    return "?";
}
