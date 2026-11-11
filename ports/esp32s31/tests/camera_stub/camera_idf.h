#ifndef INK_CAMERA_TEST_IDF_H
#define INK_CAMERA_TEST_IDF_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef struct { bool mutex, held, signalled; } StaticSemaphore_t;
typedef StaticSemaphore_t *SemaphoreHandle_t;
typedef int portMUX_TYPE;
typedef int BaseType_t;
typedef int esp_err_t;
#define IRAM_ATTR
#define ESP_OK 0
#define pdTRUE 1
#define pdFALSE 0
#define portMUX_INITIALIZER_UNLOCKED 0
#define taskENTER_CRITICAL(p) ((void)(p))
#define taskEXIT_CRITICAL(p) ((void)(p))
#define pdMS_TO_TICKS(ms) ((uint32_t)(ms))
#define GPIO_NUM_NC (-1)
#define CAM_CTLR_DATA_WIDTH_8 8
#define CAM_CLK_SRC_DEFAULT 0
#define CAM_CTLR_COLOR_RGB565 1
#define ESP_CAM_CTLR_DVP_DATA_SIG_NUM 16
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_DMA 2
typedef struct test_camera *esp_cam_ctlr_handle_t;
typedef struct { void *buffer; size_t buflen, received_size; } esp_cam_ctlr_trans_t;
typedef bool (*test_camera_cb)(esp_cam_ctlr_handle_t, esp_cam_ctlr_trans_t *, void *);
typedef struct { test_camera_cb on_get_new_trans, on_trans_finished; } esp_cam_ctlr_evt_cbs_t;
typedef struct { int data_width, data_io[16], vsync_io, de_io, pclk_io, xclk_io; } esp_cam_ctlr_dvp_pin_config_t;
typedef struct {
    int clk_src, h_res, v_res, input_data_color_type, output_data_color_type, cam_data_width;
    bool external_xtal; uint32_t xclk_freq, dma_burst_size;
    const esp_cam_ctlr_dvp_pin_config_t *pin;
} esp_cam_ctlr_dvp_config_t;
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *s);
SemaphoreHandle_t xSemaphoreCreateBinary(void);
int xSemaphoreTake(SemaphoreHandle_t s, uint32_t ticks);
void xSemaphoreGive(SemaphoreHandle_t s);
void xSemaphoreGiveFromISR(SemaphoreHandle_t s, BaseType_t *wake);
void vSemaphoreDelete(SemaphoreHandle_t s);
void vTaskDelay(uint32_t ticks);
void esp_rom_delay_us(unsigned us);
void gpio_reset_pin(int pin);
void heap_caps_free(void *p);
esp_err_t esp_cam_new_dvp_ctlr(const esp_cam_ctlr_dvp_config_t *c, esp_cam_ctlr_handle_t *out);
void *esp_cam_ctlr_alloc_buffer(esp_cam_ctlr_handle_t c, size_t n, uint32_t caps);
esp_err_t esp_cam_ctlr_register_event_callbacks(esp_cam_ctlr_handle_t c, const esp_cam_ctlr_evt_cbs_t *cb, void *arg);
esp_err_t esp_cam_ctlr_enable(esp_cam_ctlr_handle_t c);
esp_err_t esp_cam_ctlr_start(esp_cam_ctlr_handle_t c);
esp_err_t esp_cam_ctlr_stop(esp_cam_ctlr_handle_t c);
esp_err_t esp_cam_ctlr_disable(esp_cam_ctlr_handle_t c);
esp_err_t esp_cam_ctlr_del(esp_cam_ctlr_handle_t c);
#endif
