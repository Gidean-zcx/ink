/*
 * System upgrades (system services design §12), on the whole product, over several boots: each phase is a run, the flash image
 * and the card carry over (tests/integration/update.cmake). The packages come from tools/inkfw; the slots and the bootloader
 * are the simulator's behaviour model (ports/sim/hal/hal_ota_sim.c), the signature its stand-in. Results are the probe counters
 * "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_ota.h"
#include "ink/hal_storage.h"
#include "ink/sys/audio.h"
#include "ink/sys/confirm.h"
#include "ink/sys/file.h"
#include "ink/sys/hub.h"
#include "ink/sys/notify.h"
#include "ink/sys/settings.h"
#include "ink/sys/update.h"

/* The first thing the boot check says would be gone before a test task could listen. */
static void test_boot_check(void);
#define ink_main product_main
#define ink_update_boot_check test_boot_check
#include "../../system/boot/main.c"
#undef ink_update_boot_check
#undef ink_main

#include "whole_product.h"

static const char *state_of(const char *id) {
    static char s[32];
    s[0] = 0;
    ink_state_get_str(ink_state_find(id), s, sizeof s);
    return s;
}

static uint32_t running_code(void) {
    hal_ota_info_t i;
    hal_ota_info(&i);
    return i.version_code;
}

static bool notified(const char *title, char *body, size_t cap) {
    ink_db_cursor_t c = {0};
    ink_notify_view_t v;
    while (ink_notify_next(&c, &v) == 1)
        if (!strcmp(v.title, title)) {
            if (body) snprintf(body, cap, "%s", v.body);
            return true;
        }
    return false;
}

static volatile int g_results, g_available;
static char g_result_version[24];
static bool g_result_ok;
static void on_update(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    if (!strcmp(id, "update.result")) {
        ink_args_get_bool(a, "ok", &g_result_ok);
        ink_args_get_str(a, "version", g_result_version, sizeof g_result_version);
        g_results++;
    } else g_available++;
}

/* A person asks, the card comes, they say yes: how it went comes as confirm.result. */
static volatile int g_told;
static char g_told_status[16], g_told_detail[96];
static void on_confirm(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    (void)id;
    g_told_status[0] = g_told_detail[0] = 0;
    ink_args_get_str(a, "status", g_told_status, sizeof g_told_status);
    ink_args_get_str(a, "detail", g_told_detail, sizeof g_told_detail);
    g_told++;
}

static void say_yes(const char *cap, const char *args) {
    CHECK(BY_HAND(cap, args) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "%s: by hand too it asks first (%s)", cap, ink_rc_name(g_rc.status));
    unsigned token = 0;
    sscanf(g_rc.object, "confirm:%u", &token);
    CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "%s: yes", cap);
}

/* Said yes to, and what came of it. */
static bool went_how(const char *cap, const char *args) {
    int told = g_told;
    say_yes(cap, args);
    bool came = WAIT_FOR(g_told > told, 20000);
    delay_ms(1200); /* the card has gone */
    return came;
}

static void net_none(void *arg) {
    (void)arg;
    extern void sim_net_set_link(const char *);
    sim_net_set_link("none");
}
static void net_wifi(void *arg) {
    (void)arg;
    extern void sim_net_set_link(const char *);
    sim_net_set_link("wifi");
}

/* What a new version has to prove, as the phase has it. */
static bool self_check_fails(void) { return false; }
static bool self_check_hangs(void) {
    delay_ms(600000);
    return true;
}

static void test_boot_check(void) {
    if (ink_cap_subscribe("update.", on_update, NULL, INK_EXEC_DIRECT) != 0 || ink_cap_subscribe("confirm.result", on_confirm, NULL, INK_EXEC_DIRECT) != 0) osal_panic("subscribe");
    const char *check = getenv("INK_TEST_SELF_CHECK");
    if (check && !strcmp(check, "fails")) ink_update_self_check_add(self_check_fails);
    if (check && !strcmp(check, "hangs")) ink_update_self_check_add(self_check_hangs);
    ink_update_boot_check();
}

