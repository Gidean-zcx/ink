#include "ft6336u/ft6336u.h"

enum { REG_TD_STATUS = 0x02, REG_INT_MODE = 0xA4, REG_FW_VER = 0xA6, REG_CHIP_ID = 0xA3 };

static hal_i2c_status_t read_regs(hal_i2c_dev_t *dev, uint8_t reg, uint8_t *buf, size_t n) {
    return hal_i2c_write_read(dev, &reg, 1, buf, n);
}

hal_i2c_status_t ft6336u_init(hal_i2c_dev_t *dev, uint8_t *fw_version) {
    uint8_t id = 0, fw = 0;
    hal_i2c_status_t s = read_regs(dev, REG_CHIP_ID, &id, 1);
    if (s != HAL_I2C_OK) return s;
    if (id != FT6336U_CHIP_ID) return HAL_I2C_ERR;
    s = read_regs(dev, REG_FW_VER, &fw, 1);
    if (s != HAL_I2C_OK) return s;
    if (fw_version) *fw_version = fw;
    uint8_t mode[2] = {REG_INT_MODE, 0x00};
    return hal_i2c_write(dev, mode, sizeof mode);
}

hal_i2c_status_t ft6336u_read(hal_i2c_dev_t *dev, ft6336u_point_t *pts, int max_points, int *n) {
    uint8_t buf[1 + 6 * FT6336U_MAX_POINTS];
    if (max_points > FT6336U_MAX_POINTS) max_points = FT6336U_MAX_POINTS;
    if (max_points < 1) max_points = 1;
    *n = 0;
    hal_i2c_status_t s = read_regs(dev, REG_TD_STATUS, buf, 1 + 6 * (size_t)max_points);
    if (s != HAL_I2C_OK) return s;
    int count = buf[0] & 0x0F;
    if (count > max_points) count = count > FT6336U_MAX_POINTS ? 0 : max_points; /* >2: not a valid frame */
    for (int i = 0; i < count; i++) {
        const uint8_t *p = &buf[1 + 6 * i];
        ft6336u_point_t pt = {.event = p[0] >> 6, .x = ((p[0] & 0x0F) << 8) | p[1], .id = p[2] >> 4, .y = ((p[2] & 0x0F) << 8) | p[3]};
        if (pt.event == 1 || pt.event == 3) continue; /* lift up / no event */
        pts[(*n)++] = pt;
    }
    return HAL_I2C_OK;
}
