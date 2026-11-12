#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
#define HTTP_GET 1
#define HTTP_POST 2
#define HTTP_PUT 3
#define HTTP_DELETE 4
typedef void *httpd_handle_t;
typedef struct { void *user_ctx; const char *uri; int method; size_t content_len; void *test; } httpd_req_t;
typedef struct { const char *uri; int method; esp_err_t (*handler)(httpd_req_t *); void *user_ctx; } httpd_uri_t;
typedef struct { int stack_size, max_uri_handlers, max_open_sockets, recv_wait_timeout, send_wait_timeout; unsigned task_caps; bool lru_purge_enable; } httpd_config_t;
#define HTTPD_DEFAULT_CONFIG() (httpd_config_t){0}
int httpd_start(httpd_handle_t *, const httpd_config_t *);
int httpd_stop(httpd_handle_t);
int httpd_get_client_list(httpd_handle_t, size_t *, int *);
int httpd_register_uri_handler(httpd_handle_t, const httpd_uri_t *);
int httpd_resp_set_status(httpd_req_t *, const char *);
int httpd_resp_set_type(httpd_req_t *, const char *);
int httpd_resp_set_hdr(httpd_req_t *, const char *, const char *);
int httpd_resp_send(httpd_req_t *, const char *, size_t);
int httpd_resp_sendstr(httpd_req_t *, const char *);
int httpd_resp_send_chunk(httpd_req_t *, const char *, size_t);
int httpd_req_get_hdr_value_str(httpd_req_t *, const char *, char *, size_t);
size_t httpd_req_get_url_query_len(httpd_req_t *);
int httpd_req_get_url_query_str(httpd_req_t *, char *, size_t);
int httpd_query_key_value(const char *, const char *, char *, size_t);
int httpd_req_recv(httpd_req_t *, char *, size_t);
#ifdef __cplusplus
}
#endif