/* ---- over the air: found, fetched in spite of the network, checked, refused until it can be, installed ---- */

static void ota1(void) {
    CHECK(running_code() == 100, "version 1 (%u)", (unsigned)running_code());
    CHECK(!strcmp(state_of("update.version"), "1.0.0"), "state says so");
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "online");
    CHECK(AI("update.check", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"newer\":true") && strstr(g_result, "\"latest\":\"2.0.0\""), "the AI asks: there is one (%s, %s)", g_result, g_rc.detail);
    CHECK(WAIT_FOR(g_available == 1, 500) && !strcmp(state_of("update.state"), "available"), "told (%s)", state_of("update.state"));
    char body[96];
    CHECK(notified("发现系统更新", body, sizeof body) && strstr(body, "2.0.0"), "the person sees it (%s)", body);

    CHECK(AI("update.download", NULL) && g_rc.status == INK_RC_DONE, "the AI fetches it (%s)", g_rc.detail);
    CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "downloading"), 500), "downloading");
    int64_t pct = 0;
    for (int i = 0; i < 200 && pct < 30; i++) {
        delay_ms(50);
        ink_state_get_int(ink_state_find("update.progress"), &pct);
    }
    CHECK(pct >= 30, "it comes (%d)", (int)pct);
    /* the card goes to a computer for a while: it waits, and the card is not in two hands */
    hw(HW_USB_ON, 0, 0);
    CHECK(WAIT_FOR(ink_usb_disk_start() == 0, 2000), "lent, between two pieces");
    delay_ms(500); /* a piece that was on its way */
    int64_t lent_at = 0, lent_end = 0;
    ink_state_get_int(ink_state_find("update.progress"), &lent_at);
    delay_ms(5000);
    ink_state_get_int(ink_state_find("update.progress"), &lent_end);
    CHECK(lent_end == lent_at && !strcmp(state_of("update.state"), "downloading"), "it does not write (%d, %d)", (int)lent_at, (int)lent_end);
    CHECK(ink_usb_disk_stop() == 0, "back");
    hw(HW_USB_OFF, 0, 0);
    /* the network goes, and comes back: it carries on */
    int64_t part = hal_fs_size("/content/.update/2.0.0.inkfw.part");
    CHECK(part > 0, "on the card as it comes (%lld)", (long long)part);
    vtk_event_at(vtk_now() + SIM_MS(1), net_none, NULL);
    delay_ms(30000);
    int64_t part2 = hal_fs_size("/content/.update/2.0.0.inkfw.part");
    CHECK(part2 >= part && !strcmp(state_of("update.state"), "downloading"), "offline it waits (%lld)", (long long)part2);
    vtk_event_at(vtk_now() + SIM_MS(1), net_wifi, NULL);
    CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "ready"), 120000), "and finishes (%s)", state_of("update.state"));
    CHECK(hal_fs_size("/content/.update/2.0.0.inkfw") > 1000000 && hal_fs_size("/content/.update/2.0.0.inkfw.part") < 0, "whole, under its own name");
    CHECK(sim_metric_value("update.downloaded") == 1 && sim_metric_value("update.download_bad") == 0, "and it checked out");

    /* the AI goes no further */
    CHECK(AI("system.upgrade", NULL) && g_rc.status == INK_RC_INVALID, "the AI cannot install (%s)", ink_rc_name(g_rc.status));
    CHECK(AI("update.status", NULL) && strstr(g_result, "\"state\":\"ready\""), "it can see how it stands (%s)", g_result);

    /* what stands in the way */
    hw(HW_USB_ON, 0, 0);
    CHECK(WAIT_FOR(ink_usb_disk_start() == 0, 2000), "the card lent");
    CHECK(went_how("system.upgrade", NULL) && !strcmp(g_told_status, "failed") && strstr(g_told_detail, "lent"), "not with the card on a computer (%s)", g_told_detail);
    CHECK(ink_usb_disk_stop() == 0, "back");
    hw(HW_USB_OFF, 0, 0);
    hw(HW_BATTERY, 20, 0);
    delay_ms(70000);
    CHECK(went_how("system.upgrade", NULL) && !strcmp(g_told_status, "failed") && strstr(g_told_detail, "battery"), "a low battery (%s)", g_told_detail);
    hw(HW_BATTERY_CHARGING, 20, 0);
    int focus = ink_audio_focus_acquire(INK_AUDIO_PRIO_CALL, NULL, NULL, INK_EXEC_DIRECT);
    CHECK(went_how("system.upgrade", NULL) && !strcmp(g_told_status, "failed") && strstr(g_told_detail, "call"), "a call (%s)", g_told_detail);
    ink_audio_focus_release(focus);
    CHECK(sim_metric_value("ota.bytes_written") == 0, "nothing has been written so far");

    /* something of the person's stays through it */
    ink_receipt_t rc;
    ink_setting_set("audio.volume", 42, INK_SRC_TOUCH, &rc);
    delay_ms(2000);

    /* a person says go; the device restarts in the middle of this call: this run ends there */
    ink_probe_count("test.finished");
    say_yes("system.upgrade", NULL);
    /* while it lasts there is nothing else to use, and it says how far it is */
    CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "installing"), 3000), "installing (%s)", state_of("update.state"));
    delay_ms(3000);
    on_ui(read_screen);
    CHECK(strstr(g_snap, "正在安装新系统") && g_btn_n == 0, "a card over everything, no way out of it (%s, %d)", g_snap, g_btn_n);
    ink_state_get_int(ink_state_find("update.progress"), &pct);
    CHECK(pct > 0 && pct < 100 && sim_metric_value("ota.bytes_written") > 0, "under way (%d)", (int)pct);
    delay_ms(300000);
    CHECK(0, "it should have restarted");
}

