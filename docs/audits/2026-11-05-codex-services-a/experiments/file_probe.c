#include "product_probe.h"
extern int audit_fail_root, audit_pause_submit;
extern int audit_nopen(void);
extern int audit_file_task(void);
static void audit_task(void *arg) {
    (void)arg;
    delay_ms(1800);
    const char *deep = "/me/a/b/c/d/e/f/g/h/i/private.txt";
    CHECK(ink_fs_write_atomic(deep, "SYNTHETIC", 9) == 0, "create deep file");
    int wipe = ink_fs_wipe_personal();
    ink_fs_stat_t st;
    int exists = ink_fs_stat(deep, &st);
    printf("AUDIT deep_wipe result=%d remaining=%d\n", wipe, exists);
    CHECK(wipe == 0 && exists == INK_FS_ERR_NOT_FOUND, "AUD17-002 original depth case fixed");

    CHECK(ink_fs_write_atomic("/me/read-failure.txt", "SYNTHETIC", 9) == 0, "create private file");
    audit_fail_root = 1;
    wipe = ink_fs_wipe_personal();
    exists = ink_fs_stat("/me/read-failure.txt", &st);
    printf("AUDIT failed_root_open wipe=%d remaining=%d\n", wipe, exists);
    CHECK(wipe == 0 && exists == 0, "reproduced: failed root open treated as empty");

    ink_fs_dir_t *dir = NULL;
    CHECK(ink_fs_dir_open("/me", &dir) == 0, "open directory");
    int before = audit_nopen();
    hw(HW_USB_ON, 0, 0);
    int lent = ink_usb_disk_start();
    int next = ink_fs_dir_next(dir, &st);
    printf("AUDIT directory_vs_usb nopen=%d lend=%d active=%d next=%d\n", before, lent, ink_usb_disk_active(), next);
    CHECK(before == 0 && lent == 0 && next == 1, "reproduced: directory live while card lent");
    ink_fs_dir_close(dir);
    CHECK(ink_usb_disk_stop() == 0, "return card");

    CHECK(ink_fs_write_atomic("/me/source.txt", "COPY", 4) == 0, "source");
    audit_pause_submit = 1;
    int id = ink_fs_copy_start("/me/source.txt", "/me/copy.txt");
    CHECK(id > 0, "copy submitted");
    delay_ms(100);
    ink_fs_job_status_t status;
    CHECK(ink_fs_job_status(id, &status) == 0, "file status");
    CHECK(AI("job.list", NULL), "list");
    printf("AUDIT submit_vs_end file_state=%d task=%d list=%s\n", status.state, audit_file_task(), g_result);
    CHECK(status.state == INK_FS_JOB_DONE && strstr(g_result, "\"state\":\"running\"") != NULL,
          "reproduced: completed copy reported running");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="audit-files", .fn=audit_task, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "audit task");
}
