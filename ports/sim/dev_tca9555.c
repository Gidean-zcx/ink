/*
 * TCA9555 @0x20 on the Plus, register level, with what is wired to its pins (config.h @690a023,
 * see drivers/board/metalio_board.h).
 *
 * Inputs read the physical levels: keys (low while pressed), the PMIC's PWR_GOOD (P07), the RTC
 * interrupt (P16, low = event). Outputs drive the models: the PMIC pins (P00-P02), the motor (P03),
 * the amplifier (P04), the shared touch + 4G reset (P12), the power-off pulse (P13) and the shared
 * 4G + Bluetooth audio + amplifier supply (P14). Writing a register continues with the other
 * register of its pair, as on the chip.
 *
 * INT (open drain, to GPIO2 together with the accelerometer's INT1) asserts when a port 1 input
 * changes and releases when port 1's input register is read. P07 toggling with every panel power
 * cycle would raise INT on the chip as well; that is not modelled (the keys driver reads port 1).
 *
 * Checks of the official wiring rules (warnings):
 *  - P12 low resets the 4G modem along with the touch controller;
 *  - P14 low while the amplifier is enabled (the amplifier loses its supply);
 * Power-off: after MB_POWER_PULSES pulses on P13 (ASSUMPTION: the power chip acts at the end of the
 * official pulse train; its exact rule is unknown) the device loses power and the run ends.
 */
#include <string.h>

#include "board/metalio_board.h"
#include "board/metalio_epd_power.h"
#include "ink/hal_i2c.h"
#include "sim.h"

static struct {
    uint8_t out[2], pol[2], cfg[2], ptr;
    uint8_t ext[2];      /* levels driven onto the input pins from outside */
    uint8_t seen1;       /* port 1 input levels at the last read of its input register */
    bool int_low;
    bool motor, amp;
    unsigned pulses;     /* P13 rising edges with power-off spacing */
    uint64_t last_pulse;
    unsigned motor_starts, shared_rst_warn, shared_pwr_warn;
} X = {.out = {0xFF, 0xFF}, .cfg = {0xFF, 0xFF}, .ext = {0x00, 0xB7}, .seen1 = 0xB7}; /* P1 idle levels from
                                                                                    the factory boot log */

static uint8_t levels(int port) {
    uint8_t ext = X.ext[port];
    if (port == 0) ext = (uint8_t)((ext & ~EPD_PWR_GOOD) | (sim_tps65185_rails_good() ? EPD_PWR_GOOD : 0));
    return (uint8_t)((X.out[port] & ~X.cfg[port]) | (ext & X.cfg[port]));
}

static void update_int(void) {
    bool low = (levels(1) & X.cfg[1]) != (X.seen1 & X.cfg[1]);
    if (low != X.int_low) {
        X.int_low = low;
        sim_gpio2_update();
    }
}

bool sim_tca_int_low(void) { return X.int_low; }
bool sim_board_motor_on(void) { return X.motor; }
bool sim_board_amp_on(void) { return X.amp; }
bool sim_board_shared_reset_held(void) { return !(levels(1) & MB_P1_SHARED_RST); }
bool sim_board_cam_power(void) { return levels(0) & MB_P0_CAM_SCR; }

void sim_tca_input(int port, uint8_t mask, uint8_t value) { /* ISR or locked */
    X.ext[port] = (uint8_t)((X.ext[port] & ~mask) | (value & mask));
    update_int();
}