static void ota2(void) {
    CHECK(running_code() == 200, "the new version runs (%u)", (unsigned)running_code());
    CHECK(WAIT_FOR(sim_metric_value("ota.marked_valid") == 1, 10000), "it has proved itself");
    CHECK(WAIT_FOR(g_results == 1 && g_result_ok && !strcmp(g_result_version, "2.0.0"), 5000), "and says so (%d, %s)", g_results, g_result_version);
    CHECK(!strcmp(state_of("update.state"), "idle") && !strcmp(state_of("update.version"), "2.0.0"), "state (%s, %s)", state_of("update.state"), state_of("update.version"));
    char body[96];
    CHECK(notified("系统升级好了", body, sizeof body) && strstr(body, "2.0.0"), "the person hears (%s)", body);
    CHECK(WAIT_FOR(hal_fs_size("/content/.update/2.0.0.inkfw") < 0, 2000), "the package is cleared away");
    CHECK(ink_setting_get("audio.volume") == 42, "what the person had stays (%d)", (int)ink_setting_get("audio.volume"));
    /* the one it came from has resources: written after it proved itself */
    CHECK(sim_metric_value("ota.resource_bytes") == 4096 + 8192, "fonts and resources written (%.0f)", sim_metric_value("ota.resource_bytes"));
    /* the hub has nothing newer */
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "online");
    CHECK(AI("update.check", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"newer\":false"), "nothing newer (%s)", g_result);
    ink_probe_count("test.finished");
}

/* ---- from the card: what is wrong with a package, then one that does not hold ---- */

static void refused(const char *file, const char *why) {
    char args[96];
    snprintf(args, sizeof args, "{\"file\":\"%s\"}", file);
    CHECK(went_how("system.upgrade_from_card", args) && !strcmp(g_told_status, "failed") && strstr(g_told_detail, why), "%s: refused because %s (%s, %s)", file, why, g_told_status, g_told_detail);
    CHECK(sim_metric_value("ota.bytes_written") == 0, "%s: nothing written", file);
}

