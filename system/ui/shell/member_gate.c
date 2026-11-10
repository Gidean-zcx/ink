/*
 * Who is using the device (placeholder, D-64): an adult's space that has closed (idle, locked) asks who it is before anything
 * else. It shows the members; one with a PIN asks for it; the keys do nothing. The rules are the members' and the policy's.
 */
/* ui-scale-runtime: dimension x,y,diameter member_tile arguments use theme margins, rows and avatar sizes. */
#include <stdio.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/sys/exec.h"
#include "ink/sys/cap.h"
#include "ink/sys/font.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/state.h"
#include "ink/sys/ui.h"
#include "member_pages.h"
#include "shell_card.h"
#include "ink/ui_shell.h"

static struct {
    shell_card_t card;
    char picked[16];
    lv_obj_t *pad;
    bool requested;
    int page;
    ink_exec_job_t sync_job;
} G;

static void sync_gate(void *arg);
static const ink_ui_overlay_t OVERLAY;
static void page_action(ink_call_t *c) {
    int64_t delta = 0; ink_args_get_int(&c->args, "delta", &delta);
    if (!G.card.root || G.pad) { c->rc.status = INK_RC_REJECTED; return; }
    G.page += (int)delta;
    if (G.page < 0) G.page = 0;
    shell_card_close(&G.card);
    ink_ui_overlay_remove(&OVERLAY);
    sync_gate(NULL);
}
static void page_clicked(lv_event_t *e) {
    char args[40]; snprintf(args, sizeof args, "{\"delta\":%d}", (int)(intptr_t)lv_event_get_user_data(e));
    ink_receipt_t rc; ink_cap_invoke_json("shell.members_page", args, INK_SRC_TOUCH, 0, &rc);
}
static bool on_key(ink_key_t key, bool long_press) {
    if (!long_press && G.requested && ink_member_space_open() && (key == INK_KEY_HOME || key == INK_KEY_BACK)) {
        G.requested = false;
        sync_gate(NULL);
    }
    return true;
}

static int snapshot(char *buf, size_t cap) {
    int n = snprintf(buf, cap, "设备锁着：请选择家人，或输入密码。");
    return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
}

static const ink_ui_overlay_t OVERLAY = {.name = "member_gate", .level = 2, .snapshot = snapshot, .on_key = on_key};

static void on_member(lv_event_t *e) {
    const ink_member_t *m = lv_event_get_user_data(e);
    snprintf(G.picked, sizeof G.picked, "%s", m->id);
    if (ink_member_has_pin(m->id)) {
        ink_shell_member_pin_open(m->id);
    } else if (ink_member_switch(m->id) == 0) {
        G.requested = false;
        ink_exec_post(INK_EXEC_UI, &G.sync_job, sync_gate, NULL);
    }
}

static void member_tile(const ink_member_t *m, int x, int y, int diameter, bool primary) {
    lv_obj_t *b = ink_ui_pill(G.card.panel, "", x, y, diameter, diameter, primary, on_member, (void *)m);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_clip_corner(b, false, 0); /* The lock badge crosses the avatar's round edge. */
    if (!primary) lv_obj_set_style_bg_color(b, ink_ui_gray(), 0);
    char first[8] = {0};
    size_t len = 1;
    while (len < sizeof first - 1 && (m->name[len] & 0xc0) == 0x80) len++;
    memcpy(first, m->name, len);
    ink_ui_pill_text(b, INK_FONT_HEADING, INK_WEIGHT_MEDIUM);
    lv_obj_t *letter = lv_obj_get_child(b, 0);
    lv_label_set_text(letter, first);
    lv_obj_center(letter);
    if (ink_member_has_pin(m->id)) ink_ui_circle(b, "lock", diameter - INK_CIRCLE_SMALL, diameter - INK_CIRCLE_SMALL,
                                                INK_CIRCLE_SMALL, INK_ICON_SMALL, INK_SURFACE_WHITE, NULL, NULL);
    lv_obj_t *name = ink_ui_label(G.card.panel, m->name, INK_FONT_BODY, INK_WEIGHT_BOLD,
                                  x - INK_GAP, y + diameter + INK_GAP, diameter + 2 * INK_GAP);
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
}

