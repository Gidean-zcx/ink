/* Local AP/page model. Browser submissions are injected by simulator events. */
#include <stdio.h>
#include <string.h>
#include "ink/hal_provision.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "sim.h"

static struct {
    bool active, pending;
    char ssid[33], password[65];
    uint32_t next, request;
    int status;
    char body[2049];
    hal_provision_fn fn;
    void *ctx;
} AP;
static void deliver(void *arg) {
    (void)arg;
    if (AP.active && AP.pending) AP.fn(AP.ctx, AP.request, AP.body, strlen(AP.body));
    memset(AP.body, 0, sizeof AP.body);
}
static const char PAGE[] =
    "<!doctype html><html lang=\"zh-CN\"><meta charset=\"utf-8\">"
    "<title>设置这台设备</title><h1>连接网络，加入家庭</h1>"
    "<form method=\"post\" action=\"/setup\"><label>Wi-Fi<input name=\"ssid\" maxlength=\"32\"></label>"
    "<label>密码<input name=\"password\" type=\"password\" maxlength=\"64\"></label>"
    "<label>家庭服务器地址<input name=\"url\" type=\"url\"></label>"
    "<label>控制台地址<input name=\"console\" type=\"url\"></label>"
    "<label>公钥<input name=\"pin\" maxlength=\"64\"></label>"
    "<button>保存</button></form><p>Wi-Fi 密码只交给这台设备。</p></html>";

int hal_provision_start(const char *ssid, const char *password, hal_provision_fn fn, void *ctx) {
    if (!ssid || !password || !fn || strlen(ssid) > 32 || strlen(password) < 8 || strlen(password) > 64) return -1;
    vtk_lock();
    snprintf(AP.ssid, sizeof AP.ssid, "%s", ssid);
    snprintf(AP.password, sizeof AP.password, "%s", password);
    AP.active = true; AP.pending = false; AP.fn = fn; AP.ctx = ctx; AP.status = 0;
    vtk_unlock();
    ink_probe_count("pairing.hotspot_started");
    return 0;
}
void hal_provision_stop(void) {
    vtk_lock(); AP.active = false; memset(AP.password, 0, sizeof AP.password); vtk_unlock();
}
void hal_provision_reply(uint32_t request, int status) {
    vtk_lock();
    if (AP.pending && AP.request == request) { AP.status = status; AP.pending = false; }
    vtk_unlock();
}
/* Event context: no host socket, radio or real credentials. */
int sim_provision_submit(const char *password, const char *json) {
    if (!AP.active || AP.pending || !password || strcmp(password, AP.password) || !json || strlen(json) > 2048) return -1;
    AP.pending = true; AP.request = ++AP.next; AP.status = 0;
    snprintf(AP.body, sizeof AP.body, "%s", json);
    sim_sys_evt(deliver, NULL);
    return (int)AP.request;
}
int sim_provision_status(void) { return AP.status; }
bool sim_provision_active(void) { return AP.active; }
const char *sim_provision_page(void) { return AP.active ? PAGE : NULL; }