static void card1(void) {
    CHECK(running_code() == 200, "version 2 (%u)", (unsigned)running_code());
    /* at boot it looked: what can be installed is offered; the bad ones are not */
    char body[96];
    CHECK(WAIT_FOR(notified("发现系统更新", body, sizeof body), 8000) && strstr(body, "3.0.0"), "the good one on the card is offered (%s)", body);
    CHECK(g_available == 1, "once");
    refused("other-product.inkfw", "product");
    refused("bad-signature.inkfw", "signature");
    refused("bad-header.inkfw", "signature");
    refused("bad-section.inkfw", "damaged");
    refused("older.inkfw", "newer");
    refused("needs-newer.inkfw", "old");
    refused("too-big.inkfw", "large");
    refused("not-a-package.inkfw", "package");
    CHECK(went_how("system.upgrade_from_card", "{\"file\":\"../nope\"}") && !strcmp(g_told_status, "invalid"), "only that folder (%s)", g_told_status);
    CHECK(AI("system.upgrade_from_card", NULL) && g_rc.status == INK_RC_INVALID, "the AI cannot (%s)", ink_rc_name(g_rc.status));
    /* back from a computer it looks again, and what it already said it does not say twice */
    hw(HW_USB_ON, 0, 0);
    CHECK(WAIT_FOR(ink_usb_disk_start() == 0, 2000), "lent");
    CHECK(ink_usb_disk_stop() == 0, "back");
    hw(HW_USB_OFF, 0, 0);
    delay_ms(3000);
    CHECK(sim_metric_value("update.package_ignored") >= 2 * 7, "it looked again (%.0f)", sim_metric_value("update.package_ignored"));
    CHECK(g_available == 1, "said once");
    /* the good one: this run ends in it */
    ink_probe_count("test.finished");
    say_yes("system.upgrade_from_card", "{\"file\":\"good-3.inkfw\"}");
    delay_ms(300000);
    CHECK(0, "it should have restarted");
}

static void card2(void) { /* version 3, and it does not hold (INK_TEST_SELF_CHECK=fails): this comes too late, the restart is first */
    CHECK(0, "it should have restarted by now");
}

static void card3(void) {
    CHECK(running_code() == 200, "the old one again (%u)", (unsigned)running_code());
    CHECK(sim_metric_value("update.rolled_back") == 1, "it knows it went back");
    CHECK(WAIT_FOR(g_results == 1 && !g_result_ok && !strcmp(g_result_version, "3.0.0"), 2000), "and says so (%d, %s)", g_results, g_result_version);
    char body[96];
    CHECK(notified("系统升级失败", body, sizeof body) && strstr(body, "3.0.0"), "to the person too (%s)", body);
    /* the package is still there, but it is not offered again */
    CHECK(hal_fs_size("/content/update/good-3.inkfw") > 0, "the file stays");
    delay_ms(5000);
    CHECK(g_available == 0, "and is not offered again");
    CHECK(went_how("system.upgrade_from_card", "{\"file\":\"good-3.inkfw\"}") && !strcmp(g_told_status, "failed") && strstr(g_told_detail, "failed before"), "nor installed (%s)", g_told_detail);
    ink_probe_count("test.finished");
}

/* ---- the power goes ---- */

static volatile int g_cut;
static void cut_event(void *arg) {
    (void)arg;
    sim_storage_power_cut();
}

static void on_progress(void *ctx, int h, const char *id) {
    (void)ctx;
    (void)id;
    int64_t pct = 0;
    ink_state_get_int(h, &pct);
    if (pct >= 40 && !g_cut && !strcmp(state_of("update.state"), "installing")) {
        g_cut = 1;
        vtk_event_at(vtk_now() + SIM_MS(1), cut_event, NULL);
    }
}

static void cut1(void) {
    CHECK(running_code() == 200, "version 2");
    CHECK(ink_state_watch("update.progress", on_progress, NULL, INK_EXEC_DIRECT) == 0, "watch");
    ink_probe_count("test.finished");
    say_yes("system.upgrade_from_card", "{\"file\":\"good-4.inkfw\"}");
    delay_ms(600000);
    CHECK(0, "the power should have gone");
}

