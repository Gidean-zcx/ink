/* The AI panel presents the service's words; closing does not cancel its work. */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/ai.h"
#include "ink/sys/cap.h"
#include "ink/sys/exec.h"
#include "ink/sys/state.h"
#include "ai_chat.h"
#include "ai_pages.h"
#include "ink/ui_shell.h"

#define PAGES_MAX 4096 /* a 32 KB answer of text is a few hundred pages; more (nearly all line breaks) is "cannot be shown in full" */
#define PANEL_HEIGHT (INK_GAP_L + INK_CONTROL + INK_GAP_S + 3 * ink_ui_line_height(INK_FONT_BODY) + INK_GAP_S + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S + INK_CONTROL + INK_GAP_L)
static struct {
    lv_obj_t *root, *status, *question, *answer, *source, *actions, *prev, *next, *counter, *stop, *rec;
    char words[INK_AI_ANSWER_MAX], text[4096];
    char caption[128];
    uint16_t starts[PAGES_MAX + 1];
    int page, pages;
    uint32_t revision, pressed_revision;
    bool pressed;
    bool hidden, speaking, expanded;
    const ink_ui_overlay_t *overlay;
    ink_exec_job_t refine;
} A INK_PSRAM_BSS;
static void audio_changed(void *ctx, int handle, const char *id);

