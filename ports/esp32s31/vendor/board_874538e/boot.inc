/* CloudZao Metalio-E-INK4-Plus @874538e. MIT: LICENSE. Adaptations: SOURCE.md. */
void epd_board_metalio_eink4_plus_boot_release_holds(void) {
    static bool peripherals_cleared = false; // app_main 与显示初始化均可调用，外设残留只清理一次
    if (!peripherals_cleared) {
        // S31 软件复位不复位外设；注册 GPIO 服务前先清除上次运行的中断与握手 DMA。
        const gpio_num_t irq_pins[] = {NT26_SRDY_PIN, CFG_INTR, TOUCH_INT_PIN, BOOT_BUTTON_GPIO};
        uint64_t retained = 0;
        for (size_t i = 0; i < sizeof(irq_pins) / sizeof(irq_pins[0]); ++i) {
            const gpio_num_t pin = irq_pins[i];
            if (GPIO.pin[pin].int_ena != 0) retained |= 1ULL << pin;
            ESP_ERROR_CHECK(gpio_intr_disable(pin));
            ESP_ERROR_CHECK(gpio_wakeup_disable(pin));
            ESP_ERROR_CHECK(gpio_set_intr_type(pin, GPIO_INTR_DISABLE));
            if (pin < 32) gpio_ll_clear_intr_status(&GPIO, 1U << pin);
            else gpio_ll_clear_intr_status_high(&GPIO, 1U << (pin - 32));
        }
        PERIPH_RCC_ATOMIC() {
            for (int i = 0; i < UHCI_LL_NUM; ++i) {
                uhci_ll_enable_bus_clock(i, true);
                uhci_ll_reset_register(i);
                uhci_ll_enable_bus_clock(i, false);
            }
        }
        if (retained != 0) {
            ESP_LOGW(TAG, "boot: cleared retained GPIO interrupt mask=0x%llx", (unsigned long long)retained);
        }
        peripherals_cleared = true;
    }
    // 深睡 gpio_hold 跨 CPU 复位仍有效；不解则并行脚锁死为低，TPS 升压易失败
    for (size_t i = 0; i < sizeof(k_epd_bus_pins) / sizeof(k_epd_bus_pins[0]); ++i) {
        gpio_hold_dis(k_epd_bus_pins[i]);
    }
    MetalioSd_ReleaseHold();
    gpio_hold_dis(CFG_SDA);
    gpio_hold_dis(CFG_SCL);
    gpio_hold_dis(FL_COOL_PIN);
    gpio_hold_dis(FL_WARM_PIN);
    ESP_LOGI(TAG, "boot: released EPD/SD/I2C/FL gpio holds");
}
