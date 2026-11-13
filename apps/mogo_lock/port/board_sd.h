/* The member's card space stands in for Mogo's SD card: there whenever the space can be read. */
#pragma once
#include <stdbool.h>
#include <esp_err.h>
typedef struct { bool present, mounted; } board_sd_info_t;
#define ESP_ERR_NOT_FINISHED 0x10c
static inline esp_err_t board_sd_get_info(board_sd_info_t *out) { out->present = out->mounted = true; return ESP_OK; }
