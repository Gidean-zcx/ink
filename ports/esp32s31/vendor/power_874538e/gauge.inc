/* CloudZao Metalio-E-INK4-Plus @874538e. MIT: LICENSE; adaptations: SOURCE.md. */
#include "binding.h"
#define TAG "Bq27220Gauge"
    enum { kFilterSize = 60, kSocStepPct = 5 };

    static const int64_t kStartupDelayUs = 3LL * 1000 * 1000;
    static const int64_t kSamplePeriodUs = 1000LL * 1000;
    static const int64_t kSocStepUs = 30LL * 1000 * 1000;
    static const int64_t kFullHoldUs = 30LL * 1000 * 1000;
    static const uint16_t kFullConfirmMv = 4300; // 显示满电；充电截止见 CX25601N VREG
    static const uint16_t kFullExitMv = 4280;

static power_i2c_t bus_ = {0};
static bool        bus_valid_ = false;
static uint8_t     addr_ = 0x55;
static bool        ready_ = false;
static int consecutive_err_ = 0;
static int retry_counter_ = 0;
static float filter_buf_[kFilterSize] = {0};
static int   filter_idx_ = 0;
static int   filter_count_ = 0;
static float filter_sum_ = 0.0f;
static bool  filter_primed_ = false;
static int     displayed_soc_ = -1;
static uint32_t battery_snapshot_ = 0;
static osal_mutex_t *soc_mutex_;
static int64_t last_sample_us_ = 0;
static int64_t last_filter_us_ = 0;
static int64_t sample_ready_at_us_ = 3000000;
static int64_t last_soc_step_us_ = 0;
static int64_t full_above_since_us_ = 0;
static bool full_confirmed_ = false; // 满电滞回状态，唤醒后以新电压校验
static int     last_charge_dir_ = 0;
float ink_bq_VoltageToPercent(uint16_t mv, bool charging);
bool ink_bq_Begin(const power_i2c_t* bus, uint8_t addr);
bool ink_bq_ReadVoltageMv(uint16_t* mv);
bool ink_bq_ReadCurrentMa(int16_t* current_ma);
int ink_bq_ApplyMonoStep(int target_pct, bool charging);
bool ink_bq_GetBatteryLevel(int* level, bool* charging, bool* discharging, bq27220_reading_t *sample);
bool ink_bq_GetCachedBatteryLevel(int* level, bool* charging, bool* discharging);
void ink_bq_ResetFilter(void);
bool ink_bq_ReadU16(uint8_t reg, uint16_t* out);
float ink_bq_FilterPush(float sample);

static const uint8_t  kRegVoltage   = 0x08;
static const uint8_t  kRegCurrent   = 0x0C;
static const int      kI2cTimeoutMs = 200; // 含时钟拉伸
static const int      kProbeTimeoutMs = 100;
static const uint16_t kBatteryEmptyMv = 3300;
// 424860-2000：工步 6，约 1A 放电至 3.3V，以 1870.7mAh 归一化，每 10% 取点。
static const uint16_t kDischargeMv[] = {
    3300, 3481, 3565, 3624, 3663, 3702, 3757, 3828, 3919, 4021, 4211
};


float ink_bq_VoltageToPercent(uint16_t mv, bool charging) {
    if (mv <= kBatteryEmptyMv) {
        return 0.0f;
    }
    if (charging) {
        if (mv >= kFullConfirmMv) {
            return 100.0f;
        }
        return (mv - kBatteryEmptyMv) * 100.0f / (kFullConfirmMv - kBatteryEmptyMv);
    }
    for (int i = 1; i < 11; ++i) {
        if (mv <= kDischargeMv[i]) {
            return (i - 1) * 10.0f + (mv - kDischargeMv[i - 1]) * 10.0f /
                   (kDischargeMv[i] - kDischargeMv[i - 1]);
        }
    }
    return 100.0f;
}

