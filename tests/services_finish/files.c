#include "product.h"
extern int ink_test_fail_root, ink_test_pause_submit;
static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    CHECK(ink_fs_write_atomic("/me/private.txt", "SYNTHETIC", 9) == 0, "create private file");
    ink_test_fail_root = 1;
    int wipe = ink_fs_wipe_personal();
    ink_fs_stat_t st;
    CHECK(wipe == INK_FS_ERR_IO, "AUD18-101: unreadable root is an error (%d)", wipe);
    CHECK(ink_fs_stat("/me/private.txt", &st) == 0, "failed cleanup leaves data for retry");
    CHECK(ink_fs_wipe_personal() == 0 && ink_fs_stat("/me/private.txt", &st) == INK_FS_ERR_NOT_FOUND, "cleanup retry succeeds");
    CHECK(ink_fs_write_atomic("/me/source.txt", "COPY", 4) == 0, "source");
    ink_fs_dir_t *dir = NULL;
    CHECK(ink_fs_dir_open("/me", &dir) == 0, "directory");
    hw(HW_USB_ON, 0, 0);
    CHECK(ink_usb_disk_start() == INK_FS_ERR_BUSY && !ink_usb_disk_active(), "AUD18-102: directory holds the card");
    CHECK(ink_fs_dir_next(dir, &st) == 1, "directory remains usable");
    ink_fs_dir_close(dir);
    CHECK(ink_usb_disk_start() == 0, "close releases the card");
    CHECK(ink_fs_write_atomic("/me/refused.txt", "X", 1) == INK_FS_ERR_USB, "short write refused while lent");
    CHECK(ink_fs_mkdir("/me/refused") == INK_FS_ERR_USB, "mkdir refused while lent");
    CHECK(ink_usb_disk_stop() == 0, "return card");
    ink_test_pause_submit = 1;
    int id = ink_fs_copy_start("/me/source.txt", "/me/copy.txt");
    CHECK(id > 0, "copy starts");
    ink_fs_job_status_t status;
    CHECK(ink_fs_job_status(id, &status) == 0 && status.state == INK_FS_JOB_DONE, "copy finished before submit returns");
    CHECK(AI("job.list", NULL) && !strstr(g_result, "\"state\":\"running\""), "AUD18-103: task also finished (%s)", g_result);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="finish-files", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
