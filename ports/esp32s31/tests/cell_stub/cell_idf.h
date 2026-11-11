#ifndef INK_CELL_TEST_IDF_H
#define INK_CELL_TEST_IDF_H
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int esp_err_t;
typedef int uart_port_t;
typedef int gpio_num_t;
typedef const char *esp_event_base_t;
typedef void (*esp_event_handler_t)(void *, esp_event_base_t, int32_t, void *);
typedef void *esp_event_handler_instance_t;
typedef struct { int baud_rate, data_bits, parity, stop_bits, flow_ctrl, rx_flow_ctrl_thresh, source_clk; } uart_config_t;
typedef struct { uint64_t pin_bit_mask; int mode, pull_up_en, pull_down_en, intr_type; } gpio_config_t;
typedef struct esp_netif { bool connected; } esp_netif_t;
typedef struct { unsigned addr; } esp_ip4_addr_t;
typedef struct { esp_ip4_addr_t ip, netmask, gw; } esp_netif_ip_info_t;
typedef struct { esp_netif_t *esp_netif; esp_netif_ip_info_t ip_info; } ip_event_got_ip_t;
typedef unsigned esp_netif_flags_t;
typedef struct { unsigned flags; const char *if_key, *if_desc; int route_prio; } esp_netif_inherent_config_t;
typedef struct { void *handle; esp_err_t (*transmit)(void *, void *, size_t); void (*driver_free_rx_buffer)(void *, void *); } esp_netif_driver_ifconfig_t;
typedef struct { const esp_netif_inherent_config_t *base; const esp_netif_driver_ifconfig_t *driver; const void *stack; } esp_netif_config_t;
typedef struct { esp_err_t (*post_attach)(esp_netif_t *, void *); esp_netif_t *netif; } esp_netif_driver_base_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_NOT_SUPPORTED 0x106
#define UART_NUM_1 1
#define UART_NUM_2 2
#define UART_DATA_8_BITS 8
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1 1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT 0
#define UART_PIN_NO_CHANGE -1
#define GPIO_NUM_NC -1
#define GPIO_NUM_60 60
#define GPIO_MODE_OUTPUT 2
#define GPIO_MODE_INPUT 1
#define GPIO_INTR_DISABLE 0
#define GPIO_INTR_ANYEDGE 3
#define GPIO_INTR_NEGEDGE 4
#define GPIO_INTR_LOW_LEVEL 5
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_PULLUP_ONLY 1
#define IRAM_ATTR
#define EXT_RAM_BSS_ATTR
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_DMA 2
#define ESP_NETIF_FLAG_GARP 1u
#define ESP_NETIF_INHERENT_DEFAULT_ETH() { .flags = 3, .if_key = "ETH", .if_desc = "eth", .route_prio = 50 }
extern const esp_netif_config_t ink_test_eth_config;
#define ESP_NETIF_DEFAULT_ETH() ink_test_eth_config
extern esp_event_base_t IP_EVENT;
extern int GPIO;
#define gpio_ll_intr_disable(hw, pin) ((void)(hw), (void)(pin))
#define IP_EVENT_ETH_GOT_IP 1
#define IP_EVENT_ETH_LOST_IP 2
#define ESP_EVENT_ANY_ID -1
#define ESP_EVENT_DECLARE_BASE(name) extern esp_event_base_t name
#define ESP_EVENT_DEFINE_BASE(name) esp_event_base_t name = #name
#define ESP_MAC_ETH 1
#define ESP_ERROR_CHECK(x) assert((x) == ESP_OK)
#define ESP_RETURN_ON_ERROR(x,t,f,...) do { esp_err_t ink_ret = (x); if (ink_ret != ESP_OK) return ink_ret; } while (0)
#define ESP_RETURN_ON_FALSE(x,r,t,f,...) do { if (!(x)) return (r); } while (0)
#define ESP_GOTO_ON_FALSE(x,r,l,t,f,...) do { if (!(x)) { ret = (r); goto l; } } while (0)
#define ESP_LOGE(t,f,...) ink_test_log(f, ##__VA_ARGS__)
#define ESP_LOGW(t,f,...) ink_test_log(f, ##__VA_ARGS__)
#define ESP_LOGI(t,f,...) ink_test_log(f, ##__VA_ARGS__)
#define ESP_LOGD(t,f,...) ((void)0)
#define IPSTR "%u"
#define IP2STR(x) ((x)->addr)
#ifndef __containerof
#define __containerof(p,t,m) ((t *)((char *)(p) - offsetof(t,m)))
#endif
void ink_test_log(const char *, ...);
const char *esp_err_to_name(esp_err_t);
void *heap_caps_malloc(size_t, unsigned);
esp_err_t gpio_config(const gpio_config_t *);
esp_err_t gpio_set_level(int, int);
int gpio_get_level(int);
esp_err_t gpio_install_isr_service(int);
esp_err_t gpio_isr_handler_add(int, void (*)(void *), void *);
esp_err_t gpio_isr_handler_remove(int);
esp_err_t gpio_set_pull_mode(int, int);
esp_err_t gpio_wakeup_enable(int, int);
esp_err_t gpio_wakeup_disable(int);
esp_err_t gpio_sleep_sel_dis(int);
esp_err_t gpio_sleep_sel_en(int);
esp_err_t gpio_intr_disable(int);
esp_err_t gpio_intr_enable(int);
esp_err_t gpio_set_intr_type(int, int);
esp_err_t gpio_reset_pin(int);
esp_err_t uart_param_config(int, const uart_config_t *);
esp_err_t uart_set_pin(int, int, int, int, int);
esp_err_t uart_set_baudrate(int, uint32_t);
void esp_rom_delay_us(unsigned);
int64_t esp_timer_get_time(void);
esp_err_t esp_read_mac(uint8_t *, int);
esp_netif_t *esp_netif_new(const esp_netif_config_t *);
void esp_netif_destroy(esp_netif_t *);
esp_err_t esp_netif_set_mac(esp_netif_t *, uint8_t *);
esp_err_t esp_netif_set_default_netif(esp_netif_t *);
esp_err_t esp_netif_set_driver_config(esp_netif_t *, const esp_netif_driver_ifconfig_t *);
esp_err_t esp_netif_attach(esp_netif_t *, void *);
esp_err_t esp_netif_dhcpc_stop(esp_netif_t *);
void esp_netif_action_start(void *, esp_event_base_t, int32_t, void *);
void esp_netif_action_stop(void *, esp_event_base_t, int32_t, void *);
void esp_netif_action_connected(void *, esp_event_base_t, int32_t, void *);
void esp_netif_action_disconnected(void *, esp_event_base_t, int32_t, void *);
esp_err_t esp_netif_receive(esp_netif_t *, void *, size_t, void *);
esp_err_t esp_event_post(esp_event_base_t, int32_t, const void *, size_t, unsigned);
esp_err_t esp_event_handler_register(esp_event_base_t, int32_t, esp_event_handler_t, void *);
esp_err_t esp_event_handler_unregister(esp_event_base_t, int32_t, esp_event_handler_t);
esp_err_t esp_event_handler_instance_register(esp_event_base_t, int32_t, esp_event_handler_t, void *, esp_event_handler_instance_t *);
esp_err_t esp_event_handler_instance_unregister(esp_event_base_t, int32_t, esp_event_handler_instance_t);
void dns_clear_cache(void);
int tcpip_callback(void (*)(void *), void *);
typedef struct ink_test_queue *QueueHandle_t;
typedef struct ink_test_event *EventGroupHandle_t;
typedef struct ink_test_sem *SemaphoreHandle_t;
typedef void *TaskHandle_t;
typedef uint32_t TickType_t;
typedef uint32_t EventBits_t;
typedef int BaseType_t;
#define pdFALSE 0
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(x) ((TickType_t)(x))
#define portYIELD_FROM_ISR() ((void)0)
QueueHandle_t xQueueCreate(unsigned, unsigned);
void vQueueDelete(QueueHandle_t);
BaseType_t xQueueSend(QueueHandle_t, const void *, TickType_t);
BaseType_t xQueueReceive(QueueHandle_t, void *, TickType_t);
BaseType_t xQueueSendFromISR(QueueHandle_t, const void *, BaseType_t *);
EventGroupHandle_t xEventGroupCreate(void);
void vEventGroupDelete(EventGroupHandle_t);
EventBits_t xEventGroupSetBits(EventGroupHandle_t, EventBits_t);
EventBits_t xEventGroupClearBits(EventGroupHandle_t, EventBits_t);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t, EventBits_t, BaseType_t, BaseType_t, TickType_t);
BaseType_t xEventGroupSetBitsFromISR(EventGroupHandle_t, EventBits_t, BaseType_t *);
SemaphoreHandle_t xSemaphoreCreateBinary(void);
void vSemaphoreDelete(SemaphoreHandle_t);
BaseType_t xSemaphoreGive(SemaphoreHandle_t);
BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t);
BaseType_t xTaskCreate(void (*)(void *), const char *, unsigned, void *, unsigned, TaskHandle_t *);
void vTaskDelete(TaskHandle_t);
void vTaskDelay(TickType_t);
TickType_t xTaskGetTickCount(void);
#ifdef __cplusplus
}
#endif
#endif
