/* The AI panel presents the service's words; closing does not cancel its work. */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/ai.h"
#include "ink/sys/cap.h"
#include "ink/sys/exec.h"
#include "ink/ui_shell.h"

#define PANEL_HEIGHT (INK_GAP_L + INK_CONTROL + INK_GAP_S + 3 * ink_ui_line_height(INK_FONT_BODY) + INK_GAP_S + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S + INK_CONTROL + INK_GAP_L)
static struct {
    lv_obj_t *root, *status, *question, *answer, *source, *actions, *prev, *next, *counter;
    char words[4096], text[4096];
    uint16_t starts[4097];
    int page, pages;
    bool hidden;
    ink_exec_job_t refine;
} A INK_PSRAM_BSS;

int ink_shell_ai_snapshot(char *buf, size_t cap) {
    return snprintf(buf, cap, "AI 面板；%s；%s；页码=%d/%d；上一页=%d；下一页=%d。动作：ai.panel.hide、ai.panel.listen、ai.panel.next、ai.panel.prev。",
                    A.status ? lv_label_get_text(A.status) : "", A.answer ? lv_label_get_text(A.answer) : "", A.page + 1, A.pages,
                    A.page > 0, A.page + 1 < A.pages);
}
static bool key(ink_key_t k, bool lng) {
    if (lng || (k != INK_KEY_HOME && k != INK_KEY_BACK)) return false;
    ink_ai_dismiss();
    return true;
}
static const ink_ui_overlay_t OVERLAY = {.name = "Ai", .level = -1, .on_key = key, .snapshot = ink_shell_ai_snapshot};

static void refine(void *arg) {
    (void)arg;
    if (!A.root || A.hidden) return;
    ink_ui_cause(0, NULL);
    ink_ui_hint_obj(A.root, INK_REFRESH_FLASH_UI);
    lv_obj_invalidate(A.root);
}

