#include "product_probe.h"
static void audit_task(void *arg) {
    (void)arg;
    delay_ms(2000);
    static const char marker[] = "audit-personal-file";
    CHECK(ink_fs_write_atomic("/me/private.txt", marker, sizeof marker - 1) == INK_FS_OK, "private file fixture");
    CHECK(ink_storage_write_atomic("/user/sys/audit-reset-marker", marker, sizeof marker - 1) == 0, "flash fixture");
    CHECK(WAIT_FOR(!ink_storage_pending(), 5000), "fixture persisted");
    hw(HW_USB_ON, 0, 0);
    CHECK(ink_usb_disk_start() == INK_FS_OK, "lend card");
    printf("AUDIT reset usb_active=%d\n", ink_usb_disk_active());
    /* The simulator stops on reboot. The host runner subsequently checks the same SD directory. */
    ink_probe_count("test.finished");
    CHECK(BY_HAND("system.factory_reset", NULL) && g_rc.status == INK_RC_DONE, "reset accepted");
    printf("AUDIT reset receipt=%d\n", g_rc.status);
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c = {.name="audit-reset", .fn=audit_task, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c, NULL);
}
