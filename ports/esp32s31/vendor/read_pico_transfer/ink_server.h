/* Keep the upstream synchronous handlers on the existing HAL policy. */
#pragma once
#include "include/read_pico_transfer.h"
#include "cJSON.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "transfer_policy.h"
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static read_pico_transfer_status_t s_status;
static read_pico_transfer_cfg_t s_cfg;
static char s_root[160] = "/sdcard/收件箱"; /* the reader lists the inbox */
static char *s_buffer;
static bool s_stopping, s_upload_active;
static httpd_handle_t s_http;
static char ink_token[33];
static void set_error(esp_err_t error) { s_status.last_error = error; }
static bool management_request(httpd_req_t *req) {
    (void)req; char issued[33] = "";
    return ink_policy->authorize(NULL, ink_token, issued);
}
static esp_err_t index_handler(httpd_req_t *req);
static esp_err_t info_handler(httpd_req_t *req);
