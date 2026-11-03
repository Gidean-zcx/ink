#include "cx25601n/cx25601n.h"

enum { REG_ICHG_LO = 0x02, REG_ICHG_HI = 0x03, REG_CHG_CTRL0 = 0x14, REG_CHG_CTRL3 = 0x18, REG_STATUS1 = 0x1E, REG_PART_INFO = 0x38 };

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
    if (applied_ma) *applied_ma = code * CX25601N_ICHG_STEP_MA;
    return s;
}

hal_i2c_status_t cx25601n_enable_charging(hal_i2c_dev_t *dev, bool on) { return update(dev, REG_CHG_CTRL0, 0x01, 2, on); }

hal_i2c_status_t cx25601n_status(hal_i2c_dev_t *dev, cx25601n_chg_t *chg, int *vbus) {
    uint8_t v;
    hal_i2c_status_t s = rd(dev, REG_STATUS1, &v);
    if (s != HAL_I2C_OK) return s;
    *chg = (cx25601n_chg_t)((v >> 3) & 0x03);
    *vbus = v & 0x07;
    return HAL_I2C_OK;
}

hal_i2c_status_t cx25601n_ship_mode(hal_i2c_dev_t *dev) { return wr(dev, REG_CHG_CTRL3, 0x01); }