static void cut2(void) {
    CHECK(running_code() == 200, "the old system: it was never switched (%u)", (unsigned)running_code());
    CHECK(sim_metric_value("update.interrupted") == 1, "it knows what happened");
    char body[96];
    CHECK(WAIT_FOR(notified("发现系统更新", body, sizeof body), 8000) && strstr(body, "4.0.0"), "the package is offered again (%s)", body);
    ink_probe_count("test.finished");
    say_yes("system.upgrade_from_card", "{\"file\":\"good-4.inkfw\"}");
    delay_ms(300000);
    CHECK(0, "it should have restarted");
}

static void cut3(void) { /* the first boot of the new one, and the power goes before it proves itself (the script) */
    CHECK(running_code() == 400, "the new one (%u)", (unsigned)running_code());
    ink_probe_count("test.finished");
    delay_ms(60000);
    CHECK(0, "the power should have gone");
}

static void cut4(void) {
    CHECK(running_code() == 200, "the bootloader went back (%u)", (unsigned)running_code());
    CHECK(WAIT_FOR(g_results == 1 && !g_result_ok, 2000), "said so");
    ink_probe_count("test.finished");
}

/* A new system that is stuck, and says nothing at all. */
static void hang1(void) {
    ink_probe_count("test.finished");
    say_yes("system.upgrade_from_card", "{\"file\":\"good-5.inkfw\"}");
    delay_ms(300000);
    CHECK(0, "it should have restarted");
}

static void hang2(void) {
    CHECK(running_code() == 500, "the new one (%u)", (unsigned)running_code());
    ink_probe_count("test.finished");
    delay_ms(70000);
    CHECK(0, "it should have restarted within the minute");
}

static void hang3(void) {
    CHECK(running_code() == 200, "the old one again (%u)", (unsigned)running_code());
    CHECK(WAIT_FOR(g_results == 1 && !g_result_ok && !strcmp(g_result_version, "5.0.0"), 2000), "and says so (%d, %s)", g_results, g_result_version);
    ink_probe_count("test.finished");
}

/* The package changes while it is being written (a card that reads wrong, another one put in): what was checked is not what
 * went into the slot, and that is seen before anything depends on it. */
static void spoil_file(void) {
    hal_file_t *f = hal_fs_open("/content/update/good-7.inkfw", HAL_O_READ | HAL_O_WRITE);
    if (f) {
        hal_fs_seek(f, -16, 2);
        hal_fs_write(f, "changed!", 8);
        hal_fs_close(f);
    }
}

static void on_progress_spoil(void *ctx, int h, const char *id) {
    (void)ctx;
    (void)id;
    int64_t pct = 0;
    ink_state_get_int(h, &pct);
    if (pct >= 40 && !g_cut && !strcmp(state_of("update.state"), "installing")) {
        g_cut = 1;
        spoil_file();
    }
}

static void swap1(void) {
    CHECK(running_code() == 200, "version 2 (%u)", (unsigned)running_code());
    CHECK(ink_state_watch("update.progress", on_progress_spoil, NULL, INK_EXEC_DIRECT) == 0, "watch");
    int told = g_told;
    say_yes("system.upgrade_from_card", "{\"file\":\"good-7.inkfw\"}");
    CHECK(WAIT_FOR(g_told > told, 60000) && !strcmp(g_told_status, "failed"), "it says it could not (%s, %s)", g_told_status, g_told_detail);
    CHECK(g_cut == 1, "the file did change");
    CHECK(sim_metric_value("update.changed_while_written") == 1, "and it saw that");
    CHECK(!strcmp(state_of("update.state"), "failed"), "state (%s)", state_of("update.state"));
    delay_ms(1000);
    on_ui(read_screen);
    CHECK(!strstr(g_snap, "正在安装"), "the card has gone (%s)", g_snap);
    ink_probe_count("test.finished");
}

static void swap2(void) {
    CHECK(running_code() == 200, "the next boot is the old one (%u)", (unsigned)running_code());
    ink_probe_count("test.finished");
}

