/* Keyboard-only geometry. INK_KBD_KEY_HEIGHT can be overridden for review images (-DINK_KBD_KEY_HEIGHT=108). */
/* ui-scale-runtime: dimension kbd_scaled Keys, gaps and candidate cells grow with the text line by one factor. */
#ifndef INK_KEYBOARD_THEME_H
#define INK_KEYBOARD_THEME_H
#ifndef INK_KBD_KEY_HEIGHT
#define INK_KBD_KEY_HEIGHT INK_KB_KEY
#endif
#define INK_KBD_TOUCH 108        /* the smallest cell a finger is given */
#define INK_KBD_RECENT 8         /* recent symbols per member */
#define INK_KBD_IDLE_MS 400      /* Mogo: the word strip settles after three changes and this pause */
#define INK_KBD_POPUP_COLUMNS 5
#define INK_KBD_CANDIDATES 64    /* more than the text service ever offers (60) */
/* A keyboard token at the current text size: the same factor as the key height (INK_LINE_LIST is the reference line). */
static inline int kbd_scaled(int token) { return ink_ui_line_height(INK_FONT_LIST) * token / INK_LINE_LIST; }
#endif
