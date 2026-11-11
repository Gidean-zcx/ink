/* Boot is reached from hal_device_reset_reason before assets, storage or IRQ owners. */
#include "main/boards/metalio-e-ink4-plus/config.h"
#include "hal/gpio_ll.h"
#include "hal/uhci_ll.h"
#include "esp_private/periph_ctrl.h"
#include "esp_log.h"
#define TAG "epd_eink4p"
#define CFG_INTR IO_EXPANDER_INT_GPIO
#define CFG_SDA I2C_SDA_PIN
#define CFG_SCL I2C_SCL_PIN
static const gpio_num_t k_epd_bus_pins[] = {
    EPD_PIN_D0, EPD_PIN_D1, EPD_PIN_D2, EPD_PIN_D3, EPD_PIN_D4, EPD_PIN_D5, EPD_PIN_D6, EPD_PIN_D7,
    EPD_PIN_XCL, EPD_PIN_XLE, EPD_PIN_XOE, EPD_PIN_XSTL, EPD_PIN_SPV, EPD_PIN_CKV, EPD_PIN_MODE, EPD_PIN_BORDER,
};
static void MetalioSd_ReleaseHold(void) {
    const gpio_num_t pins[] = {SDMMC_CLK_PIN, SDMMC_CMD_PIN, SDMMC_D0_PIN, SDMMC_D1_PIN, SDMMC_D2_PIN, SDMMC_D3_PIN};
    for (size_t i = 0; i < sizeof pins / sizeof pins[0]; ++i) gpio_hold_dis(pins[i]);
}
#include "boot.inc"
#undef TAG
