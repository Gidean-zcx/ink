/* A frontend start fails; another task asks for ordinary capture at its unlock boundary.
 * Product source is unchanged. Only one HAL failure and the interleaving are injected. */
#include <stdio.h>
#include "ink/osal.h"
#include "ink/hal_afe.h"
#include "ink/sys/audio.h"
#include "ink/sys/exec.h"
#include "sim.h"
static int fail_afe_start(void);
static void hook_unlock(osal_mutex_t *m);
#define hal_afe_start fail_afe_start
#define osal_mutex_unlock hook_unlock
#include "../../../../system/services/audio/audio_service.c"
#undef hal_afe_start
#undef osal_mutex_unlock
#define ink_main product_main
#include "../../../../system/boot/main.c"
#undef ink_main

static osal_sem_t *go_capture, *capture_started;
static int inject_failure = 1, armed, capture_result = -99, frames;

static int fail_afe_start(void) {
    if(inject_failure) {
        inject_failure = 0;
        armed = 1;
        return -1;
    }
    return hal_afe_start();
}

static void hook_unlock(osal_mutex_t *m) {
    osal_mutex_unlock(m);
    if(armed && m==AU.lock && !AU.listen_on && !AU.cap_on &&
       !strcmp(osal_task_name(osal_task_self()),"audio")) {
        armed = 0;
        osal_sem_give(go_capture);
        osal_sem_take(capture_started, OSAL_WAIT_FOREVER);
    }
}

static void captured(void *ctx, const uint8_t *d, size_t n, uint32_t seq) {
    (void)ctx; (void)d; (void)seq;
    if(n) frames++;
}

static void capture_task(void *arg) {
    (void)arg;
    osal_sem_take(go_capture, OSAL_WAIT_FOREVER);
    capture_result = ink_audio_capture_begin(captured, NULL);
    osal_sem_give(capture_started);
    osal_task_delay_us(500000);
    int before=frames;
    osal_task_delay_us(2000000);
    printf("AUDIT_AUDIO after_failed_frontend capture_result=%d cap_on=%d listen_on=%d frames_before=%d frames_after=%d\n",
           capture_result, AU.cap_on, AU.listen_on, before, frames);
    ink_audio_capture_end();
    osal_task_delay_us(300000);
    frames=0;
    int r=ink_audio_capture_begin(captured,NULL);
    osal_task_delay_us(1000000);
    printf("AUDIT_AUDIO fresh_capture result=%d cap_on=%d frames=%d\n",r,AU.cap_on,frames);
    ink_audio_capture_end();
    ink_probe_count("audit.finished");
}

void ink_main(void) {
    go_capture=osal_sem_create(0,1);
    capture_started=osal_sem_create(0,1);
    osal_task_cfg_t c={.name="audit-capture",.fn=capture_task,.stack_bytes=8192,.priority=5,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    if(osal_task_create(&c,NULL)!=OSAL_OK) osal_panic("audit task");
    product_main();
}
