#ifndef INK_WS_FAKE_H
#define INK_WS_FAKE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define MBEDTLS_SSL_VERIFY_OPTIONAL 1
typedef struct { unsigned char *p; size_t len; } mbedtls_x509_buf;
typedef struct { mbedtls_x509_buf pk_raw; } mbedtls_x509_crt;
typedef struct { int mode; } mbedtls_ssl_config;
typedef struct { mbedtls_x509_crt cert; } esp_tls_t;
typedef struct { int timeout_ms; esp_err_t (*crt_bundle_attach)(void *); } esp_tls_cfg_t;
esp_tls_t *esp_tls_init(void);
int esp_tls_conn_new_sync(const char *, int, int, const esp_tls_cfg_t *, esp_tls_t *);
int esp_tls_conn_destroy(esp_tls_t *);
void *esp_tls_get_ssl_context(esp_tls_t *);
const mbedtls_x509_crt *mbedtls_ssl_get_peer_cert(const void *);
void mbedtls_ssl_conf_authmode(void *, int);
#define PSA_ALG_SHA_256 1
#define PSA_SUCCESS 0
int psa_hash_compute(int, const uint8_t *, size_t, uint8_t *, size_t, size_t *);
int esp_crt_bundle_attach(void *);
ssize_t esp_tls_get_bytes_avail(esp_tls_t *);
esp_err_t esp_tls_get_conn_sockfd(esp_tls_t *, int *);
ssize_t esp_tls_conn_read(esp_tls_t *, void *, size_t);
ssize_t esp_tls_conn_write(esp_tls_t *, const void *, size_t);
int ink_test_select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
int ink_test_setsockopt(int, int, int, const void *, socklen_t);
#define select ink_test_select
#define setsockopt ink_test_setsockopt
typedef struct transport *esp_transport_handle_t;
typedef int (*connect_func)(esp_transport_handle_t, const char *, int, int);
typedef int (*io_read_func)(esp_transport_handle_t, char *, int, int);
typedef int (*io_func)(esp_transport_handle_t, const char *, int, int);
typedef int (*trans_func)(esp_transport_handle_t);
typedef int (*poll_func)(esp_transport_handle_t, int);
esp_transport_handle_t esp_transport_init(void);
esp_err_t esp_transport_destroy(esp_transport_handle_t);
esp_err_t esp_transport_set_context_data(esp_transport_handle_t, void *);
void *esp_transport_get_context_data(esp_transport_handle_t);
esp_err_t esp_transport_set_func(esp_transport_handle_t, connect_func, io_read_func, io_func,
                               trans_func, poll_func, poll_func, trans_func);
int esp_transport_connect(esp_transport_handle_t, const char *, int, int);
int esp_transport_read(esp_transport_handle_t, char *, int, int);
int esp_transport_poll_read(esp_transport_handle_t, int);
enum { WS_TRANSPORT_OPCODES_CONT = 0, WS_TRANSPORT_OPCODES_TEXT = 1, WS_TRANSPORT_OPCODES_BINARY = 2,
    WS_TRANSPORT_OPCODES_CLOSE = 8, WS_TRANSPORT_OPCODES_PING = 9, WS_TRANSPORT_OPCODES_PONG = 10,
    WS_TRANSPORT_OPCODES_FIN = 128 };
typedef int ws_transport_opcodes_t;
typedef struct { const char *ws_path; bool propagate_control_frames; } esp_transport_ws_config_t;
esp_transport_handle_t esp_transport_ws_init(esp_transport_handle_t);
int esp_transport_ws_set_config(esp_transport_handle_t, const esp_transport_ws_config_t *);
int esp_transport_ws_send_raw(esp_transport_handle_t, ws_transport_opcodes_t, const char *, int, int);
int esp_transport_ws_get_read_opcode(esp_transport_handle_t);
int esp_transport_ws_get_read_payload_len(esp_transport_handle_t);
bool esp_transport_ws_get_fin_flag(esp_transport_handle_t);
enum { UF_SCHEMA, UF_HOST, UF_PORT, UF_PATH, UF_QUERY, UF_FRAGMENT, UF_USERINFO, UF_MAX };
struct http_parser_url { uint16_t field_set, port; struct { uint16_t off, len; } field_data[UF_MAX]; };
void http_parser_url_init(struct http_parser_url *);
int http_parser_parse_url(const char *, size_t, int, struct http_parser_url *);
#endif
