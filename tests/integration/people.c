/*
 * Wi-Fi management, the family's members and the PIN (system services design §11), on the whole product. INK_PEOPLE_PHASE
 * picks the run: main, reset1 and reset2 (a factory reset, and what is left at the next boot). Results are the probe counters
 * "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_storage.h"
#include "ink/hal_net.h"
#include "ink/hal_usb.h"
#include "ink/sys/audio.h"
#include "ink/sys/confirm.h"
#include "ink/sys/data.h"
#include "ink/sys/file.h"
#include "ink/sys/hub.h"
#include "ink/sys/members.h"
#include "ink/sys/net.h"
#include "ink/sys/notify.h"
#include "ink/sys/power.h"
#include "ink/sys/settings.h"
#include "ink/sys/storage.h"
#include "ink/sys/time.h"
#include "ink/sys/update.h"

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

#include "whole_product.h"

static const char *state_of(const char *id) {
    static char s[64];
    s[0] = 0;
    ink_state_get_str(ink_state_find(id), s, sizeof s);
    return s;
}
static bool authed(void) {
    int64_t v = 0;
    ink_state_get_int(ink_state_find("member.authed"), &v);
    return v != 0;
}

/* ---- Wi-Fi ---- */

static uint32_t token_of_receipt(void) {
    unsigned t = 0;
    sscanf(g_rc.object, "confirm:%u", &t);
    return t;
}

static volatile int g_wifi_results;
static char g_wifi_result[16];
static void wifi_result(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx; (void)id;
    char cap[48];
    if (ink_args_get_str(a, "cap", cap, sizeof cap) > 0 && !strcmp(cap, "wifi.forget")) {
        ink_args_get_str(a, "status", g_wifi_result, sizeof g_wifi_result);
        g_wifi_results++;
    }
}

