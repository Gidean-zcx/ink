#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM -2
#define ESP_ERR_INVALID_STATE -3
#define ESP_ERR_NOT_FOUND -4
#define CONFIG_SPIRAM 1
#define CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM 1
#define ESP_LOGE(tag,...) ((void)(tag))
#define ESP_LOGW(tag,...) ((void)(tag))
#define ESP_LOGI(tag,...) ((void)(tag))
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_INTERNAL 4
inline void *heap_caps_malloc(size_t n, unsigned) { return std::malloc(n); }
inline void *heap_caps_calloc(size_t n, size_t s, unsigned) { return std::calloc(n, s); }
inline void heap_caps_free(void *p) { std::free(p); }
typedef void *SemaphoreHandle_t;
typedef int StaticSemaphore_t;
typedef int portMUX_TYPE;
typedef int StaticQueue_t;
typedef void *QueueHandle_t;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portMAX_DELAY 0xffffffff
#define pdTRUE 1
#define taskENTER_CRITICAL(p) ((void)(p))
#define taskEXIT_CRITICAL(p) ((void)(p))
inline void *xSemaphoreCreateRecursiveMutex() { return (void *)1; }
inline void *xSemaphoreCreateMutexStatic(void *) { return (void *)1; }
inline void *xSemaphoreCreateRecursiveMutexStatic(void *) { return (void *)1; }
inline void *xSemaphoreCreateCountingStatic(int, int, void *) { return (void *)1; }
inline int xSemaphoreTake(void *, unsigned) { return 1; }
inline int xSemaphoreGive(void *) { return 1; }
inline int xSemaphoreTakeRecursive(void *, unsigned) { return 1; }
inline int xSemaphoreGiveRecursive(void *) { return 1; }
inline void vTaskDelay(unsigned) {}
typedef void *httpd_handle_t;
typedef int httpd_method_t;
enum { HTTP_GET=1, HTTP_POST, HTTPD_WS_CLIENT_WEBSOCKET=3, HTTPD_WS_TYPE_TEXT=1, HTTPD_WS_TYPE_BINARY=2, HTTPD_WS_TYPE_CLOSE=8, HTTPD_WS_TYPE_PING=9, HTTPD_WS_TYPE_PONG=10 };
struct httpd_req_t {
    void *handle=nullptr, *user_ctx=nullptr, *sess_ctx=nullptr;
    void (*free_ctx)(void *)=nullptr;
    int method=HTTP_GET, fd=-1, dispatch_error=0;
    size_t content_len=0, read=0;
    std::string uri, body, response, status="200 OK";
    std::map<std::string,std::string> headers, output;
    int frame_type=HTTPD_WS_TYPE_TEXT;
};
struct httpd_ws_frame_t { bool final=true; int type=0; uint8_t *payload=nullptr; size_t len=0; };
struct httpd_uri_t { const char *uri=nullptr; int method=0; esp_err_t (*handler)(httpd_req_t *)=nullptr; void *user_ctx=nullptr; bool is_websocket=false,handle_ws_control_frames=false; esp_err_t (*ws_pre_handshake_cb)(httpd_req_t *)=nullptr; };
struct httpd_config_t { int server_port=80,max_uri_handlers=0,max_open_sockets=0; esp_err_t (*open_fn)(void *,int)=nullptr; void (*close_fn)(void *,int)=nullptr; int stack_size=0,task_caps=0,recv_wait_timeout=0,send_wait_timeout=0; bool lru_purge_enable=false,keep_alive_enable=false; int keep_alive_idle=0,keep_alive_interval=0,keep_alive_count=0; };
#define HTTPD_DEFAULT_CONFIG() httpd_config_t{}
inline esp_err_t httpd_resp_set_status(httpd_req_t *r,const char *s) { r->status=s; return 0; }
inline esp_err_t httpd_resp_set_type(httpd_req_t *,const char *) { return 0; }
inline esp_err_t httpd_resp_set_hdr(httpd_req_t *r,const char *k,const char *v) { r->output[k]=v; return 0; }
inline esp_err_t httpd_resp_send(httpd_req_t *r,const char *s,size_t n) { r->response.assign(s,n); return 0; }
inline esp_err_t httpd_resp_sendstr(httpd_req_t *r,const char *s) { r->response=s; return 0; }
inline esp_err_t httpd_resp_send_chunk(httpd_req_t *r,const char *s,size_t n) { if(s) r->response.append(s,n); return 0; }
inline esp_err_t httpd_resp_sendstr_chunk(httpd_req_t *r,const char *s) { return httpd_resp_send_chunk(r,s,s?strlen(s):0); }
inline int httpd_req_get_hdr_value_len(httpd_req_t *r,const char *k) { return r->headers[k].size(); }
inline int httpd_req_get_hdr_value_str(httpd_req_t *r,const char *k,char *out,size_t n) { auto v=r->headers[k]; if(v.empty()||v.size()>=n) return -1; memcpy(out,v.c_str(),v.size()+1); return 0; }
inline size_t httpd_req_get_url_query_len(httpd_req_t *r) { auto p=r->uri.find('?'); return p==std::string::npos?0:r->uri.size()-p-1; }
inline int httpd_req_get_url_query_str(httpd_req_t *r,char *out,size_t n) { auto s=r->uri.substr(r->uri.find('?')+1); if(s.size()>=n) return -1; memcpy(out,s.c_str(),s.size()+1); return 0; }
inline int httpd_query_key_value(const char *q,const char *key,char *out,size_t n) { std::string s=q,k=std::string(key)+"="; size_t p=s.find(k); if(p==s.npos) return ESP_ERR_NOT_FOUND; auto v=s.substr(p+k.size()); v=v.substr(0,v.find('&')); if(v.size()>=n) return -1; memcpy(out,v.c_str(),v.size()+1); return 0; }
inline int httpd_req_recv(httpd_req_t *r,char *out,size_t n) { n=std::min(n,r->body.size()-r->read); memcpy(out,r->body.data()+r->read,n); r->read+=n; return n; }
inline int httpd_req_to_sockfd(httpd_req_t *r) { return r->fd; }
inline int httpd_ws_get_fd_info(void *,int) { return HTTPD_WS_CLIENT_WEBSOCKET; }
inline int httpd_ws_recv_frame(httpd_req_t *r,httpd_ws_frame_t *f,size_t n) { f->len=r->body.size();f->type=r->frame_type;f->final=true; if(n) { if(n<f->len) return -1; memcpy(f->payload,r->body.data(),f->len); } return 0; }
inline int httpd_ws_send_frame(httpd_req_t *r,httpd_ws_frame_t *f) { r->response.assign((const char *)f->payload,f->len); return 0; }
extern std::vector<httpd_uri_t> ink_test_routes;
inline int httpd_start(void **h,httpd_config_t *) { ink_test_routes.clear(); *h=(void *)1;return 0; }
inline int httpd_stop(void *) { return 0; }
inline int httpd_register_uri_handler(void *,const httpd_uri_t *r) { ink_test_routes.push_back(*r);return 0; }
struct esp_netif_t {};
struct esp_netif_ip_info_t { struct { uint32_t addr=0; } ip,gw,netmask; };
#define IP4_ADDR(p,a,b,c,d) ((p)->addr=((a)<<24)|((b)<<16)|((c)<<8)|(d))
#define IPSTR "%u.%u.%u.%u"
#define IP2STR(p) unsigned((p)->addr>>24),unsigned(((p)->addr>>16)&255),unsigned(((p)->addr>>8)&255),unsigned((p)->addr&255)
inline int esp_netif_init() { return 0; }
inline int esp_event_loop_create_default() { return 0; }
inline esp_netif_t *esp_netif_create_default_wifi_ap() { static esp_netif_t n;return &n; }
inline esp_netif_t *esp_netif_get_handle_from_ifkey(const char *) { return esp_netif_create_default_wifi_ap(); }
inline int esp_netif_get_ip_info(esp_netif_t *,esp_netif_ip_info_t *p) { IP4_ADDR(&p->ip,192,168,4,1);return 0; }
inline int esp_netif_dhcps_stop(void *) { return 0; }
inline int esp_netif_dhcps_start(void *) { return 0; }
inline int esp_netif_set_ip_info(void *,void *) { return 0; }
inline const char *esp_err_to_name(int) { return "fake"; }
typedef int wifi_mode_t;
enum { WIFI_MODE_NULL,WIFI_MODE_STA,WIFI_MODE_AP,WIFI_IF_AP,WIFI_AUTH_OPEN,WIFI_PS_NONE,ESP_MAC_WIFI_STA };
struct wifi_ap_record_t { uint8_t ssid[33]{}; };
struct wifi_init_config_t { bool nvs_enable=false; };
struct wifi_config_t { struct { uint8_t ssid[33]{}; uint8_t ssid_len=0,channel=0,max_connection=0,authmode=0; } ap; };
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}
inline int esp_wifi_get_mode(wifi_mode_t *m) { *m=WIFI_MODE_STA;return 0; }
inline int esp_wifi_init(void *) { return 0; }
inline int esp_wifi_set_mode(int) { return 0; }
inline int esp_wifi_set_config(int,void *) { return 0; }
inline int esp_wifi_start() { return 0; }
inline int esp_wifi_stop() { return 0; }
inline int esp_wifi_set_ps(int) { return 0; }
inline int esp_wifi_set_max_tx_power(int) { return 0; }
inline int esp_wifi_sta_get_ap_info(void *) { return 0; }
inline int esp_read_mac(uint8_t *m,int) { memset(m,0,6);return 0; }
