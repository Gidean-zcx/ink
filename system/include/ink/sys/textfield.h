/*
 * A text field for pages (app framework, placeholder look until the system UI, D-64): shows what is typed, opens the
 * keyboard when tapped; pressed long: copy, paste, ask the AI.
 */
#ifndef INK_SYS_TEXTFIELD_H
#define INK_SYS_TEXTFIELD_H

#include "ink/sys/textinput.h"
#include "ink/sys/ui.h"
#include "ink/sys/ui_semantics.h"

#ifdef __cplusplus
extern "C" {
#endif

/** UI task. `entered` is told what the person finished with (may be NULL). Tapping it starts the session; `focus` starts it now. */
lv_obj_t *ink_textfield_create(lv_obj_t *parent, const ink_text_cfg_t *cfg, void (*entered)(void *ctx, const char *value), void *ctx, bool focus);
const char *ink_textfield_text(lv_obj_t *field);

/* F2, UI task: opt a plain/search or URL field into the short-input host. Use a
 * fixed-position parent; only the field moves above the fixed keyboard, then
 * returns to its original coordinates. No page navigation or scrolling occurs.
 * Declarations live until widget deletion; spec->value borrows its live buffer.
 * Touch invokes focus_action through F1; its capability calls short_focus().
 * VALUE_CHANGED and entered report the same edits for touch and AI text.insert.
 * Call before focusing. Long text, forms and secret fields use other hosts. */
int ink_textfield_bind_short(lv_obj_t *field, const ink_ui_page_spec_t *page,
                             ink_ui_field_spec_t *spec, const ink_ui_action_spec_t *focus_action);
int ink_textfield_short_focus(lv_obj_t *field);
/* F1 page snapshot plus field identity, visible focus and fixed rectangles. */
int ink_textfield_short_snapshot(lv_obj_t *field, char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
