/*
 * Fuel gauge BQ27220 @0x55 and charger CX25601N @0x6B on the Plus, register level, both fed by
 * the battery model in hal_power_sim.c.
 *
 * BQ27220: standard commands (TI bq27220 reference manual), 16-bit little-endian from the command
 * code: Temperature 0x06 (0.1 K), Voltage 0x08, BatteryStatus 0x0A, Current 0x0C (signed, +
 * charging), RemainingCapacity 0x10, FullChargeCapacity 0x12, AverageCurrent 0x14, StateOfCharge
 * 0x2C, StateOfHealth 0x2E, DesignCapacity 0x3C. Temperature is fixed at 25 C and health at 100 %.
 *
 * CX25601N: the registers the official driver uses (cx25601n.cc @690a023, no datasheet in hand):
 * ICHG 80 mA steps (0x02[7:6], 0x03[3:0]), VREG 10 mV (0x04[7:3], 0x05[3:0]), IINDPM 20 mA
 * (0x06[7:4], 0x07[3:0]), CHG_CTRL0 0x14 bit 2 charge enable, CHG_TMR 0x15, CHG_CTRL3 0x18 = 0x01
 * battery FET off, STATUS1 0x1E ([4:3] charge state, [2:0] input type), PART_INFO 0x38, and the
 * 0x70 unlock sequence (0x00 0x50 0x57 0x44, then it reads 0x03).
 * Reset values are ASSUMPTIONS (charging enabled at 2 A, 4.35 V, input limit 3 A); the board sets
 * the charge current at boot like upstream (1 A asked, 960 mA applied).
 * Charging (ESTIMATE of a CC/CV charge): with USB in and charging enabled, ICHG up to 90 %, then
 * tapering to 60 mA at 100 %, then done. The current goes into the battery model.
 */
#include <string.h>

#include "board/metalio_board.h"
#include "bq27220/bq27220.h"
#include "cx25601n/cx25601n.h"
#include "ink/hal_i2c.h"
#include "sim.h"

#define CHG_TAPER_FROM 0.90
#define CHG_ITERM_MA 60.0
#define CHG_UPDATE_MS 2000
#define CX_PART_INFO 0x00 /* unknown on the real chip: placeholder */

/* ------------------------------------------------------------------ BQ27220 */

static struct {
    uint8_t ptr;
} G;

static int gauge_value(uint8_t cmd) {
    double cap = g_profile.batt_mah > 0 ? g_profile.batt_mah : 1;
    switch (cmd) {
    case BQ27220_TEMPERATURE: return 2982; /* 25 C in 0.1 K */
    case BQ27220_VOLTAGE: return sim_battery_mv();
    case BQ27220_BATTERY_STATUS: return sim_battery_ma() < 0 ? 0x0001 : 0; /* DSG while discharging */
    case BQ27220_CURRENT:
    case BQ27220_AVERAGE_CURRENT: return sim_battery_ma();
    case BQ27220_REMAINING_CAP: return (int)(sim_battery_soc() * cap + 0.5);
    case BQ27220_FULL_CHARGE_CAP:
    case BQ27220_DESIGN_CAP: return (int)cap;
    case BQ27220_STATE_OF_CHARGE: return (int)(sim_battery_soc() * 100.0 + 0.5);
    case BQ27220_STATE_OF_HEALTH: return 100;
    default: return 0;
    }
}

static int gauge_xfer(void *state, const uint8_t *w, size_t wlen, uint8_t *r, size_t rlen) {
    (void)state;
    if (wlen) G.ptr = w[0];
    for (size_t i = 0; i < rlen; i++) {
        uint8_t cmd = (uint8_t)((G.ptr + i) & ~1u);
        int v = gauge_value(cmd);
        r[i] = (uint8_t)(((G.ptr + i) & 1) ? (v >> 8) : v);
    }
    G.ptr = (uint8_t)(G.ptr + rlen);
    return HAL_I2C_OK;
}

/* ------------------------------------------------------------------ CX25601N */

static struct {
    uint8_t reg[0x80];
    uint8_t ptr;
    uint8_t unlock[4];
    vtk_event_id_t ev;
    bool fet_off;
} C;

static int ichg_ma(void) { return (((C.reg[0x02] >> 6) & 0x03) | ((C.reg[0x03] & 0x0F) << 2)) * CX25601N_ICHG_STEP_MA; }
static int iindpm_ma(void) { return (((C.reg[0x06] >> 4) & 0x0F) | ((C.reg[0x07] & 0x0F) << 4)) * 20; }
static bool chg_enabled(void) { return C.reg[0x14] & 0x04; }

static void set_code(uint8_t lo, uint8_t lo_mask, int lo_shift, int lo_bits, uint8_t hi, int code) {
    C.reg[lo] = (uint8_t)((C.reg[lo] & ~(lo_mask << lo_shift)) | ((code & lo_mask) << lo_shift));
    C.reg[hi] = (uint8_t)((C.reg[hi] & ~0x0F) | ((code >> lo_bits) & 0x0F));
}

