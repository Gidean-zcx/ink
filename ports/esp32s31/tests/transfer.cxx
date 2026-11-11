#include "fake.h"
#include "ink/hal_transfer.h"
#include "ap_request_worker.h"
#include <cassert>
#include <unistd.h>
#include <zlib.h>
extern "C" {
int test_begin(void);
const hal_transfer_policy_t *test_policy(void);
void test_expire(void),test_switch(void),test_plane(bool),test_full(bool),test_code(char *),test_tick(void);
int test_airplane_start(void),test_guards(void),test_attempt(void);
unsigned test_admissions(void),test_removals(void);
const char *test_path(void),*test_status(bool);
int ink_ap_start(const hal_transfer_policy_t *,bool);
void ink_ap_stop(void);
}
std::vector<httpd_uri_t> ink_test_routes;
static bool fail_start;
extern "C" void ink_transfer_wifi_init(void) {}
extern "C" void ink_transfer_wifi_pause(void) {}
extern "C" void ink_transfer_wifi_restore(bool) {}
extern "C" int hal_transfer_server_start(const hal_transfer_policy_t *p,uint16_t *port) { *port=80;return fail_start?-1:ink_ap_start(p,true); }
extern "C" void hal_transfer_server_stop(void) { ink_ap_stop(); }
extern "C" bool hal_transfer_hotspot(void) { return false; }
extern "C" int hal_transfer_host(char *s,size_t n) { snprintf(s,n,"192.0.2.42");return 0; }
extern "C" int hal_transfer_listen(uint16_t *) { return -1; }
extern "C" int hal_transfer_accept(void) { return -1; }
extern "C" int hal_transfer_read(void *,size_t) { return -1; }
extern "C" int hal_transfer_write(const void *,size_t) { return -1; }
extern "C" void hal_transfer_close(int) {}
// Fake SDK dispatch is synchronous. The real worker source is copied unchanged.
bool ApRequestWorker::Start(const char *) { return true; }
esp_err_t ApRequestWorker::Submit(httpd_req_t *r,esp_err_t (*f)(httpd_req_t *)) { r->dispatch_error=f(r);return ESP_OK; }
bool ApRequestWorker::Cleanup(void (*f)(void *),void *p) { f(p);return true; }
void ApRequestWorker::CancelRequests() {}
void ApRequestWorker::Drain() {}
void ApRequestWorker::Stop() {}
extern "C" {
extern const uint8_t kLogoStart[] asm("_binary_cloudzao_logo_png_start")={0};
extern const uint8_t kLogoEnd[] asm("_binary_cloudzao_logo_png_end")={0};
extern const uint8_t kPageStart[] asm("_binary_ap_fileman_zh_html_gz_start")={0};
extern const uint8_t kPageEnd[] asm("_binary_ap_fileman_zh_html_gz_end")={0};
extern const uint8_t kPageEnStart[] asm("_binary_ap_fileman_en_html_gz_start")={0};
extern const uint8_t kPageEnEnd[] asm("_binary_ap_fileman_en_html_gz_end")={0};
}
static httpd_uri_t route(const char *path,int method=HTTP_GET) {
    for(auto r:ink_test_routes) if(r.uri==std::string(path)&&r.method==method) return r;
    assert(false); return {};
}
static httpd_req_t request(const char *url,const std::string &cookie="",const char *body=nullptr) {
    std::string path=url;path=path.substr(0,path.find('?'));auto r=route(path.c_str(),body?HTTP_POST:HTTP_GET);
    httpd_req_t q;q.uri=url;q.method=r.method;q.user_ctx=r.user_ctx;q.headers["Cookie"]=cookie;
    if(body) { q.body=body;q.content_len=q.body.size(); }
    r.handler(&q);return q;
}
static std::string login() {
    auto r=route("/");httpd_req_t q;q.uri="/";q.user_ctx=r.user_ctx;
    char code[8];test_code(code);q.headers["X-Ink-Code"]=code;
    assert(!strstr(test_status(true),code));
    assert(r.handler(&q)==0&&q.response=="ok");
    auto cookie=q.output["Set-Cookie"];assert(cookie.find("HttpOnly")!=cookie.npos);
    assert(request("/api/storage",cookie).response.find("mounted")!=std::string::npos);return cookie;
}
static void socket_case(const std::string &cookie,bool full,bool zipped) {
    auto r=route("/ws/upload");httpd_req_t q;q.fd=socket(AF_INET,SOCK_STREAM,0);assert(q.fd>=0);q.headers["Cookie"]=cookie;
    assert(r.ws_pre_handshake_cb(&q)==0&&q.sess_ctx);
    static unsigned serial;std::string id="upload-"+std::to_string(++serial);
    q.method=0;q.headers.clear();q.body=std::string("{\"op\":\"start\",\"id\":\"upload-")+id.substr(7)+"\",\"path\":\"/收件箱\",\"name\":\"missing.txt\",\"size\":9,\"encoding\":\""+(zipped?"zlib-stream-v1":"identity")+"\"}";
    unsigned admitted=test_admissions();
    test_full(full);assert(r.handler(&q)==0);
    if(full) assert(q.response.find("\"event\":\"error\"")!=q.response.npos);
    else {
        assert(q.response.find("ready")!=q.response.npos&&test_admissions()==admitted+1);
        q.frame_type=HTTPD_WS_TYPE_BINARY;q.body="SYNTHETIC";
        if(zipped) { static const uint8_t compressed[] = {0x78, 0xda, 0xb, 0x8e, 0xf4, 0xb, 0xf1, 0x70, 0xd, 0xf1, 0x74, 0x6, 0x0, 0xe, 0x23, 0x2, 0xbc}; q.body.assign(reinterpret_cast<const char *>(compressed),sizeof compressed); }
        assert(r.handler(&q)==0);
        assert(strstr(test_status(true),"\"received\":9"));
        q.frame_type=HTTPD_WS_TYPE_TEXT;q.body="{\"op\":\"finish\",\"id\":\""+id+"\"}";
        assert(r.handler(&q)==0&&q.response.find("done")!=q.response.npos);
        assert(strstr(test_path(),"/me/收件箱/")&&test_admissions()>0);
    }
    q.free_ctx(q.sess_ctx);close(q.fd);test_full(false);
}
int main() {
    assert(test_begin());assert(request("/").response.find("授权码")!=std::string::npos);
    assert(request("/api/files").status=="403 Forbidden");
    auto root=route("/");httpd_req_t wrong;wrong.user_ctx=root.user_ctx;wrong.headers["X-Ink-Code"]="000000";
    assert(root.handler(&wrong)!=0&&wrong.status=="403 Forbidden");
    auto cookie=login();
    auto escaped=request("/api/files?path=%2F..%2Ffamily",cookie);
    fprintf(stderr,"traversal status=%s response=%.160s\n",escaped.status.c_str(),escaped.response.c_str());
    assert(escaped.status=="400 Bad Request");
    assert(request("/api/files?path=%2F",cookie).status=="200 OK");
    auto before=test_removals();auto deleted=request("/delete",cookie,"path=%2Fexample.txt");assert(deleted.response.find("deleted")!=deleted.response.npos&&test_removals()>before);
    assert(request("/mkdir",cookie,"path=%2F&name=example").response=="ok");
    assert(request("/rename",cookie,"path=%2Fexample.txt&name=missing.txt").response=="ok");
    assert(request("/move",cookie,"path=%2Fexample.txt&to=%2Fmissing").response=="ok");
    socket_case(cookie,true,false);socket_case(cookie,false,false);
    assert(test_begin());cookie=login();socket_case(cookie,false,true);
    test_switch();test_tick();fail_start=true;assert(!test_attempt());
    const char *failed_status=test_status(true);
    fprintf(stderr,"failed start guards=%d status=%.500s\n",test_guards(),failed_status);
    assert(test_guards()==0&&strstr(failed_status,"\"files\":0")&&!strstr(failed_status,"missing.txt"));
    fail_start=false;assert(test_begin());cookie=login();
    auto ws=route("/ws/upload");httpd_req_t expired;expired.fd=socket(AF_INET,SOCK_STREAM,0);expired.headers["Cookie"]=cookie;
    assert(ws.ws_pre_handshake_cb(&expired)==0);expired.method=0;expired.headers.clear();expired.body="{}";
    test_expire();assert(ws.handler(&expired)==0&&expired.dispatch_error!=0);expired.free_ctx(expired.sess_ctx);close(expired.fd);
    test_expire();assert(request("/api/storage",cookie).status=="403 Forbidden");
    assert(test_begin());cookie=login();test_switch();assert(request("/api/files",cookie).status=="403 Forbidden");
    assert(test_begin());cookie=login();test_plane(true);assert(request("/api/storage",cookie).status=="403 Forbidden");
    assert(test_begin());
    for(int i=0;i<3;i++) { wrong.response.clear();assert(root.handler(&wrong)!=0); }
    char code[8];test_code(code);wrong.headers["X-Ink-Code"]=code;assert(root.handler(&wrong)!=0);
    test_tick();assert(strstr(test_status(false),"\"active\":false"));
    assert(!test_airplane_start());
    ink_ap_stop();puts("transfer vendor routes: authorization, isolation, admission, paths, delete, identity/zlib, expiry: passed");
}
