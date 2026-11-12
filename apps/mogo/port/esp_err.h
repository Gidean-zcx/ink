#pragma once
typedef int esp_err_t;
enum { ESP_OK, ESP_FAIL = -1, ESP_ERR_NO_MEM = 0x101, ESP_ERR_INVALID_ARG,
    ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_SIZE, ESP_ERR_NOT_FOUND,
    ESP_ERR_NOT_SUPPORTED, ESP_ERR_INVALID_RESPONSE = 0x108, ESP_ERR_INVALID_CRC };
static inline const char *esp_err_to_name(int e) { (void)e; return "Mogo"; }
