/*
 * System-wide actions (available on every page): the same operations the
 * HOME key, the control centre and the AI use.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_power.h"
#include "ink/hal_storage.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/ai.h"
#include "ink/sys/cap.h"
#include "ink/sys/log.h"
#include "ink/sys/camera.h"
#include "ink/sys/data.h"
#include "ink/sys/file.h"
#include "ink/sys/input.h"
#include "ink/sys/storage.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/members.h"
#include "ink/sys/notify.h"
#include "ink/sys/power.h"
#include "ink/sys/svc.h"
#include "ink/sys/update.h"
#include "ink/sys/ui.h"

static void go_home(ink_call_t *c) {
    if (ink_camera_close_locked()) return;
    ink_receipt_t *rc = &c->rc;
    ink_ui_home();
    snprintf(rc->object, sizeof rc->object, "screen:home");
    snprintf(rc->say, sizeof rc->say, "回到主页了");
}

static void full_refresh(ink_call_t *c) {
    ink_receipt_t *rc = &c->rc;
    ink_ui_hint_frame(INK_REFRESH_FULL);
    lv_obj_invalidate(lv_screen_active());
    snprintf(rc->say, sizeof rc->say, "屏幕刷新一下");
}

static void set_volume(ink_call_t *c) {
    int64_t v = 0;
    ink_args_get_int(&c->args, "percent", &v); /* the schema checked it: 0..100 */
    if (ink_setting_set("audio.volume", v, c->src, &c->rc) == INK_RC_DONE)
        snprintf(c->rc.say, sizeof c->rc.say, "音量调到 %d", (int)v);
}

/* The keys are the same thing as the AI's capability. Locked only while something plays (system UI spec §3.5, LockPlaying): there is nothing else to hear. */
#define VOLUME_STEP 10

static int g_volume_steps;
static ink_exec_job_t g_volume_job;

static void volume_job(void *arg) { /* UI task */
    (void)arg;
    int steps = __atomic_exchange_n(&g_volume_steps, 0, __ATOMIC_ACQ_REL);
    int64_t v = ink_setting_get("audio.volume") + steps * VOLUME_STEP;
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    if (!a) return;
    ink_args_init(a);
    ink_args_int(a, "percent", v < 0 ? 0 : v > 100 ? 100 : v);
    ink_receipt_t rc;
    ink_cap_invoke("system.set_volume", a, INK_SRC_KEY, 0, &rc);
    osal_free(a);
}

static void on_volume_key(int delta, bool locked) { /* input task */
    char playing[16] = "";
    ink_state_get_str(ink_state_find("audio.now_playing"), playing, sizeof playing);
    if (locked && !strcmp(playing, "none")) return;
    __atomic_add_fetch(&g_volume_steps, delta, __ATOMIC_ACQ_REL);
    ink_exec_post(INK_EXEC_UI, &g_volume_job, volume_job, NULL);
}

static void switch_member(ink_call_t *c) {
    ink_receipt_t *rc = &c->rc;
    char id[16];
    ink_args_get_str(&c->args, "id", id, sizeof id); /* required by the schema */
    int r = ink_member_switch(id);
    if (r == -2) {
        rc->status = INK_RC_REJECTED;
        snprintf(rc->detail, sizeof rc->detail, "switching to an adult needs the PIN on the device");
        return;
    }
    if (r != 0) {
        rc->status = INK_RC_INVALID;
        snprintf(rc->detail, sizeof rc->detail, "no such member");
        return;
    }
    ink_ui_home(); /* a member's space starts at their home screen */
    snprintf(rc->object, sizeof rc->object, "member:%s", id);
    snprintf(rc->say, sizeof rc->say, "现在是%s的空间", ink_member_current()->name);
}

/*
 * Everything of the people who used this device; the system and the family's shared content stay (system services design §11.5).
 * Here only a mark and a restart. The next boot does the wiping, before any service runs: nothing is still writing, nothing has
 * read what should be gone. A power cut in the middle of it, and the boot after that does it again, since the mark goes last.
 * First the flash; the mark then changes its name for the card's part. Without a card that waits: the next boot with one, or
 * while the device is in use a look every few seconds (no card-detect line is known). Any card: its folders of this product are cleared, whichever device
 * they came from (§11.5).
 */
#define RESET_MARK "/user/sys/reset.pending"
#define RESET_CARD_MARK "/user/sys/reset.card"

#define CARD_LOOK_US 5000000ull

static bool g_card_waits, g_card_cleared, g_reset_done;
static ink_exec_job_t g_reset_job, g_card_job;
static ink_svc_deadline_t g_card_at;

