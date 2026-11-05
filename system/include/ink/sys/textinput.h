/*
 * Text input (system services design §9): one input session at a time, the pinyin engine and the clipboard. The keyboard
 * panel and the text field are the system shell's and the app framework's; what they show comes from here, and so does
 * what the AI can do: a field filled in for the person goes through the same session.
 *
 * Session functions run in the UI task. Keys are never dropped: they come through the UI's input queue.
 */
#ifndef INK_SYS_TEXTINPUT_H
#define INK_SYS_TEXTINPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { INK_TEXT_PLAIN = 0, INK_TEXT_NUMBER, INK_TEXT_PHONE, INK_TEXT_PASSWORD } ink_text_type_t;
typedef enum { INK_ENTER_DONE = 0, INK_ENTER_SEND, INK_ENTER_SEARCH, INK_ENTER_NEXT } ink_text_enter_t;

typedef struct {
    ink_text_type_t type;
    ink_text_enter_t enter;
    const char *initial, *hint;
    size_t max;      /* bytes; 0 = 1 KiB, at most 4 KiB */
    bool ai_hidden;  /* the AI does not see what is typed here (a password is never seen) */
} ink_text_cfg_t;

typedef struct {
    void (*changed)(void *ctx);                    /* value or composing changed */
    void (*entered)(void *ctx, const char *value); /* the enter key, nothing being composed */
    void (*ended)(void *ctx);                      /* the session is over, whoever ended it */
} ink_text_client_t;

typedef struct {
    char word[40];
    uint8_t consumed; /* letters of the composing string it takes */
} ink_text_cand_t;

#define INK_TEXT_PAGE 6 /* candidates at a time */

/** A new session (the last one ends): its id (> 0), or -1. */
int ink_text_focus(const ink_text_cfg_t *cfg, const ink_text_client_t *client, void *ctx);
void ink_text_blur(int session);
bool ink_text_focused(void);
/** UI task: semantic input snapshot, with credential contents masked. */
int ink_text_snapshot(char *out, size_t cap);
ink_text_type_t ink_text_type(void);
ink_text_enter_t ink_text_enter_kind(void);
const char *ink_text_hint(void);
bool ink_text_ai_hidden(void);

/* What the keyboard (or a test) feeds in. */
void ink_text_key(char c);
void ink_text_backspace(void);
/** A candidate, by its place in the whole list. */
void ink_text_pick(int index);
void ink_text_enter(void);
/** Text as it is: what is being composed goes in as typed, then this. In place of what is selected, or at the cursor. */
void ink_text_commit_utf8(const char *s);

/* The cursor and the selection, counted in characters as a label and the AI count them. Keys, candidates and backspace all work
 * there. Nobody drags on electronic paper: a tap puts the cursor, a long press selects a word, the arrows adjust (system UI spec). */
size_t ink_text_cursor(void);
/** start == end: nothing is selected. */
void ink_text_selection(size_t *start, size_t *end);
void ink_text_set_cursor(size_t at);
void ink_text_select(size_t start, size_t end);
/** The word around this place: letters and digits that go together; in Chinese two characters, until there is a word list to go by. */
void ink_text_select_word(size_t at);
/** The arrows: nothing selected, the cursor moves; something selected, its end (or its start) does. */
void ink_text_move(int chars, bool end);
/** What is selected, bytes copied; -1 for a password, which does not come out. */
int ink_text_selected(char *out, size_t cap);
/** Keys in a row are one step. A password keeps no history. */
bool ink_text_undo(void);
bool ink_text_redo(void);
bool ink_text_can_undo(void);
bool ink_text_can_redo(void);

const char *ink_text_value(void);
const char *ink_text_composing(void);
/** Candidates from `first` on: how many (up to max). */
int ink_text_candidates(int first, ink_text_cand_t *out, int max);

int ink_text_service_init(void);
/** The keyboard: what it shows changed, or there is no session any more. UI task. */
void ink_text_set_presenter(void (*changed)(void));

/*
 * Dictation (system services design §9.5): the hub turns speech into words (the device itself only knows command words); they
 * show in the keyboard's panel as they come and go into the field at the end, to be edited like anything typed. Not for a
 * password. Over with the session, the lock, a call or an alarm, nothing kept; when it fails, what was final stays and the notice
 * says why. All in the UI task.
 */
typedef enum { INK_VOICE_IDLE = 0, INK_VOICE_CONNECTING, INK_VOICE_LISTENING, INK_VOICE_FINISHING } ink_voice_state_t;
/** This field has a microphone key: not a password, a microphone there, and (online) the hub offers it to this member. */
bool ink_text_voice_possible(void);
/** 0, or -1 and a notice when there is something to say about it. */
int ink_text_voice_start(void);
/** Said: what is still on its way comes, and goes into the field. */
void ink_text_voice_finish(void);
/** Back to the keyboard, nothing goes in. */
void ink_text_voice_cancel(void);
/** UI task. A call, an alarm, the AI key: the microphone goes at once, what was said so far still goes into the field. */
void ink_text_voice_interrupt(void);
ink_voice_state_t ink_text_voice_state(void);
const char *ink_text_voice_state_name(void);
/** The end of what was heard so far, for the panel's fixed area. */
const char *ink_text_voice_heard(void);
/** Why it did not go on, until the next key. */
const char *ink_text_voice_notice(void);

/* ---- the clipboard: one text, not stored, gone with the member ---- */

#define INK_CLIP_MAX 4096
/** 0, or -1 (too long). `sensitive`: gone after a minute, never shown to the AI. `src`: ink_source_t. */
int ink_clip_set(const char *text, bool sensitive, uint8_t src);
/** Bytes copied (NUL-terminated), 0 = empty, -1 = this source may not read it. */
int ink_clip_get(char *out, size_t cap, uint8_t src, bool *sensitive);
void ink_clip_clear(void);

#ifdef __cplusplus
}
#endif
#endif
