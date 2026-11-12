/* Authorization surrounds every upstream route, including reads and downloads. */
static const char ink_login[] = "<!doctype html><meta charset=utf-8><title>无线传文件</title><form><label>授权码<input id=c inputmode=numeric pattern='[0-9]{6}' required></label><button>打开文件</button></form><p id=r></p><script>async function login(){let x=await fetch('/',{headers:{'X-Ink-Code':c.value}});if(x.ok)location.replace('/');else r.textContent='授权码不正确或已失效'}document.querySelector('form').onsubmit=e=>{e.preventDefault();login()};c.value=new URLSearchParams(location.hash.slice(1)).get('code')||'';history.replaceState(null,'',location.pathname);if(c.value)login();</script>";
extern const uint8_t ink_page_start[] asm("_binary_upload_html_gz_start");
extern const uint8_t ink_page_end[] asm("_binary_upload_html_gz_end");
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return httpd_resp_send(req, (const char *)ink_page_start, ink_page_end - ink_page_start);
}
static uint64_t ink_free_bytes(void *ctx) {
    (void)ctx; hal_transfer_io_t io = {.op = HAL_TRANSFER_SPACE};
    return ink_io(&io) ? 0 : io.free_bytes;
}
static esp_err_t info_handler(httpd_req_t *req) {
    cJSON *j = cJSON_CreateObject();
    if (!j) return ESP_ERR_NO_MEM;
    cJSON_AddBoolToObject(j, "is_flash", false); cJSON_AddNumberToObject(j, "free_bytes", (double)ink_free_bytes(NULL));
    return send_json(req, j);
}
static esp_err_t ink_dispatch(httpd_req_t *req) {
    char code[8] = "", cookie[96] = "", token[33] = "", issued[33] = "";
    httpd_req_get_hdr_value_str(req, "X-Ink-Code", code, sizeof code);
    httpd_req_get_hdr_value_str(req, "Cookie", cookie, sizeof cookie);
    const char *value = strstr(cookie, "ink_transfer=");
    if (value && (value == cookie || value[-1] == ' ' || value[-1] == ';') && strlen(value + 13) >= 32)
        memcpy(token, value + 13, 32);
    bool root = req->method == HTTP_GET && !strcmp(req->uri, "/");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (!ink_policy->authorize(code, token, issued)) {
        if (root && !*code) {
            if (*token) httpd_resp_set_hdr(req, "Set-Cookie", "ink_transfer=; Max-Age=0; HttpOnly; SameSite=Strict; Path=/");
            httpd_resp_set_type(req, "text/html; charset=utf-8");
            return httpd_resp_sendstr(req, ink_login);
        }
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_set_type(req, "application/json; charset=utf-8");
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_sendstr(req, "{\"error\":\"授权码不正确或已失效，请在设备重新开启\"}");
        return ESP_FAIL; /* never read an unauthorized body */
    }
    if (*issued) {
        static char header[96];
        snprintf(header, sizeof header, "ink_transfer=%s; HttpOnly; SameSite=Strict; Path=/", issued);
        httpd_resp_set_hdr(req, "Set-Cookie", header); memcpy(token, issued, sizeof token);
    }
    memcpy(ink_token, token, sizeof token);
    if (root && *code) return httpd_resp_sendstr(req, "ok");
    const httpd_uri_t *route = req->user_ctx;
    ink_receiving = req->method == HTTP_PUT;
    esp_err_t result = route->handler(req);
    ink_receiving = false;
    return result;
}
void ink_pico_stop(void) {
    portENTER_CRITICAL(&s_lock); s_stopping = true; portEXIT_CRITICAL(&s_lock);
    if (s_http) {
        /* A client stalled mid-request would hold httpd_stop for the receive timeout (or forever, sending slowly). */
        int fds[4]; size_t n = sizeof fds / sizeof *fds;
        if (httpd_get_client_list(s_http, &n, fds) == ESP_OK)
            for (size_t k = 0; k < n; k++) shutdown(fds[k], SHUT_RDWR);
        httpd_stop(s_http); s_http = NULL;
    }
    free(s_buffer); s_buffer = NULL; ink_token[0] = 0;
}
int ink_pico_start(const hal_transfer_policy_t *policy) {
    ink_policy = policy; s_stopping = s_upload_active = false; ink_token[0] = 0;
    memset(&s_status, 0, sizeof s_status); memset(&s_cfg, 0, sizeof s_cfg);
    strcpy(s_root, "/sdcard/收件箱"); s_cfg.free_bytes_cb = ink_free_bytes;
    s_buffer = heap_caps_malloc(16384, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_buffer) return -1;
    httpd_config_t http = HTTPD_DEFAULT_CONFIG();
    http.stack_size = 16384; http.max_uri_handlers = 16; http.max_open_sockets = 3;
    http.recv_wait_timeout = 15; http.send_wait_timeout = 15;
    http.task_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT; http.lru_purge_enable = true;
    if (httpd_start(&s_http, &http) != ESP_OK) { ink_pico_stop(); return -1; }
    static const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = index_handler},
        {.uri = "/info", .method = HTTP_GET, .handler = info_handler},
        {.uri = "/upload", .method = HTTP_PUT, .handler = upload_handler},
        {.uri = "/image-upload", .method = HTTP_PUT, .handler = image_upload_handler},
        {.uri = "/font-upload", .method = HTTP_PUT, .handler = font_upload_handler},
        {.uri = "/music-upload", .method = HTTP_PUT, .handler = music_upload_handler},
        {.uri = "/file-upload", .method = HTTP_PUT, .handler = file_upload_handler},
        {.uri = "/file", .method = HTTP_GET, .handler = file_download_handler},
        {.uri = "/files", .method = HTTP_GET, .handler = files_list_handler},
        {.uri = "/files", .method = HTTP_POST, .handler = files_mutation_handler},
        {.uri = "/books", .method = HTTP_GET, .handler = books_handler},
        {.uri = "/books", .method = HTTP_DELETE, .handler = books_handler},
        {.uri = "/books", .method = HTTP_POST, .handler = books_handler},
    };
    for (size_t i = 0; i < sizeof routes / sizeof *routes; ++i) {
        httpd_uri_t r = routes[i]; r.handler = ink_dispatch; r.user_ctx = (void *)&routes[i];
        if (httpd_register_uri_handler(s_http, &r) != ESP_OK) { ink_pico_stop(); return -1; }
    }
    return 1;
}
