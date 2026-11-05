#define ink_main product_main
#include "../../../../system/boot/main.c"
#undef ink_main
#include "../../../../tests/integration/whole_product.h"

static char memory[32];
static int close_result;
static void app_start(void) { memory[0] = 0; }
static void app_stop(void) { memory[0] = 0; }
static int app_save(void *buf, size_t cap) { return snprintf(buf, cap, "%s", memory); }
static void app_restore(const void *buf, size_t len) { snprintf(memory, sizeof memory, "%.*s", (int)len, (const char *)buf); }
static const ink_app_ops_t OPS = {.on_start = app_start, .on_stop = app_stop, .save_state = app_save, .restore_state = app_restore};
static void page(lv_obj_t *scr, void *arg) { (void)scr; (void)arg; }
static const ink_activity_t PAGE = {.name = "audit_page", .app = "audit_app", .create = page};
static const ink_app_def_t APP = {.id = "audit_app", .title = "audit", .entry = &PAGE, .ops = &OPS, .child_ok = true};
static void open_app(void) { CHECK(ink_app_open(APP.id, NULL, NULL) == 0, "open"); }
static void close_app(void) { close_result = ink_app_close(APP.id); }
static void drain(void) {
    for (int i = 0; i < 500 && ink_storage_pending(); i++) delay_ms(10);
    CHECK(ink_storage_pending() == 0, "storage drained");
}
static void run_audit(void *arg) {
    (void)arg;
    delay_ms(1500);
    on_ui(open_app);
    snprintf(memory, sizeof memory, "old-state");
    on_ui(close_app);
    drain();
    on_ui(open_app);
    CHECK(!strcmp(memory, "old-state"), "baseline restore");
    snprintf(memory, sizeof memory, "new-state");
    drain();
    ink_storage_hold_writes(true);
    for (int i = 0; i < 8; i++) {
        char path[64];
        snprintf(path, sizeof path, "/user/sys/audit-queue-%d", i);
        CHECK(ink_storage_write_atomic(path, "x", 1) == 0, "fill queue slot %d", i);
    }
    int pending = ink_storage_pending();
    on_ui(close_app);
    ink_storage_hold_writes(false);
    drain();
    on_ui(open_app);
    printf("AUDIT app close_result=%d queue_before=%d restored=%s expected=new-state\n", close_result, pending, memory);
    CHECK(close_result == 0 && pending == 8 && !strcmp(memory, "old-state"), "reproduce silently lost app state");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    CHECK(ink_app_register(&APP) == 0, "register test app");
    osal_task_cfg_t cfg = {.name = "audit-app", .fn = run_audit, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "create audit");
}
