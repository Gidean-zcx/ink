/* Text input and the clipboard on the soft bus: what the AI can fill in for the person, through the same session. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/cap.h"
#include "ink/sys/json.h"
#include "ink/sys/textinput.h"
#include "text_internal.h"

static void fail(ink_call_t *c, ink_rc_t st, const char *why) {
    c->rc.status = st;
    snprintf(c->rc.detail, sizeof c->rc.detail, "%s", why);
}

static bool target_ok(ink_call_t *c) {
    if (!ink_text_focused()) {
        fail(c, INK_RC_FAILED, "no text field has the focus");
        return false;
    }
    if (c->src == INK_SRC_TOUCH || c->src == INK_SRC_KEY || c->src == INK_SRC_SYSTEM) return true;
    int64_t session = 0, revision = 0;
    if (!ink_args_get_int(&c->args, "session", &session) || !ink_args_get_int(&c->args, "revision", &revision) ||
        session != ink_text_target_session() || revision != ink_text_edit_revision()) {
        fail(c, INK_RC_STALE, "read text.state and use its session and revision");
        return false;
    }
    return true;
}

static void text_result(ink_call_t *c, int session) {
    snprintf(c->rc.object, sizeof c->rc.object, "text:%d", session);
    if (session == ink_text_target_session()) c->rc.revision = ink_text_edit_revision();
}

static bool editable(ink_call_t *c);

int ink_text_snapshot(char *out, size_t cap) {
    static const char *const TYPE[] = {"plain", "number", "phone", "password"};
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    if (!ink_text_focused()) ink_jw_raw(&w, "{\"focused\":false}");
    else {
        ink_jw_raw(&w, "{\"focused\":true,\"type\":");
        ink_jw_str(&w, TYPE[ink_text_type()]);
        ink_jw_raw(&w, ",\"hint\":");
        ink_jw_str(&w, ink_text_hint());
        if (ink_text_type() == INK_TEXT_PASSWORD || ink_text_ai_hidden()) ink_jw_raw(&w, ",\"hidden\":true");
        else {
            ink_jw_raw(&w, ",\"session\":");
            ink_jw_int(&w, ink_text_target_session());
            ink_jw_raw(&w, ",\"revision\":");
            ink_jw_int(&w, ink_text_edit_revision());
            ink_jw_raw(&w, ",\"value\":");
            ink_jw_str(&w, ink_text_value());
            ink_jw_raw(&w, ",\"composing\":");
            ink_jw_str(&w, ink_text_composing());
            size_t sel_start, sel_end;
            ink_text_selection(&sel_start, &sel_end);
            ink_jw_raw(&w, ",\"cursor\":");
            ink_jw_int(&w, (int64_t)ink_text_cursor());
            ink_jw_raw(&w, ",\"selection_start\":");
            ink_jw_int(&w, (int64_t)sel_start);
            ink_jw_raw(&w, ",\"selection_end\":");
            ink_jw_int(&w, (int64_t)sel_end);
            if (ink_text_can_undo()) ink_jw_raw(&w, ",\"can_undo\":true");
            if (ink_text_can_redo()) ink_jw_raw(&w, ",\"can_redo\":true");
            if (ink_text_voice_state() != INK_VOICE_IDLE) {
                ink_jw_raw(&w, ",\"heard\":");
                ink_jw_str(&w, ink_text_voice_heard());
            }
        }
        ink_jw_raw(&w, ",\"voice\":");
        ink_jw_str(&w, ink_text_voice_state_name());
        ink_jw_raw(&w, ink_text_voice_possible() ? ",\"voice_possible\":true}" : "}");
    }
    if (w.overflow) { if (cap) out[0] = 0; return -1; }
    return (int)w.len;
}

static void cap_state(ink_call_t *c) {
    char *out = osal_malloc(16 * 1024, OSAL_MEM_EXTERNAL);
    if (!out) { fail(c, INK_RC_FAILED, "out of memory"); return; }
    if (ink_text_snapshot(out, 16 * 1024) < 0) fail(c, INK_RC_FAILED, "too long");
    else ink_cap_result(c, out);
    osal_free(out);
}

static void cap_insert(ink_call_t *c) { /* UI task */
    if (!ink_text_focused()) {
        fail(c, INK_RC_FAILED, "no text field has the focus");
        return;
    }
    if (ink_text_type() == INK_TEXT_PASSWORD || ink_text_ai_hidden()) {
        fail(c, INK_RC_REJECTED, "this field is the person's to fill in");
        return;
    }
    if (!target_ok(c)) return;
    int session = ink_text_target_session();
    char *text = osal_malloc(INK_ARGS_TEXT, OSAL_MEM_EXTERNAL);
    if (!text) {
        fail(c, INK_RC_FAILED, "out of memory");
        return;
    }
    ink_args_get_str(&c->args, "text", text, INK_ARGS_TEXT);
    ink_text_commit_utf8(text);
    osal_free(text);
    text_result(c, session);
}

