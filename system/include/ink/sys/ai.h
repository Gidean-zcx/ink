/*
 * AI service: the system-wide AI key (push-to-talk, D-17) and the answer
 * sheet. Any screen can be asked about: the foreground activity's page
 * snapshot goes with the question (G-002, foundation §6.3).
 */
#ifndef INK_SYS_AI_H
#define INK_SYS_AI_H

#include "ink/sys/i18n.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bytes of one answer the device keeps and shows (PSRAM; ~10,000 Chinese characters, far beyond a spoken
 * answer). Longer answers are cut at a whole character and end with INK_AI_ANSWER_CUT. */
#define INK_AI_ANSWER_MAX 32768
#define INK_AI_ANSWER_CUT TR_NOOP("\n回答太长，只显示了前面部分。")

typedef enum {
    INK_AI_ERROR_NONE, INK_AI_ERROR_OFFLINE, INK_AI_ERROR_LOST,
    INK_AI_ERROR_TIMEOUT, INK_AI_ERROR_BUDGET
} ink_ai_error_t;

int ink_ai_init(void);
/** UI task: the AI key went down / up at physical time t_us. */
void ink_ai_key(bool pressed, uint64_t t_us);
/** UI task: a question in words, about what is on screen; the answer comes on the sheet, as for a spoken one. */
void ink_ai_ask_text(const char *text);
void ink_ai_ask_photo(const char *thumbnail_json);
void ink_ai_show_answer(const char *answer);
bool ink_ai_can_replay(void);
int ink_ai_replay(void);
int ink_ai_history_init(void);
void ink_ai_history_save(const char *member, const char *question, const char *answer, const char *page);
/** UI task: stop this turn's speech, including later frames; keep its text. */
void ink_ai_stop_speech(void);
/** Any task: the device's own words end the current turn on the sheet, e.g. a receipt when the link is about to go;
 *  what the hub sends after that adds to them, a failure of the link no longer replaces them. */
void ink_ai_note(const char *text);
/** UI task: close the sheet if open; returns true if it was open. */
bool ink_ai_dismiss(void);

#ifdef __cplusplus
}
#endif
#endif
