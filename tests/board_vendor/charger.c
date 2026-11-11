/* Synthetic register and gauge values; no hardware or simulator approximation. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bq27220/bq27220.h"
#include "cx25601n/cx25601n.h"
#include "ink/osal.h"

#define CHECK(ok, why) do { if (!(ok)) { fprintf(stderr, "%s (line %d)\n", why, __LINE__); exit(1); } checks++; } while (0)
struct hal_i2c_dev { bool gauge; };
static hal_i2c_dev_t charger, gauge = {.gauge = true};
static uint8_t regs[256], key[4];
static int checks, gauge_mv, gauge_ma, cuts, kicks, delays;
static bool uv_done;

hal_i2c_status_t hal_i2c_write(hal_i2c_dev_t *dev, const uint8_t *data, size_t len) {
    CHECK(dev == &charger && len == 2, "charger register write");
    if (data[0] == 0x70) { memmove(key, key + 1, 3); key[3] = data[1]; }
    if (data[0] == 0x18 && data[1] == 1) cuts++;
    if (data[0] == 0x16 && (regs[0x16] & 0x20) && !(data[1] & 0x20)) kicks++;
    regs[data[0]] = data[1];
    return HAL_I2C_OK;
}
hal_i2c_status_t hal_i2c_write_read(hal_i2c_dev_t *dev, const uint8_t *w, size_t wn, uint8_t *r, size_t rn) {
    CHECK(wn == 1, "register read command");
    if (dev == &gauge) {
        CHECK(rn == 2, "gauge reads 16 bits");
        int v = *w == BQ27220_VOLTAGE ? gauge_mv : *w == BQ27220_CURRENT ? gauge_ma : 50;
        r[0] = (uint8_t)v;
        r[1] = (uint8_t)((uint16_t)v >> 8);
    } else {
        CHECK(dev == &charger && rn == 1, "charger reads one byte");
        static const uint8_t unlock[] = {0, 0x50, 0x57, 0x44};
        *r = *w == 0x70 ? (memcmp(key, unlock, sizeof key) == 0 ? 3 : 0) : regs[*w];
    }
    return HAL_I2C_OK;
}
void osal_task_delay_us(uint64_t us) {
    CHECK(!(regs[0x14] & 4) && !(regs[0x16] & 0x10), "kick clears EN_TERM and HIZ");
    CHECK((delays == 0 && us == 200000 && !(regs[0x16] & 0x20)) ||
          (delays == 1 && us == 500000 && (regs[0x16] & 0x20)), "kick holds off 200 ms then on 500 ms");
    delays++;
}
static void reset(void) {
    memset(regs, 0, sizeof regs);
    uv_done = false;
    cuts = kicks = delays = 0;
    regs[0x16] = 0x20;
}
static uint32_t sample(int mv, int ma, cx25601n_chg_t state, int vbus) {
    gauge_mv = mv;
    gauge_ma = ma;
    regs[0x1E] = (uint8_t)((state << 3) | vbus);
    bq27220_reading_t g;
    CHECK(bq27220_reading(&gauge, &g) == HAL_I2C_OK && g.mv == mv && g.ma == ma, "actual gauge driver decodes sample");
    delays = 0;
    return cx25601n_poll(&charger, g.mv, g.ma, &uv_done);
}
static void defaults(void) {
    reset();
    memset(regs, 0xAA, sizeof regs);
    regs[0x04] = 0x9B; regs[0x05] = 0xAD; /* 4350 mV; preserve low/upper bits. */
    regs[0x12] = 0x95; regs[0x13] = 0xAA; /* 180 mA */
    regs[0x14] = 0xAE;
    int mv, iterm;
    bool term;
    CHECK(cx25601n_charge_config(&charger, &mv, &iterm, &term) == HAL_I2C_OK && mv == 4350 && iterm == 180 && term,
          "boot before reads existing VREG ITERM EN_TERM");
    CHECK(cx25601n_init_defaults(&charger) == HAL_I2C_OK, "vendor defaults initialize");
    CHECK(cx25601n_charge_config(&charger, &mv, &iterm, &term) == HAL_I2C_OK && mv == 4400 && iterm == 60 && term,
          "boot after VREG 4400 mV ITERM 60 mA; EN_TERM preserved as vendor");
    CHECK(regs[0x04] == 0xC3 && regs[0x05] == 0xAD && regs[0x12] == 0x35 && regs[0x13] == 0xAA,
          "split fields preserve unrelated bits");
    CHECK(regs[0x10] == 0xCA && regs[0x11] == 0xAA, "precharge code 12");
    CHECK(regs[0x15] == 0xAA && regs[0x16] == 0xA8, "DPDM off HIZ off WDT off EN_CHG on");
    CHECK(regs[0x02] == 0x2A && regs[0x03] == 0xA3 && regs[0x06] == 0x0A && regs[0x07] == 0xA3,
          "1 A requested: ICHG and IINDPM 960 mA");
    CHECK(regs[0x86] == 6 && regs[0x3A] == 0x10 && regs[0x46] == 0x20 && regs[0x70] == 0,
          "private registers unlocked written and locked");
    CHECK(regs[0x14] == 0xAF && regs[0x18] == 0xAA && regs[0x1A] == 0xAA && regs[0x23] == 0xBE && regs[0x24] == 0xAA,
          "remaining vendor control fields");
    CHECK(cx25601n_enable_charging(&charger, false) == HAL_I2C_OK && !(regs[0x16] & 0x20) && (regs[0x14] & 4),
          "charge enable and termination are independent");
}
static void undervoltage(void) {
    reset();
    sample(3101, 10, CX25601N_CC, 0);
    CHECK(cuts == 0, "3101 mV must not cut BATFET (catches 3.2 V threshold)");
    sample(3100, 10, CX25601N_CC, 3);
    CHECK(cuts == 0, "external power must not cut BATFET");
    sample(2499, 10, CX25601N_CC, 0);
    CHECK(cuts == 0, "invalid voltage must not cut BATFET");
    sample(3100, 10, CX25601N_CC, 0);
    CHECK(cuts == 1 && uv_done, "3100 mV without input cuts BATFET");
    sample(3100, 10, CX25601N_CC, 0);
    CHECK(cuts == 1, "UV latch suppresses repeat writes");
    sample(3200, 10, CX25601N_CC, 0);
    CHECK(uv_done, "exact 100 mV recovery retains latch as vendor");
    sample(3201, 10, CX25601N_CC, 0);
    CHECK(!uv_done && regs[0x18] == 1, "above 100 mV recovery resets latch, not BATFET register");
    sample(2500, 10, CX25601N_CC, 0);
    CHECK(cuts == 2, "valid lower boundary cuts after recovery");
}
static void recharge(void) {
    reset();
    CHECK(sample(4399, 4, CX25601N_NOT_CHARGING, 3) == 2000 && kicks == 1 && delays == 2, "below cutoff idle low current kicks at 2 s");
    CHECK(!(regs[0x14] & 4), "below cutoff disables termination");
    int before = kicks;
    for (int state = CX25601N_CC; state <= CX25601N_TOPOFF; state++) sample(4200, 0, (cx25601n_chg_t)state, 3);
    CHECK(kicks == before, "non-idle charger must not kick");
    sample(4200, 5, CX25601N_NOT_CHARGING, 3);
    CHECK(kicks == before, "5 mA must not kick");
    CHECK(sample(4400, 0, CX25601N_NOT_CHARGING, 3) == 10000 && kicks == before && (regs[0x14] & 4), "at cutoff: terminate, no kick, 10 s");
    CHECK(sample(2499, 0, CX25601N_NOT_CHARGING, 3) == 10000 && kicks == before, "invalid voltage: no kick, 10 s");
    regs[0x04] = regs[0x05] = 0;
    sample(4200, -100, CX25601N_NOT_CHARGING, 0);
    CHECK(kicks == before + 1, "negative current clamps to zero as vendor");
    int mv, iterm;
    bool term;
    CHECK(cx25601n_charge_config(&charger, &mv, &iterm, &term) == HAL_I2C_OK && mv == 4400, "periodic VREG rewrite restores drift");
}
int main(void) {
    defaults(); undervoltage(); recharge();
    printf("board-vendor charger: %d checks passed\n", checks);
    return 0;
}
