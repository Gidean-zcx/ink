/*
 * Register-level models of the Plus panel power chips, answering the real
 * board drivers through hal_i2c (D-41).
 *
 *   (the TCA9555 that drives its pins is modelled in dev_tca9555.c)
 *   TPS65185 @0x68 e-paper PMIC. SLEEP while WAKEUP is low (no I2C answer,
 *                  registers back to reset values); a PWRUP rising edge
 *                  starts the rails, which are good after the four UPSEQ1
 *                  strobe delays; VCOM is driven once VCOM_CTRL is high and
 *                  ENABLE has VCOM_EN.
 *
 * Checks that would otherwise only show up on the device, some as damage:
 *   - VCOM driven at a value other than the panel's (profile panel_vcom_mv,
 *     the label on the panel; --panel-vcom-mv overrides it);
 *   - a waveform started without rails and VCOM good (display HAL asks);
 *   - rails powered down less than 12 us after the gate output (XOE) went
 *     low (panel spec 6.1), or WAKEUP dropped while the rails were up.
 *
 * Timing values marked (est.) are model estimates to be replaced by
 * measurements (SP-01); the strobe-delay encoding follows the datasheet.
 */
#include <string.h>

#include "metalio_plus/metalio_epd_power.h"
#include "ink/hal_i2c.h"
#include "sim.h"
#include "tps65185/tps65185.h"

#define WAKE_US 3000       /* (est.) WAKEUP high -> I2C answers */
#define RAMP_US 1000       /* (est.) after the last strobe until the rails report good */
#define VCOM_SETTLE_US 2000 /* (est.) VCOM_EN -> VCOM power-good bit */
#define PG_RAILS 0xF8      /* model's PG register with rails good; VCOM adds bit 1 (-> 0xFA) */
#define REVID_MODEL 0x65   /* placeholder: the device's REVID has not been read yet */

/* ------------------------------------------------------------- TPS65185 */

static struct {
    bool wakeup, pwrup, vcom_ctrl, oe;
    uint64_t t_awake;   /* I2C answers from here on */
    uint64_t t_rails;   /* rails good from here on; 0 = off */
    uint64_t t_vcom_en; /* ENABLE.VCOM_EN written at; 0 = not */
    uint64_t t_oe_low;
    uint8_t reg[TPS_REG_COUNT];
    uint8_t ptr;
    unsigned power_ups, order_warnings, vcom_mismatches, vcom_seen_mv;
    bool warned_vcom;
} T;

/* reset values the model uses; only VCOM (not the panel's) and UPSEQ1 matter to the checks */
static const uint8_t TPS_RESET[TPS_REG_COUNT] = {
    [TPS_REG_VCOM1] = 0x7D, [TPS_REG_UPSEQ0] = 0xE4, [TPS_REG_UPSEQ1] = 0x55, [TPS_REG_REVID] = REVID_MODEL,
};

static uint64_t strobes_us(uint8_t upseq1) {
    uint64_t us = 0;
    for (int i = 0; i < 4; i++) us += 3000u * (1u + ((upseq1 >> (2 * i)) & 3u)); /* 3, 6, 9 or 12 ms */
    return us;
}

static bool rails_good(uint64_t now) { return T.t_rails && now >= T.t_rails; }

static bool vcom_driven(uint64_t now) {
    return rails_good(now) && T.vcom_ctrl && (T.reg[TPS_REG_ENABLE] & TPS_ENABLE_VCOM) && T.t_vcom_en;
}

static unsigned vcom_mv(void) {
    return (((unsigned)(T.reg[TPS_REG_VCOM2] & 1) << 8) | T.reg[TPS_REG_VCOM1]) * TPS_VCOM_STEP_MV;
}

static void rails_good_cb(void *arg) { /* ISR */
    (void)arg;
    sim_tca_reeval();
}

