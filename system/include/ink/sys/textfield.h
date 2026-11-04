/*
 * A text field for pages (app framework, placeholder look until the system UI, D-64): shows what is typed, opens the
 * keyboard when tapped; pressed long: copy, paste, ask the AI.
 */
#ifndef INK_SYS_TEXTFIELD_H
#define INK_SYS_TEXTFIELD_H

#include "ink/sys/textinput.h"
#include "ink/sys/ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/** UI task. `entered` is told what the person finished with (may be NULL). Tapping it starts the session; `focus` starts it now. */
lv_obj_t *ink_textfield_create(lv_obj_t *parent, const ink_text_cfg_t *cfg, void (*entered)(void *ctx, const char *value), void *ctx, bool focus);
const char *ink_textfield_text(lv_obj_t *field);

#ifdef __cplusplus
}
#endif
#endif
