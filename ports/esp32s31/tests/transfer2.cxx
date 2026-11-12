/* Actual kiikoread routes and system policy; synthetic HTTP and file boundaries. */
#include "esp_http_server.h"
#include "ink/hal_transfer.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>
extern "C" {
void test_make(const char *p);
int test_begin(void), test_guards(void), test_attempt(void), test_airplane_start(void);
const hal_transfer_policy_t *test_policy(void);
void test_expire(void), test_switch(void), test_plane(bool), test_full(bool), test_code(char *), test_tick(void), test_clear_files(void);
unsigned test_admissions(void), test_removals(void);
int test_exists(const char *), test_bytes(const char *, const void *, size_t);
const char *test_status(bool);
int ink_pico_start(const hal_transfer_policy_t *);
void ink_pico_stop(void);
extern const uint8_t ink_page_start[] asm("_binary_upload_html_gz_start") = {31};
extern const uint8_t ink_page_end[] asm("_binary_upload_html_gz_end") = {0};
}
struct Reply {
    std::string body, response, status = "200 OK";
    std::map<std::string, std::string> headers, output;
    size_t read = 0, limit = SIZE_MAX;
    bool expire_on_read = false;
};
static std::vector<httpd_uri_t> routes;
static bool fail_start;
extern "C" int hal_transfer_server_start(const hal_transfer_policy_t *p, uint16_t *port) { *port = 80; return fail_start ? -1 : ink_pico_start(p); }
extern "C" void hal_transfer_server_stop(void) { ink_pico_stop(); }
extern "C" bool hal_transfer_hotspot(void) { return false; }
extern "C" int hal_transfer_host(char *s, size_t n) { snprintf(s, n, "192.0.2.42"); return 0; }
extern "C" int hal_transfer_listen(uint16_t *) { return -1; }
extern "C" int hal_transfer_accept(void) { return -1; }
extern "C" int hal_transfer_read(void *, size_t) { return -1; }
extern "C" int hal_transfer_write(const void *, size_t) { return -1; }
extern "C" void hal_transfer_close(int) {}
static Reply &state(httpd_req_t *q) { return *static_cast<Reply *>(q->test); }
extern "C" int httpd_start(httpd_handle_t *h, const httpd_config_t *c) { assert(c->stack_size == 16384 && c->task_caps == 6); routes.clear(); *h = (void *)1; return 0; }
extern "C" int httpd_stop(httpd_handle_t) { return 0; }
extern "C" int httpd_get_client_list(httpd_handle_t, size_t *n, int *) { *n = 0; return 0; }
extern "C" int httpd_register_uri_handler(httpd_handle_t, const httpd_uri_t *r) { routes.push_back(*r); return 0; }
extern "C" int httpd_resp_set_status(httpd_req_t *q, const char *s) { state(q).status = s; return 0; }
extern "C" int httpd_resp_set_type(httpd_req_t *, const char *) { return 0; }
extern "C" int httpd_resp_set_hdr(httpd_req_t *q, const char *k, const char *v) { state(q).output[k] = v; return 0; }
extern "C" int httpd_resp_send(httpd_req_t *q, const char *p, size_t n) { if (state(q).output.count("Content-Encoding")) state(q).response = "GZIP_PAGE"; else state(q).response.assign(p, n); return 0; }
extern "C" int httpd_resp_sendstr(httpd_req_t *q, const char *p) { state(q).response = p; return 0; }
extern "C" int httpd_resp_send_chunk(httpd_req_t *q, const char *p, size_t n) { if (p) state(q).response.append(p, n); return 0; }
extern "C" int httpd_req_get_hdr_value_str(httpd_req_t *q, const char *k, char *p, size_t n) { auto s = state(q).headers[k]; if (s.empty() || s.size() >= n) return -1; memcpy(p, s.c_str(), s.size() + 1); return 0; }
extern "C" size_t httpd_req_get_url_query_len(httpd_req_t *q) { const char *p = strchr(q->uri, '?'); return p ? strlen(p + 1) : 0; }
extern "C" int httpd_req_get_url_query_str(httpd_req_t *q, char *p, size_t n) { const char *s = strchr(q->uri, '?'); if (!s || strlen(++s) >= n) return -1; strcpy(p, s); return 0; }
extern "C" int httpd_query_key_value(const char *query, const char *key, char *p, size_t n) {
    std::string s = std::string("&") + query, k = std::string("&") + key + "=";
    auto at = s.find(k); if (at == s.npos) return ESP_ERR_NOT_FOUND;
    auto v = s.substr(at + k.size()); v = v.substr(0, v.find('&'));
    if (v.size() >= n) return -1;
    memcpy(p, v.c_str(), v.size() + 1); return 0;
}
extern "C" int httpd_req_recv(httpd_req_t *q, char *p, size_t n) {
    auto &r = state(q);
    if (r.expire_on_read) { test_expire(); r.expire_on_read = false; }
    if (r.read >= r.limit) return 0;
    n = std::min({n, r.body.size() - r.read, r.limit - r.read});
    memcpy(p, r.body.data() + r.read, n); r.read += n; return (int)n;
}
static Reply request(const char *url, const std::string &cookie = "", int method = HTTP_GET,
                     const std::string &body = "", const char *code = "", size_t limit = SIZE_MAX, bool expire = false) {
    Reply r; r.body = body; r.headers["Cookie"] = cookie; r.headers["X-Ink-Code"] = code; r.limit = limit; r.expire_on_read = expire;
    std::string path = url; path = path.substr(0, path.find('?'));
    for (const auto &route : routes) if (path == route.uri && method == route.method) {
        httpd_req_t q = {route.user_ctx, url, method, body.size(), &r}; route.handler(&q); return r;
    }
    r.status = "404 Not Found"; return r;
}
static std::string login() {
    char code[8]; test_code(code); assert(!strstr(test_status(true), code));
    auto r = request("/", "", HTTP_GET, "", code);
    assert(r.status == "200 OK" && r.response == "ok");
    auto cookie = r.output["Set-Cookie"]; assert(cookie.find("HttpOnly") != cookie.npos);
    assert(request("/info", cookie).response.find("free_bytes") != std::string::npos); return cookie;
}
static void expiry_only() {
    assert(test_begin()); char code[8], token[33] = ""; test_code(code);
    assert(test_policy()->authorize(code, "", token));
    test_expire();
    assert(request("/info", std::string("ink_transfer=") + token).status == "403 Forbidden");
}
int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--expiry-only")) { expiry_only(); return 0; }
    assert(test_begin()); assert(request("/").response.find("授权码") != std::string::npos);
    auto stale = request("/", "ink_transfer=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"); /* last session's cookie */
    assert(stale.response.find("授权码") != std::string::npos && stale.output["Set-Cookie"].find("Max-Age=0") != std::string::npos);
    assert(request("/files").status == "403 Forbidden");
    for (int i = 0; i < 3; i++) assert(request("/", "", HTTP_GET, "", "000000").status == "403 Forbidden");
    char code[8]; test_code(code); assert(request("/", "", HTTP_GET, "", code).status == "403 Forbidden");
    test_tick(); assert(strstr(test_status(false), "\"active\":false"));
    assert(test_begin()); auto cookie = login();
    assert(request("/", cookie).response == "GZIP_PAGE");
    assert(request("/files", cookie).response.find("收件箱") != std::string::npos);
    assert(request("/files?path=%E6%94%B6%E4%BB%B6%E7%AE%B1", cookie).status == "200 OK");
    for (const char *bad : {"/files?path=..%2Fkid", "/files?path=%2F.m%2Fkid", "/files?path=.m%2Fkid"})
        assert(request(bad, cookie).status != "200 OK");
    assert(request("/wifi", cookie, HTTP_POST, "{}").status == "404 Not Found");
    assert(request("/clock", cookie, HTTP_POST, "{}").status == "404 Not Found");
    assert(request("/signature", cookie).status == "404 Not Found");
    std::string gbk, toc;
    if (argc > 1) {
        std::ifstream a(std::string(argv[1]) + "/synthetic.txt", std::ios::binary), b(std::string(argv[1]) + "/synthetic.toc.json");
        gbk.assign(std::istreambuf_iterator<char>(a), {}); toc.assign(std::istreambuf_iterator<char>(b), {});
    } else {
        for (int i = 0; i < 8; i++) { gbk += "\xb5\xda" + std::to_string(i + 1) + "\xd5\xc2 \xb2\xe2\xca\xd4\n"; gbk += std::string(1500, 'X') + "\n"; }
        toc = "{\"encoding\":\"gbk\",\"entries\":[{\"t\":\"第一章\",\"o\":0}]}";
    }
    assert(!gbk.empty() && toc.find("gbk") != toc.npos);
    unsigned admitted = test_admissions();
    auto cancelled = request("/upload?name=synthetic.txt", cookie, HTTP_PUT, gbk, "", 100);
    assert(cancelled.status == "408 Request Timeout" && test_admissions() > admitted);
    assert(!test_exists("/me/收件箱/synthetic.txt") && !test_exists("/me/收件箱/synthetic.txt.part"));
    assert(request("/upload?name=synthetic.txt", cookie, HTTP_PUT, gbk).status == "200 OK");
    assert(test_bytes("/me/收件箱/synthetic.txt", gbk.data(), gbk.size()));
    assert(request("/file-upload?path=%E6%94%B6%E4%BB%B6%E7%AE%B1%2Fsynthetic.toc.json", cookie, HTTP_PUT, toc).status == "200 OK");
    assert(test_bytes("/me/收件箱/synthetic.toc.json", toc.data(), toc.size()));
    assert(request("/file?path=%E6%94%B6%E4%BB%B6%E7%AE%B1%2Fsynthetic.txt", cookie).response == gbk);
    assert(request("/files?path=%E6%94%B6%E4%BB%B6%E7%AE%B1", cookie).response.find("synthetic.toc.json") != std::string::npos);
    assert(request("/files", cookie, HTTP_POST, "{\"action\":\"rename\",\"path\":\"收件箱/synthetic.txt\",\"target\":\"收件箱/renamed.txt\"}").status == "200 OK");
    assert(test_bytes("/me/收件箱/renamed.txt", gbk.data(), gbk.size()));
    assert(request("/files", cookie, HTTP_POST, "{\"action\":\"copy\",\"path\":\"收件箱/renamed.txt\",\"target\":\"收件箱/copied.txt\"}").status == "200 OK");
    assert(test_bytes("/me/收件箱/copied.txt", gbk.data(), gbk.size()));
    assert(strstr(test_status(true), "\"files\":1")); /* the book only: not its contents file, not the copy */
    unsigned removed = test_removals();
    for (const char *name : {"renamed", "copied"}) {
        std::string body = std::string("{\"action\":\"delete\",\"path\":\"收件箱/") + name + ".txt\"}";
        assert(request("/files", cookie, HTTP_POST, body).status == "200 OK");
    }
    assert(test_removals() == removed + 2 && !test_exists("/me/收件箱/renamed.txt") && !test_exists("/me/收件箱/copied.txt"));
    assert(request("/files", cookie, HTTP_POST, "{\"action\":\"mkdir\",\"path\":\"收件箱\",\"target\":\"收件箱/new\"}").status == "200 OK");
    for (const char *endpoint : {"/font-upload?name=synthetic.ttf", "/image-upload?name=synthetic.png", "/music-upload?name=synthetic.mp3"})
        assert(request(endpoint, cookie, HTTP_PUT, "SYNTHETIC").status == "200 OK");
    assert(test_exists("/me/收件箱/synthetic.ttf") && test_exists("/me/照片/synthetic.png") && test_exists("/me/音乐/synthetic.mp3"));
    admitted = test_admissions(); test_full(true); auto full = request("/upload?name=full.txt", cookie, HTTP_PUT, gbk); test_full(false);
    assert(full.status == "507 Insufficient Storage" && full.response.find("存储空间不足") != std::string::npos);
    assert(!test_exists("/me/收件箱/full.txt.part") && test_admissions() == admitted);
    auto invalid = request("/upload?name=invalid%3A.txt", cookie, HTTP_PUT, gbk);
    assert(invalid.status == "400 Bad Request" && invalid.response.find("无效") != std::string::npos);
    auto longname = request("/upload?name=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.txt", cookie, HTTP_PUT, gbk);
    assert(longname.status == "400 Bad Request" && longname.response.find("存储空间不足") == std::string::npos);
    test_make("/me/收件箱/stale.txt.part"); /* left by a power cut */
    assert(request("/upload?name=stale.txt", cookie, HTTP_PUT, gbk).status == "200 OK" && test_bytes("/me/收件箱/stale.txt", gbk.data(), gbk.size()));
    auto interrupted = request("/upload?name=expired.txt", cookie, HTTP_PUT, gbk, "", SIZE_MAX, true);
    assert(interrupted.status != "200 OK" && !test_exists("/me/收件箱/expired.txt.part"));
    for (auto route : routes) { /* after expiry the page itself asks for a new code; everything else is refused */
        auto r = request(route.uri, cookie, route.method, "{}");
        assert(!strcmp(route.uri, "/") && route.method == HTTP_GET ? r.response.find("授权码") != std::string::npos : r.status == "403 Forbidden");
    }
    assert(test_begin()); cookie = login(); test_switch();
    assert(request("/files", cookie).status == "403 Forbidden");
    test_tick(); fail_start = true; assert(!test_attempt());
    assert(test_guards() == 0 && strstr(test_status(true), "\"files\":0") && !strstr(test_status(true), "synthetic"));
    fail_start = false; assert(test_begin()); cookie = login(); test_plane(true);
    assert(request("/info", cookie).status == "403 Forbidden");
    assert(!test_airplane_start());
    ink_pico_stop(); test_clear_files();
    puts("kiikoread routes: stale cookie, GBK + TOC into the inbox, cancel, copy, rename, batch delete, long name, leftover part, every-request expiry, isolation, admission, full, invalid, settings 404: passed");
}
