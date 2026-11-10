/* A second restricted page proves the runtime does not recognize camera names. */
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"

static lv_obj_t *restricted_root;
static unsigned ran;
static volatile bool pin_done;
static int pin_result;
static const ink_ui_overlay_t RESTRICTED;
static void told(void *ctx, int result, uint32_t wait) { (void)ctx; (void)wait; pin_result = result; pin_done = true; }
static void action(ink_call_t *c) { (void)c; ran++; }
static bool close_page(ink_key_t key, bool lng) {
    if (!lng && (key == INK_KEY_HOME || key == INK_KEY_BACK) && restricted_root) {
        ink_ui_overlay_remove(&RESTRICTED); lv_obj_delete(restricted_root); restricted_root = NULL;
        return true;
    }
    return false;
}
static const char *const ALLOWED[] = {"audit14.allowed", "system.home", "shell.back", NULL};
static const ink_activity_t PAGE = {.name = "Audit14Restricted", .locked_caps = ALLOWED, .on_key = close_page};
static const ink_ui_overlay_t RESTRICTED = {.name = "Audit14Restricted", .activity = &PAGE, .level = 3, .on_key = close_page};
static void open_page(void) {
    restricted_root = lv_obj_create(lv_layer_sys()); ink_ui_overlay_add(&RESTRICTED, restricted_root);
    CHECK(ink_ui_locked_activity() == &PAGE, "active page publishes its own restricted declaration");
}
static void run(void *arg) {
    (void)arg; ink_power_hold(INK_NEED_AWAKE); test_start_home();
    static const ink_cap_t CAPS[] = {
        {.id = "audit14.allowed", .title = "合成允许动作", .activity = "Audit14Restricted", .child_ok = true, .run = action},
        {.id = "audit14.secret", .title = "合成禁止动作", .while_locked = true, .child_ok = true, .run = action}};
    CHECK(!ink_cap_register(CAPS, 2), "register capabilities unrelated to camera names");
    CHECK(ink_member_set_pin("owner", "", "1111", told, NULL) == 0 && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "set a synthetic adult PIN");
    ink_member_test_reset();
    ink_power_lock(); CHECK(WAIT_FOR(ink_power_state() == INK_PWR_LOCKED && !ink_ui_busy(), 5000), "lock the unauthenticated member space");
    for (int i = 0; i < 3; i++) {
        on_ui(open_page);
        CHECK(!ink_ui_page_current() && !ink_member_space_open(), "the underlying private page remains unavailable");
        CHECK(BY_HAND("audit14.allowed", NULL) && g_rc.status == INK_RC_DONE, "declared action works without opening the private page: %s %.120s", ink_rc_name(g_rc.status), g_rc.detail);
        CHECK(BY_HAND("audit14.secret", NULL) && g_rc.status == INK_RC_REJECTED, "page declaration narrows global while_locked permission");
        CHECK(AI("audit14.allowed", NULL) && g_rc.status == INK_RC_REJECTED, "AI cannot use a restricted page");
        if (i == 0) CHECK(BY_HAND("system.home", NULL) && g_rc.status == INK_RC_DONE, "HOME capability delegates to the declared page: %s %.120s", ink_rc_name(g_rc.status), g_rc.detail);
        else if (i == 2) CHECK(BY_HAND("shell.back", NULL) && g_rc.status == INK_RC_DONE, "BACK capability delegates to the declared page: %s %.120s", ink_rc_name(g_rc.status), g_rc.detail);
        else key(HAL_KEY_HOME);
        CHECK(WAIT_FOR(!ink_ui_locked_activity(), 3000) && !restricted_root, "declared page closes via capability and physical HOME");
        CHECK(ink_power_state() == INK_PWR_LOCKED && !ink_member_space_open(), "closing the restricted page preserves the lock");
    }
    CHECK(ran == 3, "only the three allowed touch calls reached a handler");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "audit14.locked", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("audit14 restricted-page test");
}
