/* CloudZao Metalio-E-INK4-Plus @874538e. MIT: LICENSE. Adaptations: SOURCE.md. */
#define constexpr static const
constexpr uint8_t kRegCtrlReg1 = 0x20;
constexpr uint8_t kRegCtrlReg2 = 0x21;
constexpr uint8_t kRegCtrlReg3 = 0x22;
constexpr uint8_t kRegCtrlReg4 = 0x23;
constexpr uint8_t kRegCtrlReg5 = 0x24;
constexpr uint8_t kRegCtrlReg6 = 0x25;
constexpr uint8_t kRegOutXL = 0x28;
constexpr uint8_t kRegAoi1Cfg = 0x30;
constexpr uint8_t kRegAoi1Src = 0x31;
constexpr uint8_t kRegAoi1Ths = 0x32;
constexpr uint8_t kRegAoi1Dur = 0x33;
constexpr uint8_t kAutoIncMask = 0x80;
// CTRL_REG1=0x57：ODR=100Hz，XYZ 使能；CTRL_REG4=0x88：BDU+HR，±2g
constexpr uint8_t kCtrlReg1Val = 0x57;
constexpr uint8_t kCtrlReg2Val = 0x01; // HPIS1：AOI1 用高通后数据，活动检测不受重力常偏
constexpr uint8_t kCtrlReg3Val = 0x40; // I1_AOI1 → INT1
constexpr uint8_t kCtrlReg4Val = 0x88;
constexpr uint8_t kCtrlReg5Val = 0x08; // LIR_INT1：锁存至读 AOI1_SRC
// CTRL_REG6：INT_PP_OD(bit0)=1 开漏 + H_LACTIVE(bit1)=1 低有效，与 TCA INT 线与
constexpr uint8_t kCtrlReg6Val = 0x03;
constexpr uint8_t kAoi1CfgVal = 0x2A; // OR：XHIE|YHIE|ZHIE
constexpr uint8_t kAoi1ThsVal = 0x10; // ±2g 约 16mg/LSB → ~256mg
constexpr uint8_t kAoi1DurVal = 0x00;
constexpr float   kMgPerLsb = 1.0f;

#define WriteReg(r,v) (wr(dev, (r), (v)) == HAL_I2C_OK)
#define ReadRegs(r,p,n) (hal_i2c_write_read(dev, &(uint8_t){r}, 1, (p), (n)) == HAL_I2C_OK)
static bool ink_vendor_accel_configure(hal_i2c_dev_t *dev) {
    if (!WriteReg(kRegCtrlReg1, kCtrlReg1Val)) {
        return false;
    }
    if (!WriteReg(kRegCtrlReg2, kCtrlReg2Val)) {
        return false;
    }
    if (!WriteReg(kRegCtrlReg3, kCtrlReg3Val)) {
        return false;
    }
    if (!WriteReg(kRegCtrlReg4, kCtrlReg4Val)) {
        return false;
    }
    if (!WriteReg(kRegCtrlReg5, kCtrlReg5Val)) {
        return false;
    }
    // INT1 开漏低有效：与 TCA 共用 GPIO2 时不把总线钳成推挽高
    if (!WriteReg(kRegCtrlReg6, kCtrlReg6Val)) {
        return false;
    }
    if (!WriteReg(kRegAoi1Ths, kAoi1ThsVal)) {
        return false;
    }
    if (!WriteReg(kRegAoi1Dur, kAoi1DurVal)) {
        return false;
    }
    return WriteReg(kRegAoi1Cfg, kAoi1CfgVal);
}

static void ink_vendor_accel_silence(hal_i2c_dev_t *dev) {
    (void)WriteReg(kRegAoi1Cfg, 0x00);
    (void)WriteReg(kRegCtrlReg3, 0x00);
    (void)WriteReg(kRegCtrlReg5, 0x00);
    uint8_t src;
    (void)ReadRegs(kRegAoi1Src, &src, 1);
    (void)WriteReg(kRegCtrlReg1, 0x10); // ODR=1Hz、XYZ 关（勿 PD，易钳死共享 INT）
    (void)WriteReg(kRegCtrlReg6, 0x00); // 推挽高有效：空闲驱高
}

static bool ink_vendor_accel_read(hal_i2c_dev_t *dev, int *ax, int *ay, int *az) {
    uint8_t buf[6] = {0};
    if (!ReadRegs((uint8_t)(kRegOutXL | kAutoIncMask), buf, sizeof(buf))) {
        return false;
    }
    const int16_t rx = (int16_t)((buf[1] << 8) | buf[0]);
    const int16_t ry = (int16_t)((buf[3] << 8) | buf[2]);
    const int16_t rz = (int16_t)((buf[5] << 8) | buf[4]);
    *ax = (int)((rx >> 4) * kMgPerLsb);
    *ay = (int)((ry >> 4) * kMgPerLsb);
    *az = (int)((rz >> 4) * kMgPerLsb);
    return true;
}

#undef WriteReg
#undef ReadRegs

#undef constexpr