typedef struct { bool media, marked; } reset_request_t;
static bool write_reset_mark(bool media) { /* the storage task */
    hal_fs_mkdir("/user/sys");
    hal_file_t *f = hal_fs_open(RESET_MARK, HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    if (!f) return false;
    bool ok = hal_fs_write(f, &media, 1) == 1;
    return hal_fs_close(f) == 0 && ok;
}

static void mark_reset(void *arg) { /* the storage task, guarded */
    static const char *const KEEP_NOTHING[] = {NULL};
    reset_request_t *r = arg;
    if (write_reset_mark(r->media)) r->marked = true;
    else { /* the flash is full, the very reason for this perhaps: what is there goes now */
        ink_storage_wipe_user(KEEP_NOTHING);
        r->marked = write_reset_mark(r->media);
    }
}

static void wipe_flash(void *arg) { /* the storage task, guarded */
    (void)arg;
    /* not whose the device was but what it is: which version did not hold, which fonts are half written */
    static const char *const KEEP[] = {RESET_MARK, INK_UPDATE_STATE_FILE, NULL};
    if (ink_storage_wipe_user(KEEP)) hal_fs_rename(RESET_MARK, RESET_CARD_MARK); /* else the next boot tries again */
}

static void forget_card_mark(void *arg) { /* the storage task, guarded */
    (void)arg;
    hal_fs_remove(RESET_CARD_MARK);
}

void ink_factory_reset_resume(void) { /* boot: nothing else runs yet */
    bool just_now = ink_storage_size(RESET_MARK) >= 0;
    if (just_now) {
        ink_storage_run(wipe_flash, NULL, true);
        ink_probe_count("system.factory_reset_done");
    }
    if (ink_storage_size(RESET_CARD_MARK) < 0) return;
    bool media = false;
    g_card_waits = ink_storage_read(RESET_CARD_MARK, &media, 1) < 0 || ink_fs_wipe_reset(media) != INK_FS_OK;
    if (g_card_waits) return;
    ink_storage_run(forget_card_mark, NULL, true);
    ink_probe_count("system.factory_reset_card_at_boot");
    g_card_cleared = !just_now; /* the person was told it would wait */
    g_reset_done = just_now;
}

bool ink_factory_reset_completed(void) { bool done = g_reset_done; g_reset_done = false; return done; }

static void tell_card_cleared(void) {
    ink_probe_count("system.factory_reset_card_done");
    ink_notify_t n = {.channel = "system", .title = "存储卡上的个人文件清掉了", .body = "清空这台设备时没来得及清的，现在清了", .tag = "system.reset_card", .prio = INK_NOTIFY_NORMAL, .src = INK_SRC_SYSTEM};
    ink_notify_post(&n, NULL);
}

static void card_look(void *arg);

static void card_job(void *arg) { /* exec.bg */
    (void)arg;
    if (!g_card_waits) return;
    if (hal_fs_card_present() && ink_fs_card_hold() == INK_FS_OK) {
        bool media = false;
        bool cleared = ink_storage_read(RESET_CARD_MARK, &media, 1) >= 0 && ink_fs_wipe_reset(media) == INK_FS_OK;
        ink_fs_card_release();
        if (cleared) {
            g_card_waits = false;
            ink_storage_run(forget_card_mark, NULL, true);
            tell_card_cleared();
            return;
        }
    }
    if (ink_power_state() != INK_PWR_LOCKED) ink_svc_at(&g_card_at, osal_now_us() + CARD_LOOK_US, card_look, NULL); /* locked: nobody uses it, and the chip sleeps */
}

static void card_look(void *arg) { /* the timer task: what it does blocks on the card */
    (void)arg;
    ink_exec_post(INK_EXEC_BG, &g_card_job, card_job, NULL);
}

static void on_power_for_card(void *ctx, int h, const char *id) { /* unlocked: looking again */
    (void)ctx;
    (void)h;
    (void)id;
    card_look(NULL);
}

static void restart_job(void *arg) { /* exec.bg, after the answer */
    (void)arg;
    osal_task_delay_us(300000);
    hal_power_reboot();
}

static ink_rc_t ink_factory_reset_prepare(ink_args_t *args, ink_receipt_t *rc, ink_source_t src) {
    (void)src;
    bool media = false;
    ink_args_get_bool(args, "clear_media", &media);
    ink_args_init(args);
    if (ink_args_bool(args, "clear_media", media) ||
        ink_args_str(args, "_media", media ? "；同时清空存储卡上的书和音频" : "；存储卡上的书和音频保留")) {
        snprintf(rc->detail, sizeof rc->detail, "confirmation arguments are full");
        return INK_RC_FAILED;
    }
    return INK_RC_DONE;
}

static void factory_reset(ink_call_t *c) { /* exec.bg */
    if (ink_usb_disk_active()) { /* nothing can be done about the card from here, and it is the person's to take back */
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "the card is lent to a computer: take it back first");
        snprintf(c->rc.say, sizeof c->rc.say, "存储卡借给电脑了，先收回再清空");
        return;
    }
    reset_request_t r = {0};
    ink_args_get_bool(&c->args, "clear_media", &r.media);
    ink_storage_run(mark_reset, &r, true);
    if (!r.marked) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "cannot write to the flash");
        snprintf(c->rc.say, sizeof c->rc.say, "这台设备的存储写不进去，没能清空");
        return;
    }
    ink_probe_count("system.factory_resets");
    snprintf(c->rc.say, sizeof c->rc.say, "%s", hal_fs_card_present() ? "清空这台设备，马上重启" : "清空这台设备，马上重启。存储卡不在，下次插上时再清卡上的");
    ink_exec_post(INK_EXEC_BG, &g_reset_job, restart_job, NULL);
}

