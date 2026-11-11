#ifndef INK_CELL_TEST_IDF_H
#define INK_CELL_TEST_IDF_H
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef int esp_err_t;
typedef int uart_port_t;
typedef const char *esp_event_base_t;
typedef void (*esp_event_handler_t)(void *, esp_event_base_t, int32_t, void *);
typedef struct { int baud_rate, data_bits, parity, stop_bits, flow_ctrl, source_clk; } uart_config_t;
typedef struct { uint64_t pin_bit_mask; int mode, intr_type; } gpio_config_t;
typedef struct esp_netif { bool connected; } esp_netif_t;
typedef struct { esp_netif_t *esp_netif; } ip_event_got_ip_t;
typedef struct { unsigned flags; const char *if_key, *if_desc; int route_prio; } esp_netif_inherent_config_t;
typedef struct { void *handle; esp_err_t (*transmit)(void *, void *, size_t); void (*driver_free_rx_buffer)(void *, void *); } esp_netif_driver_ifconfig_t;
typedef struct { const esp_netif_inherent_config_t *base; const esp_netif_driver_ifconfig_t *driver; const void *stack; } esp_netif_config_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE -2
#define UART_NUM_2 2
#define UART_DATA_8_BITS 8
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1 1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT 0
#define UART_PIN_NO_CHANGE -1
#define GPIO_MODE_OUTPUT 2
#define GPIO_MODE_INPUT 1
#define GPIO_INTR_ANYEDGE 3
#define EXT_RAM_BSS_ATTR
#define ESP_NETIF_FLAG_GARP 1u
#define ESP_NETIF_INHERENT_DEFAULT_ETH() { .flags = 3 }
#define ESP_NETIF_NETSTACK_DEFAULT_ETH NULL
#define IP_EVENT "ip"
#define IP_EVENT_ETH_GOT_IP 1
#define IP_EVENT_ETH_LOST_IP 2
#define ESP_MAC_ETH 1
#define ESP_ERROR_CHECK(x) assert((x) == ESP_OK)
esp_err_t gpio_config(const gpio_config_t *);
esp_err_t gpio_set_level(int, int);
esp_err_t gpio_pullup_en(int);
int gpio_get_level(int);
esp_err_t gpio_install_isr_service(int);
esp_err_t gpio_isr_handler_add(int, void (*)(void *), void *);
esp_err_t uart_driver_install(int, int, int, int, void *, int);
esp_err_t uart_param_config(int, const uart_config_t *);
esp_err_t uart_set_pin(int, int, int, int, int);
int uart_read_bytes(int, void *, size_t, unsigned);
int uart_write_bytes(int, const void *, size_t);
esp_err_t uart_flush_input(int);
void esp_rom_delay_us(unsigned);
esp_err_t esp_read_mac(uint8_t *, int);
esp_netif_t *esp_netif_new(const esp_netif_config_t *);
esp_err_t esp_netif_set_mac(esp_netif_t *, uint8_t *);
esp_err_t esp_netif_set_default_netif(esp_netif_t *);
void esp_netif_action_start(void *, esp_event_base_t, int32_t, void *);
void esp_netif_action_connected(void *, esp_event_base_t, int32_t, void *);
void esp_netif_action_disconnected(void *, esp_event_base_t, int32_t, void *);
esp_err_t esp_netif_receive(esp_netif_t *, void *, size_t, void *);
esp_err_t esp_event_handler_register(esp_event_base_t, int32_t, esp_event_handler_t, void *);
#endif
