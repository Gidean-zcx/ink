#ifndef INK_VENDOR_TEST_IDF_H
#define INK_VENDOR_TEST_IDF_H
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_TIMEOUT 1
#define ESP_ERR_INVALID_RESPONSE 2
#define ESP_ERR_NOT_FOUND 3
#define ESP_ERR_INVALID_STATE 4
#define ESP_ERR_INVALID_ARG 5
#define ESP_ERR_NO_MEM 6
#define IRAM_ATTR
#define ESP_ERROR_CHECK(x) assert((x) == ESP_OK)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
typedef int esp_err_t;
const char *esp_err_to_name(int e);
typedef int gpio_num_t;
#define GPIO_MODE_INPUT 0
#define GPIO_MODE_OUTPUT 1
#define GPIO_MODE_INPUT_OUTPUT_OD 2
#define GPIO_PULLUP_ENABLE 1
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE 0
#define GPIO_INTR_NEGEDGE 1
#define GPIO_INTR_ANYEDGE 2
#define GPIO_NUM_NC -1
#define GPIO_NUM_0 0
#define GPIO_NUM_1 1
#define GPIO_NUM_2 2
#define GPIO_NUM_3 3
#define GPIO_NUM_4 4
#define GPIO_NUM_5 5
#define GPIO_NUM_6 6
#define GPIO_NUM_7 7
#define GPIO_NUM_8 8
#define GPIO_NUM_9 9
#define GPIO_NUM_10 10
#define GPIO_NUM_11 11
#define GPIO_NUM_12 12
#define GPIO_NUM_13 13
#define GPIO_NUM_14 14
#define GPIO_NUM_15 15
#define GPIO_NUM_16 16
#define GPIO_NUM_17 17
#define GPIO_NUM_18 18
#define GPIO_NUM_19 19
#define GPIO_NUM_20 20
#define GPIO_NUM_21 21
#define GPIO_NUM_22 22
#define GPIO_NUM_23 23
#define GPIO_NUM_24 24
#define GPIO_NUM_25 25
#define GPIO_NUM_26 26
#define GPIO_NUM_27 27
#define GPIO_NUM_28 28
#define GPIO_NUM_29 29
#define GPIO_NUM_30 30
#define GPIO_NUM_31 31
#define GPIO_NUM_32 32
#define GPIO_NUM_33 33
#define GPIO_NUM_34 34
#define GPIO_NUM_35 35
#define GPIO_NUM_36 36
#define GPIO_NUM_37 37
#define GPIO_NUM_38 38
#define GPIO_NUM_39 39
#define GPIO_NUM_40 40
#define GPIO_NUM_41 41
#define GPIO_NUM_42 42
#define GPIO_NUM_43 43
#define GPIO_NUM_44 44
#define GPIO_NUM_45 45
#define GPIO_NUM_46 46
#define GPIO_NUM_47 47
#define GPIO_NUM_48 48
#define GPIO_NUM_49 49
#define GPIO_NUM_50 50
#define GPIO_NUM_51 51
#define GPIO_NUM_52 52
#define GPIO_NUM_53 53
#define GPIO_NUM_54 54
#define GPIO_NUM_55 55
#define GPIO_NUM_56 56
#define GPIO_NUM_57 57
#define GPIO_NUM_58 58
#define GPIO_NUM_59 59
#define GPIO_NUM_60 60
#define GPIO_NUM_61 61
typedef struct { uint64_t pin_bit_mask; int mode, pull_up_en, pull_down_en, intr_type; } gpio_config_t;
esp_err_t gpio_config(const gpio_config_t *c);
esp_err_t gpio_set_level(int pin, int level);
int gpio_get_level(int pin);
esp_err_t gpio_reset_pin(int pin);
esp_err_t gpio_intr_disable(int pin);
esp_err_t gpio_wakeup_disable(int pin);
esp_err_t gpio_set_intr_type(int pin, int type);
esp_err_t gpio_hold_dis(int pin);
typedef struct { struct { unsigned int_ena; } pin[64]; } gpio_dev_t;
extern gpio_dev_t GPIO;
void gpio_ll_clear_intr_status(gpio_dev_t *g, uint32_t mask);
void gpio_ll_clear_intr_status_high(gpio_dev_t *g, uint32_t mask);
#define UHCI_LL_NUM 2
#define PERIPH_RCC_ATOMIC() for (int ink_once = 1; ink_once; ink_once = 0)
void uhci_ll_enable_bus_clock(int i, bool on);
void uhci_ll_reset_register(int i);
void esp_rom_delay_us(uint32_t us);
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef struct { bool held; void *owner; } StaticSemaphore_t;
typedef StaticSemaphore_t *SemaphoreHandle_t;
typedef void *TaskHandle_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define taskENTER_CRITICAL(p) portENTER_CRITICAL(p)
#define taskEXIT_CRITICAL(p) portEXIT_CRITICAL(p)
#define portMAX_DELAY UINT32_MAX
#define configTICK_RATE_HZ 1000
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define eSetBits 0
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *s);
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks);
BaseType_t xSemaphoreGive(SemaphoreHandle_t s);
TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t s);
TickType_t xTaskGetTickCount(void);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
void vTaskDelay(TickType_t ticks);
BaseType_t xTaskCreatePinnedToCore(void (*fn)(void *), const char *name, unsigned stack, void *arg, int priority, TaskHandle_t *out, int core);
BaseType_t xTaskNotify(TaskHandle_t t, uint32_t bits, int mode);
BaseType_t xTaskNotifyWait(uint32_t clear, uint32_t end, uint32_t *bits, TickType_t ticks);
typedef struct ink_test_i2c_bus *i2c_master_bus_handle_t;
typedef struct ink_test_i2c_dev *i2c_master_dev_handle_t;
typedef int i2c_port_t;
#define I2C_NUM_1 1
#define I2C_CLK_SRC_DEFAULT 0
#define I2C_ADDR_BIT_LEN_7 0
#define I2C_LL_INTR_MASK 0xffffffffu
#define I2C_LL_GET_HW(n) (n)
typedef struct { int i2c_port, sda_io_num, scl_io_num, clk_source, glitch_ignore_cnt, intr_priority, trans_queue_depth; struct { bool enable_internal_pullup; } flags; } i2c_master_bus_config_t;
typedef struct { int dev_addr_length; uint16_t device_address; uint32_t scl_speed_hz, scl_wait_us; } i2c_device_config_t;
typedef struct { const uint8_t *write_buffer; size_t buffer_size; } i2c_master_transmit_multi_buffer_info_t;
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *c, i2c_master_bus_handle_t *b);
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t b);
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t b, const i2c_device_config_t *c, i2c_master_dev_handle_t *d);
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t d);
esp_err_t i2c_master_get_bus_handle(int p, i2c_master_bus_handle_t *b);
esp_err_t i2c_master_probe(i2c_master_bus_handle_t b, uint8_t a, int ms);
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t d, const uint8_t *p, size_t n, int ms);
esp_err_t i2c_master_receive(i2c_master_dev_handle_t d, uint8_t *p, size_t n, int ms);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t d, const uint8_t *w, size_t wn, uint8_t *r, size_t rn, int ms);
esp_err_t i2c_master_multi_buffer_transmit(i2c_master_dev_handle_t d, const i2c_master_transmit_multi_buffer_info_t *b, size_t n, int ms);
void i2c_ll_disable_intr_mask(int hw, uint32_t m);
void i2c_ll_clear_intr_mask(int hw, uint32_t m);
#define LEDC_TIMER_10_BIT 10
#define LEDC_LOW_SPEED_MODE 0
#define LEDC_TIMER_0 0
#define LEDC_CHANNEL_0 0
#define LEDC_CHANNEL_1 1
#define LEDC_AUTO_CLK 0
#define LEDC_INTR_DISABLE 0
typedef int ledc_channel_t;
typedef struct { int speed_mode, duty_resolution, timer_num, freq_hz, clk_cfg; } ledc_timer_config_t;
typedef struct { int gpio_num, speed_mode, channel, intr_type, timer_sel, duty, hpoint; struct { int output_invert; } flags; } ledc_channel_config_t;
esp_err_t ledc_timer_config(const ledc_timer_config_t *c);
esp_err_t ledc_channel_config(const ledc_channel_config_t *c);
esp_err_t ledc_set_duty(int mode, ledc_channel_t channel, uint32_t duty);
esp_err_t ledc_update_duty(int mode, ledc_channel_t channel);
#define ESP_TIMER_TASK 0
typedef struct { void (*callback)(void *); void *arg; int dispatch_method; const char *name; bool skip_unhandled_events; } esp_timer_create_args_t;
typedef struct ink_test_timer *esp_timer_handle_t;
esp_err_t esp_timer_create(const esp_timer_create_args_t *a, esp_timer_handle_t *t);
esp_err_t esp_timer_stop(esp_timer_handle_t t);
esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t us);
int64_t esp_timer_get_time(void);
#endif