static void cap_submit(ink_call_t *c) { /* UI task */
    if (!ink_text_focused()) {
        fail(c, INK_RC_FAILED, "no text field has the focus");
        return;
    }
    if (ink_text_type() == INK_TEXT_PASSWORD || ink_text_ai_hidden()) {
        fail(c, INK_RC_REJECTED, "this field is the person's to fill in");
        return;
    }
    if (!target_ok(c)) return;
    int session = ink_text_target_session();
    ink_text_enter();
    text_result(c, session);
}

/* What the microphone key does: the AI's comes with a confirmation card, the person's does not. */
static void cap_voice_start(ink_call_t *c) { /* UI task */
    if (!ink_text_focused()) {
        fail(c, INK_RC_FAILED, "no text field has the focus");
        return;
    }
    if (ink_text_type() == INK_TEXT_PASSWORD || (c->src != INK_SRC_TOUCH && c->src != INK_SRC_KEY && ink_text_ai_hidden())) {
        fail(c, INK_RC_REJECTED, "this field is the person's to fill in");
        return;
    }
    if (!target_ok(c)) return;
    if (ink_text_voice_start() != 0) fail(c, INK_RC_FAILED, ink_text_voice_notice()[0] ? ink_text_voice_notice() : "not now");
    else snprintf(c->rc.say, sizeof c->rc.say, "请说");
}

static void cap_voice_finish(ink_call_t *c) { /* UI task */
    if (c->src != INK_SRC_TOUCH && c->src != INK_SRC_KEY && (ink_text_type() == INK_TEXT_PASSWORD || ink_text_ai_hidden())) {
        fail(c, INK_RC_REJECTED, "this field is the person's to fill in");
        return;
    }
    if (!target_ok(c)) return;
    if (ink_text_voice_state() == INK_VOICE_IDLE) fail(c, INK_RC_FAILED, "nobody is dictating");
    else ink_text_voice_finish();
}

static void cap_voice_cancel(ink_call_t *c) { /* UI task */
    if (c->src != INK_SRC_TOUCH && c->src != INK_SRC_KEY && (ink_text_type() == INK_TEXT_PASSWORD || ink_text_ai_hidden())) {
        fail(c, INK_RC_REJECTED, "this field is the person's to fill in");
        return;
    }
    if (!target_ok(c)) return;
    if (ink_text_voice_state() == INK_VOICE_IDLE) fail(c, INK_RC_FAILED, "nobody is dictating");
    else ink_text_voice_cancel();
}

static void cap_clip_read(ink_call_t *c) {
    char *text = osal_malloc(INK_CLIP_MAX + 1, OSAL_MEM_EXTERNAL);
    char *out = osal_malloc(INK_CLIP_MAX * 2 + 64, OSAL_MEM_EXTERNAL);
    if (!text || !out) fail(c, INK_RC_FAILED, "out of memory");
    else if (ink_clip_get(text, INK_CLIP_MAX + 1, (uint8_t)c->src, NULL) < 0) fail(c, INK_RC_REJECTED, "the clipboard is not open to this caller (setting ai.read_clipboard)");
    else {
        ink_jw_t w;
        ink_jw_init(&w, out, INK_CLIP_MAX * 2 + 64);
        ink_jw_raw(&w, "{\"text\":");
        ink_jw_str(&w, text);
        ink_jw_raw(&w, "}");
        if (w.overflow) fail(c, INK_RC_FAILED, "too long");
        else ink_cap_result(c, out);
    }
    osal_free(out);
    osal_free(text);
}

/* The field is there, and it is one the AI may touch. */
static bool editable(ink_call_t *c) {
    if (!ink_text_focused()) fail(c, INK_RC_FAILED, "no text field has the focus");
    else if (ink_text_type() == INK_TEXT_PASSWORD || ink_text_ai_hidden()) fail(c, INK_RC_REJECTED, "this field is the person's to fill in");
    else return target_ok(c);
    return false;
}

