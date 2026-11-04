/* Audit-only: product sources are included unchanged; two narrow hooks observe SMS and
 * inject the documented synchronous INK_HUB_ERR_NOMEM return. No actual networking/device. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/hal_cellular.h"
#include "ink/hal_net.h"
#include "ink/sys/cap.h"
#include "ink/sys/comm.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/hub.h"
#include "ink/sys/members.h"
#include "ink/sys/net.h"
#include "ink/sys/power.h"
#include "ink/sys/res.h"
#include "ink/sys/rt.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "sim.h"

static int observe_sms(const char *to, const char *text, uint32_t timeout);
#define hal_cell_sms_send observe_sms
#include "../../../../system/services/comm/comm_service.c"
#undef hal_cell_sms_send
static int fail_hub_call(const char *method, const char *params, int64_t timeout, ink_hub_reply_fn cb, void *ctx);
#define ink_hub_call fail_hub_call
#include "../../../../system/services/hub/rt.c"
#undef ink_hub_call

static int fail_next, calls, closed;
static int fail_hub_call(const char *method, const char *params, int64_t timeout, ink_hub_reply_fn cb, void *ctx) {
    calls++;
    if (fail_next) { fail_next = 0; return INK_HUB_ERR_NOMEM; }
    return ink_hub_call(method, params, timeout, cb, ctx);
}
static int observe_sms(const char *to, const char *text, uint32_t timeout) {
    size_t n = strlen(text);
    int r = hal_cell_sms_send(to, text, timeout);
    printf("AUDIT sms actual_bytes=%zu ends_abc=%d ends_a=%d hal_return=%d\n", n,
           n >= 3 && !strcmp(text+n-3,"abc"), n && text[n-1]=='a', r);
    return r;
}
static void rt_closed(void *ctx, int sid, int reason) { (void)ctx; (void)sid; (void)reason; closed++; }
static volatile int sms_done;
static void sms_receipt(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    printf("AUDIT sms final_receipt=%s say=%s\n",ink_rc_name(rc->status),rc->say);
    sms_done=1;
}
static void probe_rt(void) {
    ink_rt_handler_t h = {.on_closed=rt_closed};
    fail_next=1;
    int sid=ink_rt_open("voice.chat",NULL,&h);
    int i=find(sid);
    printf("AUDIT rt_nomem sid=%d open=%d inflight=%d calls=%d closed=%d\n",sid,ink_rt_open_count(),i>=0?RT.s[i].inflight:-1,calls,closed);
    osal_task_delay_us(12000000);
    /* Invoke exactly the callback registered for a new connection, with memory healthy. */
    on_online(NULL);
    osal_task_delay_us(1000000);
    printf("AUDIT rt_recovered open=%d ready=%d inflight=%d calls=%d closed=%d\n",ink_rt_open_count(),ink_rt_ready(sid),RT.s[i].inflight,calls,closed);
    ink_rt_close(sid);
}
static void probe_sms(void) {
    osal_task_delay_us(6000000);
    char message[340];
    for(int i=0;i<106;i++) memcpy(message+3*i,"中",3);
    memcpy(message+318,"abc",4);
    ink_args_t args; ink_args_init(&args);
    int a=ink_args_str(&args,"to","10086"), b=ink_args_str(&args,"text",message);
    char why[120]={0};
    int valid=ink_args_check(&args,COMM_CAPS[0].input,why,sizeof why);
    ink_call_opts_t opts={.src=INK_SRC_SYSTEM,.reply_exec=INK_EXEC_DIRECT,.done=sms_receipt};
    int r=ink_cap_call("message.send",&args,&opts);
    printf("AUDIT sms requested_bytes=%zu requested_chars=109 args=%d/%d schema=%d accepted=%d\n",strlen(message),a,b,valid,r);
    for(int w=0;!sms_done && w<3000;w++) osal_task_delay_us(10000);
    char sent_to[24],sent_text[400];
    int sent=sim_net_sent_sms(0,sent_to,sizeof sent_to,sent_text,sizeof sent_text);
    printf("AUDIT sms modem_sent=%d stored_bytes=%zu equals_requested=%d\n",sent,sent>0?strlen(sent_text):0,sent>0&&!strcmp(sent_text,message));
}
static int online_count;
static void count_online(void *ctx) { (void)ctx; online_count++; }
static void status(const char *where) {
    hal_link_status_t st; hal_net_link_status(&st);
    printf("AUDIT net %s prefer=%lld type=%d up=%d hub=%d online_count=%d\n",where,(long long)ink_setting_get("net.prefer"),st.type,st.up,ink_hub_online(),online_count);
}
static void prefer(int v) {
    ink_receipt_t rc;
    ink_setting_set("net.prefer",v,INK_SRC_SYSTEM,&rc);
    osal_task_delay_us(5000000);
}
static void probe_net(void) {
    ink_hub_on_online(count_online,NULL);
    ink_hub_keep(true);
    prefer(1); status("wifi");
    prefer(2); status("wifi_to_lte");
    prefer(0); status("auto");
    /* The script removes cellular coverage at 18 s; Wi-Fi stays available. */
    osal_task_delay_us(10000000); status("auto_cell_down_wifi_available");
    prefer(1); status("manual_wifi");
    ink_hub_keep(false);
}
static void task(void *ctx) {
    (void)ctx;
    if(ink_display_init() || ink_storage_init() || ink_cap_init() || ink_exec_init() || ink_res_init() || ink_state_init() ||
       ink_settings_init() || ink_power_init() || ink_net_init() || ink_hub_init("wss://hub.local/v1/device") ||
       ink_rt_init() || ink_comm_init()) osal_panic("audit init");
    ink_members_init();
    const char *mode=getenv("AUDIT_CASE");
    if(mode && !strcmp(mode,"sms")) probe_sms();
    else if(mode && !strcmp(mode,"rt")) probe_rt();
    else probe_net();
    fflush(stdout);
}
void ink_main(void) {
    osal_task_cfg_t cfg={.name="audit",.fn=task,.stack_bytes=8192,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    if(osal_task_create(&cfg,NULL)!=OSAL_OK) osal_panic("audit task");
}
