/* Inside the text service. */
#ifndef INK_TEXT_INTERNAL_H
#define INK_TEXT_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

/* The pinyin engine. The candidates of a composing string, longest key first, by the dictionary's frequency within a key. */
typedef struct {
    const char *word; /* in the dictionary: use it under the service lock */
    uint16_t word_len;
    uint8_t consumed;
    uint16_t freq;
} py_cand_t;

#define PY_DICT_PATH "/content/.system/ime/base.ikd"
#define PY_DICT_ASSET "ime/base.ikd" /* the same file in the firmware's asset pack */

/** Have the dictionary: loads in the background the first time; `ready` runs in the UI task when it is in. */
void py_want_dict(void (*ready)(void));
/** Under the service lock. How many (up to max). */
int py_lookup(const char *composing, py_cand_t *out, int max);
bool py_dict_loaded(void);
void py_init(void);

/* Dictation (voice_input.c). */
int text_voice_init(void);
void text_voice_on_blur(void);
void text_voice_clear_notice(void);
/** The keyboard's: what it shows changed. */
void text_presenter_kick(void);

/* UI task: asynchronous edits name this target and revision. */
int ink_text_target_session(void);
uint32_t ink_text_edit_revision(void);
bool ink_text_session_current(void);
int text_caps_init(void);
void clip_init(void);

#endif