bool ink_bq_Begin(const power_i2c_t* bus, uint8_t addr) {
    if (bus == NULL) {
        ESP_LOGW(TAG, "ink_bq_Begin() called with null bus");
        return false;
    }
    bus_ = *bus;
    bus_valid_ = true;
    addr_ = addr;
    if (ready_) {
        return true;
    }

    esp_err_t probe = power_i2c_probe(&bus_, addr_, kProbeTimeoutMs);
    if (probe != ESP_OK) {
        static bool warned = false;
        if (!warned) {
            ESP_LOGW(TAG,
                     "BQ27220 @0x%02X probe NACK (err=0x%x)，"
                     "暂不显示电量；插上电池/上电后会自动重试",
                     addr_, probe);
            warned = true;
        }
        return false;
    }

    // 仅电压读通才算 ready。手册：标准命令 ≤2 次/秒，间隔至少 500ms
    uint16_t mv = 0;
    for (int i = 0; i < 2; ++i) {
        if (i > 0) {
            osal_task_delay_us((uint64_t)(500) * 1000);
        }
        if (ink_bq_ReadU16(kRegVoltage, &mv) && mv >= 2500 && mv <= 5000) {
            ready_ = true;
            ESP_LOGI(TAG, "BQ27220 online @0x%02X, voltage=%u mV", addr_, mv);
            return true;
        }
    }
    ESP_LOGW(TAG, "BQ27220 @0x%02X ACK 但读电压失败，稍后重试", addr_);
    return false;
}

bool ink_bq_ReadVoltageMv(uint16_t* mv) {
    if (!bus_valid_) {
        return false;
    }
    if (!ready_ && !ink_bq_Begin(&bus_, addr_)) {
        return false;
    }
    return ink_bq_ReadU16(kRegVoltage, mv);
}

bool ink_bq_ReadCurrentMa(int16_t* current_ma) {
    if (!bus_valid_) {
        return false;
    }
    if (!ready_ && !ink_bq_Begin(&bus_, addr_)) {
        return false;
    }
    uint16_t raw = 0;
    if (!ink_bq_ReadU16(kRegCurrent, &raw)) {
        return false;
    }
    (*current_ma) = (int16_t)(raw);
    return true;
}

int ink_bq_ApplyMonoStep(int target_pct, bool charging) {
    if (target_pct < 0) {
        target_pct = 0;
    }
    if (target_pct > 100) {
        target_pct = 100;
    }
    target_pct = (target_pct / kSocStepPct) * kSocStepPct;

    const int64_t now = (int64_t)osal_now_us();
    if (displayed_soc_ < 0) {
        displayed_soc_ = target_pct;
        last_soc_step_us_ = now;
        return displayed_soc_;
    }

    if (now - last_soc_step_us_ < kSocStepUs) {
        return displayed_soc_;
    }

    if (charging) {
        if (target_pct > displayed_soc_) {
            displayed_soc_ += kSocStepPct;
            last_soc_step_us_ = now;
        }
    } else {
        if (target_pct < displayed_soc_) {
            displayed_soc_ -= kSocStepPct;
            last_soc_step_us_ = now;
        }
    }
    return displayed_soc_;
}

bool ink_bq_GetBatteryLevel(int* level, bool* charging, bool* discharging, bq27220_reading_t *sample) {
    if ((int64_t)osal_now_us() < sample_ready_at_us_) {
        __atomic_store_n(&battery_snapshot_, 0, __ATOMIC_RELEASE);
        return false;
    }
    if (!ready_) {
        full_above_since_us_ = 0;
        if (++retry_counter_ % 10 != 1) {
            __atomic_store_n(&battery_snapshot_, 0, __ATOMIC_RELEASE);
            return false;
        }
        if (!bus_valid_ || !ink_bq_Begin(&bus_, addr_)) {
            __atomic_store_n(&battery_snapshot_, 0, __ATOMIC_RELEASE);
            return false;
        }
    }

    uint16_t mv = 0;
    if (!ink_bq_ReadU16(kRegVoltage, &mv) || mv < 2500 || mv > 5000) {
        full_above_since_us_ = 0;
        __atomic_store_n(&battery_snapshot_, 0, __ATOMIC_RELEASE);
        return false;
    }
    int16_t current_ma = 0;
    if (!ink_bq_ReadCurrentMa(&current_ma)) {
        full_above_since_us_ = 0;
        __atomic_store_n(&battery_snapshot_, 0, __ATOMIC_RELEASE);
        return false;
    }
    const int64_t now = (int64_t)osal_now_us();
    if (last_sample_us_ != 0 && now - last_sample_us_ > 2 * kSamplePeriodUs) {
        full_above_since_us_ = 0;
    }
    last_sample_us_ = now;
    const bool raw_charging = (current_ma > 5);
    // 两条曲线的百分比不能混合滤波；保留显示值及正常步进限制。
    if (displayed_soc_ >= 0 && raw_charging != (last_charge_dir_ > 0)) {
        filter_primed_ = false;
    }
    float smoothed;
    if (!filter_primed_ || now - last_filter_us_ >= kSamplePeriodUs) {
        smoothed = ink_bq_FilterPush(ink_bq_VoltageToPercent(mv, raw_charging));
        last_filter_us_ = now;
    } else {
        smoothed = filter_sum_ / filter_count_;
    }
    int target = (int)(smoothed);
    if (target < 0) {
        target = 0;
    }
    if (target > 100) {
        target = 100;
    }

    bool full_ok = false;
    if (mv >= kFullConfirmMv) {
        if (full_above_since_us_ == 0) {
            full_above_since_us_ = now;
        }
        full_ok = (now - full_above_since_us_ >= kFullHoldUs);
    } else {
        full_above_since_us_ = 0;
    }
    if (full_ok) {
        full_confirmed_ = true;
    }
    if (mv < kFullExitMv) {
        full_confirmed_ = false;
    }
    if (target >= 100 && !full_confirmed_) {
        target = 100 - kSocStepPct;
    }
    if (full_confirmed_) {
        target = 100;
    }

    (*discharging) = (current_ma < -5);
    const int charge_dir = raw_charging ? 1 : ((*discharging) ? -1 : 0);

    if (displayed_soc_ >= 0 && charge_dir != last_charge_dir_) {
        last_soc_step_us_ = now;
        last_charge_dir_ = charge_dir;
    } else if (displayed_soc_ < 0) {
        last_charge_dir_ = charge_dir;
    }

    // 尾充终止后电流可归零，已确认满电仍允许沿步进升至 100%。
    (*level) = ink_bq_ApplyMonoStep(target, raw_charging || full_confirmed_);
    (*charging) = raw_charging && ((*level) < 100);
    __atomic_store_n(&battery_snapshot_, 0x100u | (uint32_t)(*level) |
        ((*charging) ? 0x200u : 0) | ((*discharging) ? 0x400u : 0), __ATOMIC_RELEASE);
    sample->mv = mv;
    sample->ma = current_ma;
    return true;
}