/* The power goes while the fonts are being written, after it has proved itself: the rest at the next boot. */
static void res1(void) {
    ink_probe_count("test.finished");
    say_yes("system.upgrade_from_card", "{\"file\":\"good-6.inkfw\"}");
    delay_ms(300000);
    CHECK(0, "it should have restarted");
}

static void res2(void) { /* the script cuts it */
    CHECK(running_code() == 600, "the new one (%u)", (unsigned)running_code());
    CHECK(WAIT_FOR(sim_metric_value("ota.resource_bytes") >= 8192, 5000), "writing what is not in the slot (%.0f)", sim_metric_value("ota.resource_bytes"));
    ink_probe_count("test.finished");
    delay_ms(60000);
    CHECK(0, "the power should have gone");
}

static void res3(void) {
    CHECK(running_code() == 600, "it stays: it had proved itself (%u)", (unsigned)running_code());
    CHECK(sim_metric_value("ota.marked_valid") == 0, "and does not have to again");
    CHECK(WAIT_FOR(sim_metric_value("ota.resource_bytes") >= 409600, 60000), "the resources again (%.0f)", sim_metric_value("ota.resource_bytes"));
    delay_ms(500);
    CHECK(sim_metric_value("ota.resource_bytes") == 409600, "the font, which was done, not again (%.0f)", sim_metric_value("ota.resource_bytes"));
    CHECK(hal_fs_size("/content/update/good-6.inkfw") > 0, "the person's package stays where they put it");
    CHECK(AI("update.status", NULL) && strstr(g_result, "\"state\":\"idle\""), "nothing left to do (%s)", g_result);
    ink_probe_count("test.finished");
}

/* AUD16-004, 005: writing them fails, or the package is not what it was: it stays to be done, and nothing bad is written. */
static void rfail1(void) {
    ink_probe_count("test.finished");
    say_yes("system.upgrade_from_card", "{\"file\":\"good-7.inkfw\"}");
    delay_ms(300000);
    CHECK(0, "it should have restarted");
}

static void rfail2(void) { /* the font goes in, then the resource area says no */
    CHECK(running_code() == 700, "the new one (%u)", (unsigned)running_code());
    CHECK(WAIT_FOR(sim_metric_value("update.resources_failed") == 1, 60000), "it knows");
    double wrote = sim_metric_value("ota.resource_bytes");
    CHECK(wrote >= 8192 && wrote < 409600, "some went in (%.0f)", wrote);
    char body[120];
    CHECK(WAIT_FOR(notified("系统更新还差一点", body, sizeof body), 2000), "and says so");
    CHECK(hal_fs_size("/content/update/good-7.inkfw") > 0, "the package stays");
    ink_probe_count("test.finished");
}

static void rbad(void) { /* not what it was: the header is still good, the last section is not */
    CHECK(running_code() == 700, "it stays (%u)", (unsigned)running_code());
    CHECK(WAIT_FOR(sim_metric_value("update.resources_failed") == 1, 60000), "it tries, and it sees");
    CHECK(sim_metric_value("ota.resource_bytes") == 0, "nothing of it is written (%.0f)", sim_metric_value("ota.resource_bytes"));
    ink_probe_count("test.finished");
}

static void rother(void) { /* another version under that name: well signed, and not this system's */
    CHECK(running_code() == 700, "it stays (%u)", (unsigned)running_code());
    CHECK(WAIT_FOR(sim_metric_value("update.resources_failed") == 1, 60000), "it tries, and it sees");
    CHECK(sim_metric_value("ota.resource_bytes") == 0, "nothing of it is written (%.0f)", sim_metric_value("ota.resource_bytes"));
    ink_probe_count("test.finished");
}

