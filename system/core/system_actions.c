/*
 * System-wide actions (available on every page): the same operations the
 * HOME key, the control centre and the AI use.
 */
#include <stdio.h>

#include "ink/sys/actions.h"
#include "ink/sys/audio.h"
#include "ink/sys/members.h"
#include "ink/sys/ui.h"

static void go_home(const ink_json_t *args, int atok, ink_receipt_t *rc) {
    (void)args;
    (void)atok;
    ink_ui_home();
    snprintf(rc->object, sizeof rc->object, "screen:home");
    snprintf(rc->say, sizeof rc->say, "回到主页了");
}

static void full_refresh(const ink_json_t *args, int atok, ink_receipt_t *rc) {
    (void)args;
    (void)atok;
    ink_ui_hint_frame(INK_REFRESH_FULL);
    lv_obj_invalidate(lv_screen_active());
    snprintf(rc->say, sizeof rc->say, "屏幕刷新一下");
}

static void set_volume(const ink_json_t *args, int atok, ink_receipt_t *rc) {
    int64_t v;
    if (!args || !ink_json_int(args, ink_json_find(args, atok, "percent"), &v) || v < 0 || v > 100) {
        rc->status = INK_RC_INVALID;
        snprintf(rc->detail, sizeof rc->detail, "percent must be 0..100");
        return;
    }
    ink_audio_set_volume((int)v);
    snprintf(rc->object, sizeof rc->object, "setting:volume");
    rc->revision = (uint32_t)v;
    snprintf(rc->say, sizeof rc->say, "音量调到 %d", (int)v);
}

static void switch_member(const ink_json_t *args, int atok, ink_receipt_t *rc) {
    char id[16];
    if (!args || ink_json_str(args, ink_json_find(args, atok, "id"), id, sizeof id) < 0) {
        rc->status = INK_RC_INVALID;
        snprintf(rc->detail, sizeof rc->detail, "id required");
        return;
    }
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

static void factory_reset(const ink_json_t *args, int atok, ink_receipt_t *rc) {
    (void)args;
    (void)atok;
    rc->status = INK_RC_FAILED;
    snprintf(rc->detail, sizeof rc->detail, "not implemented yet");
}

static const ink_action_t SYSTEM_ACTIONS[] = {
    {.id = "system.home", .title = "回到主页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = go_home},
    {.id = "system.full_refresh", .title = "整屏刷新（清残影）", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = full_refresh},
    {.id = "system.set_volume",
     .title = "调音量",
     .args = "{\"type\":\"object\",\"properties\":{\"percent\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":100}},"
             "\"required\":[\"percent\"]}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .run = set_volume},
    {.id = "system.switch_member",
     .title = "切换成员",
     .args = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     .effect = INK_FX_NAVIGATE,
     .child_ok = true,
     .run = switch_member},
    {.id = "system.factory_reset", .title = "恢复出厂设置", .effect = INK_FX_SYSTEM, .child_ok = false, .run = factory_reset},
};

int ink_system_actions_init(void) {
    return ink_actions_register(SYSTEM_ACTIONS, (int)(sizeof SYSTEM_ACTIONS / sizeof SYSTEM_ACTIONS[0]));
}
