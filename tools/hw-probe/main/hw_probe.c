/*
 * hw-probe: phase-1 measurement firmware for the Metalio E-Ink4 Plus (docs/hardware/README.md).
 *
 * 1. Reports the hardware: chip, CPU clock, PSRAM, flash, reset reason, running slot and every
 *    internal/DMA/PSRAM heap region.
 * 2. Runs the iss calibration kernels (tools/iss/kernels) on the real ESP32-S31, pinned to core 1,
 *    so tools/hw-probe/compare.py can set their cycle counts against the instruction-set simulator.
 *
 * It touches nothing else: no Wi-Fi/BT/PHY, no NVS, no I2C (panel power, modem and keys stay as
 * the factory firmware left them), no flash writes (core dump off). Output lines start with
 * "HWPROBE" on the console (UART0 and the USB Serial/JTAG).
 */
#include <inttypes.h>
#include <stdio.h>

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_clk_tree.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_ota_ops.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

int k_feed_main(void);  /* tools/iss/kernels/k_feed.c, main() renamed */
int kernels_main(void); /* tools/iss/kernels/kmain.c, main() renamed */

static void report_heap(const char *name, uint32_t caps) {
    printf("HWPROBE heap %s total=%u free=%u largest=%u min_free=%u\n", name,
           (unsigned)heap_caps_get_total_size(caps), (unsigned)heap_caps_get_free_size(caps),
           (unsigned)heap_caps_get_largest_free_block(caps), (unsigned)heap_caps_get_minimum_free_size(caps));
}

static void report_hw(void) {
    esp_chip_info_t ci;
    esp_chip_info(&ci);
    uint32_t cpu_hz = 0;
    esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &cpu_hz);
    printf("HWPROBE chip model=%d revision=%d cores=%d features=0x%08" PRIx32 " cpu_hz=%" PRIu32 "\n", (int)ci.model,
           ci.revision, ci.cores, ci.features, cpu_hz);

    uint32_t flash_id = 0, flash_bytes = 0;
    esp_flash_read_id(NULL, &flash_id);
    esp_flash_get_size(NULL, &flash_bytes);
    printf("HWPROBE flash id=0x%06" PRIx32 " bytes=%" PRIu32 "\n", flash_id, flash_bytes);
    printf("HWPROBE psram bytes=%u\n", (unsigned)esp_psram_get_size());

    const esp_partition_t *run = esp_ota_get_running_partition();
    const esp_app_desc_t *app = esp_app_get_description();
    printf("HWPROBE boot reset_reason=%d running=%s@0x%" PRIx32 " app=%s idf=%s\n", (int)esp_reset_reason(),
           run ? run->label : "?", run ? run->address : 0, app->version, app->idf_ver);

    report_heap("internal", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    report_heap("dma", MALLOC_CAP_DMA);
    report_heap("psram", MALLOC_CAP_SPIRAM);
    heap_caps_print_heap_info(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); /* every region, like heap_init */
}

static void probe_task(void *arg) {
    (void)arg;
    report_hw();
    printf("HWPROBE feed begin\n");
    k_feed_main();
    printf("HWPROBE kernels begin\n");
    int bad = kernels_main();
    printf("HWPROBE done bad=%d\n", bad);
    vTaskDelete(NULL);
}

void app_main(void) {
    /* core 1 keeps the kernels away from the system tasks on core 0 */
    xTaskCreatePinnedToCore(probe_task, "probe", 32 * 1024, NULL, 5, NULL, 1);
}
