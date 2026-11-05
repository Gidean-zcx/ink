#include "product_probe.h"
extern void audit_text_fail_next(void);
extern int audit_text_nundo(void), audit_text_nredo(void);
static void edits(void) {
    ink_text_cfg_t cfg = {.type=INK_TEXT_PLAIN, .initial="A", .max=4095};
    CHECK(ink_text_focus(&cfg, NULL, NULL)>0, "focused");
    ink_text_commit_utf8("B");
    ink_text_commit_utf8("C");
    CHECK(ink_text_undo() && !strcmp(ink_text_value(), "AB"), "undo C");
    audit_text_fail_next();
    ink_text_commit_utf8("D");
    printf("AUDIT after_failed_snapshot text=%s undo=%d redo=%d can_redo=%d\n", ink_text_value(), audit_text_nundo(), audit_text_nredo(), ink_text_can_redo());
    CHECK(!strcmp(ink_text_value(), "ABD") && ink_text_can_redo(), "reproduced: new edit retained stale redo");
    CHECK(ink_text_redo(), "stale redo accepted");
    printf("AUDIT after_redo text=%s\n", ink_text_value());
    CHECK(!strcmp(ink_text_value(), "ABC"), "reproduced: newest edit discarded");
    ink_text_blur(0);
    cfg.initial="中文"; cfg.max=4;
    CHECK(ink_text_focus(&cfg,NULL,NULL)>0, "bounded initial text");
    const unsigned char *v=(const unsigned char *)ink_text_value();
    printf("AUDIT initial_4_bytes hex=%02x%02x%02x%02x cursor=%zu\n",v[0],v[1],v[2],v[3],ink_text_cursor());
    CHECK(v[3]==0xe6 && v[4]==0, "reproduced: initial text clipped inside a character");
    ink_text_blur(0);
}
static void audit_task(void *arg) { (void)arg; delay_ms(1800); on_ui(edits); ink_probe_count("test.finished"); }
void ink_main(void) {
    product_main();
    osal_task_cfg_t c={.name="audit-text-oom",.fn=audit_task,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c,NULL);
}