static void cap_select(ink_call_t *c) { /* UI task */
    int64_t start = 0, end = 0;
    ink_args_get_int(&c->args, "start", &start);
    if (!ink_args_get_int(&c->args, "end", &end)) end = start;
    if (!editable(c)) return;
    int session = ink_text_target_session();
    ink_text_select((size_t)start, (size_t)end);
    text_result(c, session);
}

static void cap_undo(ink_call_t *c) { /* UI task */
    if (!editable(c)) return;
    int session = ink_text_target_session();
    if (!ink_text_undo()) fail(c, INK_RC_FAILED, "nothing to undo");
    else text_result(c, session);
}

static void cap_redo(ink_call_t *c) { /* UI task */
    if (!editable(c)) return;
    int session = ink_text_target_session();
    if (!ink_text_redo()) fail(c, INK_RC_FAILED, "nothing to redo");
    else text_result(c, session);
}

static void cap_clip_write(ink_call_t *c) {
    char *text = osal_malloc(INK_ARGS_TEXT, OSAL_MEM_EXTERNAL);
    if (!text) {
        fail(c, INK_RC_FAILED, "out of memory");
        return;
    }
    bool sensitive = false;
    ink_args_get_str(&c->args, "text", text, INK_ARGS_TEXT);
    ink_args_get_bool(&c->args, "sensitive", &sensitive);
    if (ink_clip_set(text, sensitive, (uint8_t)c->src) != 0) fail(c, INK_RC_FAILED, "too long");
    osal_free(text);
}

#define TARGET_FIELDS "\"session\":{\"type\":\"integer\",\"minimum\":1,\"description\":\"AI must use text.state session\"},\"revision\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295,\"description\":\"AI must use text.state revision\"}"
#define TARGET_SCHEMA "{\"type\":\"object\",\"properties\":{" TARGET_FIELDS "}}"

static const ink_cap_t TEXT_CAPS[] = {
    {.id = "text.state", .title = "现在有没有文本框在输入，里面是什么（密码不给看）", .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true, .run = cap_state},
    {.id = "text.insert", .title = "往有焦点的文本框里写字（等于替用户打字）", .input = "{\"type\":\"object\",\"properties\":{" TARGET_FIELDS ",\"text\":{\"type\":\"string\",\"maxLength\":300}},\"required\":[\"text\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = cap_insert},
    {.id = "text.select", .title = "放光标或选一段字（按字数算；不给 end 就是放光标。text.insert 写在这里，选中的被替换）",
     .input = "{\"type\":\"object\",\"properties\":{" TARGET_FIELDS ",\"start\":{\"type\":\"integer\",\"minimum\":0},\"end\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"start\"]}",
     .effect = INK_FX_NAVIGATE, .child_ok = true, .run = cap_select},
    {.id = "text.undo", .input = TARGET_SCHEMA, .title = "撤销文本框里刚才的修改", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = cap_undo},
    {.id = "text.redo", .input = TARGET_SCHEMA, .title = "把撤销的改回来", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = cap_redo},
    {.id = "text.submit", .input = TARGET_SCHEMA, .title = "按回车（完成、发送、搜索）", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = cap_submit},
    {.id = "text.voice_start", .input = TARGET_SCHEMA, .title = "打开语音输入：说的话变成文字进文本框", .ask = "打开语音输入（声音会发给家庭服务器）", .effect = INK_FX_EXTERNAL, .child_ok = true, .run = cap_voice_start},
    {.id = "text.voice_finish", .input = TARGET_SCHEMA, .title = "说完了，结束语音输入", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = cap_voice_finish},
    {.id = "text.voice_cancel", .input = TARGET_SCHEMA, .title = "取消语音输入，什么都不写进去", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = cap_voice_cancel},
    {.id = "text.voice_ended", .title = "语音输入结束了", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "clipboard.read", .title = "读剪贴板（用户在设置里打开才行）", .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_clip_read},
    {.id = "clipboard.write", .title = "写剪贴板", .input = "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\",\"maxLength\":300},\"sensitive\":{\"type\":\"boolean\"}},\"required\":[\"text\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_clip_write},
};

int text_caps_init(void) { return ink_cap_register(TEXT_CAPS, (int)(sizeof TEXT_CAPS / sizeof TEXT_CAPS[0])); }
