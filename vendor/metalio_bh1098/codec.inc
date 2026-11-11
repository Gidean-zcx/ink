/* BTAudioCodecDuplex::Start and BTAudioCodec::Write/Read @874538e.
 * C HAL owns lifecycle, PSRAM buffers, volume, DMA preload and channel enables. */
static bool ink_vendor_codec_start(void) {
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_SLAVE,
        .dma_desc_num = INK_AUDIO_DMA,
        .dma_frame_num = INK_AUDIO_CHUNK,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .allow_pd = false,
        .intr_priority = 0,
        .tx_destination = I2S_DESTINATION_DMA,
        .rx_destination = I2S_DESTINATION_DMA,
    };
    if (i2s_new_channel(&chan_cfg, &A.tx, &A.rx) != ESP_OK) return false;

    i2s_std_config_t std_cfg = {0};
    std_cfg.clk_cfg.sample_rate_hz = HAL_AUDIO_RATE;
    std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    #if SOC_I2S_HW_VERSION_2
    std_cfg.clk_cfg.ext_clk_freq_hz = 0;
    #endif
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_32BIT;
    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
    std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
    std_cfg.slot_cfg.ws_width = I2S_DATA_BIT_WIDTH_32BIT;
    std_cfg.slot_cfg.ws_pol = false;
    std_cfg.slot_cfg.bit_shift = true;
    #if SOC_I2S_HW_VERSION_2
    std_cfg.slot_cfg.left_align = true;
    std_cfg.slot_cfg.big_endian = false;
    std_cfg.slot_cfg.bit_order_lsb = false;
    #endif
    std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.bclk = 42;
    std_cfg.gpio_cfg.ws = 43;
    std_cfg.gpio_cfg.dout = 44;
    std_cfg.gpio_cfg.din = 45;

    esp_err_t err = i2s_channel_init_std_mode(A.tx, &std_cfg);
    if (err == ESP_OK) {
        err = i2s_channel_init_std_mode(A.rx, &std_cfg);
    }
    if (err == ESP_OK) {
        i2s_event_callbacks_t cb = {.on_recv_q_ovf = ink_rx_lost};
        err = i2s_channel_register_event_callback(A.rx, &cb, NULL);
    }
    return err == ESP_OK;
}

static int ink_vendor_codec_write(const int16_t* data, int samples, uint32_t timeout) {
    int32_t *buffer = A.out;
    const int32_t volume_factor = 65536;

    for (int i = 0; i < samples; i++) {
        int64_t temp = (int64_t)data[i] * volume_factor;
        int32_t processed = temp > INT32_MAX   ? INT32_MAX
                            : temp < INT32_MIN ? INT32_MIN
                                               : (int32_t)temp;
        buffer[(size_t)i * 2] = processed;
        buffer[(size_t)i * 2 + 1] = processed;
    }

    size_t bytes_written = 0;
    i2s_channel_write(A.tx, buffer,
                     (size_t)samples * 2 * sizeof(int32_t), &bytes_written, timeout);
    return (int)(bytes_written / (2 * sizeof(int32_t)));
}

static int ink_vendor_codec_read(void) {
    size_t bytes_read = 0;
    int32_t *bit32_buffer = A.in;
    const esp_err_t err = i2s_channel_read(A.rx, bit32_buffer,
                                         INK_AUDIO_CHUNK * 2 * sizeof(int32_t), &bytes_read, 250);
    if (err != ESP_OK) {
        return 0;
    }

    int samples = (int)(bytes_read / sizeof(int32_t));
    for (int i = 0; i < samples; i++) {
        int32_t value = bit32_buffer[(size_t)i] >> 12;
        int16_t *dest = i % 2 ? A.ref : A.mic;
        dest[i / 2] = (value > INT16_MAX)    ? INT16_MAX
                     : (value < -INT16_MAX) ? -INT16_MAX
                                            : (int16_t)value;
    }
    return samples / 2;
}
