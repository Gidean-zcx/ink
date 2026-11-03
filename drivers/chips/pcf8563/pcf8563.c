#include "pcf8563/pcf8563.h"

enum { REG_CTRL2 = 0x01, REG_SECONDS = 0x02, REG_MIN_ALARM = 0x09 };
#define CTRL2_AIE 0x02
#define CTRL2_AF 0x08
#define VL 0x80
#define AE 0x80 /* alarm field disabled */

static uint8_t bcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
static int dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }

/* Howard Hinnant's days_from_civil / civil_from_days */
int64_t pcf8563_days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

void pcf8563_civil_from_days(int64_t z, int *y, int *m, int *d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yoe + era * 400 + (*m <= 2));
}

hal_i2c_status_t pcf8563_get(hal_i2c_dev_t *dev, int64_t *unix_s, bool *valid) {
    uint8_t reg = REG_SECONDS, t[7];
    hal_i2c_status_t s = hal_i2c_write_read(dev, &reg, 1, t, sizeof t);
    if (s != HAL_I2C_OK) return s;
    int sec = dec(t[0] & 0x7F), min = dec(t[1] & 0x7F), hour = dec(t[2] & 0x3F), day = dec(t[3] & 0x3F);
    int month = dec(t[5] & 0x1F), year = 2000 + dec(t[6]) - ((t[5] & 0x80) ? 100 : 0); /* century bit: 19xx */
    *unix_s = pcf8563_days_from_civil(year, month, day) * 86400 + hour * 3600 + min * 60 + sec;
    if (valid) *valid = !(t[0] & VL);
    return HAL_I2C_OK;
}

hal_i2c_status_t pcf8563_set(hal_i2c_dev_t *dev, int64_t unix_s) {
    int64_t days = unix_s / 86400, rem = unix_s % 86400;
    int y, m, d;
    pcf8563_civil_from_days(days, &y, &m, &d);
    int wday = (int)((days % 7 + 11) % 7); /* 1970-01-01 was a Thursday (4) */
    uint8_t b[8] = {REG_SECONDS, bcd((int)(rem % 60)), bcd((int)(rem / 60 % 60)), bcd((int)(rem / 3600)), bcd(d), (uint8_t)wday,
                    (uint8_t)(bcd(m) | (y < 2000 ? 0x80 : 0)), bcd(y % 100)};
    return hal_i2c_write(dev, b, sizeof b);
}

hal_i2c_status_t pcf8563_set_alarm(hal_i2c_dev_t *dev, int64_t unix_s) {
    int64_t days = unix_s / 86400, rem = unix_s % 86400;
    int y, m, d;
    pcf8563_civil_from_days(days, &y, &m, &d);
    uint8_t a[5] = {REG_MIN_ALARM, bcd((int)(rem / 60 % 60)), bcd((int)(rem / 3600)), bcd(d), AE};
    hal_i2c_status_t s = hal_i2c_write(dev, a, sizeof a);
    uint8_t c[2] = {REG_CTRL2, CTRL2_AIE}; /* AF cleared, alarm interrupt on */
    return s == HAL_I2C_OK ? hal_i2c_write(dev, c, sizeof c) : s;
}

hal_i2c_status_t pcf8563_clear_alarm(hal_i2c_dev_t *dev) {
    uint8_t c[2] = {REG_CTRL2, 0};
    hal_i2c_status_t s = hal_i2c_write(dev, c, sizeof c);
    uint8_t a[5] = {REG_MIN_ALARM, AE, AE, AE, AE};
    return s == HAL_I2C_OK ? hal_i2c_write(dev, a, sizeof a) : s;
}

hal_i2c_status_t pcf8563_alarm_flag(hal_i2c_dev_t *dev, bool *fired) {
    uint8_t reg = REG_CTRL2, v;
    hal_i2c_status_t s = hal_i2c_write_read(dev, &reg, 1, &v, 1);
    if (s == HAL_I2C_OK) *fired = v & CTRL2_AF;
    return s;
}
