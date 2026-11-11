#include "cx25601n/cx25601n.h"
#include "ink/osal.h"

enum { REG_ICHG_LO = 0x02, REG_ICHG_HI = 0x03, REG_CHG_CTRL0 = 0x14, REG_CHG_CTRL1 = 0x16,
       REG_CHG_CTRL3 = 0x18, REG_STATUS1 = 0x1E, REG_PART_INFO = 0x38, VREG_MV = 4400, UV_MV = 3100 };

static hal_i2c_status_t rd(hal_i2c_dev_t *dev, uint8_t reg, uint8_t *v) { return hal_i2c_write_read(dev, &reg, 1, v, 1); }
static hal_i2c_status_t wr(hal_i2c_dev_t *dev, uint8_t reg, uint8_t v) {
    uint8_t b[2] = {reg, v};
    return hal_i2c_write(dev, b, 2);
}
static hal_i2c_status_t update(hal_i2c_dev_t *dev, uint8_t reg, uint8_t mask, int shift, uint8_t value) {
    uint8_t v;
    hal_i2c_status_t s = rd(dev, reg, &v);
    if (s != HAL_I2C_OK) return s;
    v = (uint8_t)((v & ~(mask << shift)) | ((value & mask) << shift));
    return wr(dev, reg, v);
}

hal_i2c_status_t cx25601n_part_info(hal_i2c_dev_t *dev, uint8_t *part) { return rd(dev, REG_PART_INFO, part); }

hal_i2c_status_t cx25601n_set_charge_current(hal_i2c_dev_t *dev, int ma, int *applied_ma) {
    if (ma < CX25601N_ICHG_MIN_MA) ma = CX25601N_ICHG_MIN_MA;
    if (ma > CX25601N_ICHG_MAX_MA) ma = CX25601N_ICHG_MAX_MA;
    int code = ma / CX25601N_ICHG_STEP_MA;
    hal_i2c_status_t s = update(dev, REG_ICHG_LO, 0x03, 6, (uint8_t)(code & 0x03));
    if (s == HAL_I2C_OK) s = update(dev, REG_ICHG_HI, 0x0F, 0, (uint8_t)((code >> 2) & 0x0F));
    /* Vendor cx25601n.cc:548-581: IINDPM follows the quantized ICHG. */
    int input = code * 4;
    if (input < 5) input = 5;
    if (input > 150) input = 150;
    if (s == HAL_I2C_OK) s = update(dev, 0x06, 0x0F, 4, (uint8_t)(input & 0x0F));
    if (s == HAL_I2C_OK) s = update(dev, 0x07, 0x0F, 0, (uint8_t)(input >> 4));
    if (applied_ma) *applied_ma = code * CX25601N_ICHG_STEP_MA;
    return s;
}

/* Vendor cx25601n.cc:428-447: EN_CHG is distinct from EN_TERM. */
hal_i2c_status_t cx25601n_enable_charging(hal_i2c_dev_t *dev, bool on) {
    hal_i2c_status_t s = on ? update(dev, REG_CHG_CTRL1, 1, 4, 0) : HAL_I2C_OK;
    return s == HAL_I2C_OK ? update(dev, REG_CHG_CTRL1, 1, 5, on) : s;
}

hal_i2c_status_t cx25601n_status(hal_i2c_dev_t *dev, cx25601n_chg_t *chg, int *vbus) {
    uint8_t v;
    hal_i2c_status_t s = rd(dev, REG_STATUS1, &v);
    if (s != HAL_I2C_OK) return s;
    *chg = (cx25601n_chg_t)((v >> 3) & 0x03);
    *vbus = v & 0x07;
    return HAL_I2C_OK;
}

hal_i2c_status_t cx25601n_ship_mode(hal_i2c_dev_t *dev) { return wr(dev, REG_CHG_CTRL3, 0x01); }

/* Adapted from CloudZao/Metalio-E-INK4-Plus @874538e, common/power/cx25601n.cc:89-209,314-341.
 * Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd.
 * Copyright (c) 2025 Project Contributors. MIT; see LICENSE.vendor. */
static hal_i2c_status_t set_vreg(hal_i2c_dev_t *dev) {
    hal_i2c_status_t s = update(dev, 0x04, 0x1F, 3, (VREG_MV / 10) & 0x1F);
    return s == HAL_I2C_OK ? update(dev, 0x05, 0x0F, 0, (VREG_MV / 10) >> 5) : s;
}