static void paginate(void) {
    lv_obj_update_layout(A.root);
    int w = lv_obj_get_width(A.answer), h = lv_obj_get_height(A.answer);
    const lv_font_t *font = lv_obj_get_style_text_font(A.answer, LV_PART_MAIN);
    int letter = lv_obj_get_style_text_letter_space(A.answer, LV_PART_MAIN);
    int line = lv_obj_get_style_text_line_space(A.answer, LV_PART_MAIN);
    size_t at = 0, len = strlen(A.words);
    A.pages = 0; A.starts[0] = 0;
    do {
        size_t end = at;
        while (end < len) {
            size_t next = end + 1;
            while (next < len && (A.words[next] & 0xc0) == 0x80) next++;
            memcpy(A.text, A.words + at, next - at); A.text[next - at] = 0;
            lv_point_t size;
            lv_text_get_size(&size, A.text, font, letter, line, w, LV_TEXT_FLAG_NONE);
            if (size.y > h) break;
            end = next;
        }
        if (end == at && at < len) { A.pages = 0; break; }
        at = end; A.starts[++A.pages] = (uint16_t)at;
    } while (at < len);
    if (A.page >= A.pages) A.page = A.pages ? A.pages - 1 : 0;
}
static void draw_words(void) {
    if (A.pages) {
        size_t len = A.starts[A.page + 1] - A.starts[A.page];
        memcpy(A.text, A.words + A.starts[A.page], len); A.text[len] = 0;
    } else snprintf(A.text, sizeof A.text, "回答无法完整显示，请重新打开面板。");
    lv_label_set_text(A.answer, A.text);
    char count[32]; snprintf(count, sizeof count, "%d / %d", A.page + 1, A.pages);
    lv_label_set_text(A.counter, count);
    if (A.page > 0) lv_obj_remove_state(A.prev, LV_STATE_DISABLED); else lv_obj_add_state(A.prev, LV_STATE_DISABLED);
    if (A.page + 1 < A.pages) lv_obj_remove_state(A.next, LV_STATE_DISABLED); else lv_obj_add_state(A.next, LV_STATE_DISABLED);
    ink_ui_hint_obj(A.answer, INK_REFRESH_UI);
    ink_ui_hint_obj(A.actions, INK_REFRESH_UI);
}
void ink_shell_ai_words(const char *text) {
    if (!A.root) return;
    snprintf(A.words, sizeof A.words, "%s", text);
    if (text[0]) { lv_obj_add_flag(A.question, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(A.answer, LV_OBJ_FLAG_HIDDEN); }
    paginate(); draw_words();
}

bool ink_shell_ai_hide(void) {
    if (!A.root || A.hidden) return false;
    ink_ui_hint_obj(A.root, INK_REFRESH_FLASH_UI);
    lv_obj_add_flag(A.root, LV_OBJ_FLAG_HIDDEN);
    ink_ui_overlay_remove(&OVERLAY);
    A.hidden = true;
    return true;
}
void ink_shell_ai_reveal(void) {
    if (!A.root || !A.hidden) return;
    lv_obj_remove_flag(A.root, LV_OBJ_FLAG_HIDDEN);
    ink_ui_overlay_add(&OVERLAY, A.root);
    A.hidden = false;
}
void ink_shell_ai_closed(void) {
    if (!A.root) return;
    ink_ui_hint_obj(A.root, INK_REFRESH_FLASH_UI);
    ink_ui_overlay_remove(&OVERLAY);
    A.root = A.status = A.question = A.answer = A.source = A.actions = A.prev = A.next = A.counter = NULL;
    memset(A.words, 0, sizeof A.words); memset(A.text, 0, sizeof A.text);
    A.page = A.pages = 0; A.hidden = false;
}

void ink_shell_ai_status(const char *s) {
    if (!A.root) return;
    bool done = strstr(s, "回答完成") || !strcmp(s, "回答中");
    const char *caption = strstr(s, "声音没能播放") ? (done ? "回答完成 · 声音没能播放" : "声音没能播放") : done ? "" : strstr(s, "正在听") ? "正在听 · 松开结束" : strstr(s, "正在想") ? "正在看这一页" : s;
    lv_label_set_text(A.status, caption);
    if (done) {
        lv_obj_add_flag(A.question, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(A.answer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(A.source, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(A.actions, LV_OBJ_FLAG_HIDDEN);
    } else if (!A.words[0]) {
        lv_obj_remove_flag(A.question, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(A.answer, LV_OBJ_FLAG_HIDDEN);
    }
    ink_ui_hint_obj(A.status, done ? INK_REFRESH_UI : INK_REFRESH_FAST);
}

static void action(ink_call_t *c) {
    if (!A.root || A.hidden) { c->rc.status = INK_RC_STALE; return; }
    const char *id = c->cap->id;
    if (!strcmp(id, "ai.panel.hide")) ink_shell_ai_hide();
    else if (!strcmp(id, "ai.panel.listen")) ink_ai_tap();
    else if (!strcmp(id, "ai.panel.next")) { if (A.page + 1 < A.pages) A.page++; draw_words(); }
    else if (!strcmp(id, "ai.panel.prev")) { if (A.page) A.page--; if (A.answer) draw_words(); }
}
static const ink_cap_t CAPS[] = {
    {.id = "ai.panel.hide", .title = "收起 AI 面板（继续处理）", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
    {.id = "ai.panel.listen", .title = "用话筒问这一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
    {.id = "ai.panel.next", .title = "答案下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
    {.id = "ai.panel.prev", .title = "答案上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action}
};

void ink_shell_ai_open(lv_obj_t **root, lv_obj_t **status, lv_obj_t **question, lv_obj_t **answer) {
    A.root = ink_ui_panel(lv_layer_sys(), PANEL_HEIGHT, true);
    ink_ui_box(A.root, (INK_CANVAS_W - INK_ICON_STATE) / 2, INK_GAP_M, INK_ICON_STATE, INK_GAP_S, -1, INK_SURFACE_BLACK, false);
    ink_ui_circle(A.root, "ai", INK_MARGIN, INK_GAP_L + (INK_CONTROL - INK_CIRCLE_SMALL) / 2, INK_CIRCLE_SMALL, INK_ICON_MEDIUM, INK_SURFACE_BLACK, NULL, NULL);
    A.status = ink_ui_label(A.root, "", INK_FONT_LIST, INK_WEIGHT_MEDIUM,
                            INK_MARGIN + INK_CIRCLE_SMALL + INK_GAP_M, INK_GAP_L + (INK_CONTROL - ink_ui_line_height(INK_FONT_LIST)) / 2, INK_CANVAS_W - 2 * INK_PRIMARY - INK_MARGIN);
    lv_obj_t *close = ink_ui_circle(A.root, "close", INK_CANVAS_W - INK_MARGIN - INK_CONTROL, INK_GAP_L, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL);
    ink_shell_bind(close, "ai.panel.hide", NULL);
    A.answer = ink_ui_label(A.root, "", INK_FONT_BODY, INK_WEIGHT_REGULAR, INK_MARGIN, INK_GAP_L + INK_CONTROL + INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_height(A.answer, ink_ui_px(3 * ink_ui_line_height(INK_FONT_BODY)));
    A.question = ink_ui_label(A.root, "", INK_FONT_BODY, INK_WEIGHT_REGULAR, INK_MARGIN, INK_GAP_L + INK_CONTROL + INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN);
    A.source = ink_ui_label(A.root, "出自这一页", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN,
                            PANEL_HEIGHT - INK_GAP_L - INK_CONTROL - INK_GAP_S - ink_ui_line_height(INK_FONT_NOTE), INK_CANVAS_W - 2 * INK_MARGIN);
    A.actions = ink_ui_box(A.root, INK_MARGIN, PANEL_HEIGHT - INK_GAP_L - INK_CONTROL,
                            INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, 0, INK_SURFACE_WHITE, false);
    lv_obj_t *next = ink_ui_pill(A.actions, "继续问", 0, 0, 2 * INK_CONTROL, INK_CONTROL, false, NULL, NULL);
    ink_shell_bind(next, "ai.panel.listen", NULL);
    int x = 2 * INK_CONTROL + INK_GAP;
    A.prev = ink_ui_circle(A.actions, "back", x, 0, INK_CONTROL, INK_ICON_SMALL, INK_SURFACE_WHITE, NULL, NULL);
    ink_shell_bind(A.prev, "ai.panel.prev", NULL);
    int last = INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL;
    A.counter = ink_ui_line(A.actions, "1 / 1", INK_FONT_LIST, INK_WEIGHT_MEDIUM, x + INK_CONTROL + INK_GAP_S,
                            (INK_CONTROL - ink_ui_line_height(INK_FONT_LIST)) / 2, last - x - INK_CONTROL - 2 * INK_GAP_S);
    lv_obj_set_style_text_align(A.counter, LV_TEXT_ALIGN_CENTER, 0);
    A.next = ink_ui_circle(A.actions, "next", last, 0, INK_CONTROL, INK_ICON_SMALL, INK_SURFACE_WHITE, NULL, NULL);
    ink_shell_bind(A.next, "ai.panel.next", NULL);
    lv_obj_add_flag(A.source, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(A.actions, LV_OBJ_FLAG_HIDDEN);
    A.hidden = false; A.page = 0; A.pages = 1; A.words[0] = 0; A.starts[0] = A.starts[1] = 0;
    draw_words();
    ink_ui_overlay_add(&OVERLAY, A.root);
    ink_ui_hint_obj(A.root, INK_REFRESH_FAST);
    ink_exec_post(INK_EXEC_UI, &A.refine, refine, NULL);
    *root = A.root; *status = A.status; *question = A.question; *answer = A.answer;
}

int ink_shell_ai_init(void) { return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS); }