static void rgood(void) { /* the right one again: the rest, the font was done */
    CHECK(running_code() == 700, "it stays (%u)", (unsigned)running_code());
    CHECK(WAIT_FOR(sim_metric_value("ota.resource_bytes") >= 409600, 60000), "the resources (%.0f)", sim_metric_value("ota.resource_bytes"));
    delay_ms(500);
    CHECK(sim_metric_value("ota.resource_bytes") == 409600 && sim_metric_value("update.resources_failed") == 0, "all of them, once (%.0f)", sim_metric_value("ota.resource_bytes"));
    ink_probe_count("test.finished");
}

static void rdone(void) { /* and that was the end of it */
    delay_ms(3000);
    CHECK(sim_metric_value("ota.resource_bytes") == 0 && sim_metric_value("update.resources_failed") == 0, "nothing more to do (%.0f)", sim_metric_value("ota.resource_bytes"));
    ink_probe_count("test.finished");
}

/* ---- a download across a power cut: from where it stopped ---- */

static void resume1(void) {
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "online");
    CHECK(AI("update.check", NULL) && g_rc.status == INK_RC_DONE, "there is one");
    CHECK(AI("update.download", NULL) && g_rc.status == INK_RC_DONE, "it comes");
    int64_t pct = 0;
    for (int i = 0; i < 400 && pct < 40; i++) {
        delay_ms(25);
        ink_state_get_int(ink_state_find("update.progress"), &pct);
    }
    CHECK(pct >= 40, "far enough (%d)", (int)pct);
    ink_probe_count("test.finished");
    vtk_event_at(vtk_now() + SIM_MS(1), cut_event, NULL);
    delay_ms(10000);
    CHECK(0, "the power should have gone");
}

static void resume2(void) {
    int64_t part = hal_fs_size("/content/.update/2.0.0.inkfw.part");
    CHECK(part > 400000, "what came is still there (%lld)", (long long)part);
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "online");
    CHECK(AI("update.check", NULL) && g_rc.status == INK_RC_DONE, "there is one");
    CHECK(AI("update.download", NULL) && g_rc.status == INK_RC_DONE, "again");
    /* it is in the list of what goes on, for everyone; only an adult stops it (system UI spec §7.7) */
    CHECK(AI("job.list", NULL) && strstr(g_result, "\"kind\":\"update.download\"") && strstr(g_result, "下载系统更新 2.0.0") && strstr(g_result, "\"can_cancel\":true"), "in the list (%s)", g_result);
    int job = 0;
    const char *at = strstr(g_result, "{\"id\":");
    CHECK(at && sscanf(at + 6, "%d", &job) == 1, "its number");
    char args[32];
    snprintf(args, sizeof args, "{\"id\":%d}", job);
    CHECK(ink_member_switch("kid") == 0, "a child");
    CHECK(AI("job.list", NULL) && strstr(g_result, "update.download"), "they see it too");
    CHECK(AI("job.cancel", args) && g_rc.status == INK_RC_FAILED && strstr(g_rc.detail, "adult"), "and cannot stop it (%s)", g_rc.detail);
    ink_member_test_reset();
    delay_ms(100);
    CHECK(AI("job.cancel", args) && g_rc.status == INK_RC_DONE, "an adult can (%s)", g_rc.detail);
    CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "available"), 5000), "it stops (%s)", state_of("update.state"));
    CHECK(AI("job.list", NULL) && strstr(g_result, "\"state\":\"cancelled\""), "and the list says so (%s)", g_result);
    int64_t kept = hal_fs_size("/content/.update/2.0.0.inkfw.part");
    CHECK(kept >= part, "what came stays (%lld)", (long long)kept);
    double before_stop = sim_metric_value("hub.requests.update/fetch");
    CHECK(AI("update.download", NULL) && g_rc.status == INK_RC_DONE, "and on from there");
    CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "ready"), 60000), "and it finishes (%s)", state_of("update.state"));
    (void)before_stop;
    CHECK(AI("job.list", NULL) && strstr(g_result, "下载好了") && strstr(g_result, "\"percent\":100"), "the list has the result (%s)", g_result);
    CHECK(sim_metric_value("update.download_bad") == 0, "whole and good");
    double pieces = sim_metric_value("hub.requests.update/fetch");
    CHECK(pieces > 10 && pieces < 60, "only the rest was asked for: a whole one is 86 pieces (%.0f)", pieces);
    ink_probe_count("test.finished");
}

