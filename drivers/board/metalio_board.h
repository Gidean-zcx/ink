/*
 * Metalio E-Ink4 Plus board: the TCA9555 IO expander and what hangs on it (config.h @690a023).
 * One expander instance and one lock, shared by the panel power sequence, the keys, the motor, the
 * amplifier, the shared rails and the power-off pulse.
 *
 * Port 0: P00 VCOM_CTRL, P01 PWRUP, P02 WAKEUP (TPS65185), P03 motor, P04 amplifier (NS4150),
 *         P05 camera/screen power; P07 input TPS65185 PWR_GOOD.
 * Port 1: P10 VOL+, P11 VOL-, P17 POWER (inputs, low = pressed), P16 RTC INT (input, low = event,
 *         board V1.4); P12 touch + 4G shared reset (low = both held in reset), P13 power-off pulse
 *         (default low), P14 4G + Bluetooth audio + amplifier supply.
 * The expander's INT goes to GPIO2, wired with the accelerometer's INT1.
 */
#ifndef INK_METALIO_BOARD_H
#define INK_METALIO_BOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "ink/hal_i2c.h"
#include "tca9555/tca9555.h"

#define MB_P0_MOTOR 0x08      /* vibration motor, high = on */
#define MB_P0_AMP 0x10        /* NS4150 amplifier enable, high = on */
#define MB_P0_CAM_SCR 0x20    /* camera / screen power, high = on (default on) */
#define MB_P1_VOL_UP 0x01
#define MB_P1_VOL_DOWN 0x02
#define MB_P1_SHARED_RST 0x04 /* touch + 4G reset, high = run */
#define MB_P1_PWR_PULSE 0x08  /* power-off pulse to the power chip */
#define MB_P1_SHARED_PWR 0x10 /* 4G + Bluetooth audio + amplifier supply, high = on */
#define MB_P1_RTC_INT 0x40
#define MB_P1_POWER_KEY 0x80
#define MB_P1_KEYS (MB_P1_VOL_UP | MB_P1_VOL_DOWN | MB_P1_POWER_KEY)

#define MB_POWER_PULSES 15        /* upstream PWR_PULSE_COUNT */
#define MB_POWER_PULSE_MS 100     /* each high and each low phase (PWR_PULSE_INTERVAL_MS) */
#define MB_MOTOR_PULSE_MS 50      /* upstream VIBRATION_MOTOR_PULSE_MS */

/** Set up the expander with the official directions and power-on levels (screen power, shared
 *  rails and reset high; motor, amplifier and power-off pulse low). Task context, once. */
hal_i2c_status_t metalio_board_init(void);
bool metalio_board_ready(void);
/** The shared instance, for drivers that take one (panel power). Hold the lock around its use. */
tca9555_t *metalio_ioexp(void);
void metalio_board_lock(void);
void metalio_board_unlock(void);
/** Output bits of a port (locked). */
hal_i2c_status_t metalio_board_set(int port, uint8_t mask, uint8_t value);
/** Input levels of a port (locked); reading port 1 releases the expander's INT. */
hal_i2c_status_t metalio_board_read(int port, uint8_t *levels);
/* Light sleep, in upstream's order (power_hw_light_sleep_once and the board functions it calls,
 * @690a023): mask -> (GPIO2 idle high) -> arm_wake -> cut_rails -> sleep -> restore_keys -> (20 ms)
 * -> restore_rails. */
/** Safe output levels first (the PMIC pins, P06, P07, P13, P15 low; the keys P10, P11, P17 high, their
 *  idle level; P16 high when it will be a wake input, else low; motor, amplifier, P05, P12 and P14 as
 *  they are), then every pin an output, then both ports read, so nothing on the expander holds INT
 *  (upstream ioexp_mask_for_sleep). Stop reading the keys before. */
hal_i2c_status_t metalio_board_mask_for_sleep(bool rtc_wake);
/** The wake inputs back as inputs, with INT idle: POWER P17 (upstream enable_power_wake_pin) and,
 *  when asked, RTC INT P16 (upstream leaves it masked and wakes by timer only). An input that is
 *  already active (a key held, an alarm pending) raises INT at once, so the sleep ends at once. */
hal_i2c_status_t metalio_board_arm_wake(bool power_key, bool rtc);
/** The shared rails off in the official order: P05 (camera/screen) first, then P14 (4G, Bluetooth
 *  audio, amplifier) unless keep_shared_power. What hangs on them must be idle. */
hal_i2c_status_t metalio_board_cut_rails(bool keep_shared_power);
/** After the wake: directions as at boot, P13 low, both ports read (upstream ioexp_restore_keys);
 *  the levels read come back in p0, p1 (may be NULL). Read the wake inputs before calling this. The
 *  inputs settle for ~20 ms (upstream waits that long before taking the key baseline). */
hal_i2c_status_t metalio_board_restore_keys(uint8_t *p0, uint8_t *p1);
/** The shared rails back in the official order: P14, then P05 (upstream sleep_restore_rails). */
hal_i2c_status_t metalio_board_restore_rails(void);
/** The output latch of a port as last written (locked). */
uint8_t metalio_board_output(int port);
/** Changes each time P14 comes back on (any context): the modem, the Bluetooth audio chip and the
 *  amplifier lost their state and boot again; metalio_board_shared_power_since_us says when. */
unsigned metalio_board_shared_power_epoch(void);
uint64_t metalio_board_shared_power_since_us(void);
/** Turn the device off: MB_POWER_PULSES pulses on P13, as upstream after a 3 s POWER press. Does not
 *  return on the device once the power chip reacts. */
void metalio_power_off(void);

#endif