static void wifi(void) {
    CHECK(ink_cap_subscribe("confirm.result", wifi_result, NULL, INK_EXEC_DIRECT) == 0, "Wi-Fi confirmation results");
    CHECK(!strcmp(state_of("wifi.state"), "idle"), "idle (%s)", state_of("wifi.state"));
    CHECK(AI("wifi.scan", NULL) && g_rc.status == INK_RC_DONE, "the AI looks around (%s)", g_rc.detail);
    CHECK(strstr(g_result, "\"ssid\":\"家里\"") && strstr(g_result, "\"secured\":false"), "it sees them (%s)", g_result);
    const char *near = strstr(g_result, "家里"), *cafe = strstr(g_result, "咖啡店"), *far = strstr(g_result, "邻居");
    CHECK(near && cafe && far && near < cafe && cafe < far, "strongest first (%s)", g_result);
    CHECK(AI("wifi.list", NULL) && !strcmp(g_result, "[]"), "it knows none yet (%s)", g_result);
    CHECK(!hal_net_available(HAL_LINK_WIFI), "so none is available");

    CHECK(AI("wifi.connect", "{\"ssid\":\"家里\",\"password\":\"hunter2hunter2\"}") && g_rc.status == INK_RC_REJECTED, "AI cannot enter a password");
    /* wrong password, by hand */
    CHECK(BY_HAND("wifi.connect", "{\"ssid\":\"家里\",\"password\":\"wrong\"}") && g_rc.status == INK_RC_FAILED && strstr(g_rc.detail, "password"), "the wrong password (%s)", g_rc.detail);
    CHECK(!strcmp(state_of("wifi.state"), "failed"), "says so");
    CHECK(AI("wifi.list", NULL) && !strstr(g_result, "家里"), "and it is not kept");
    CHECK(BY_HAND("wifi.connect", "{\"ssid\":\"没有这个\"}") && g_rc.status == INK_RC_FAILED && strstr(g_rc.detail, "around"), "one that is not there (%s)", g_rc.detail);

    /* right */
    CHECK(BY_HAND("wifi.connect", "{\"ssid\":\"家里\",\"password\":\"hunter2hunter2\"}") && g_rc.status == INK_RC_DONE, "person joins with their password (%s)", g_rc.detail);
    CHECK(!strcmp(state_of("wifi.state"), "connected") && !strcmp(state_of("wifi.ssid"), "家里"), "state (%s, %s)", state_of("wifi.state"), state_of("wifi.ssid"));
    CHECK(WAIT_FOR(hal_net_available(HAL_LINK_WIFI), 1000), "available to the net service now");
    CHECK(WAIT_FOR(!strcmp(state_of("net.type"), "wifi"), 15000), "which chooses it (%s)", state_of("net.type"));
    CHECK(AI("wifi.list", NULL) && !strcmp(g_result, "[\"家里\"]"), "names only (%s)", g_result);

    CHECK(AI("wifi.connect", "{\"ssid\":\"家里\"}") && g_rc.status == INK_RC_DONE, "D-105: AI reconnects to a stored network directly");
    /* the password is nobody's to read */
    CHECK(AI("data.collections", NULL) && !strstr(g_result, "net.wifi"), "the AI does not even see the collection (%s)", g_result);
    CHECK(AI("data.list", "{\"collection\":\"net.wifi\"}") && g_rc.status != INK_RC_DONE, "nor can read it (%s)", ink_rc_name(g_rc.status));
    CHECK(AI("state.list", NULL) && !strstr(g_result, "hunter2"), "not in the state");

    /* the network goes; it comes back */
    sim_wifi_ap_del("家里");
    CHECK(WAIT_FOR(!hal_net_available(HAL_LINK_WIFI), 1000), "gone away");
    sim_wifi_ap_add("家里", -60, "hunter2hunter2");
    CHECK(WAIT_FOR(hal_net_available(HAL_LINK_WIFI), 1000), "back: it knows it");
    sim_wifi_ap_add("家里", -60, "changed-the-password");
    CHECK(WAIT_FOR(!hal_net_available(HAL_LINK_WIFI), 1000), "with another password it does not count");
    sim_wifi_ap_add("家里", -60, "hunter2hunter2");

    /* forget */
    CHECK(AI("wifi.forget", "{\"ssid\":\"家里\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "the AI forgetting one needs a yes");
    CHECK(ink_confirm_withdraw(token_of_receipt(), INK_SRC_AI) == 0, "taken back");
    delay_ms(1200);
    CHECK(BY_HAND("wifi.forget", "{\"ssid\":\"家里\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "by hand too");
    int results = g_wifi_results;
    CHECK(ink_confirm_decide(token_of_receipt(), true, INK_SRC_TOUCH) == 0 && WAIT_FOR(g_wifi_results > results, 2000) && !strcmp(g_wifi_result, "done"), "person confirms forgetting the saved password");
    CHECK(WAIT_FOR(!hal_net_available(HAL_LINK_WIFI), 1000), "not known any more");
    CHECK(BY_HAND("wifi.forget", "{\"ssid\":\"家里\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "second attempt still asks");
    results = g_wifi_results;
    CHECK(ink_confirm_decide(token_of_receipt(), true, INK_SRC_TOUCH) == 0 && WAIT_FOR(g_wifi_results > results, 2000) && !strcmp(g_wifi_result, "failed"), "and not twice");
    /* a child cannot */
    CHECK(ink_member_switch("kid") == 0, "a child");
    CHECK(BY_HAND("wifi.connect", "{\"ssid\":\"咖啡店\"}") && g_rc.status == INK_RC_REJECTED, "cannot join networks (%s)", ink_rc_name(g_rc.status));
    CHECK(AI("wifi.scan", NULL) && g_rc.status == INK_RC_DONE, "but can look");
    ink_member_test_reset();
    delay_ms(300);
}

/* ---- the PIN ---- */

static volatile int g_pin_done, g_pin_result;
static volatile uint32_t g_pin_wait;
static void pin_told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx;
    g_pin_result = result;
    g_pin_wait = wait_s;
    g_pin_done++;
}

static int unlock(const char *id, const char *pin) {
    int d = g_pin_done;
    if (ink_member_unlock(id, pin, pin_told, NULL) != 0) return -99;
    return WAIT_FOR(g_pin_done > d, 5000) ? g_pin_result : -98;
}
static int set_pin(const char *id, const char *old_pin, const char *new_pin) {
    int d = g_pin_done;
    if (ink_member_set_pin(id, old_pin, new_pin, pin_told, NULL) != 0) return -99;
    return WAIT_FOR(g_pin_done > d, 5000) ? g_pin_result : -98;
}

static void go_home(void) { ink_ui_home(); }

static void members_and_pin(void) {
    CHECK(AI("member.list", NULL) && strstr(g_result, "\"id\":\"owner\"") && strstr(g_result, "\"role\":\"child\"") && strstr(g_result, "\"has_pin\":false"), "who there is (%s)", g_result);
    CHECK(ink_member_space_open() && authed(), "with no PIN the space is open");

    /* no one but the person */
    CHECK(AI("member.set_pin", "{\"id\":\"owner\",\"new_pin\":\"1234\"}") && g_rc.status == INK_RC_INVALID, "not the AI (%s)", ink_rc_name(g_rc.status));
    CHECK(call_as("member.unlock", "{\"id\":\"owner\",\"pin\":\"1234\"}", INK_SRC_REMOTE) && g_rc.status == INK_RC_INVALID, "not the hub");
    CHECK(AI("caps.find", "{\"query\":\"PIN\"}") && !strstr(g_result, "member.unlock") && !strstr(g_result, "member.set_pin"), "not even told they exist (%s)", g_result);

    /* setting it: digits, four to eight */
    CHECK(set_pin("owner", NULL, "12a4") == INK_PIN_BAD_FORM, "digits");
    CHECK(set_pin("owner", NULL, "123") == INK_PIN_BAD_FORM, "at least four");
    double t0 = (double)osal_now_us();
    CHECK(set_pin("owner", NULL, "123456") == INK_PIN_OK, "set");
    double ms = ((double)osal_now_us() - t0) / 1000.0;
    CHECK(ms > 50 && ms < 400, "it takes a moment on purpose (%.0f ms)", ms);
    CHECK(ink_member_has_pin("owner") && ink_member_space_open(), "the one who set it stays in");
    CHECK(AI("member.list", NULL) && strstr(g_result, "\"has_pin\":true"), "the list says so, and no more (%s)", g_result);
    /* nothing of it to be read anywhere */
    CHECK(AI("data.list", "{\"collection\":\"member.table\"}") && g_rc.status != INK_RC_DONE, "the table is closed");

    /* changing it needs the old one */
    CHECK(set_pin("owner", "000000", "654321") == INK_PIN_WRONG, "the wrong old one");
    CHECK(set_pin("owner", "123456", "654321") == INK_PIN_OK, "the right one");

    /* D-95: reading stays open; only time spent locked counts. */
    CHECK(setting_by_hand("member.idle_return_s", 60), "a minute with human confirmation");
    ink_power_activity(osal_now_us());
    CHECK(unlock("owner", "654321") == INK_PIN_OK, "in");
    delay_ms(50000);
    CHECK(ink_member_space_open(), "for a while");
    ink_power_activity(osal_now_us());
    delay_ms(50000);
    CHECK(ink_member_space_open(), "touching it keeps it open");
    delay_ms(20000);
    CHECK(ink_member_space_open(), "reading past the grace stays open");
    ink_power_lock();
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 2000) && ink_member_space_open(), "a fresh lock keeps the grace");
    CHECK(WAIT_FOR(!ink_member_space_open(), 62000), "locked past the grace: it closes");
    CHECK(WAIT_FOR(!authed(), 2000), "state says so");
    ink_power_unlock(osal_now_us());
    delay_ms(500);
    on_ui(read_screen);
    CHECK(!strcmp(g_top, "member_gate"), "the gate is up (%s)", g_top);

    /* what gets through a closed space */
    CHECK(AI("state.list", NULL) && g_rc.status == INK_RC_REJECTED, "the AI gets nothing (%s)", ink_rc_name(g_rc.status));
    CHECK(AI("alarm.dismiss", "{\"id\":0}") && g_rc.status == INK_RC_REJECTED, "not even what cannot wait");
    CHECK(BY_HAND("reader.list_books", NULL) && g_rc.status == INK_RC_REJECTED, "a hand does not get past it either (%s)", ink_rc_name(g_rc.status));
    CHECK(BY_HAND("alarm.dismiss", "{\"id\":0}") && g_rc.status != INK_RC_REJECTED, "but can stop an alarm (%s)", ink_rc_name(g_rc.status));
    CHECK(BY_HAND("member.list", NULL) && g_rc.status == INK_RC_DONE, "and see who there is");

    /* wrong guesses */
    CHECK(unlock("owner", "111111") == INK_PIN_WRONG && g_pin_wait == 0, "one wrong");
    CHECK(unlock("owner", "222222") == INK_PIN_WRONG && g_pin_wait == 0, "two");
    CHECK(unlock("owner", "333333") == INK_PIN_WRONG && g_pin_wait == 30, "three: thirty seconds (%u)", (unsigned)g_pin_wait);
    CHECK(unlock("owner", "654321") == INK_PIN_WAIT && g_pin_wait > 20, "even the right one must wait (%d, %u)", g_pin_result, (unsigned)g_pin_wait);
    CHECK(!ink_member_space_open(), "still closed");
    delay_ms(31000);
    CHECK(unlock("owner", "444444") == INK_PIN_WRONG && g_pin_wait == 60, "again wrong: twice as long (%u)", (unsigned)g_pin_wait);
    delay_ms(61000);
    CHECK(unlock("owner", "654321") == INK_PIN_OK && ink_member_space_open() && authed(), "right: in");
    CHECK(unlock("owner", "111111") == INK_PIN_WRONG && g_pin_wait == 0, "from the start");
    CHECK(unlock("owner", "654321") == INK_PIN_OK, "in");
    delay_ms(600);
    on_ui(read_screen);
    CHECK(strcmp(g_top, "member_gate"), "the gate is gone (%s)", g_top);

    /* The configured lock grace is also used by the member gate. */
    CHECK(setting_by_hand("member.idle_return_s", 30), "half a minute with human confirmation");
    ink_power_activity(osal_now_us());
    ink_power_lock();
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 2000) && ink_member_space_open(), "locked: still in the grace");
    CHECK(WAIT_FOR(!ink_member_space_open(), 32000), "the lock grace expires");
    ink_power_unlock(osal_now_us());
    delay_ms(800);
    on_ui(read_screen);
    CHECK(!strcmp(g_top, "member_gate") && g_btn_n >= 2, "unlocked: the gate (%s, %d)", g_top, g_btn_n);

    /* by finger: the child needs nothing, the adult the PIN */
    CHECK(ink_member_switch("owner") == -2, "an adult with a PIN cannot simply be switched to");
    tap(g_btn_x[1], g_btn_y[1]); /* the child */
    CHECK(WAIT_FOR(!strcmp(ink_member_current()->id, "kid"), 1000), "a finger on the child (%s)", ink_member_current()->id);
    CHECK(WAIT_FOR((on_ui(read_screen), strcmp(g_top, "member_gate") != 0), 2000), "and the gate goes (%s)", g_top);
    CHECK(ink_member_space_open(), "a child's space is simply open");
    CHECK(ink_member_switch("owner") == -2, "back takes the PIN");
    CHECK(AI("system.switch_member", "{\"id\":\"owner\"}") && g_rc.status == INK_RC_REJECTED, "the AI cannot do it for them");
    CHECK(unlock("owner", "654321") == INK_PIN_OK && !strcmp(ink_member_current()->id, "owner"), "with it, the adult");
    on_ui(go_home);
    delay_ms(600);
}

static int add_results;
static char add_status[24];
static void add_result(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx; (void)id;
    ink_args_get_str(a, "status", add_status, sizeof add_status);
    add_results++;
}
static bool add_confirmed(const char *args, const char *status) {
    int before = add_results, pin_before = g_pin_done;
    if (!BY_HAND("member.add", args) || g_rc.status != INK_RC_PENDING_CONFIRMATION) return false;
    if (ink_confirm_accept_with_pin(token_of_receipt(), "654321", pin_told, NULL) != 0) return false;
    return WAIT_FOR(g_pin_done > pin_before && add_results > before, 5000) && !strcmp(add_status, status);
}

static void add_and_remove(void) {
    CHECK(ink_cap_subscribe("confirm.result", add_result, NULL, INK_EXEC_DIRECT) == 0, "listen to the confirmed result");
    CHECK(AI("member.add", "{\"id\":\"grandma\",\"name\":\"奶奶\",\"role\":\"adult\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "the AI adding someone needs a yes");
    CHECK(ink_confirm_withdraw(token_of_receipt(), INK_SRC_AI) == 0, "taken back");
    delay_ms(1200);
    CHECK(add_confirmed("{\"id\":\"grandma\",\"name\":\"奶奶\",\"role\":\"adult\"}", "done"), "by hand (%s)", g_rc.detail);
    CHECK(add_confirmed("{\"id\":\"grandma\"}", "failed"), "once");
    CHECK(add_confirmed("{\"id\":\"../x\"}", "invalid"), "a name for a folder (%s)", ink_rc_name(g_rc.status));
    CHECK(AI("member.list", NULL) && strstr(g_result, "奶奶"), "listed (%s)", g_result);

    /* with their own data, on the flash and on the card */
    ink_notify_t forher = {.title = "给奶奶", .prio = INK_NOTIFY_LOW, .member = "grandma"};
    CHECK(ink_notify_post(&forher, NULL) == 0, "something for them");
    delay_ms(500);
    hal_fs_mkdir("/content/.m");
    hal_fs_mkdir("/content/.m/grandma");
    hal_file_t *f = hal_fs_open("/content/.m/grandma/photo.jpg", HAL_O_WRITE | HAL_O_CREATE);
    CHECK(f != NULL, "a file of theirs");
    if (f) hal_fs_close(f);
    CHECK(ink_storage_write_atomic("/user/m/grandma/reader.state", "page 3", 6) == 0, "what an app kept for them");
    CHECK(WAIT_FOR(ink_storage_size("/user/m/grandma/reader.state") == 6, 2000), "on the flash");

    /* the card on a computer: their files cannot be reached, so they are not removed */
    hw(HW_USB_ON, 0, 0);
    CHECK(WAIT_FOR(ink_usb_disk_start() == 0, 2000), "the card is lent");
    CHECK(BY_HAND("member.remove", "{\"id\":\"grandma\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "asked");
    int lent = g_pin_done;
    CHECK(ink_confirm_accept_with_pin(token_of_receipt(), "654321", pin_told, NULL) == 0 && WAIT_FOR(g_pin_done > lent, 5000), "with the PIN");
    delay_ms(500);
    CHECK(AI("member.list", NULL) && strstr(g_result, "奶奶"), "they stay");
    CHECK(ink_usb_disk_stop() == 0, "the card comes back");
    hw(HW_USB_OFF, 0, 0);
    delay_ms(1200);

    /* removing someone takes a parent, whoever asks */
    CHECK(BY_HAND("member.remove", "{\"id\":\"grandma\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "by hand too it comes with a card (%s)", ink_rc_name(g_rc.status));
    uint32_t token = token_of_receipt();
    ink_confirm_view_t v;
    CHECK(ink_confirm_list(&v, 1) == 1 && v.parent && strstr(v.detail, "grandma"), "which asks for the PIN");
    CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) != 0, "a plain yes is not enough");
    int d = g_pin_done;
    CHECK(ink_confirm_accept_with_pin(token, "000000", pin_told, NULL) == 0 && WAIT_FOR(g_pin_done > d, 5000) && g_pin_result == INK_PIN_WRONG, "the wrong PIN (%d)", g_pin_result);
    CHECK(AI("member.list", NULL) && strstr(g_result, "奶奶"), "nothing has happened");
    d = g_pin_done;
    CHECK(ink_confirm_accept_with_pin(token, "654321", pin_told, NULL) == 0 && WAIT_FOR(g_pin_done > d, 5000) && g_pin_result == INK_PIN_OK, "the right one (%d)", g_pin_result);
    CHECK(WAIT_FOR((AI("member.list", NULL), !strstr(g_result, "奶奶")), 3000), "gone (%s)", g_result);
    CHECK(WAIT_FOR(hal_fs_size("/content/.m/grandma/photo.jpg") < 0, 3000), "their files on the card too");
    CHECK(ink_storage_size("/user/m/grandma/notify.inbox.snap") < 0 && ink_storage_size("/user/m/grandma/notify.inbox.1.log") < 0, "and on the flash");
    CHECK(ink_storage_size("/user/m/grandma/reader.state") < 0, "what is not a collection too");
    delay_ms(1200);

    /* the one in use and the last adult stay */
    int t = BY_HAND("member.remove", "{\"id\":\"owner\"}");
    uint32_t tk = token_of_receipt();
    d = g_pin_done;
    CHECK(t && ink_confirm_accept_with_pin(tk, "654321", pin_told, NULL) == 0 && WAIT_FOR(g_pin_done > d, 5000), "asked");
    delay_ms(500);
    CHECK(AI("member.list", NULL) && strstr(g_result, "\"owner\""), "the one using the device is not removed");
    delay_ms(1200);
}

static void reboot_keeps(void) {
    /* the wait across a restart is checked by the reset phases: here, what is stored is what is read */
    CHECK(ink_member_has_pin("owner"), "a PIN");
}

/* ---- a factory reset ---- */

static char g_reset_status[20], g_reset_say[96];
static int g_reset_results;

static void on_reset_result(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    (void)id;
    ink_args_get_str(a, "status", g_reset_status, sizeof g_reset_status);
    ink_args_get_str(a, "say", g_reset_say, sizeof g_reset_say);
    g_reset_results++;
}

/* The AI asks, a person says yes with the PIN. */
static void yes_to_reset(void) {
    CHECK(AI("system.factory_reset", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "the AI asks (%s)", ink_rc_name(g_rc.status));
    CHECK(ink_confirm_accept_with_pin(token_of_receipt(), "1234", NULL, NULL) == 0, "yes");
}

static void reset1(void) {
    CHECK(set_pin("owner", NULL, "1234") == INK_PIN_OK, "a PIN");
    CHECK(unlock("owner", "1111") == INK_PIN_WRONG, "a wrong guess, stored");
    CHECK(unlock("owner", "1234") == INK_PIN_OK, "in");
    ink_receipt_t rc;
    ink_setting_set("audio.volume", 33, INK_SRC_TOUCH, &rc);
    /* what is about the device, not about anyone */
    static const char UPDATE_STATE[] = "{\"blocked\":123}";
    ink_storage_write_atomic(INK_UPDATE_STATE_FILE, UPDATE_STATE, sizeof UPDATE_STATE - 1);
    /* the card is lent to a computer (AUD16-006): it cannot be cleared from here, so nothing happens at all */
    CHECK(ink_cap_subscribe("confirm.result", on_reset_result, NULL, INK_EXEC_DIRECT) == 0, "listening");
    CHECK(WAIT_FOR(hal_usb_host_connected(), 6000), "a cable");
    CHECK(ink_usb_disk_start() == 0, "the card is lent");
    int results = g_reset_results;
    yes_to_reset();
    CHECK(WAIT_FOR(g_reset_results > results, 5000), "an answer");
    CHECK(!strcmp(g_reset_status, "failed") && strstr(g_reset_say, "存储卡借给电脑了"), "it says why not (%s, %s)", g_reset_status, g_reset_say);
    CHECK(ink_storage_size("/user/sys/reset.pending") < 0, "nothing is marked");
    delay_ms(1000);
    CHECK(sim_metric_value("system.factory_resets") == 0, "and it does not restart");
    CHECK(ink_usb_disk_stop() == 0, "taken back");
    delay_ms(1000);
    /* some of everyone's */
    CHECK(ink_fs_write_atomic("/me/mine.txt", "mine", 4) == 0, "a file");
    CHECK(ink_fs_write_atomic("/me/a/b/c/d/e/f/g/h/i/deep.txt", "mine", 4) == 0, "and one deep down (AUD17-002)");
    CHECK(ink_fs_write_atomic("/family/shared.txt", "shared", 6) == 0, "a shared one");
    ink_notify_t n = {.title = "一条", .prio = INK_NOTIFY_LOW};
    ink_notify_post(&n, NULL);
    delay_ms(2000);
    while (ink_storage_pending()) delay_ms(50);
    results = g_reset_results;
    /* the AI asks: a card, with the PIN */
    CHECK(AI("system.factory_reset", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "the AI asks (%s)", ink_rc_name(g_rc.status));
    uint32_t token = token_of_receipt();
    ink_confirm_view_t v;
    CHECK(ink_confirm_list(&v, 1) == 1 && v.parent, "it takes a parent");
    delay_ms(600);
    on_ui(read_screen);
    CHECK(strstr(g_snap, "清掉所有家人的数据"), "the card says what (%s)", g_snap);
    ink_probe_count("test.finished"); /* the device restarts on the yes */
    CHECK(ink_confirm_accept_with_pin(token, "1234", NULL, NULL) == 0, "yes");
    CHECK(WAIT_FOR(g_reset_results > results + 0, 5000) && !strcmp(g_reset_status, "done") && strstr(g_reset_say, "马上重启"), "it says so (%s, %s)", g_reset_status, g_reset_say);
    CHECK(ink_storage_size("/user/sys/reset.pending") >= 0, "and the mark is there before it says so");
    delay_ms(30000);
    CHECK(0, "it should have restarted");
}

static void reset2(void) {
    CHECK(!ink_member_has_pin("owner"), "no PIN any more");
    CHECK(ink_setting_get("audio.volume") == 70, "settings as new (%d)", (int)ink_setting_get("audio.volume"));
    CHECK(ink_member_space_open(), "the first boot again");
    ink_fs_stat_t st;
    CHECK(ink_fs_stat("/me/mine.txt", &st) == INK_FS_ERR_NOT_FOUND, "a member's files are gone");
    CHECK(ink_fs_stat("/me/a/b/c/d/e/f/g/h/i/deep.txt", &st) == INK_FS_ERR_NOT_FOUND, "however deep");
    CHECK(hal_fs_size("/content/.m/owner/a") < 0, "folders too");
    CHECK(ink_fs_stat("/family/shared.txt", &st) == 0, "the family's are not");
    CHECK(hal_fs_size("/content/books/星星邮差.txt") > 0, "nor the books");
    CHECK(ink_storage_size("/user/sys/reset.pending") < 0 && ink_storage_size("/user/sys/reset.card") < 0, "nothing waits");
    char state[32] = "";
    CHECK(ink_storage_read(INK_UPDATE_STATE_FILE, state, sizeof state - 1) > 0 && strstr(state, "blocked"), "what is about the device stays (%s)", state);
    ink_probe_count("test.finished");
}

/* The card is not there (AUD16-006): the flash is cleared, and the card's part waits for it. */
static void nocard1(void) {
    CHECK(set_pin("owner", NULL, "1234") == INK_PIN_OK, "a PIN");
    CHECK(ink_fs_write_atomic("/me/on-card.txt", "mine", 4) == 0, "a file on the card");
    CHECK(ink_fs_write_atomic("/family/stays.txt", "shared", 6) == 0, "a shared one");
    CHECK(WAIT_FOR(!hal_fs_card_present(), 15000), "the card is taken out");
    CHECK(ink_cap_subscribe("confirm.result", on_reset_result, NULL, INK_EXEC_DIRECT) == 0, "listening");
    ink_probe_count("test.finished");
    yes_to_reset();
    CHECK(WAIT_FOR(g_reset_results > 0, 5000), "an answer");
    CHECK(strstr(g_reset_say, "存储卡不在"), "it says what is left (%s)", g_reset_say);
    delay_ms(30000);
    CHECK(0, "it should have restarted");
}

static void nocard_flash_clean(void) {
    CHECK(!ink_member_has_pin("owner"), "no PIN any more");
    CHECK(ink_storage_size("/user/sys/reset.pending") < 0 && ink_storage_size("/user/sys/reset.card") >= 0, "the card's part waits");
}

static void nocard_card_clean(void) {
    ink_fs_stat_t st;
    CHECK(ink_fs_stat("/me/on-card.txt", &st) == INK_FS_ERR_NOT_FOUND, "what was on the card is gone");
    CHECK(ink_fs_stat("/family/stays.txt", &st) == 0, "the family's stays");
    CHECK(ink_storage_size("/user/sys/reset.card") < 0, "nothing waits any more");
    ink_db_cursor_t c = {0};
    ink_notify_view_t v;
    bool told = false;
    while (ink_notify_next(&c, &v) == 1) told = told || strstr(v.title, "存储卡上的个人文件");
    CHECK(told, "and the person is told");
    CHECK(ink_fs_write_atomic("/me/new-owner.txt", "new", 3) == 0, "someone new starts");
}

static void nocard2(void) { /* the script puts it in, while the device runs */
    nocard_flash_clean();
    CHECK(!hal_fs_card_present(), "still no card");
    CHECK(WAIT_FOR(hal_fs_card_present(), 15000), "it is put in");
    CHECK(WAIT_FOR(sim_metric_value("system.factory_reset_card_done") == 1, 7000), "cleared as soon as it is seen");
    delay_ms(500);
    nocard_card_clean();
    while (ink_storage_pending()) delay_ms(50);
    delay_ms(500);
    ink_probe_count("test.finished");
}

static void nocard3(void) { /* a later boot does not clear what is new */
    ink_fs_stat_t st;
    CHECK(sim_metric_value("system.factory_reset_card_done") == 0, "nothing to do any more");
    CHECK(ink_fs_stat("/me/new-owner.txt", &st) == 0, "what the new one saved stays");
    ink_probe_count("test.finished");
}

static void noboot2(void) { /* a boot without it */
    nocard_flash_clean();
    CHECK(!hal_fs_card_present(), "still no card");
    delay_ms(1000);
    ink_probe_count("test.finished");
}

static void noboot3(void) { /* a boot with it: before anything could read it */
    CHECK(sim_metric_value("system.factory_reset_card_at_boot") == 1, "cleared at the boot, before anything could read it");
    CHECK(sim_metric_value("system.factory_reset_card_done") == 1, "and the person is told");
    delay_ms(500);
    nocard_card_clean();
    ink_probe_count("test.finished");
}

/* The family's table is there and cannot be read (AUD16-001): nobody gets in as a default adult without a PIN. INK_TEST_TABLE_FAILS. */
static void table1(void) {
    CHECK(set_pin("owner", NULL, "1234") == INK_PIN_OK, "a PIN");
    ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 6000), "stored");
    ink_probe_count("test.finished");
}

static void table2(void) { /* two reads fail: the boot's and the first try */
    CHECK(sim_metric_value("members.table_unread") >= 1, "the boot could not read it");
    CHECK(ink_member_has_pin("owner") && ink_member_any_pin(), "so an adult may have a PIN");
    CHECK(!ink_member_space_open(), "the space is closed");
    CHECK(AI("file.list", "{\"path\":\"/me\"}") && g_rc.status == INK_RC_REJECTED, "the AI does not get in (%s)", ink_rc_name(g_rc.status));
    CHECK(ink_member_switch("owner") == -2, "no way in without the PIN");
    CHECK(ink_member_switch("kid") == 0 && ink_member_switch("owner") == -2, "nor by going round through a child");
    ink_member_test_reset();
    CHECK(unlock("owner", "1234") == INK_PIN_WAIT, "it cannot tell yet, so it asks to wait");
    CHECK(!ink_member_space_open(), "still closed");
    ink_probe_count("test.finished"); /* the next try can read it: everything starts over (AUD17-003) */
    int d = g_pin_done;
    CHECK(ink_member_unlock("owner", "1234", pin_told, NULL) == 0, "again");
    for (int i = 0; i < 400; i++) { /* until the power goes: nothing opens meanwhile */
        CHECK(!ink_member_space_open() && g_pin_done == d, "nothing opens, nothing is decided (%d)", g_pin_done - d);
        delay_ms(10);
    }
    CHECK(0, "it should have restarted");
}

/* The card says no to being cleared (AUD17-002): it is not called done, and it stays to do. */
static void stuck2(void) {
    ink_fs_stat_t st;
    CHECK(!ink_member_has_pin("owner"), "the flash is clean");
    CHECK(ink_storage_size("/user/sys/reset.card") >= 0, "the card's part waits");
    CHECK(sim_metric_value("system.factory_reset_card_at_boot") == 0, "not called done");
    CHECK(ink_fs_stat("/me/mine.txt", &st) == 0 || hal_fs_size("/content/.m/owner/stuck/left.txt") > 0, "what could not go is still there");
    delay_ms(6000);
    CHECK(ink_storage_size("/user/sys/reset.card") >= 0, "it keeps looking");
    ink_probe_count("test.finished");
}

static void stuck3(void) {
    CHECK(sim_metric_value("system.factory_reset_card_at_boot") == 1, "now it can");
    CHECK(hal_fs_size("/content/.m/owner/stuck/left.txt") < 0, "and it is gone");
    CHECK(ink_storage_size("/user/sys/reset.card") < 0, "nothing waits");
    ink_probe_count("test.finished");
}

static void stuck1(void) {
    CHECK(ink_fs_write_atomic("/me/stuck/left.txt", "mine", 4) == 0, "a file");
    reset1();
}

static void table3(void) { /* a normal boot: the table as it was */
    CHECK(sim_metric_value("members.table_unread") == 0, "read");
    CHECK(ink_member_has_pin("owner") && !ink_member_space_open(), "the PIN is still there");
    CHECK(unlock("owner", "1234") == INK_PIN_OK, "and it is the same one");
    ink_probe_count("test.finished");
}

static void test_task(void *arg) {
    (void)arg;
    delay_ms(1500);
    const char *phase = getenv("INK_PEOPLE_PHASE");
    if (phase && !strcmp(phase, "reset1")) reset1();
    else if (phase && !strcmp(phase, "reset2")) reset2();
    else if (phase && !strcmp(phase, "stuck1")) stuck1();
    else if (phase && !strcmp(phase, "stuck2")) stuck2();
    else if (phase && !strcmp(phase, "stuck3")) stuck3();
    else if (phase && !strcmp(phase, "nocard1")) nocard1();
    else if (phase && !strcmp(phase, "nocard2")) nocard2();
    else if (phase && !strcmp(phase, "nocard3")) nocard3();
    else if (phase && !strcmp(phase, "noboot2")) noboot2();
    else if (phase && !strcmp(phase, "noboot3")) noboot3();
    else if (phase && !strcmp(phase, "table1")) table1();
    else if (phase && !strcmp(phase, "table2")) table2();
    else if (phase && !strcmp(phase, "table3")) table3();
    else {
        wifi();
        members_and_pin();
        add_and_remove();
        reboot_keeps();
        ink_probe_count("test.finished");
    }
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "peopletest", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("test task");
}