/* The authenticated hub already obtained the parent's explicit wipe choice. */
int ink_factory_reset_from_hub(void) { /* exec.bg */
    reset_request_t r = {0};
    ink_storage_run(mark_reset, &r, true);
    if (!r.marked) return -1;
    ink_probe_count("system.factory_resets");
    return ink_exec_post(INK_EXEC_BG, &g_reset_job, restart_job, NULL);
}

static void cap_reboot(ink_call_t *c) { /* exec.bg */
    snprintf(c->rc.say, sizeof c->rc.say, "重启");
    ink_db_flush_all();
    for (int i = 0; i < 100 && (!ink_db_idle() || ink_storage_pending()); i++) osal_task_delay_us(50000);
    if (!ink_log_flush()) { c->rc.status = INK_RC_FAILED; return; }
    hal_power_reboot();
}

#define LIST_CAP 4096

static void state_list(ink_call_t *c) {
    char *out = osal_malloc(LIST_CAP, OSAL_MEM_EXTERNAL);
    if (!out || ink_state_json(out, LIST_CAP, INK_EXPOSE_AI) < 0) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, out ? "too long" : "out of memory");
    } else ink_cap_result(c, out);
    osal_free(out);
}

static void settings_list(ink_call_t *c) {
    char *out = osal_malloc(LIST_CAP * 2, OSAL_MEM_EXTERNAL);
    if (!out || ink_settings_json(out, LIST_CAP * 2) < 0) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, out ? "too long" : "out of memory");
    } else ink_cap_result(c, out);
    osal_free(out);
}

static const ink_cap_t SYSTEM_ACTIONS[] = {
    {.id = "system.home", .title = "回到主页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = go_home},
    {.id = "system.full_refresh", .title = "刷新屏幕", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = full_refresh},
    {.id = "system.set_volume",
     .title = "调音量",
     .input = "{\"type\":\"object\",\"properties\":{\"percent\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":100}},"
             "\"required\":[\"percent\"]}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .while_locked = true,
     .run = set_volume},
    {.id = "system.switch_member",
     .title = "切换家人",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     .effect = INK_FX_NAVIGATE,
     .child_ok = true,
     .run = switch_member},
    {.id = "state.list", .title = "系统现在的状态（电量、网络、前台应用……）", .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = state_list},
    {.id = "settings.list", .title = "所有设置，各自的类型、取值范围和现在的值", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = settings_list},
    {.id = "system.factory_reset", .title = "清空这台设备（清掉所有家人的数据，不能撤销）", .input = "{\"type\":\"object\",\"properties\":{\"clear_media\":{\"type\":\"boolean\"}}}", .ask = "清空这台设备：清掉所有家人的数据，不能撤销{_media}", .effect = INK_FX_SYSTEM, .ask_by_hand = true, .parent_pin = true, .child_ok = false, .exec = INK_EXEC_BG, .prepare_confirm = ink_factory_reset_prepare, .run = factory_reset},
    {.id = "system.reboot", .title = "重启", .effect = INK_FX_SYSTEM, .child_ok = true, .while_locked = true, .exec = INK_EXEC_BG, .run = cap_reboot},
};

int ink_system_actions_init(void) {
    ink_input_set_volume_key_hook(on_volume_key);
    if (g_card_cleared) tell_card_cleared();
    if (g_card_waits) {
        if (ink_state_watch("power.state", on_power_for_card, NULL, INK_EXEC_DIRECT) != 0) return -1;
        card_look(NULL);
    }
    return ink_cap_register(SYSTEM_ACTIONS, (int)(sizeof SYSTEM_ACTIONS / sizeof SYSTEM_ACTIONS[0]));
}
