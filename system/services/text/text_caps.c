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

static void cap_state(ink_call_t *c) { /* UI task */
    static const char *const TYPE[] = {"plain", "number", "phone", "password"};
    char *out = osal_malloc(16 * 1024, OSAL_MEM_EXTERNAL);
    if (!out) {
        fail(c, INK_RC_FAILED, "out of memory");
        return;
    }
    ink_jw_t w;
    ink_jw_init(&w, out, 16 * 1024);
    if (!ink_text_focused()) ink_jw_raw(&w, "{\"focused\":false}");
    else {
        ink_jw_raw(&w, "{\"focused\":true,\"type\":");
        ink_jw_str(&w, TYPE[ink_text_type()]);
        ink_jw_raw(&w, ",\"hint\":");
        ink_jw_str(&w, ink_text_hint());
        if (ink_text_type() == INK_TEXT_PASSWORD || ink_text_ai_hidden()) ink_jw_raw(&w, ",\"hidden\":true");
        else {
            ink_jw_raw(&w, ",\"value\":");
            ink_jw_str(&w, ink_text_value());
            ink_jw_raw(&w, ",\"composing\":");
            ink_jw_str(&w, ink_text_composing());
        }
        ink_jw_raw(&w, "}");
    }
    if (w.overflow) fail(c, INK_RC_FAILED, "too long");
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
    char *text = osal_malloc(INK_ARGS_TEXT, OSAL_MEM_EXTERNAL);
    if (!text) {
        fail(c, INK_RC_FAILED, "out of memory");
        return;
    }
    ink_args_get_str(&c->args, "text", text, INK_ARGS_TEXT);
    ink_text_commit_utf8(text);
    osal_free(text);
    snprintf(c->rc.object, sizeof c->rc.object, "text:focused");
}

static void cap_submit(ink_call_t *c) { /* UI task */
    if (!ink_text_focused()) {
        fail(c, INK_RC_FAILED, "no text field has the focus");
        return;
    }
    if (ink_text_type() == INK_TEXT_PASSWORD) {
        fail(c, INK_RC_REJECTED, "this field is the person's to fill in");
        return;
    }
    ink_text_enter();
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

static const ink_cap_t TEXT_CAPS[] = {
    {.id = "text.state", .title = "现在有没有文本框在输入，里面是什么（密码不给看）", .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true, .run = cap_state},
    {.id = "text.insert", .title = "往有焦点的文本框里写字（等于替用户打字）", .input = "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\",\"maxLength\":300}},\"required\":[\"text\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = cap_insert},
    {.id = "text.submit", .title = "按回车（完成、发送、搜索）", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = cap_submit},
    {.id = "clipboard.read", .title = "读剪贴板（用户在设置里打开才行）", .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_clip_read},
    {.id = "clipboard.write", .title = "写剪贴板", .input = "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\",\"maxLength\":300},\"sensitive\":{\"type\":\"boolean\"}},\"required\":[\"text\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_clip_write},
};

int text_caps_init(void) { return ink_cap_register(TEXT_CAPS, (int)(sizeof TEXT_CAPS / sizeof TEXT_CAPS[0])); }