static void charger_tick(void *arg) { /* ISR: re-evaluate while USB is in */
    (void)arg;
    C.ev = 0;
    sim_charger_update();
}

void sim_charger_update(void) { /* ISR or locked */
    if (!sim_board_present()) {
        sim_power_set_charge_ma(0);
        return;
    }
    bool vbus = sim_power_vbus();
    double soc = sim_battery_soc(), ma = 0;
    int state = CX25601N_NOT_CHARGING;
    if (vbus && chg_enabled() && soc < 0.999) {
        double limit = ichg_ma(), input = iindpm_ma() * 5.0 * 0.9 / 3.8; /* input power to battery current */
        if (input < limit) limit = input;
        ma = soc < CHG_TAPER_FROM ? limit : CHG_ITERM_MA + (limit - CHG_ITERM_MA) * (1.0 - soc) / (1.0 - CHG_TAPER_FROM);
        state = soc < CHG_TAPER_FROM ? CX25601N_CC : CX25601N_CV;
    }
    sim_power_set_charge_ma(ma);
    C.reg[0x1E] = (uint8_t)((state << 3) | (vbus ? 3 : 0)); /* DCP: a wall adapter */
    if (C.ev) vtk_event_cancel(C.ev);
    C.ev = 0;
    if (vbus) {
        uint64_t now = vtk_in_isr() ? vtk_isr_now() : vtk_now();
        C.ev = vtk_event_at(now + SIM_MS(CHG_UPDATE_MS), charger_tick, NULL);
    }
    if (C.fet_off && !vbus) sim_power_off("battery disconnected (charger ship mode)");
}

static int charger_xfer(void *state, const uint8_t *w, size_t wlen, uint8_t *r, size_t rlen) {
    (void)state;
    if (wlen) C.ptr = w[0] & 0x7F;
    bool changed = false;
    for (size_t i = 1; i < wlen; i++, C.ptr = (uint8_t)((C.ptr + 1) & 0x7F)) {
        if (C.ptr == 0x70) {
            memmove(C.unlock, C.unlock + 1, 3);
            C.unlock[3] = w[i];
            continue;
        }
        if (C.ptr == 0x1E || C.ptr == 0x38) continue; /* read-only */
        C.reg[C.ptr] = w[i];
        if (C.ptr == 0x18 && (w[i] & 0x01)) C.fet_off = true;
        changed = true;
    }
    for (size_t i = 0; i < rlen; i++, C.ptr = (uint8_t)((C.ptr + 1) & 0x7F)) {
        if (C.ptr == 0x70) r[i] = (C.unlock[0] == 0x00 && C.unlock[1] == 0x50 && C.unlock[2] == 0x57 && C.unlock[3] == 0x44) ? 0x03 : 0x00;
        else if (C.ptr == 0x38) r[i] = CX_PART_INFO;
        else r[i] = C.reg[C.ptr];
    }
    if (changed) {
        vtk_lock();
        sim_charger_update();
        vtk_unlock();
    }
    return HAL_I2C_OK;
}

/* ------------------------------------------------------------------ setup and reads */

static hal_i2c_dev_t *g_gauge, *g_charger;

void sim_power_chips_init(void) { /* task context (boot) */
    static const sim_i2c_model_t gauge = {gauge_xfer}, charger = {charger_xfer};
    set_code(0x02, 0x03, 6, 2, 0x03, 2000 / CX25601N_ICHG_STEP_MA);
    set_code(0x04, 0x1F, 3, 5, 0x05, 4350 / 10);
    set_code(0x06, 0x0F, 4, 4, 0x07, 3000 / 20);
    C.reg[0x14] = 0x04;
    C.reg[0x15] = 0x40;
    sim_i2c_attach(0, BQ27220_ADDR, &gauge, NULL);
    sim_i2c_attach(0, CX25601N_ADDR, &charger, NULL);
    g_gauge = hal_i2c_device(0, BQ27220_ADDR, "fuel-gauge");
    g_charger = hal_i2c_device(0, CX25601N_ADDR, "charger");
    int applied = 0; /* upstream metalio_power.cc: 1 A, charging on */
    if (cx25601n_set_charge_current(g_charger, 1000, &applied) != HAL_I2C_OK || cx25601n_enable_charging(g_charger, true) != HAL_I2C_OK)
        sim_warn("board: CX25601N setup failed");
    sim_metric_set("power.charge_current_set_ma", applied);
}

bool sim_board_battery(hal_battery_t *out) { /* task context */
    if (!g_gauge) return false;
    bq27220_reading_t g;
    cx25601n_chg_t chg;
    int vbus = 0;
    if (bq27220_reading(g_gauge, &g) != HAL_I2C_OK || cx25601n_status(g_charger, &chg, &vbus) != HAL_I2C_OK) return false;
    out->percent = g.soc;
    out->millivolts = g.mv;
    out->charging = chg == CX25601N_CC || chg == CX25601N_CV || chg == CX25601N_TOPOFF;
    out->external_power = vbus != 0;
    return true;
}