hal_i2c_status_t cx25601n_charge_config(hal_i2c_dev_t *dev, int *vreg_mv, int *iterm_ma, bool *en_term) {
    uint8_t vl, vh, il, ih, term;
    hal_i2c_status_t s = rd(dev, 0x04, &vl);
    if (s == HAL_I2C_OK) s = rd(dev, 0x05, &vh);
    if (s == HAL_I2C_OK) s = rd(dev, 0x12, &il);
    if (s == HAL_I2C_OK) s = rd(dev, 0x13, &ih);
    if (s == HAL_I2C_OK) s = rd(dev, REG_CHG_CTRL0, &term);
    if (s != HAL_I2C_OK) return s;
    *vreg_mv = ((vl >> 3) | ((vh & 0x0F) << 5)) * 10;
    *iterm_ma = ((il >> 3) | ((ih & 1) << 5)) * 10;
    *en_term = (term & 4) != 0;
    return HAL_I2C_OK;
}

hal_i2c_status_t cx25601n_init_defaults(hal_i2c_dev_t *dev) {
    /* Vendor :314-328; preserve unrelated register bits. */
    static const uint8_t defaults[][4] = {
        {0x15, 1, 6, 0}, {0x16, 1, 4, 0}, {0x16, 3, 0, 0},
        {0x10, 15, 4, 12}, {0x11, 1, 0, 0}, {0x12, 31, 3, 6}, {0x13, 1, 0, 0}
    };
    for (unsigned i = 0; i < sizeof defaults / sizeof defaults[0]; i++) {
        const uint8_t *d = defaults[i];
        hal_i2c_status_t s = update(dev, d[0], d[1], d[2], d[3]);
        if (s != HAL_I2C_OK) return s;
    }
    hal_i2c_status_t s = set_vreg(dev);
    if (s == HAL_I2C_OK) s = cx25601n_set_charge_current(dev, 1000, NULL);
    if (s != HAL_I2C_OK) return s;
    uint8_t unlocked = 0;
    for (int i = 0; i < 10 && unlocked != 3; i++) {
        static const uint8_t key[] = {0, 0x50, 0x57, 0x44};
        for (unsigned j = 0; j < sizeof key; j++) {
            s = wr(dev, 0x70, key[j]);
            if (s != HAL_I2C_OK) return s;
        }
        s = rd(dev, 0x70, &unlocked);
        if (s != HAL_I2C_OK) return s;
    }
    if (unlocked != 3) return HAL_I2C_ERR;
    static const uint8_t private_regs[][2] = {{0x86, 6}, {0x3A, 0x10}, {0x46, 0x20}, {0x70, 0}};
    for (unsigned i = 0; i < sizeof private_regs / sizeof private_regs[0]; i++) {
        s = wr(dev, private_regs[i][0], private_regs[i][1]);
        if (s != HAL_I2C_OK) return s;
    }
    static const uint8_t controls[][4] = {{0x14, 1, 0, 1}, {0x18, 1, 2, 0}, {0x1A, 1, 7, 1}, {0x23, 7, 2, 7}, {0x24, 1, 3, 1}};
    for (unsigned i = 0; i < sizeof controls / sizeof controls[0]; i++) {
        const uint8_t *d = controls[i];
        s = update(dev, d[0], d[1], d[2], d[3]);
        if (s != HAL_I2C_OK) return s;
    }
    return cx25601n_enable_charging(dev, true);
}

uint32_t cx25601n_poll(hal_i2c_dev_t *dev, int mv, int ma, bool *uv_done) {
    bool below = mv >= 2500 && mv < VREG_MV;
    uint32_t interval = below ? 2000 : 10000; /* Vendor :162-175. */
    cx25601n_chg_t stat;
    int vbus;
    if (set_vreg(dev) != HAL_I2C_OK || cx25601n_status(dev, &stat, &vbus) != HAL_I2C_OK) return interval;
    /* Vendor :185-197: the latch resets strictly above 3200 mV; it does not reconnect BATFET. */
    if (!*uv_done && mv >= 2500 && mv <= UV_MV && vbus == 0) {
        if (cx25601n_ship_mode(dev) == HAL_I2C_OK) *uv_done = true;
    } else if (*uv_done && mv > UV_MV + 100) *uv_done = false;
    if (update(dev, REG_CHG_CTRL0, 1, 2, !below) != HAL_I2C_OK) return interval;
    if (stat == CX25601N_NOT_CHARGING && ma < 5 && below) { /* Vendor :135-159,201-207; negative current clamps to zero. */
        if (cx25601n_enable_charging(dev, false) != HAL_I2C_OK || update(dev, REG_CHG_CTRL1, 1, 4, 0) != HAL_I2C_OK) return interval;
        osal_task_delay_us(200000);
        if (cx25601n_enable_charging(dev, true) != HAL_I2C_OK) return interval;
        osal_task_delay_us(500000);
        cx25601n_status(dev, &stat, &vbus);
    }
    return interval;
}
