/* CloudZao/Metalio-E-INK4-Plus @874538e; MIT, see LICENSE and SOURCE.md. */
static const char* TAG_TPS = "tps65185";
static const int EPDIY_TPS_ADDR = 0x68;
#define I2C1_LOCK_MS 100

static hal_i2c_status_t i2c_master_read_slave_ex(hal_i2c_dev_t * port, int reg, uint8_t* out) {
    if (!metalio_i2c1_lock(I2C1_LOCK_MS)) return HAL_I2C_TIMEOUT;
    uint8_t value = 0;
    const hal_i2c_status_t ret = metalio_i2c1_read_reg_stop_locked(port, EPDIY_TPS_ADDR,
                                                          (uint8_t)reg, &value, 1, 1000);
    metalio_i2c1_unlock();
    if (ret == HAL_I2C_OK && out != NULL) *out = value;
    return ret;
}

static uint8_t i2c_master_read_slave(hal_i2c_dev_t * i2c_num, int reg) {
    uint8_t v = 0;
    hal_i2c_status_t ret = i2c_master_read_slave_ex(i2c_num, reg, &v);
    if (ret != HAL_I2C_OK) {
        ESP_LOGW(TAG_TPS, "read reg 0x%02X: %s", reg, hal_i2c_status_name(ret));
        return 0;
    }
    return v;
}

static hal_i2c_status_t i2c_master_write_slave(hal_i2c_dev_t * port, uint8_t reg, uint8_t* data, size_t size) {
    if (!metalio_i2c1_lock(I2C1_LOCK_MS)) return HAL_I2C_TIMEOUT;
    const hal_i2c_status_t ret = metalio_i2c1_write_reg_locked(port, EPDIY_TPS_ADDR, reg, data, size, 1000);
    metalio_i2c1_unlock();
    return ret;
}

hal_i2c_status_t tps_write_register(hal_i2c_dev_t * port, int reg, uint8_t value) {
    uint8_t w_data[1] = {value};
    return i2c_master_write_slave(port, reg, w_data, 1);
}

uint8_t tps_read_register(hal_i2c_dev_t * i2c_num, int reg) {
    return i2c_master_read_slave(i2c_num, reg);
}

hal_i2c_status_t tps_read_register_ex(hal_i2c_dev_t * i2c_num, int reg, uint8_t* value) {
    return i2c_master_read_slave_ex(i2c_num, reg, value);
}

hal_i2c_status_t tps_probe(hal_i2c_dev_t * port) {
    if (!metalio_i2c1_lock(I2C1_LOCK_MS)) return HAL_I2C_TIMEOUT;
    const hal_i2c_status_t ret = metalio_i2c1_probe_locked(port, EPDIY_TPS_ADDR, 50);
    metalio_i2c1_unlock();
    return ret;
}

void tps_set_vcom(hal_i2c_dev_t * i2c_num, unsigned vcom_mV) {
    unsigned val = vcom_mV / 10;
    hal_i2c_status_t err = tps_write_register(i2c_num, 4, (val & 0x100) >> 8);
    if (err != HAL_I2C_OK) {
        ESP_LOGW(TAG_TPS, "VCOM2 write: %s", hal_i2c_status_name(err));
        return;
    }
    err = tps_write_register(i2c_num, 3, val & 0xFF);
    if (err != HAL_I2C_OK) {
        ESP_LOGW(TAG_TPS, "VCOM1 write: %s", hal_i2c_status_name(err));
    }
}

