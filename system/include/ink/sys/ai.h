/*
 * AI service: the system-wide AI key (push-to-talk, D-17) and the answer
 * sheet. Any screen can be asked about: the foreground activity's page
 * snapshot goes with the question (G-002, foundation §6.3).
 */
#ifndef INK_SYS_AI_H
#define INK_SYS_AI_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ink_ai_init(void);
/** UI task: the AI key went down / up at physical time t_us. */
void ink_ai_key(bool pressed, uint64_t t_us);
/** UI task: a question in words, about what is on screen; the answer comes on the sheet, as for a spoken one. */
void ink_ai_ask_text(const char *text);
/** UI task: stop this turn's speech, including later frames; keep its text. */
void ink_ai_stop_speech(void);
/** UI task: close the sheet if open; returns true if it was open. */
bool ink_ai_dismiss(void);

#ifdef __cplusplus
}
#endif
#endif
