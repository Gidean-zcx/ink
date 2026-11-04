/* Reuse the existing integration fixtures and original remote.c; inject only a legal region sequence. */
#define ink_main original_lifecycle_main
#include "../../../../tests/integration/lifecycle.c"
#undef ink_main

static osal_sem_t *audit_step;
static ink_exec_job_t audit_job;

static void begin_canvas(void *arg) {
    (void)arg;
    RS.lock = osal_mutex_create();
    RS.w = RS.h = 4;
    RS.active = true;
    RS.gen++;
    RS.px = osal_malloc(16, OSAL_MEM_EXTERNAL);
    RS.back = osal_malloc(16, OSAL_MEM_EXTERNAL);
    memset(RS.px, 0xFF, 16);
    memset(RS.back, 0xFF, 16);
    overlap_setup(NULL);
    RS.spot_timer = lv_timer_create(spot_expired, FEEDBACK_MS, NULL);
    lv_timer_pause(RS.spot_timer);
    osal_sem_give(audit_step);
}

static void expire_feedback(void *arg) {
    (void)arg;
    /* One feedback pixel from the last displayed (white) picture, temporarily inverted. */
    RS.spot = (lv_area_t){1,0,1,0};
    RS.spot_on = true;
    RS.px[1] ^= 0xFF;
    printf("AUDIT_REMOTE before_restore display=%u partial_back=%u complete=%d updates=%u\n", RS.px[1], RS.back[1], RS.holding, RS.updates);
    spot_expired(RS.spot_timer);
    printf("AUDIT_REMOTE after_restore display=%u expected_display=255 complete=%d updates=%u owed=%d\n", RS.px[1], RS.holding, RS.updates, RS.owed);
    osal_sem_give(audit_step);
}

static void finish_canvas(void *arg) {
    (void)arg;
    printf("AUDIT_REMOTE after_LAST display=%u updates=%u owed=%d\n", RS.px[1], RS.updates, RS.owed);
    lv_timer_delete(RS.spot_timer);
    RS.spot_timer = NULL;
    overlap_clean(NULL);
    RS.active = false;
    ink_exec_post(INK_EXEC_MEDIA, &RS.release_job, release_canvas, NULL);
    osal_sem_give(audit_step);
}

static void ui_step(void (*fn)(void *)) {
    ink_exec_post(INK_EXEC_UI, &audit_job, fn, NULL);
    osal_sem_take(audit_step, OSAL_WAIT_FOREVER);
}

static void audit_task(void *arg) {
    (void)arg;
    if(ink_display_init() || ink_cap_init() || ink_exec_init() || ink_ui_start(&T_HOME)) osal_panic("audit init");
    osal_task_delay_us(1000000);
    ui_step(begin_canvas);
    queue_region(1, false); /* first region of B; LAST has not arrived */
    osal_task_delay_us(100000);
    ui_step(expire_feedback);
    queue_region(2, true); /* positive control: now B is complete */
    osal_task_delay_us(100000);
    ui_step(finish_canvas);
    ink_probe_count("audit.finished");
}

void ink_main(void) {
    audit_step = osal_sem_create(0,1);
    osal_task_cfg_t c={.name="audit-remote",.fn=audit_task,.stack_bytes=8192,.priority=3,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    if(osal_task_create(&c,NULL)!=OSAL_OK) osal_panic("audit task");
}