int ink_shell_ai_snapshot(char *buf, size_t cap) {
    return ink_ai_chat_panel_snapshot(A.status ? lv_label_get_text(A.status) : "", A.question && !lv_obj_has_flag(A.question, LV_OBJ_FLAG_HIDDEN) ? lv_label_get_text(A.question) : "",
        A.answer && !lv_obj_has_flag(A.answer, LV_OBJ_FLAG_HIDDEN) ? lv_label_get_text(A.answer) : "", A.page, A.pages, A.speaking, A.revision, buf, cap);
}
static bool key(ink_key_t k, bool lng) {
    if (!lng && A.pages > 1 && A.words[0] && !A.speaking && (k == INK_KEY_NEXT || k == INK_KEY_PREV)) {
        ink_receipt_t rc;
        ink_cap_invoke(k == INK_KEY_NEXT ? "ai.panel.next" : "ai.panel.prev", NULL, INK_SRC_KEY, 0, &rc);
        return true;
    }
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

/* Pages from page `from` on: the pages before it end where they did (a streamed answer only grows). */
static void paginate(int from) {
    lv_obj_update_layout(A.root);
    int w = lv_obj_get_width(A.answer), h = lv_obj_get_height(A.answer);
    const lv_font_t *font = lv_obj_get_style_text_font(A.answer, LV_PART_MAIN);
    int letter = lv_obj_get_style_text_letter_space(A.answer, LV_PART_MAIN);
    int line = lv_obj_get_style_text_line_space(A.answer, LV_PART_MAIN);
    size_t at = A.starts[from], len = strlen(A.words);
    A.pages = from;
    do {
        size_t end = at;
        while (end < len) {
            size_t next = end + 1;
            while (next < len && (A.words[next] & 0xc0) == 0x80) next++;
            if (next - at >= sizeof A.text) break;
            memcpy(A.text, A.words + at, next - at); A.text[next - at] = 0;
            lv_point_t size;
            lv_text_get_size(&size, A.text, font, letter, line, w, LV_TEXT_FLAG_NONE);
            if (size.y > h) break;
            end = next;
        }
        if ((end == at && at < len) || A.pages >= PAGES_MAX) { A.pages = 0; break; }
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
    A.revision++;
    ink_ui_hint_obj(A.answer, INK_REFRESH_UI);
    ink_ui_hint_obj(A.actions, INK_REFRESH_UI);
}
static void layout(bool expanded) {
    A.expanded = expanded;
    int extra = expanded ? 2 * ink_ui_line_height(INK_FONT_BODY) + INK_GAP_M : 0;
    int height = PANEL_HEIGHT + extra;
    lv_obj_set_y(A.root, ink_ui_px(INK_CANVAS_H - height));
    lv_obj_set_height(A.root, ink_ui_px(height + INK_RADIUS_PANEL));
    lv_obj_set_height(A.answer, ink_ui_px(3 * ink_ui_line_height(INK_FONT_BODY) + extra));
    lv_obj_set_y(A.source, ink_ui_px(height - INK_GAP_L - INK_CONTROL - INK_GAP_S - ink_ui_line_height(INK_FONT_NOTE)));
    lv_obj_set_y(A.actions, ink_ui_px(height - INK_GAP_L - INK_CONTROL));
    lv_obj_set_y(A.stop, ink_ui_px(height - INK_GAP_L - INK_CONTROL));
}
void ink_shell_ai_words(const char *text) {
    if (!A.root) return;
    size_t had = strlen(A.words);
    bool grows = A.expanded && A.pages > 1 && !strncmp(text, A.words, had); /* more of the same answer */
    snprintf(A.words, sizeof A.words, "%s", text);
    if (text[0]) { lv_obj_add_flag(A.question, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(A.answer, LV_OBJ_FLAG_HIDDEN); }
    if (grows) paginate(A.pages - 1);
    else {
        layout(false); paginate(0);
        if (A.pages > 1) { layout(true); paginate(0); }
    }
    draw_words();
    audio_changed(NULL, 0, NULL);
}

bool ink_shell_ai_hide(void) {
    if (!A.root || A.hidden) return false;
    ink_ui_hint_obj(A.root, INK_REFRESH_FLASH_UI);
    lv_obj_add_flag(A.root, LV_OBJ_FLAG_HIDDEN);
    ink_ui_overlay_remove(A.overlay);
    A.hidden = true;
    return true;
}
void ink_shell_ai_reveal(void) {
    if (!A.root || !A.hidden) return;
    lv_obj_remove_flag(A.root, LV_OBJ_FLAG_HIDDEN);
    ink_ui_overlay_add(A.overlay, A.root);
    A.hidden = false;
}
void ink_shell_ai_closed(void) {
    if (!A.root) return;
    ink_ui_hint_obj(A.root, INK_REFRESH_FLASH_UI);
    ink_ui_overlay_remove(A.overlay);
    A.root = A.status = A.question = A.answer = A.source = A.actions = A.prev = A.next = A.counter = A.stop = A.rec = NULL;
    memset(A.words, 0, sizeof A.words); memset(A.text, 0, sizeof A.text);
    A.page = A.pages = 0; A.hidden = A.expanded = false;
}

static void audio_changed(void *ctx, int handle, const char *id) {
    (void)ctx; (void)handle; (void)id;
    if (!A.root) return;
    char playing[16] = "";
    ink_state_get_str(ink_state_find("audio.now_playing"), playing, sizeof playing);
    bool was_speaking = A.speaking;
    A.speaking = A.words[0] && !strstr(A.caption, "声音没能播放") && !strcmp(playing, "voice");
    if (was_speaking != A.speaking) A.revision++;
    lv_label_set_text(A.status, A.speaking ? "正在读给你听" : A.caption);
    if (A.speaking) lv_obj_remove_flag(A.stop, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(A.stop, LV_OBJ_FLAG_HIDDEN);
    if (strstr(A.caption, "正在录音")) lv_obj_remove_flag(A.rec, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(A.rec, LV_OBJ_FLAG_HIDDEN); /* one tap gives the microphone back */
    if (A.speaking || !A.words[0]) { lv_obj_add_flag(A.actions, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(A.source, LV_OBJ_FLAG_HIDDEN); }
    else { lv_obj_remove_flag(A.actions, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(A.source, LV_OBJ_FLAG_HIDDEN); }
}

void ink_shell_ai_status(const char *s, ink_ai_error_t error) {
    if (!A.root) return;
    bool done = strstr(s, "回答完成") || !strcmp(s, "回答中");
    const char *caption = strstr(s, "声音没能播放") ? (done ? "回答完成 · 声音没能播放" : "声音没能播放") : done ? "" : strstr(s, "正在听") ? (ink_ai_chat_tapped() ? "正在听 · 停一下自动结束" : "正在听 · 松开结束") : strstr(s, "正在想") ? "正在看这一页" : s;
    if (strcmp(A.caption, caption)) A.revision++;
    snprintf(A.caption, sizeof A.caption, "%s", caption);
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
    audio_changed(NULL, 0, NULL);
    const ink_ui_overlay_t *overlay = ink_ai_pages_status(error, A.root);
    if (!overlay) overlay = &OVERLAY;
    if (A.overlay != overlay) {
        ink_ui_overlay_remove(A.overlay);
        A.overlay = overlay;
        if (!A.hidden) ink_ui_overlay_add(A.overlay, A.root);
    }
}

static void action(ink_call_t *c) {
    if (!strcmp(c->cap->id, "ai.panel.listen")) { ink_cap_invoke("shell.ai_key", NULL, c->src, 0, &c->rc); return; }
    if (!A.root || A.hidden) { c->rc.status = INK_RC_STALE; return; }
    const char *id = c->cap->id;
    if (!strcmp(id, "ai.panel.hide")) ink_shell_ai_hide();
    else if (!strcmp(id, "ai.panel.next")) { if (A.page + 1 < A.pages) A.page++; draw_words(); }
    else if (!strcmp(id, "ai.panel.prev")) { if (A.page) A.page--; if (A.answer) draw_words(); }
    if (!strcmp(id, "ai.panel.next") || !strcmp(id, "ai.panel.prev")) ink_ui_content_changed();
}
static const ink_cap_t CAPS[] = {
    {.id = "ai.panel.hide", .app = "ai", .title = "收起 AI 面板（继续处理）", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
    {.id = "ai.panel.listen", .app = "ai", .title = "用话筒问这一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
    {.id = "ai.panel.next", .app = "ai", .title = "答案下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
    {.id = "ai.panel.prev", .app = "ai", .title = "答案上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action}
};

static void panel_event(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) { A.pressed = true; A.pressed_revision = A.revision; }
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (A.pressed && A.pressed_revision != A.revision) lv_event_stop_processing(e);
        A.pressed = false;
    }
}
static void panel_bind(lv_obj_t *obj, const char *cap) {
    lv_obj_add_event_cb(obj, panel_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(obj, panel_event, LV_EVENT_CLICKED, NULL);
    ink_shell_bind(obj, cap, NULL);
}

void ink_shell_ai_open(lv_obj_t **root, lv_obj_t **status, lv_obj_t **question, lv_obj_t **answer) {
    A.root = ink_ui_panel(lv_layer_sys(), PANEL_HEIGHT, true);
    ink_ui_box(A.root, (INK_CANVAS_W - INK_ICON_STATE) / 2, INK_GAP_M, INK_ICON_STATE, INK_GAP_S, -1, INK_SURFACE_BLACK, false);
    ink_ui_circle(A.root, "ai", INK_MARGIN, INK_GAP_L + (INK_CONTROL - INK_CIRCLE_SMALL) / 2, INK_CIRCLE_SMALL, INK_ICON_MEDIUM, INK_SURFACE_BLACK, NULL, NULL);
    A.status = ink_ui_label(A.root, "", INK_FONT_NOTE, INK_WEIGHT_MEDIUM,
                            INK_MARGIN + INK_CIRCLE_SMALL + INK_GAP_M, INK_GAP_L + (INK_CONTROL - 2 * ink_ui_line_height(INK_FONT_NOTE)) / 2, INK_CANVAS_W - 2 * INK_PRIMARY - INK_MARGIN);
    lv_obj_t *close = ink_ui_circle(A.root, "close", INK_CANVAS_W - INK_MARGIN - INK_CONTROL, INK_GAP_L, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL);
    panel_bind(close, "ai.panel.hide");
    A.answer = ink_ui_label(A.root, "", INK_FONT_BODY, INK_WEIGHT_REGULAR, INK_MARGIN, INK_GAP_L + INK_CONTROL + INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_height(A.answer, ink_ui_px(3 * ink_ui_line_height(INK_FONT_BODY)));
    A.question = ink_ui_label(A.root, "", INK_FONT_BODY, INK_WEIGHT_REGULAR, INK_MARGIN, INK_GAP_L + INK_CONTROL + INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN);
    A.source = ink_ui_label(A.root, "出自这一页", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN,
                            PANEL_HEIGHT - INK_GAP_L - INK_CONTROL - INK_GAP_S - ink_ui_line_height(INK_FONT_NOTE), INK_CANVAS_W - 2 * INK_MARGIN);
    A.actions = ink_ui_box(A.root, INK_MARGIN, PANEL_HEIGHT - INK_GAP_L - INK_CONTROL,
                            INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, 0, INK_SURFACE_WHITE, false);
    lv_obj_t *next = ink_ui_pill(A.actions, "对话记录", 0, 0, 2 * INK_CONTROL, INK_CONTROL, false, NULL, NULL);
    panel_bind(next, "shell.ai_home");
    int x = 2 * INK_CONTROL + INK_GAP;
    A.prev = ink_ui_circle(A.actions, "back", x, 0, INK_CONTROL, INK_ICON_SMALL, INK_SURFACE_WHITE, NULL, NULL);
    panel_bind(A.prev, "ai.panel.prev");
    int last = INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL;
    A.counter = ink_ui_line(A.actions, "1 / 1", INK_FONT_LIST, INK_WEIGHT_MEDIUM, x + INK_CONTROL + INK_GAP_S,
                            (INK_CONTROL - ink_ui_line_height(INK_FONT_LIST)) / 2, last - x - INK_CONTROL - 2 * INK_GAP_S);
    lv_obj_set_style_text_align(A.counter, LV_TEXT_ALIGN_CENTER, 0);
    A.next = ink_ui_circle(A.actions, "next", last, 0, INK_CONTROL, INK_ICON_SMALL, INK_SURFACE_WHITE, NULL, NULL);
    panel_bind(A.next, "ai.panel.next");
    A.stop = ink_ui_pill(A.root, "停下", INK_MARGIN, PANEL_HEIGHT - INK_GAP_L - INK_CONTROL,
        2 * INK_CONTROL, INK_CONTROL, false, NULL, NULL);
    panel_bind(A.stop, "shell.ai_stop");
    lv_obj_add_flag(A.stop, LV_OBJ_FLAG_HIDDEN);
    A.rec = ink_ui_pill(A.root, "停止录音", INK_MARGIN, PANEL_HEIGHT - INK_GAP_L - INK_CONTROL, 3 * INK_CONTROL, INK_CONTROL, true, NULL, NULL);
    panel_bind(A.rec, "audio.record_stop");
    lv_obj_add_flag(A.rec, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(A.source, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(A.actions, LV_OBJ_FLAG_HIDDEN);
    A.hidden = A.expanded = false; A.page = 0; A.pages = 1; A.words[0] = 0; A.starts[0] = A.starts[1] = 0;
    A.caption[0] = 0; A.speaking = false; A.pressed = false;
    draw_words();
    A.overlay = &OVERLAY; ink_ui_overlay_add(A.overlay, A.root);
    ink_ui_hint_obj(A.root, INK_REFRESH_FAST);
    ink_exec_post(INK_EXEC_UI, &A.refine, refine, NULL);
    *root = A.root; *status = A.status; *question = A.question; *answer = A.answer;
}

int ink_shell_ai_init(void) {
    return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_ai_chat_init() ||
        ink_state_watch("audio.now_playing", audio_changed, NULL, INK_EXEC_UI);
}