static void tps_pins(bool wakeup, bool pwrup, bool vcom_ctrl) {
    uint64_t now = vtk_now();
    if (T.pwrup && !pwrup && T.t_rails) {
        if (T.oe || now - T.t_oe_low < SIM_US(12)) {
            T.order_warnings++;
            sim_warn("display: TPS65185 rails went down %s the gate output (XOE) was low for 12 us "
                     "(panel spec 6.1)", T.oe ? "while" : "before");
        }
        T.t_rails = 0;
        T.t_vcom_en = 0;
    }
    if (T.wakeup && !wakeup) {
        if (T.t_rails) {
            T.order_warnings++;
            sim_warn("display: TPS65185 WAKEUP went low with the rails up (PWRUP must fall first)");
        }
        memcpy(T.reg, TPS_RESET, sizeof T.reg);
        T.t_rails = 0;
        T.t_vcom_en = 0;
    }
    if (!T.wakeup && wakeup) T.t_awake = now + SIM_US(WAKE_US);
    if (wakeup && pwrup && !T.pwrup) {
        uint64_t start = now > T.t_awake ? now : T.t_awake;
        T.t_rails = start + SIM_US(strobes_us(T.reg[TPS_REG_UPSEQ1]) + RAMP_US);
        T.power_ups++;
        vtk_lock(); /* PWR_GOOD (TCA9555 P07) rises then: the expander's INT follows */
        vtk_event_at(T.t_rails, rails_good_cb, NULL);
        vtk_unlock();
    }
    T.wakeup = wakeup;
    T.pwrup = pwrup;
    T.vcom_ctrl = vcom_ctrl;
}

static uint8_t tps_read_reg(uint8_t reg, uint64_t now) {
    uint8_t v = T.reg[reg];
    if (reg == TPS_REG_PG) {
        v = rails_good(now) ? PG_RAILS : 0;
        if (vcom_driven(now) && now >= T.t_vcom_en + SIM_US(VCOM_SETTLE_US)) v |= 0x02;
    } else if (reg == TPS_REG_INT1 || reg == TPS_REG_INT2) {
        T.reg[reg] = 0; /* cleared by reading */
    }
    return v;
}

static int tps_xfer(void *state, const uint8_t *w, size_t wlen, uint8_t *r, size_t rlen) {
    (void)state;
    uint64_t now = vtk_now();
    if (!T.wakeup || now < T.t_awake) return HAL_I2C_NACK; /* SLEEP: I2C off */
    if (wlen) T.ptr = w[0];
    for (size_t i = 1; i < wlen; i++, T.ptr++) {
        if (T.ptr >= TPS_REG_COUNT || T.ptr == TPS_REG_REVID || T.ptr == TPS_REG_PG) continue;
        if (T.ptr == TPS_REG_ENABLE && (w[i] & TPS_ENABLE_VCOM) && !(T.reg[TPS_REG_ENABLE] & TPS_ENABLE_VCOM))
            T.t_vcom_en = now;
        T.reg[T.ptr] = w[i];
    }
    for (size_t i = 0; i < rlen; i++, T.ptr++) r[i] = T.ptr < TPS_REG_COUNT ? tps_read_reg(T.ptr, now) : 0;
    return HAL_I2C_OK;
}

bool sim_tps65185_panel_ready(void) {
    uint64_t now = vtk_now();
    bool ready = vcom_driven(now) && now >= T.t_vcom_en + SIM_US(VCOM_SETTLE_US);
    if (ready) T.vcom_seen_mv = vcom_mv();
    if (ready && vcom_mv() != g_profile.panel_vcom_mv) {
        T.vcom_mismatches++;
        if (!T.warned_vcom)
            sim_warn("display: VCOM driven at -%u mV but the panel needs -%u mV (wrong VCOM can damage the "
                     "panel)", vcom_mv(), g_profile.panel_vcom_mv);
        T.warned_vcom = true;
    }
    return ready;
}

void sim_tps65185_gate(bool oe) {
    if (T.oe && !oe) T.t_oe_low = vtk_now();
    T.oe = oe;
}

/* ------------------------------------------------------------------ setup */

void sim_epd_chips_init(void) {
    static const sim_i2c_model_t tps = {tps_xfer};
    memcpy(T.reg, TPS_RESET, sizeof T.reg);
    sim_i2c_attach(0, TPS65185_ADDR, &tps, NULL);
}

void sim_tps65185_pins(bool wakeup, bool pwrup, bool vcom_ctrl) { tps_pins(wakeup, pwrup, vcom_ctrl); }
bool sim_tps65185_rails_good(void) { return rails_good(vtk_now()); }

void sim_tps65185_report(void) {
    sim_metric_set("display.pmic.power_ups", T.power_ups);
    sim_metric_set("display.pmic.order_warnings", T.order_warnings);
    sim_metric_set("display.pmic.vcom_mismatches", T.vcom_mismatches);
    sim_metric_set("display.pmic.vcom_mv", T.vcom_seen_mv); /* last value driven onto the panel */
}
