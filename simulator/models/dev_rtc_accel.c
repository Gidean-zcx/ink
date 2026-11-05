/*
 * RTC PCF8563 @0x51 and accelerometer SC7A20H @0x19 on the Plus, register level.
 *
 * PCF8563 (NXP datasheet): time in BCD at 0x02..0x08, kept from the simulated wall clock (boot time
 * --> g_sim.wall_start_us) plus what software sets; VL (time lost) stays clear unless a scenario
 * says the backup supply failed. Alarm registers 0x09..0x0C (bit 7 = field ignored), AIE/AF in
 * Control_status_2 (0x01). When the alarm matches, AF is set and, with AIE, INT goes low until AF is
 * cleared: on the Plus V1.4 it reaches TCA9555 P16, hence the expander's INT, GPIO2 and the wake
 * logic (config.h @690a023).
 *
 * SC7A20H: WHO_AM_I 0x11, CTRL1..6 0x20..0x25, OUT_X_L..OUT_Z_H 0x28..0x2D (auto-increment with
 * bit 7 of the register address), AOI1_CFG/SRC/THS/DUR 0x30..0x33. Output is gravity in the
 * device's axes, 12 bits left-justified (1 mg/LSB at +-2 g). The axes follow the screen in portrait
 * (x right, y up, z out of the screen): ASSUMPTION, how the chip sits on the board is unknown.
 * A movement (shake, orientation change) above the AOI1 threshold sets AOI1_SRC.IA and drives
 * INT1 (latched until AOI1_SRC is read when CTRL5.LIR is set); INT1 shares GPIO2 with the
 * expander's INT.
 */
#include <string.h>

#include "metalio_plus/metalio_board.h"
#include "ink/hal_i2c.h"
#include "pcf8563/pcf8563.h"
#include "sc7a20h/sc7a20h.h"
#include "sim.h"

static uint64_t now_ns(void) { return vtk_in_isr() ? vtk_isr_now() : vtk_now(); }

/* ------------------------------------------------------------------ PCF8563 */

static struct {
    uint8_t reg[16], ptr;
    int64_t offset_s;     /* RTC = boot wall time + virtual time + offset */
    uint8_t staged[7];    /* a time write in progress (0x02..0x08) */
    bool vl;
    vtk_event_id_t alarm_ev;
    unsigned alarms;
} R;

static int64_t rtc_now_s(void) { return g_sim.wall_start_us / 1000000 + (int64_t)(now_ns() / 1000000000ull) + R.offset_s; }
static uint8_t bcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
static int dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }

static bool g_line_stuck_low; /* the factory log reads P16 low all the time (N2-1): a board variant, unexplained */

static void rtc_int(void) { /* INT low while AF and AIE: TCA9555 P16 */
    bool low = g_line_stuck_low || ((R.reg[0x01] & 0x08) && (R.reg[0x01] & 0x02));
    if (sim_board_present()) sim_tca_input(1, MB_P1_RTC_INT, low ? 0 : MB_P1_RTC_INT);
}

static bool alarm_matches(int64_t t) {
    int64_t days = t / 86400, rem = t % 86400;
    int y, m, d;
    pcf8563_civil_from_days(days, &y, &m, &d);
    int wday = (int)((days % 7 + 11) % 7);
    int vals[4] = {(int)(rem / 60 % 60), (int)(rem / 3600), d, wday};
    bool any = false;
    for (int i = 0; i < 4; i++) {
        uint8_t a = R.reg[0x09 + i];
        if (a & 0x80) continue;
        any = true;
        if (dec(a & 0x7F) != vals[i]) return false;
    }
    return any;
}

static void alarm_cb(void *arg) { /* ISR: the alarm minute came */
    (void)arg;
    R.alarm_ev = 0;
    R.reg[0x01] |= 0x08; /* AF */
    R.alarms++;
    rtc_int(); /* INT -> P16 -> the expander's INT -> GPIO2: a wake only if the sleep armed P16 */
}