static void sync_gate(void *arg) { /* UI task */
    (void)arg;
    bool want = (G.requested || !ink_member_space_open()) && ink_power_state() != INK_PWR_LOCKED;
    if (!want) {
        if (G.card.root) {
            shell_card_close(&G.card);
            G.pad = NULL;
            ink_ui_overlay_remove(&OVERLAY);
        }
        return;
    }
    if (G.card.root) return;
    shell_card_open(&G.card);
    shell_card_full(&G.card);
    ink_ui_overlay_add(&OVERLAY, G.card.root);
    ink_shell_statusbar(G.card.panel, true);
    ink_shell_camera_button(G.card.panel, false);
    shell_card_text(&G.card, G.requested ? "谁在用？" : "", "");
    shell_card_clear_buttons(&G.card);
    lv_obj_add_flag(G.card.row, LV_OBJ_FLAG_HIDDEN);
    const ink_member_t *m, *current = ink_member_current();
    int count = 0, total = 0, per_page = G.requested ? 4 : 2;
    for (int i = 0; (m = ink_member_at(i)) != NULL; i++) if (G.requested || strcmp(m->id, current->id)) total++;
    int pages = (total + per_page - 1) / per_page;
    if (G.page >= pages) G.page = pages ? pages - 1 : 0;
    if (!G.requested) {
        member_tile(current, (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, INK_STATUS_BAR + 2 * INK_BLOCK,
                    INK_CIRCLE_LARGE, true);
        ink_ui_notice(G.card.panel, "输密码，回到刚才的地方", INK_STATUS_BAR + 2 * INK_BLOCK + INK_CIRCLE_LARGE + INK_GAP + ink_ui_line_height(INK_FONT_BODY) + INK_GAP_M);
        ink_ui_notice(G.card.panel, "换一位家人", INK_CANVAS_H - INK_BLOCK_L - INK_CIRCLE_APP - 2 * ink_ui_line_height(INK_FONT_BODY) - INK_CONTROL);
    }
    for (int i = 0; (m = ink_member_at(i)) != NULL; i++) {
        if (!G.requested && !strcmp(m->id, current->id)) continue;
        int at = count++;
        if (at < G.page * per_page || at >= (G.page + 1) * per_page) continue;
        at %= per_page;
        int diameter = G.requested ? INK_CIRCLE_LARGE : INK_CIRCLE_APP;
        int cell = (INK_CANVAS_W - 2 * INK_MARGIN) / 2;
        int x = INK_MARGIN + (at % 2) * cell + (cell - diameter) / 2;
        int y = G.requested ? INK_STATUS_BAR + 2 * INK_BLOCK + INK_PRIMARY + (at / 2) * (diameter + ink_ui_line_height(INK_FONT_BODY) + INK_BLOCK)
                            : INK_CANVAS_H - INK_BLOCK - diameter - ink_ui_line_height(INK_FONT_BODY) - INK_CONTROL;
        member_tile(m, x, y, diameter, i == 0);
    }
    if (G.requested && ink_member_space_open() && current->role == INK_ROLE_ADULT) {
        lv_obj_t *b = ink_ui_pill(G.card.panel, "设置或修改密码", INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL,
                                  INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, false, NULL, NULL);
        ink_shell_bind(b, "shell.pin_set", NULL);
    }
    if (pages > 1) ink_ui_pagebar(G.card.panel, G.page + 1, pages, INK_CANVAS_H - INK_MARGIN - 2 * INK_CONTROL - INK_GAP, page_clicked);
    /* the one whose space it is first */
    snprintf(G.picked, sizeof G.picked, "%s", ink_member_current()->id);
    G.pad = NULL;
}

void ink_shell_members_open(bool returning) {
    if (!returning) { ink_shell_member_switch_open(); return; }
    G.requested = true;
    G.page = 0;
    ink_shell_close();
    sync_gate(NULL);
}

void ink_shell_member_gate_done(void) { G.requested = false; sync_gate(NULL); }

static void on_change(void *ctx, int h, const char *id) {
    (void)ctx;
    (void)h;
    (void)id;
    ink_exec_post(INK_EXEC_UI, &G.sync_job, sync_gate, NULL);
}

int ink_shell_member_gate_init(void) {
    static const ink_cap_t page = {.id = "shell.members_page", .title = "翻选人页面（只由人在设备上操作）", .expose = INK_EXPOSE_OWNER,
        .input = "{\"type\":\"object\",\"properties\":{\"delta\":{\"type\":\"integer\",\"minimum\":-1,\"maximum\":1}},\"required\":[\"delta\"]}",
        .effect = INK_FX_NAVIGATE, .child_ok = true, .while_locked = true, .run = page_action};
    if (ink_cap_register(&page, 1) != 0) return -1;
    if (ink_state_watch("member.authed", on_change, NULL, INK_EXEC_DIRECT) != 0 || ink_state_watch("power.state", on_change, NULL, INK_EXEC_DIRECT) != 0) return -1;
    ink_exec_post(INK_EXEC_UI, &G.sync_job, sync_gate, NULL);
    return 0;
}
