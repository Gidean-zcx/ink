/* AI pages and the shared physical-key capability bridge. */
#ifndef INK_AI_CHAT_H
#define INK_AI_CHAT_H
#include "ink/ui_shell.h"
int ink_ai_chat_init(void);
void ink_ai_chat_key(bool pressed, uint64_t t_us);
bool ink_ai_chat_tapped(void);
/* status: what the panel shows; key: the untranslated caption it was drawn from, for the panel's state */
int ink_ai_chat_panel_snapshot(const char *status, const char *key, const char *question, const char *answer,
                               int page, int pages, bool speaking, uint32_t revision, char *out, size_t cap);
#endif