/* ---- by itself, if asked to ---- */

static void auto_wifi(void) {
    ink_receipt_t rc;
    CHECK(ink_setting_set("update.auto_download", 1, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "the person wants it");
    ink_hub_demand();
    CHECK(WAIT_FOR(g_available == 1, 40000), "it looks when it is online, and finds it");
    CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "ready"), 60000), "and fetches it (%s)", state_of("update.state"));
    char body[96];
    CHECK(notified("系统更新", body, sizeof body) && strstr(body, "2.0.0"), "and says so (%s)", body);
    CHECK(ink_setting_get("update.auto_download") == 1 && running_code() == 100, "installing is still a person's");
    /* not every time it comes online */
    vtk_event_at(vtk_now() + SIM_MS(1), net_none, NULL);
    CHECK(WAIT_FOR(!ink_hub_online(), 40000), "offline");
    vtk_event_at(vtk_now() + SIM_MS(1), net_wifi, NULL);
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 60000), "online");
    delay_ms(1000);
    CHECK(sim_metric_value("hub.requests.update/check") == 1, "once a day is enough (%.0f)", sim_metric_value("hub.requests.update/check"));
    ink_probe_count("test.finished");
}

static void auto_mobile(void) {
    ink_receipt_t rc;
    CHECK(ink_setting_set("update.auto_download", 1, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "the person wants it");
    ink_hub_demand();
    CHECK(WAIT_FOR(g_available == 1, 60000), "found on mobile data");
    delay_ms(3000);
    CHECK(sim_metric_value("update.download_started") == 0, "that costs money: not by itself");
    CHECK(AI("update.download", NULL) && g_rc.status == INK_RC_REJECTED, "nor for the AI (%s)", ink_rc_name(g_rc.status));
    CHECK(BY_HAND("update.download", NULL) && g_rc.status == INK_RC_DONE, "a person can (%s)", g_rc.detail);
    CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "ready"), 120000), "and it comes (%s)", state_of("update.state"));
    ink_probe_count("test.finished");
}

static void test_task(void *arg) {
    (void)arg;
    delay_ms(1500);
    const char *phase = getenv("INK_UPDATE_PHASE");
    if (!strcmp(phase, "ota1")) ota1();
    else if (!strcmp(phase, "ota2")) ota2();
    else if (!strcmp(phase, "card1")) card1();
    else if (!strcmp(phase, "card2")) card2();
    else if (!strcmp(phase, "card3")) card3();
    else if (!strcmp(phase, "cut1")) cut1();
    else if (!strcmp(phase, "cut2")) cut2();
    else if (!strcmp(phase, "cut3")) cut3();
    else if (!strcmp(phase, "cut4")) cut4();
    else if (!strcmp(phase, "hang1")) hang1();
    else if (!strcmp(phase, "hang2")) hang2();
    else if (!strcmp(phase, "hang3")) hang3();
    else if (!strcmp(phase, "swap1")) swap1();
    else if (!strcmp(phase, "swap2")) swap2();
    else if (!strcmp(phase, "resume1")) resume1();
    else if (!strcmp(phase, "resume2")) resume2();
    else if (!strcmp(phase, "res1")) res1();
    else if (!strcmp(phase, "res2")) res2();
    else if (!strcmp(phase, "res3")) res3();
    else if (!strcmp(phase, "rfail1")) rfail1();
    else if (!strcmp(phase, "rfail2")) rfail2();
    else if (!strcmp(phase, "rbad")) rbad();
    else if (!strcmp(phase, "rother")) rother();
    else if (!strcmp(phase, "rgood")) rgood();
    else if (!strcmp(phase, "rdone")) rdone();
    else if (!strcmp(phase, "auto_wifi")) auto_wifi();
    else if (!strcmp(phase, "auto_mobile")) auto_mobile();
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "updatetest", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("test task");
}
