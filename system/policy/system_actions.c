/*
 * System-wide actions (available on every page): the same operations the
 * HOME key, the control centre and the AI use.
 */
#include <stdio.h>

#include "ink/hal_power.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/ai.h"
#include "ink/sys/cap.h"
#include "ink/sys/data.h"
#include "ink/sys/file.h"
#include "ink/sys/storage.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/members.h"
#include "ink/sys/ui.h"

static void go_home(ink_call_t *c) {
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

static void switch_member(ink_call_t *c) {
    ink_receipt_t *rc = &c->rc;
    char id[16];
    ink_args_get_str(&c->args, "id", id, sizeof id); /* required by the schema */
    if (c->src == INK_SRC_AI) ink_ai_switching_member();
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

/* Everything of the people who used this device; the system and the family's shared content stay (system services design §12.6). */
static void wipe_user(void *arg) { /* the storage task, guarded */
    (void)arg;
    ink_storage_wipe_user();
}

static void factory_reset_job(void *arg) { /* exec.bg */
    (void)arg;
    ink_probe_count("system.factory_resets");
    ink_fs_wipe_personal();
    ink_storage_run(wipe_user, NULL, true);
    ink_data_forget_all();
    hal_power_reboot();
}

static ink_exec_job_t g_reset_job;

static void factory_reset(ink_call_t *c) {
    snprintf(c->rc.say, sizeof c->rc.say, "恢复出厂，马上重启");
    ink_exec_post(INK_EXEC_BG, &g_reset_job, factory_reset_job, NULL);
}

static void cap_reboot(ink_call_t *c) { /* exec.bg */
    snprintf(c->rc.say, sizeof c->rc.say, "重启");
    ink_db_flush_all();
    for (int i = 0; i < 100 && (!ink_db_idle() || ink_storage_pending()); i++) osal_task_delay_us(50000);
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
    {.id = "system.full_refresh", .title = "整屏刷新（清残影）", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = full_refresh},
    {.id = "system.set_volume",
     .title = "调音量",
     .input = "{\"type\":\"object\",\"properties\":{\"percent\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":100}},"
             "\"required\":[\"percent\"]}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .run = set_volume},
    {.id = "system.switch_member",
     .title = "切换成员",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     .effect = INK_FX_NAVIGATE,
     .child_ok = true,
     .run = switch_member},
    {.id = "state.list", .title = "系统现在的状态（电量、网络、前台应用……）", .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = state_list},
    {.id = "settings.list", .title = "所有设置，各自的类型、取值范围和现在的值", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = settings_list},
    {.id = "system.factory_reset", .title = "恢复出厂设置（清掉所有成员的数据，不能撤销）", .ask = "恢复出厂设置：清掉所有成员的数据，不能撤销", .effect = INK_FX_SYSTEM, .parent_pin = true, .child_ok = false, .exec = INK_EXEC_DIRECT, .run = factory_reset},
    {.id = "system.reboot", .title = "重启", .effect = INK_FX_SYSTEM, .child_ok = true, .while_locked = true, .exec = INK_EXEC_BG, .run = cap_reboot},
};

int ink_system_actions_init(void) {
    return ink_cap_register(SYSTEM_ACTIONS, (int)(sizeof SYSTEM_ACTIONS / sizeof SYSTEM_ACTIONS[0]));
}
