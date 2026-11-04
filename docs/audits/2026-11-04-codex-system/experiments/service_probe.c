/* Audit-only integration probes, linked to the unmodified system and simulator libraries. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/hal_cellular.h"
#include "ink/hal_net.h"
#include "ink/sys/audio.h"
#include "ink/sys/cap.h"
#include "ink/sys/comm.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/hub.h"
#include "ink/sys/net.h"
#include "ink/sys/power.h"
#include "ink/sys/res.h"
#include "ink/sys/rt.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "sim.h"

static osal_sem_t *block;
static volatile int blocking, applied, watched, replies, ran;
static uint64_t requested, completed;
static ink_rc_t reply_status;
static void noop(void *arg) { (void)arg; }
static void blocker(void *arg) {
    (void)arg;
    blocking = 1;
    osal_sem_take(block, OSAL_WAIT_FOREVER);
}
static void apply(void *ctx, int64_t v) { (void)ctx; applied = (int)v; }
static void watch(void *ctx, int h, const char *id) { (void)ctx; (void)h; (void)id; watched++; }
static void caprun(ink_call_t *c) { (void)c; ran++; }
static void reply(void *ctx, const ink_receipt_t *rc) {
    (void)ctx; replies++; reply_status = rc->status; completed = osal_now_us();
}
static const ink_cap_t caps[] = {
    {.id="audit.direct", .exec=INK_EXEC_DIRECT, .run=caprun},
    {.id="audit.bg", .exec=INK_EXEC_BG, .timeout_ms=20, .run=caprun},
};
static int early_nested;
static void early_run(ink_call_t *c) {
    ink_cap_defer(c);
    ink_cap_done(c); /* permitted early completion; no further use of c by this provider */
    ink_call_opts_t o={.src=INK_SRC_SYSTEM,.reply_exec=INK_EXEC_DIRECT,.done=reply};
    early_nested=ink_cap_call("audit.bg",NULL,&o);
}
static const ink_cap_t early_cap={.id="audit.early",.exec=INK_EXEC_DIRECT,.run=early_run};
static void early_probe(void) {
    ink_cap_register(caps,2); ink_cap_register(&early_cap,1);
    block=osal_sem_create(0,1);
    ink_exec_post(INK_EXEC_BG,blocker,NULL);
    while(!blocking) osal_task_delay_us(1000);
    ink_call_opts_t o={.src=INK_SRC_SYSTEM,.reply_exec=INK_EXEC_DIRECT};
    int r=ink_cap_call("audit.early",NULL,&o);
    printf("AUDIT early before_unblock first=%d nested=%d replies=%d ran=%d status=%s\n",r,early_nested,replies,ran,ink_rc_name(reply_status));
    osal_sem_give(block);
    osal_task_delay_us(50000);
    printf("AUDIT early after_unblock replies=%d ran=%d late=%.0f\n",replies,ran,sim_metric_value("cap.late"));
}
static const ink_setting_def_t setting = {
    .id="audit.setting", .title="audit", .type=INK_SET_INT, .min=0, .max=10,
    .apply=apply, .apply_exec=INK_EXEC_BG,
};
static const ink_state_def_t state = {.id="audit.state", .type=INK_ST_INT, .title="audit"};
static void bus_probe(void) {
    ink_cap_register(caps, 2);
    ink_settings_define(&setting, 1);
    int h=ink_state_define(&state);
    ink_state_watch("audit.state", watch, NULL, INK_EXEC_BG);
    block=osal_sem_create(0, 1);
    ink_exec_post(INK_EXEC_BG, blocker, NULL);
    while (!blocking) osal_task_delay_us(1000);
    int queued=0;
    for (int i=0; i<16; i++) queued += ink_exec_post(INK_EXEC_BG, noop, NULL)==0;
    ink_receipt_t rc;
    ink_setting_set("audit.setting", 7, INK_SRC_TOUCH, &rc);
    ink_state_set_int(h, 1);
    ink_call_opts_t o={.src=INK_SRC_SYSTEM, .reply_exec=INK_EXEC_BG, .done=reply};
    int accepted=ink_cap_call("audit.direct", NULL, &o);
    osal_sem_give(block);
    osal_task_delay_us(1500000);
    printf("AUDIT backpressure queued=%d call_return=%d replies=%d setting_rc=%s stored=%lld applied=%d watch=%d dropped_apply=%.0f dropped_reply=%.0f dropped_watch=%.0f\n",
        queued, accepted, replies, ink_rc_name(rc.status), (long long)ink_setting_get("audit.setting"), applied, watched,
        sim_metric_value("settings.apply_dropped"), sim_metric_value("cap.reply_dropped"), sim_metric_value("state.watch_dropped"));
    ink_setting_set("audit.setting", 7, INK_SRC_TOUCH, &rc);
    osal_task_delay_us(50000);
    printf("AUDIT retry_same_value applied=%d watch=%d\n", applied, watched);
    blocking=0; replies=0; ran=0;
    ink_exec_post(INK_EXEC_BG, blocker, NULL);
    while (!blocking) osal_task_delay_us(1000);
    o.reply_exec=INK_EXEC_DIRECT;
    requested=osal_now_us();
    accepted=ink_cap_call("audit.bg", NULL, &o);
    osal_task_delay_us(500000);
    printf("AUDIT deadline before_unblock return=%d replies=%d ran=%d timeouts=%.0f\n", accepted, replies, ran, sim_metric_value("cap.timeouts"));
    osal_sem_give(block);
    osal_task_delay_us(50000);
    printf("AUDIT deadline after_unblock replies=%d ran=%d status=%s elapsed_ms=%.1f deadline_ms=20\n",
        replies, ran, ink_rc_name(reply_status), (double)(completed-requested)/1000.0);
}
static void sim_setting_probe(void) {
    osal_task_delay_us(7000000);
    hal_cell_status_t st={0};
    int ok=hal_cell_status(&st);
    char value[32]="";
    ink_state_get_str(ink_state_find("cell.sim"), value, sizeof value);
    printf("AUDIT sim_boot setting=%lld status_ok=%d slot=%d published=%s\n", (long long)ink_setting_get("cell.sim_slot"), ok, st.slot, value);
    ink_receipt_t rc;
    ink_setting_set("cell.sim_slot", 1, INK_SRC_TOUCH, &rc);
    osal_task_delay_us(2000000);
    hal_cell_status(&st);
    printf("AUDIT sim_reapply rc=%s slot=%d switched=%.0f\n", ink_rc_name(rc.status), st.slot, sim_metric_value("comm.sim_switched"));
}
static volatile int packets, ends, restart=-99;
static void capture(void *ctx, const uint8_t *data, size_t len, uint32_t seq) {
    (void)ctx; (void)data; (void)seq;
    if (len) packets++;
    else if (++ends==1) {
        restart=ink_audio_capture_begin(capture, NULL);
        printf("AUDIT capture end_callback_restart=%d packets_before=%d\n", restart, packets);
        fflush(stdout);
    }
}
static void capture_probe(void) {
    osal_task_delay_us(2500000);
    int listen=ink_audio_listen(true, NULL, NULL);
    int first=ink_audio_capture_begin(capture, NULL);
    osal_task_delay_us(500000);
    ink_audio_capture_end();
    osal_task_delay_us(1500000);
    printf("AUDIT capture first=%d listen=%d restart=%d ends=%d packets=%d\n", first, listen, restart, ends, packets);
    ink_audio_capture_end();
}
static void task(void *arg) {
    (void)arg;
    const char *mode=getenv("AUDIT_CASE");
    if (!mode) mode="bus";
    if (ink_display_init() || ink_storage_init() || ink_cap_init() || ink_exec_init() || ink_res_init() || ink_state_init()) osal_panic("audit init");
    if (!strcmp(mode,"sim_boot")) {
        const char *seed="{\"cell.sim_slot\":1}";
        ink_storage_write_atomic(INK_SETTINGS_PATH, seed, strlen(seed));
        while (ink_storage_pending()) osal_task_delay_us(20000);
    }
    if (ink_settings_init() || ink_power_init() || ink_net_init() || ink_hub_init("wss://hub.local/v1/device") || ink_rt_init() || ink_tools_init() || ink_audio_init() || ink_comm_init()) osal_panic("audit services");
    if (!strcmp(mode,"bus")) bus_probe();
    else if (!strcmp(mode,"sim_boot")) sim_setting_probe();
    else if (!strcmp(mode,"capture")) capture_probe();
    else if (!strcmp(mode,"early")) early_probe();
    fflush(stdout);
}
void ink_main(void) {
    osal_task_cfg_t cfg={.name="audit", .fn=task, .stack_bytes=8192, .priority=OSAL_PRIO_NORMAL, .core=-1};
    if (osal_task_create(&cfg,NULL)!=OSAL_OK) osal_panic("audit task");
}