bool ink_bq_GetCachedBatteryLevel(int* level, bool* charging, bool* discharging) {
    const uint32_t snapshot = __atomic_load_n(&battery_snapshot_, __ATOMIC_ACQUIRE);
    if (!(snapshot & 0x100u)) return false;
    (*level) = snapshot & 0xffu;
    (*charging) = (snapshot & 0x200u) != 0;
    (*discharging) = (snapshot & 0x400u) != 0;
    return true;
}

void ink_bq_ResetFilter(void) {
    __atomic_store_n(&battery_snapshot_, 0, __ATOMIC_RELEASE);
    osal_mutex_lock(soc_mutex_, OSAL_WAIT_FOREVER);
    __atomic_store_n(&battery_snapshot_, 0, __ATOMIC_RELEASE);
    sample_ready_at_us_ = (int64_t)osal_now_us() + kStartupDelayUs;
    filter_idx_ = 0;
    filter_count_ = 0;
    filter_sum_ = 0.0f;
    filter_primed_ = false;
    displayed_soc_ = -1;
    last_soc_step_us_ = 0;
    full_above_since_us_ = 0;
    last_charge_dir_ = 0;
    last_sample_us_ = 0;
    last_filter_us_ = 0;
    osal_mutex_unlock(soc_mutex_);
}

bool ink_bq_ReadU16(uint8_t reg, uint16_t* out) {
    if (!bus_valid_ || out == NULL) {
        return false;
    }
    uint8_t buf[2] = {0};
    esp_err_t err = power_i2c_write_read(&bus_, addr_, reg, buf, sizeof(buf), kI2cTimeoutMs);
    if (err != ESP_OK) {
        if (++consecutive_err_ % 10 == 1) {
            ESP_LOGW(TAG, "BQ27220 read reg 0x%02X failed: 0x%x (consecutive=%d)", reg, err,
                     consecutive_err_);
        }
        // 连续失败则降 ready，交给 GetBatteryLevel 节流重挂
        if (consecutive_err_ >= 5) {
            ready_ = false;
        }
        return false;
    }
    consecutive_err_ = 0;
    *out = (uint16_t)(buf[0]) | ((uint16_t)(buf[1]) << 8);
    return true;
}

float ink_bq_FilterPush(float sample) {
    if (!filter_primed_) {
        for (int i = 0; i < kFilterSize; ++i) {
            filter_buf_[i] = sample;
        }
        filter_sum_ = sample * kFilterSize;
        filter_count_ = kFilterSize;
        filter_idx_ = 0;
        filter_primed_ = true;
        return sample;
    }
    filter_sum_ -= filter_buf_[filter_idx_];
    filter_buf_[filter_idx_] = sample;
    filter_sum_ += sample;
    filter_idx_ = (filter_idx_ + 1) % kFilterSize;
    if (filter_count_ < kFilterSize) {
        filter_count_++;
    }
    return filter_sum_ / filter_count_;
}
