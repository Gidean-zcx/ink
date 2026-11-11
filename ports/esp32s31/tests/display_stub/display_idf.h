#ifndef INK_DISPLAY_TEST_IDF_H
#define INK_DISPLAY_TEST_IDF_H
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
typedef struct test_sem *SemaphoreHandle_t;
typedef struct test_task *TaskHandle_t;
typedef int BaseType_t;
typedef unsigned UBaseType_t;
#define IRAM_ATTR
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define configMAX_PRIORITIES 25
#define pdMS_TO_TICKS(ms) ((uint32_t)(ms) / 10)
#define configASSERT(x) assert(x)
#define portYIELD_FROM_ISR(x) ((void)(x))
#define GPIO_MODE_OUTPUT 1
SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateBinary(void);
int xSemaphoreTake(SemaphoreHandle_t sem, uint32_t timeout);
void xSemaphoreGive(SemaphoreHandle_t sem);
void xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t *woken);
int xTaskCreatePinnedToCore(void (*fn)(void *), const char *name, unsigned stack, void *arg, unsigned priority, TaskHandle_t *out, int core);
void xTaskNotifyGive(TaskHandle_t task);
unsigned ulTaskNotifyTake(int clear, uint32_t ticks);
void vTaskDelay(uint32_t ticks);
void vTaskPrioritySet(TaskHandle_t task, unsigned priority);
uint32_t xTaskGetTickCount(void);
int xPortGetCoreID(void);
void taskYIELD(void);
void gpio_reset_pin(int pin);
void gpio_set_direction(int pin, int direction);
void gpio_set_level(int pin, int level);
void esp_rom_delay_us(unsigned us);
typedef struct { int data[16], clock, ckv, start_pulse, leh, stv; } lcd_bus_config_t;
typedef struct {
    size_t pixel_clock;
    int ckv_high_time, line_front_porch, le_high_time, bus_width;
    lcd_bus_config_t bus;
    int dummy_clocks_after_data, white_border_pixels;
} LcdEpdConfig_t;
void epd_lcd_init(const LcdEpdConfig_t *cfg, int w, int h);
int epd_lcd_get_display_lines(void);
void epd_lcd_line_source_cb(bool (*fn)(void *, uint8_t *), void *ctx);
void epd_lcd_frame_done_cb(void (*fn)(void *), void *ctx);
void epd_lcd_start_frame(void);
void epd_lcd_abort_frame(void);
typedef struct { int size; atomic_int current, last; uint8_t **bufs; size_t element_size; } LineQueue_t;
LineQueue_t lq_init(int size, int bytes);
uint8_t *lq_current(LineQueue_t *q);
void lq_commit(LineQueue_t *q);
int lq_read(LineQueue_t *q, uint8_t *out);
void lq_reset(LineQueue_t *q);
#define EPD_PIN_D0 8
#define EPD_PIN_D1 9
#define EPD_PIN_D2 10
#define EPD_PIN_D3 11
#define EPD_PIN_D4 12
#define EPD_PIN_D5 13
#define EPD_PIN_D6 14
#define EPD_PIN_D7 15
#define EPD_PIN_XCL 16
#define EPD_PIN_XLE 17
#define EPD_PIN_XOE 18
#define EPD_PIN_MODE 19
#define EPD_PIN_XSTL 4
#define EPD_PIN_CKV 40
#define EPD_PIN_SPV 35
#define EPD_PIN_BORDER 37
#define EPD_VCOM_MV 1200
#define DISPLAY_WHITE_BORDER_PIXELS 0
#endif
