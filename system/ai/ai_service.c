/*
 * AI service and answer sheet (UI task).
 *
 * Key down: open the sheet in "listening" state with a FAST refresh and
 *   pre-connect the hub link (the connection setup overlaps the speech).
 * Key up:  [audio pipeline not modelled yet: a fixed transcript stands in]
 *   send ai.ask with the page snapshot; answer chunks (ai.delta) are appended
 *   to a fixed region and refreshed as they come (the refresh scheduler folds
 *   chunks that arrive while the panel is busy); the result ends the answer.
 * Errors are shown in place (offline, no answer, connection lost).
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/ai.h"
#include "ink/sys/font.h"
#include "ink/sys/hub.h"
#include "ink/sys/json.h"
#include "ink/sys/ui.h"

#define ANSWER_CAP 2048
#define SNAPSHOT_CAP 3072
#define ASK_TIMEOUT_US (45ull * 1000000ull)

static const char *SIM_TRANSCRIPT = "这一页讲了什么？";

static struct {
    lv_obj_t *sheet, *status, *question, *answer;
    bool open, asking;
    uint64_t t_press, t_release;
    int req_id; /* hub request id of the current ask (matches ai.delta params.id) */
    int seq;    /* local ask counter (matches the reply callback's ctx) */
    bool first_text_seen;
    char *answer_text; /* PSRAM */
    size_t answer_len;
} AI;

typedef struct {
    int req_id;
    int kind; /* 0 delta, 1 done, 2 error */
    int status;
    char text[];
} ai_post_t;

/* ---------------------------------------------------------------- sheet */

static const lv_font_t *font_pt(double pt) {
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    ink_font_t *f = ink_font_for_pt(pt, pi.dpi);
    return f ? ink_font_lvgl(f) : ink_ui_font(0);
}

