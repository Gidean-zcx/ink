/* Controlled interleavings at existing unlocked/yield boundaries. Product sources are included
 * unchanged. Hooks stand in for another task running at those exact boundaries, not new logic. */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/hal_jpeg.h"
#include "ink/sys/cap.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"

static void audit_unlock(osal_mutex_t *m);
#define osal_mutex_unlock audit_unlock
#include "../../../../system/policy/cap.c"
#undef osal_mutex_unlock

static int audit_jpeg(const uint8_t *src, size_t len, uint8_t *dst, int w, int h);
#define hal_jpeg_decode_gray audit_jpeg
#include "../../../../system/apps/remote/remote.c"
#undef hal_jpeg_decode_gray

static bool armed, switch_session;
static int runs, replies, retry_rc = -99;
static ink_call_opts_t opts;
static void audit_unlock(osal_mutex_t *m) {
    osal_mutex_unlock(m);
    if (!armed || m != CB.lock) return;
    for (int i=0; i<POOL_N; ++i) {
        pending_t *p=&CB.pool[i];
        if (p->used && p->replied && !p->answered && !strcmp(p->rid,"same-request")) {
            armed=false;
            retry_rc=ink_cap_call("audit.once",NULL,&opts);
            return;
        }
    }
}
static int audit_jpeg(const uint8_t *src, size_t len, uint8_t *dst, int w, int h) {
    if (switch_session) {
        /* remote_destroy + remote_create can run on UI while decode yields: retain canvas,
         * clear it, install a new sid. No free is injected and dimensions are unchanged. */
        switch_session=false;
        osal_mutex_lock(RS.lock,OSAL_WAIT_FOREVER);
        RS.sid=2;
        memset(RS.px,255,(size_t)RS.w*(size_t)RS.h);
        RS.ndirty=0;
        osal_mutex_unlock(RS.lock);
    }
    return hal_jpeg_decode_gray(src,len,dst,w,h);
}
static void run(ink_call_t *c) { (void)c; runs++; }
static void done(void *ctx,const ink_receipt_t *rc) { (void)ctx; (void)rc; replies++; }
static const ink_cap_t cap={.id="audit.once",.exec=INK_EXEC_DIRECT,.run=run};
static void task(void *arg) {
    (void)arg;
    if (ink_display_init() || ink_cap_init() || ink_exec_init()) osal_panic("audit init");
    ink_cap_register(&cap,1);
    opts=(ink_call_opts_t){.src=INK_SRC_SYSTEM,.request_id="same-request",.reply_exec=INK_EXEC_DIRECT,.done=done};
    armed=true;
    int first=ink_cap_call("audit.once",NULL,&opts);
    printf("AUDIT dedup first=%d retry=%d runs=%d replies=%d (one request_id)\n",first,retry_rc,runs,replies);

    RS.lock=osal_mutex_create(); RS.sid=1; RS.w=4; RS.h=4;
    RS.px=osal_malloc(16,OSAL_MEM_EXTERNAL);
    memset(RS.px,255,16); RS.dsc.data=RS.px;
    /* One real SJ image region, 2x2 black, no LAST: it must not be a committed screen yet. */
    uint8_t data[]={0,0,0,0,2,0,2,0,2,0,'S','J',2,0,2,0,0};
    region_job_t *j=osal_malloc(sizeof *j+sizeof data,OSAL_MEM_EXTERNAL);
    memset(j,0,sizeof *j); j->sid=1; j->len=sizeof data;
    memcpy(j->data,data,sizeof data); switch_session=true;
    decode_region(j);
    int changed=0;
    for (int i=0;i<16;i++) changed += RS.dsc.data[i]!=255;
    printf("AUDIT remote old_frame_sid=1 current_sid=%d published_pixels_changed=%d dirty=%d last_flag=0\n",RS.sid,changed,RS.ndirty);
    RS.sid=0; release_canvas(NULL);
    fflush(stdout);
}
void ink_main(void) {
    osal_task_cfg_t cfg={.name="audit",.fn=task,.stack_bytes=8192,.priority=OSAL_PRIO_NORMAL,.core=-1};
    if (osal_task_create(&cfg,NULL)!=OSAL_OK) osal_panic("audit task");
}