static void schedule_alarm(void) { /* ISR or locked */
    if (R.alarm_ev) vtk_event_cancel(R.alarm_ev);
    R.alarm_ev = 0;
    int64_t now = rtc_now_s(), start = (now / 60 + 1) * 60;
    for (int64_t t = start; t < start + 32 * 86400; t += 60)
        if (alarm_matches(t)) {
            R.alarm_ev = vtk_event_at(now_ns() + (uint64_t)(t - now) * 1000000000ull, alarm_cb, NULL);
            return;
        }
}

static uint8_t rtc_reg(uint8_t r) {
    if (r < 0x02 || r > 0x08) return R.reg[r & 0x0F];
    int64_t t = rtc_now_s(), days = t / 86400, rem = t % 86400;
    int y, m, d;
    pcf8563_civil_from_days(days, &y, &m, &d);
    switch (r) {
    case 0x02: return (uint8_t)(bcd((int)(rem % 60)) | (R.vl ? 0x80 : 0));
    case 0x03: return bcd((int)(rem / 60 % 60));
    case 0x04: return bcd((int)(rem / 3600));
    case 0x05: return bcd(d);
    case 0x06: return (uint8_t)((days % 7 + 11) % 7);
    case 0x07: return (uint8_t)(bcd(m) | (y < 2000 ? 0x80 : 0));
    default: return bcd(y % 100);
    }
}

static int rtc_xfer(void *state, const uint8_t *w, size_t wlen, uint8_t *r, size_t rlen) {
    (void)state;
    if (wlen) R.ptr = w[0] & 0x0F;
    bool time_written = false, alarm_changed = false;
    for (size_t i = 1; i < wlen; i++, R.ptr = (uint8_t)((R.ptr + 1) & 0x0F)) {
        if (R.ptr >= 0x02 && R.ptr <= 0x08) {
            R.staged[R.ptr - 2] = w[i];
            if (R.ptr == 0x08) time_written = true;
        } else {
            R.reg[R.ptr] = w[i];
            if (R.ptr == 0x01 || (R.ptr >= 0x09 && R.ptr <= 0x0C)) alarm_changed = true;
        }
    }
    if (time_written) { /* the driver writes the whole time at once */
        const uint8_t *t = R.staged;
        int year = 2000 + dec(t[6]) - ((t[5] & 0x80) ? 100 : 0);
        int64_t set = pcf8563_days_from_civil(year, dec(t[5] & 0x1F), dec(t[3] & 0x3F)) * 86400 + dec(t[2] & 0x3F) * 3600 +
                      dec(t[1] & 0x7F) * 60 + dec(t[0] & 0x7F);
        R.offset_s += set - rtc_now_s();
        R.vl = t[0] & 0x80;
        alarm_changed = true;
    }
    if (alarm_changed) {
        vtk_lock();
        rtc_int();
        schedule_alarm();
        vtk_unlock();
    }
    for (size_t i = 0; i < rlen; i++, R.ptr = (uint8_t)((R.ptr + 1) & 0x0F)) r[i] = rtc_reg(R.ptr);
    return HAL_I2C_OK;
}

void sim_rtc_lose_time(void) { R.vl = true; } /* ISR (script): the backup supply failed */
void sim_rtc_line_stuck_low(void) { /* ISR (script) */
    g_line_stuck_low = true;
    rtc_int();
}
int64_t sim_rtc_now_s(void) { return rtc_now_s(); }
unsigned sim_rtc_alarms(void) { return R.alarms; }

/* ------------------------------------------------------------------ SC7A20H */

static struct {
    uint8_t reg[0x40], ptr;
    int g[3];       /* gravity reading in mg, device axes */
    bool ia;        /* AOI1 latched */
    bool int1_low;
    unsigned motions;
} A = {.g = {0, 1000, 0}}; /* upright in portrait */

static void accel_int(void) {
    bool active_low = A.reg[0x25] & 0x02; /* H_LACTIVE */
    bool asserted = A.ia && (A.reg[0x22] & 0x40);
    bool low = asserted == active_low ? asserted : false; /* active high INT1 cannot pull the shared line low */
    if (low != A.int1_low) {
        A.int1_low = low;
        sim_gpio2_update();
    }
}

