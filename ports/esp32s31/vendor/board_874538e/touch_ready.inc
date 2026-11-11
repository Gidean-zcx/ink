/* Vendor Attempt app-readiness phase only; firmware writes are excluded. SOURCE.md. */
static bool ink_vendor_touch_ready(hal_i2c_dev_t *dev, uint8_t *fw) {
    uint8_t chip = 0, version = 0;
    const uint8_t id_reg = 0xa3, version_reg = 0xa6;
    bool app_valid = false;
    for (int i = 0; i < 5; ++i) {
        const bool id_ok = read_regs(dev, id_reg, &chip, 1) == HAL_I2C_OK;
        const bool version_ok = read_regs(dev, version_reg, &version, 1) == HAL_I2C_OK;
        if (id_ok && version_ok && chip == 0x64 && version != 0xff) {
            app_valid = true;
            break;
        }
        osal_task_delay_us(200000);
    }
    if (app_valid && fw) *fw = version;
    return app_valid;
}
