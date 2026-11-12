/* Mogo NVS values live in the captured member's data collection. */
#pragma once
#include <esp_err.h>
#include <stddef.h>
#include <stdint.h>
typedef struct ink_mogo_nvs *nvs_handle_t;
typedef struct ink_mogo_iterator *nvs_iterator_t;
typedef enum { NVS_TYPE_ANY, NVS_TYPE_BLOB, NVS_TYPE_STR, NVS_TYPE_U32, NVS_TYPE_U8 } nvs_type_t;
typedef struct { char namespace_name[16], key[16]; nvs_type_t type; } nvs_entry_info_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define NVS_DEFAULT_PART_NAME "nvs"
#define ESP_ERR_NVS_NOT_FOUND ESP_ERR_NOT_FOUND
#define ESP_ERR_NVS_INVALID_LENGTH ESP_ERR_INVALID_SIZE
#define nvs_open ink_mogo_nvs_open
#define nvs_close ink_mogo_nvs_close
#define nvs_get_blob ink_mogo_nvs_get_blob
#define nvs_set_blob ink_mogo_nvs_set_blob
#define nvs_get_str ink_mogo_nvs_get_str
#define nvs_set_str ink_mogo_nvs_set_str
#define nvs_get_u32 ink_mogo_nvs_get_u32
#define nvs_set_u32 ink_mogo_nvs_set_u32
#define nvs_get_u8 ink_mogo_nvs_get_u8
#define nvs_set_u8 ink_mogo_nvs_set_u8
#define nvs_erase_key ink_mogo_nvs_erase_key
#define nvs_commit ink_mogo_nvs_commit
#define nvs_entry_find ink_mogo_nvs_entry_find
#define nvs_entry_next ink_mogo_nvs_entry_next
#define nvs_entry_info ink_mogo_nvs_entry_info
#define nvs_release_iterator ink_mogo_nvs_release_iterator
esp_err_t nvs_open(const char *, int, nvs_handle_t *);
void nvs_close(nvs_handle_t);
esp_err_t nvs_get_blob(nvs_handle_t, const char *, void *, size_t *);
esp_err_t nvs_set_blob(nvs_handle_t, const char *, const void *, size_t);
esp_err_t nvs_get_str(nvs_handle_t, const char *, char *, size_t *);
esp_err_t nvs_set_str(nvs_handle_t, const char *, const char *);
esp_err_t nvs_get_u32(nvs_handle_t, const char *, uint32_t *);
esp_err_t nvs_set_u32(nvs_handle_t, const char *, uint32_t);
esp_err_t nvs_get_u8(nvs_handle_t, const char *, uint8_t *);
esp_err_t nvs_set_u8(nvs_handle_t, const char *, uint8_t);
esp_err_t nvs_erase_key(nvs_handle_t, const char *);
esp_err_t nvs_commit(nvs_handle_t);
esp_err_t nvs_entry_find(const char *, const char *, nvs_type_t, nvs_iterator_t *);
esp_err_t nvs_entry_next(nvs_iterator_t *);
esp_err_t nvs_entry_info(nvs_iterator_t, nvs_entry_info_t *);
void nvs_release_iterator(nvs_iterator_t);