static void accel_motion(int mg) { /* ISR or locked: a movement of this size on some axis */
    uint8_t cfg = A.reg[0x30];
    int ths = (A.reg[0x32] & 0x7F) * 16; /* +-2 g: ~16 mg per LSB */
    if (!(A.reg[0x20] & 0x07) || !(cfg & 0x2A) || mg <= ths) return;
    A.ia = true;
    A.motions++;
    accel_int();
}

static uint8_t accel_reg(uint8_t r) {
    if (r == 0x0F) return 0x11;
    if (r >= 0x28 && r <= 0x2D) {
        int axis = (r - 0x28) / 2;
        int v = (A.reg[0x20] & (1 << axis)) ? A.g[axis] : 0;
        int raw = v * 16; /* 12 bits left-justified, 1 mg/LSB */
        return (uint8_t)(((r - 0x28) & 1) ? (raw >> 8) : raw);
    }
    if (r == 0x31) {
        uint8_t v = A.ia ? 0x40 | 0x2A : 0;
        if (A.reg[0x24] & 0x08) { /* LIR: reading releases the latch */
            A.ia = false;
            accel_int();
        }
        return v;
    }
    return A.reg[r & 0x3F];
}

static int accel_xfer(void *state, const uint8_t *w, size_t wlen, uint8_t *r, size_t rlen) {
    (void)state;
    bool inc = true;
    if (wlen) {
        A.ptr = w[0] & 0x7F;
        inc = (w[0] & 0x80) || wlen > 2;
    }
    for (size_t i = 1; i < wlen; i++, A.ptr = (uint8_t)((A.ptr + 1) & 0x3F)) A.reg[A.ptr & 0x3F] = w[i];
    if (wlen > 1) {
        vtk_lock();
        accel_int();
        vtk_unlock();
    }
    vtk_lock(); /* reading AOI1_SRC may release INT1 */
    for (size_t i = 0; i < rlen; i++) {
        r[i] = accel_reg(A.ptr);
        if (inc) A.ptr = (uint8_t)((A.ptr + 1) & 0x3F);
    }
    vtk_unlock();
    return HAL_I2C_OK;
}

bool sim_accel_int_low(void) { return A.int1_low; }
unsigned sim_accel_motions(void) { return A.motions; }

void sim_accel_orient(const char *name) { /* ISR (script, window) */
    static const struct { const char *n; int g[3]; } O[] = {
        {"portrait", {0, 1000, 0}},        {"upside_down", {0, -1000, 0}}, {"landscape_left", {1000, 0, 0}},
        {"landscape_right", {-1000, 0, 0}}, {"face_up", {0, 0, 1000}},     {"face_down", {0, 0, -1000}},
    };
    for (unsigned i = 0; i < sizeof O / sizeof O[0]; i++)
        if (!strcmp(name, O[i].n)) {
            int change = 0;
            for (int k = 0; k < 3; k++) {
                int dlt = O[i].g[k] - A.g[k];
                if ((dlt < 0 ? -dlt : dlt) > change) change = dlt < 0 ? -dlt : dlt;
                A.g[k] = O[i].g[k];
            }
            accel_motion(change);
            return;
        }
}

void sim_accel_shake(void) { accel_motion(1500); } /* ISR (script, window) */

void sim_accel_orientation(int g[3]) { memcpy(g, A.g, sizeof A.g); }

/* ------------------------------------------------------------------ setup */

void sim_rtc_accel_init(void) { /* task context (boot) */
    static const sim_i2c_model_t rtc = {rtc_xfer}, acc = {accel_xfer};
    for (int i = 0; i < 4; i++) R.reg[0x09 + i] = 0x80; /* alarm fields off */
    sim_i2c_attach(0, PCF8563_ADDR, &rtc, NULL);
    sim_i2c_attach(0, SC7A20H_ADDR, &acc, NULL);
}

void sim_rtc_accel_report(void) {
    sim_metric_set("rtc.alarms", R.alarms);
    sim_metric_set("accel.motions", A.motions);
}
