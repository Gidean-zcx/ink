/*
 * Metalio E-Ink4 Plus on the ESP32-S31: GPIO numbers that only the device needs (the simulator wires
 * chips by I2C address, not by pin). Upstream CloudZao/Metalio-E-INK4-Plus @690a023; sources in the
 * port-hal-1 report. Pins not checked on this unit are marked so.
 */
#ifndef INK_S31_BOARD_H
#define INK_S31_BOARD_H

#include <stdbool.h>

/* Shared I2C1 bus: GPIO0 SDA, GPIO1 SCL, 100 kHz (upstream metalio_i2c1.c; tools/hw-bench scanned it). */
#define S31_I2C_PORT 1
#define S31_I2C_SDA 0
#define S31_I2C_SCL 1
#define S31_I2C_HZ 100000
#define S31_I2C_TIMEOUT_MS 50 /* upstream passes 50 ms to its expander transfers (as hal_i2c_sim.c) */

/* Interrupt lines, active low, falling edge (upstream config.h, metalio_touch.c). */
#define S31_GPIO_EXP_INT 2   /* TCA9555 INT wired with the SC7A20H INT1 */
#define S31_GPIO_TOUCH_INT 5 /* FT6336U INT */
#define S31_GPIO_AI_KEY 61   /* BOOT key, simulator/profiles/metalio-plus.json */
#define S31_GPIO_MODEM_SRDY 3 /* NT26 SRDY; wake polarity remains an on-device check */

/* Input/clock defaults from simulator/profiles/metalio-plus.json. */
#define S31_PANEL_ROTATION 90
#define S31_KEY_DEBOUNCE_US 20000u
#define S31_TOUCH_POLL_MS 10
#define S31_IDLE_MHZ 160

/* hal_uart ports: 1 = BH1098G (UART1, TX39 RX38, 115200), 2 = NT26 (UART2, TX58 RX59). */
#define S31_UART1_TX 39
#define S31_UART1_RX 38
#define S31_UART2_TX 58
#define S31_UART2_RX 59

/* Front light: GPIO6 cold, GPIO7 warm into the YX6016N, LEDC 20 kHz, 10 bit (upstream frontlight.cc). */
#define S31_LIGHT_COLD 6
#define S31_LIGHT_WARM 7
#define S31_LIGHT_HZ 20000

typedef void (*s31_irq_fn)(void *ctx);
/** Call fn on the port's interrupt task after each falling edge of gpio, and again while the line
 *  stays low after a service. Boot, task context, before the line can fire. 0 ok. */
int s31_irq_watch(int gpio, s31_irq_fn fn, void *ctx);

/** v2.0.77 flash drain (main/display/epd_nvs_guard.cc, main/boards/common/storage/sd_log_writer.cc): around each
 *  flash read, write or erase, wait for the I2C1 transfer in flight and quiet the bus interrupts
 *  (metalio_i2c1_lock_for_flash), so no transfer straddles the window where the flash operation turns the cache
 *  off. Lock order as the vendor's: display flash guard -> I2C -> flash. Returns whether the bus lock was taken:
 *  false when this task already holds the bus (synchronous transfers, so none is in flight); pass it to the end. */
bool s31_flash_begin(void);
void s31_flash_end(bool taken);

#endif
