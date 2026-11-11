/* CloudZao Metalio-E-INK4-Plus @874538e. MIT: LICENSE. Adaptations: SOURCE.md. */
#define TAG "MetalioKeys"
static const int64_t kDebounceUs = 30000;
enum { kBoot, kVolUp, kVolDown, kPower, kCount };
static uint8_t ink_vendor_p1 = 0xff;
typedef struct {
    const char* label;
    int io; // <0：GPIO BOOT；≥0：TCA 脚号
    bool idle_high;
    bool prev_down;
    int64_t last_change_us;
} KeySlot;

static KeySlot ink_vendor_keys[kCount] = {
    {"BOOT", -1, true, false, 0},
    {"音量+", EPD_IO_VOL_UP, true, false, 0},
    {"音量-", EPD_IO_VOL_DOWN, true, false, 0},
    {"电源", EPD_IO_POWER, true, false, 0},
};

static bool level_high(const KeySlot *k) {
    if (k->io < 0) {
        return gpio_get_level(BOOT_BUTTON_GPIO) != 0;
    }
    return (ink_vendor_p1 & (1u << (k->io & 7))) != 0;
}

static bool is_down(const KeySlot *k) {
    return level_high(k) != k->idle_high;
}

static bool refresh_p1() {
    uint8_t p1 = ink_vendor_p1;
    const bool p1_ok = epd_board_metalio_eink4_plus_ioexp_read_port(1, &p1);
    if (p1_ok) {
        ink_vendor_p1 = p1;
    }
    uint8_t p0 = 0;
    const bool p0_ok = epd_board_metalio_eink4_plus_ioexp_read_port(0, &p0); // 清 TCA INT
    return p1_ok && p0_ok;
}

static void apply_edge(KeySlot *k, bool down, int64_t now) {
    if (down == k->prev_down) {
        return;
    }
    if (now - k->last_change_us < kDebounceUs) {
        return;
    }
    k->last_change_us = now;
    k->prev_down = down;

    ESP_LOGI(TAG, "%s %s raw=%d", k->label, down ? "DOWN" : "UP", level_high(k) ? 1 : 0);

}

static void sample_all() {
    const int64_t now = esp_timer_get_time();
    refresh_p1();
    for (int i = 0; i < kCount; ++i) {
        KeySlot *k = &ink_vendor_keys[i];
        apply_edge(k, is_down(k), now);
    }
}

static void ink_vendor_keys_begin(bool boot) {
    refresh_p1();
    vTaskDelay(pdMS_TO_TICKS(20));
    refresh_p1();
    for (int i = 0; i < kCount; ++i) {
        KeySlot *k = &ink_vendor_keys[i];
        if (boot) k->idle_high = (k->io < 0) ? level_high(k) : true;
        k->prev_down = is_down(k);
        k->last_change_us = esp_timer_get_time();
    }
}
static uint32_t ink_vendor_key_mask(void) {
    static const hal_key_t keys[] = {HAL_KEY_AI, HAL_KEY_VOL_UP, HAL_KEY_VOL_DOWN, HAL_KEY_POWER};
    _Static_assert(sizeof keys / sizeof keys[0] == kCount, "key mapping fits");
    uint32_t mask = 0;
    for (int i = 0; i < kCount; ++i)
        if (ink_vendor_keys[i].prev_down) mask |= 1u << keys[i];
    return mask;
}
#undef TAG
