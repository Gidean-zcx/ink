#include "product_probe.h"
static void audit_task(void *arg) {
    (void)arg; delay_ms(1800);
    const char *p = "/me/a/b/c/d/e/f/g/h/i/private.txt";
    CHECK(ink_fs_write_atomic(p, "AUDIT-SYNTHETIC", 15) == INK_FS_OK, "make deep personal file through file service");
    int r = ink_fs_wipe_personal();
    ink_fs_stat_t st;
    int remains = ink_fs_stat(p, &st);
    printf("AUDIT wipe_result=%d deep_file_stat=%d\n", r, remains);
    CHECK(r == INK_FS_OK && remains == INK_FS_OK, "reproduce successful wipe leaving personal file");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c = {.name="audit-reset", .fn=audit_task, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c, NULL);
}