/* Output pins changed (bits in `rise` went high, `fall` went low). Kernel context of the writer. */
static void outputs_changed(uint8_t rise0, uint8_t fall0, uint8_t rise1, uint8_t fall1) {
    uint8_t l0 = levels(0), l1 = levels(1);
    if ((rise0 | fall0) & (EPD_PWR_WAKEUP | EPD_PWR_PWRUP | EPD_PWR_VCOM_CTRL))
        sim_tps65185_pins(l0 & EPD_PWR_WAKEUP, l0 & EPD_PWR_PWRUP, l0 & EPD_PWR_VCOM_CTRL);
    if ((rise0 | fall0) & MB_P0_MOTOR) {
        X.motor = l0 & MB_P0_MOTOR;
        if (X.motor) X.motor_starts++;
        sim_power_consumer("motor", X.motor, 80.0); /* (est.) coin motor current */
    }
    if ((rise0 | fall0) & MB_P0_AMP) X.amp = l0 & MB_P0_AMP;
    if (rise0 & MB_P0_AMP && !(l1 & MB_P1_SHARED_PWR)) {
        X.shared_pwr_warn++;
        sim_warn("board: amplifier enabled (TCA9555 P04) while its supply P14 is off");
    }
    if (fall1 & MB_P1_SHARED_RST) {
        if (sim_net_modem_registered()) {
            X.shared_rst_warn++;
            sim_warn("board: P12 low resets the touch controller AND the 4G modem (shared reset); the modem was "
                     "registered");
        }
        sim_net_modem_hold_reset(true);
    }
    if (rise1 & MB_P1_SHARED_RST) sim_net_modem_hold_reset(false);
    if (fall1 & MB_P1_SHARED_PWR) {
        if (X.amp) {
            X.shared_pwr_warn++;
            sim_warn("board: P14 cut while the amplifier is enabled (4G, Bluetooth audio and amplifier share it)");
        }
        sim_net_modem_power(false);
        sim_bt_audio_power(false);
    }
    if (rise1 & MB_P1_SHARED_PWR) {
        sim_net_modem_power(true);
        sim_bt_audio_power(true);
    }
    if (rise1 & MB_P1_PWR_PULSE) {
        uint64_t now = vtk_now(), gap = now - X.last_pulse;
        X.pulses = X.last_pulse && gap >= SIM_MS(MB_POWER_PULSE_MS) && gap <= SIM_MS(4 * MB_POWER_PULSE_MS) ? X.pulses + 1 : 1;
        X.last_pulse = now;
        if (X.pulses >= MB_POWER_PULSES) sim_power_off("power-off pulses on P13");
    }
}

static int tca_xfer(void *state, const uint8_t *w, size_t wlen, uint8_t *r, size_t rlen) {
    (void)state;
    uint8_t before0 = levels(0), before1 = levels(1);
    if (wlen) X.ptr = w[0] & 7;
    for (size_t i = 1; i < wlen; i++, X.ptr ^= 1) {
        int port = X.ptr & 1;
        switch (X.ptr >> 1) {
        case 1: X.out[port] = w[i]; break;
        case 2: X.pol[port] = w[i]; break;
        case 3: X.cfg[port] = w[i]; break;
        default: break; /* input registers are read-only */
        }
    }
    if (wlen > 1) {
        uint8_t after0 = levels(0), after1 = levels(1);
        uint8_t o0 = (uint8_t)~X.cfg[0], o1 = (uint8_t)~X.cfg[1]; /* output pins */
        outputs_changed((uint8_t)(after0 & ~before0 & o0), (uint8_t)(before0 & ~after0 & o0), (uint8_t)(after1 & ~before1 & o1),
                        (uint8_t)(before1 & ~after1 & o1));
    }
    for (size_t i = 0; i < rlen; i++, X.ptr ^= 1) {
        int port = X.ptr & 1;
        switch (X.ptr >> 1) {
        case 0:
            r[i] = levels(port) ^ X.pol[port];
            if (port == 1) X.seen1 = levels(1); /* reading the port that changed releases INT */
            break;
        case 1: r[i] = X.out[port]; break;
        case 2: r[i] = X.pol[port]; break;
        default: r[i] = X.cfg[port]; break;
        }
    }
    vtk_lock(); /* the output hooks above lock for themselves */
    update_int();
    vtk_unlock();
    return HAL_I2C_OK;
}

bool sim_board_present(void) {
    if (g_profile.pmic_tps65185) return true;
    for (int k = 0; k < HAL_KEY_COUNT; k++)
        if (g_profile.key_source[k] == 1) return true;
    return false;
}

void sim_board_init(void) { /* task context (boot) */
    if (!sim_board_present()) return;
    sim_tca9555_init();
    if (metalio_board_init() != HAL_I2C_OK) sim_warn("board: TCA9555 init failed");
    sim_power_chips_init();
    sim_sensors_init();
    sim_camera_init();
    sim_bt_audio_init();
}

void sim_tca9555_init(void) {
    static const sim_i2c_model_t tca = {tca_xfer};
    sim_i2c_attach(0, TCA9555_ADDR_BASE, &tca, NULL);
}

void sim_tca9555_report(void) {
    sim_rtc_accel_report();
    sim_bt_audio_report();
    sim_metric_set("board.motor_starts", X.motor_starts);
    sim_metric_set("board.shared_reset_warnings", X.shared_rst_warn);
    sim_metric_set("board.shared_power_warnings", X.shared_pwr_warn);
}