static void sheet_open(void) {
    if (AI.open) return;
    lv_obj_t *top = lv_layer_top();
    AI.sheet = lv_obj_create(top);
    lv_obj_remove_style_all(AI.sheet);
    lv_obj_set_size(AI.sheet, lv_pct(100), lv_pct(58));
    lv_obj_align(AI.sheet, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(AI.sheet, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(AI.sheet, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(AI.sheet, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(AI.sheet, ink_dp(2), 0);
    lv_obj_set_style_border_color(AI.sheet, lv_color_black(), 0);
    lv_obj_set_style_pad_all(AI.sheet, ink_dp(16), 0);
    lv_obj_set_flex_flow(AI.sheet, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(AI.sheet, ink_dp(10), 0);
    lv_obj_remove_flag(AI.sheet, LV_OBJ_FLAG_SCROLLABLE);

    AI.status = lv_label_create(AI.sheet);
    lv_obj_set_style_text_font(AI.status, font_pt(6), 0);
    lv_obj_set_style_text_color(AI.status, lv_color_hex(0x444444), 0);
    AI.question = lv_label_create(AI.sheet);
    lv_obj_set_style_text_font(AI.question, font_pt(7), 0);
    lv_obj_set_width(AI.question, lv_pct(100));
    lv_label_set_long_mode(AI.question, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(AI.question, "");
    AI.answer = lv_label_create(AI.sheet);
    lv_obj_set_style_text_font(AI.answer, font_pt(8), 0);
    lv_obj_set_width(AI.answer, lv_pct(100));
    lv_label_set_long_mode(AI.answer, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(AI.answer, "");
    AI.open = true;
}

static void set_status(const char *s) {
    if (!AI.open) return;
    lv_label_set_text(AI.status, s);
    ink_ui_hint_obj(AI.status, INK_REFRESH_FAST);
}

bool ink_ai_dismiss(void) {
    if (!AI.open) return false;
    lv_obj_delete(AI.sheet);
    AI.sheet = NULL;
    AI.open = false;
    AI.asking = false;
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI); /* the page underneath comes back cleanly */
    return true;
}

/* -------------------------------------------------------------- results */

static void ui_apply(void *arg) { /* UI task */
    ai_post_t *p = arg;
    bool match = p->kind == 0 ? p->req_id == AI.req_id : p->req_id == AI.seq;
    if (!AI.open || !match) {
        osal_free(p);
        return;
    }
    if (p->kind == 0) {
        size_t n = strlen(p->text);
        if (AI.answer_len + n < ANSWER_CAP) {
            memcpy(AI.answer_text + AI.answer_len, p->text, n + 1);
            AI.answer_len += n;
        }
        lv_label_set_text(AI.answer, AI.answer_text);
        ink_ui_hint_obj(AI.answer, INK_REFRESH_UI);
        if (!AI.first_text_seen) {
            AI.first_text_seen = true;
            set_status("回答中");
            ink_ui_cause(AI.t_release, "ai.release_to_first_text_visible_ms");
        }
    } else if (p->kind == 1) {
        AI.asking = false;
        set_status("回答完成 · 按 AI 键继续问，按 HOME 关闭");
        ink_ui_cause(AI.t_release, "ai.release_to_done_visible_ms");
        ink_probe_count("ai.answers");
    } else {
        AI.asking = false;
        const char *msg = p->status == INK_HUB_ERR_OFFLINE ? "现在没有网络，稍后再问我"
                          : p->status == INK_HUB_ERR_TIMEOUT ? "中枢没有回应，请再试一次"
                          : p->status == INK_HUB_ERR_LOST    ? "网络断了，回答没收全，请再试一次"
                                                             : "出了点问题，请再试一次";
        set_status(msg);
        ink_ui_cause(AI.t_release, "ai.release_to_error_visible_ms");
        ink_probe_count("ai.failures");
    }
    osal_free(p);
}

static void post(int req_id, int kind, int status, const char *text) {
    size_t n = text ? strlen(text) : 0;
    const char *prev = osal_mem_tag("ai");
    ai_post_t *p = osal_malloc(sizeof *p + n + 1, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!p) return;
    p->req_id = req_id;
    p->kind = kind;
    p->status = status;
    memcpy(p->text, text ? text : "", n + 1);
    ink_ui_post(ui_apply, p);
}

static void on_delta(void *ctx, const char *method, const ink_json_t *j, int params) { /* hub task */
    (void)ctx;
    if (strcmp(method, "ai.delta")) return;
    int64_t id = 0;
    char text[512];
    if (!ink_json_int(j, ink_json_find(j, params, "id"), &id)) return;
    if (ink_json_str(j, ink_json_find(j, params, "text"), text, sizeof text) < 0) return;
    post((int)id, 0, 0, text);
}

static void on_reply(void *ctx, int status, const ink_json_t *j, int result) { /* hub task */
    (void)j;
    (void)result;
    int id = (int)(intptr_t)ctx;
    post(id, status == INK_HUB_OK ? 1 : 2, status, NULL);
}

/* ------------------------------------------------------------------ key */

static void ask(void) {
    const char *prev = osal_mem_tag("ai");
    char *snap = osal_malloc(SNAPSHOT_CAP, OSAL_MEM_EXTERNAL);
    char *params = osal_malloc(SNAPSHOT_CAP * 2 + 256, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!snap || !params) {
        osal_free(snap);
        osal_free(params);
        return;
    }
    const char *app = "";
    int n = ink_ui_snapshot(snap, SNAPSHOT_CAP, &app);
    ink_jw_t w;
    ink_jw_init(&w, params, SNAPSHOT_CAP * 2 + 256);
    ink_jw_raw(&w, "{\"question\":");
    ink_jw_str(&w, SIM_TRANSCRIPT);
    ink_jw_raw(&w, ",\"page\":{\"app\":");
    ink_jw_str(&w, app);
    ink_jw_raw(&w, ",\"text\":");
    ink_jw_strn(&w, snap, n > 0 ? (size_t)n : 0);
    ink_jw_raw(&w, "}}");
    AI.answer_len = 0;
    AI.answer_text[0] = 0;
    AI.first_text_seen = false;
    AI.asking = true;
    AI.seq++;
    int id = ink_hub_call("ai.ask", w.overflow ? "{\"question\":\"?\"}" : params, ASK_TIMEOUT_US, on_reply,
                          (void *)(intptr_t)AI.seq);
    AI.req_id = id;
    osal_free(snap);
    osal_free(params);
}

void ink_ai_key(bool pressed, uint64_t t_us) {
    if (pressed) {
        AI.t_press = t_us;
        ink_hub_demand(); /* pre-connect while the user speaks */
        if (AI.open && !AI.asking) {
            lv_label_set_text(AI.answer, "");
            lv_label_set_text(AI.question, "");
        }
        bool was_open = AI.open;
        sheet_open();
        set_status("正在听…松开 AI 键结束");
        /* first sight of the sheet: drive it with the fast binary waveform (it is black on white);
         * later updates use GL16 and clean up the anti-aliased text */
        if (!was_open) ink_ui_hint_obj(AI.sheet, INK_REFRESH_FAST);
        ink_ui_cause(t_us, "ai.press_to_listening_visible_ms");
        ink_probe_count("ai.presses");
        return;
    }
    if (!AI.open) return;
    AI.t_release = t_us;
    char q[96];
    snprintf(q, sizeof q, "你问：%s", SIM_TRANSCRIPT);
    lv_label_set_text(AI.question, q);
    ink_ui_hint_obj(AI.question, INK_REFRESH_FAST);
    set_status("正在想…");
    ask();
}

int ink_ai_init(void) {
    const char *prev = osal_mem_tag("ai");
    AI.answer_text = osal_malloc(ANSWER_CAP, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!AI.answer_text) return -1;
    AI.answer_text[0] = 0;
    return ink_hub_subscribe("ai.", on_delta, NULL);
}
