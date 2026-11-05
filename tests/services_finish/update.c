#include "product.h"
#include "ink/sys/layout.h"
static int refuse_state;
static int state_write(const char *path, const void *data, size_t len) {
    if (refuse_state && !strcmp(path, INK_UPDATE_STATE_FILE)) return -1;
    return ink_storage_write_atomic(path, data, len);
}
static int state_borrow(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx) {
    if (refuse_state && !strcmp(path, INK_UPDATE_STATE_FILE)) {
        if (refuse_state == 2) { done(ctx, -1); return 0; }
        return -1;
    }
    return ink_storage_write_borrow(path, data, len, done, ctx);
}
static int state_read(const char *path, void *data, size_t len) {
    if (getenv("INK_FINISH_LOSE_STATE") && !strcmp(path, INK_UPDATE_STATE_FILE)) return -1;
    return ink_storage_read(path, data, len);
}
#define ink_storage_write_atomic state_write
#define ink_storage_write_borrow state_borrow
#define ink_storage_read state_read
#ifndef INK_TEST_UPDATE_SOURCE
#define INK_TEST_UPDATE_SOURCE "../../system/services/update/update_service.c"
#endif
#include INK_TEST_UPDATE_SOURCE
#undef ink_storage_write_atomic
#undef ink_storage_write_borrow
#undef ink_storage_read

static void restore_offer(void *arg) { (void)arg; setenv("INK_SIM_UPDATE_PKG", getenv("INK_FINISH_OLD_PKG"), 1); }
static void change_offer(void *arg) { (void)arg; setenv("INK_SIM_UPDATE_PKG", getenv("INK_FINISH_NEW_PKG"), 1); }
static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    const char *phase = getenv("INK_FINISH_PHASE");
    if (!strcmp(phase, "download")) {
        ink_hub_demand();
        CHECK(WAIT_FOR(ink_hub_online(), 30000), "hub online");
        CHECK(AI("update.check", NULL) && g_rc.status == INK_RC_DONE, "first offer");
        CHECK(AI("update.download", NULL) && g_rc.status == INK_RC_DONE, "download starts");
        CHECK(WAIT_FOR(U.dl_off > 0, 10000), "first bytes");
        vtk_event_at(vtk_now() + SIM_MS(1), change_offer, NULL);
        delay_ms(10);
        CHECK(AI("update.check", NULL) && g_rc.status == INK_RC_DONE && U.offer_code == 300, "new candidate arrives");
        bool available = getenv("INK_FINISH_RESTORE_OFFER") != NULL;
        if (available) {
            vtk_event_at(vtk_now() + SIM_MS(1), restore_offer, NULL);
            delay_ms(10);
        }
        CHECK(WAIT_FOR(!U.job, 60000), "download ends");
        pkg_t pkg;
        if (available) CHECK(read_header("/content/.update/2.0.0.inkfw", &pkg) == PKG_OK && pkg.code == 200 && hal_fs_size("/content/.update/2.0.0.inkfw") == 131284,
              "AUD18-108: active package identity and size stay fixed");
        else CHECK(hal_fs_size("/content/.update/2.0.0.inkfw") < 0 && sim_metric_value("update.download_failed") == 1,
                   "AUD18-108: unavailable pinned package cannot be reported ready");
    } else if (!strcmp(phase, "tables")) {
        const char *names[] = {"normal", "overlap", "duplicate", "unknown", "zero", "past_eof", "truncated", "overflow", "unsigned_damage"};
        for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
            char path[96]; pkg_t pkg;
            snprintf(path, sizeof path, "/content/cases/%s.inkfw", names[i]);
            pkg_err_t e = check_package(path, &pkg);
            CHECK((i == 0) == (e == PKG_OK), "AUD18-107: table %s result %d", names[i], e);
        }
    } else if (!strcmp(phase, "install") || !strcmp(phase, "refuse") || !strcmp(phase, "blocked")) {
        if (!strcmp(phase, "refuse")) refuse_state = atoi(getenv("INK_FINISH_REFUSE"));
        CHECK(BY_HAND("system.upgrade_from_card", "{\"file\":\"next.inkfw\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "installation asks");
        unsigned token = 0;
        sscanf(g_rc.object, "confirm:%u", &token);
        CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "person accepts");
        if (!strcmp(phase, "install")) { ink_probe_count("test.finished"); return; }
        delay_ms(5000);
        CHECK(sim_metric_value("update.installed") == 0 && U.me.version_code < 300, "AUD18-104/105: unsafe install refused");
        if (!strcmp(phase, "blocked")) CHECK(!strcmp(U.st.state, "resources") && U.st.code == 200 && U.st.path[0], "AUD18-105: recovery transaction survives");
    } else {
        delay_ms(6000);
        if (getenv("INK_SIM_OTA_RESOURCE_FAILS")) CHECK(!strcmp(U.st.state, "resources") && U.st.path[0], "resource failure keeps transaction");
        else CHECK(!U.st.state[0] && sim_metric_value("ota.resource_bytes") == 12288, "resources fully recovered");
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="finish-update", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
