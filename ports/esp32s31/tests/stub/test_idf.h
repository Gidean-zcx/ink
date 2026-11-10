#ifndef S31_TEST_IDF_H
#define S31_TEST_IDF_H
#include <stdbool.h>
#include <stdint.h>
typedef int esp_err_t;
typedef int gpio_num_t;
typedef void *esp_pm_lock_handle_t;
typedef struct { int max_freq_mhz, min_freq_mhz; bool light_sleep_enable; } esp_pm_config_t;
#define CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ 320
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_PM_CPU_FREQ_MAX 0
#define ESP_SLEEP_WAKEUP_ALL 1
#define ESP_SLEEP_WAKEUP_EXT1 3
#define ESP_SLEEP_WAKEUP_TIMER 4
#define ESP_SLEEP_WAKEUP_GPIO 7
#define ESP_EXT1_WAKEUP_ANY_LOW 0
#define ESP_EXT1_WAKEUP_ANY_HIGH 1
#define GPIO_INTR_LOW_LEVEL 4
#define GPIO_MODE_INPUT 1
esp_err_t gpio_set_direction(int pin, int mode);
esp_err_t gpio_pullup_en(int pin);
esp_err_t gpio_pulldown_dis(int pin);
void usb_serial_jtag_ll_enable_bus_clock(bool on);
void usb_serial_jtag_ll_phy_enable_pad(bool on);
void usb_serial_jtag_ll_phy_set_defaults(void);
bool usb_serial_jtag_is_connected(void);
esp_err_t esp_pm_configure(const void *cfg);
esp_err_t esp_pm_lock_create(int type, int arg, const char *name, esp_pm_lock_handle_t *out);
esp_err_t esp_pm_lock_acquire(esp_pm_lock_handle_t lock);
esp_err_t esp_pm_lock_release(esp_pm_lock_handle_t lock);
esp_err_t esp_sleep_disable_wakeup_source(int source);
esp_err_t esp_sleep_disable_ext1_wakeup_io(uint64_t pins);
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t us);
esp_err_t esp_sleep_enable_ext1_wakeup_io(uint64_t pins, int level);
esp_err_t esp_sleep_enable_gpio_wakeup(void);
esp_err_t esp_light_sleep_start(void);
uint32_t esp_sleep_get_wakeup_causes(void);
uint64_t esp_sleep_get_ext1_wakeup_status(void);
esp_err_t gpio_wakeup_enable(int pin, int level);
esp_err_t gpio_wakeup_disable(int pin);
esp_err_t rtc_gpio_deinit(int pin);
int gpio_get_level(int pin);
void esp_restart(void);
void vTaskSuspendAll(void);
int xTaskResumeAll(void);
#endif
