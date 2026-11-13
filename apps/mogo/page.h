#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <graft.h>
/* Why a page shows an explanation instead of the native view (D-138: never a blank page). */
enum { INK_MOGO_R_NONE, INK_MOGO_R_NO_CARD, INK_MOGO_R_NO_FONT, INK_MOGO_R_BAD_FONT, INK_MOGO_R_BAD_BOOK, INK_MOGO_R_EMPTY };
/* One engine operation, run on the BG executor for the reader capabilities. */
enum { INK_MOGO_OP_LOAD, INK_MOGO_OP_OPEN, INK_MOGO_OP_SHELF, INK_MOGO_OP_TURN, INK_MOGO_OP_LIST, INK_MOGO_OP_GOTO_PAGE,
    INK_MOGO_OP_READ, INK_MOGO_OP_CHAPTERS, INK_MOGO_OP_TOC, INK_MOGO_OP_ENTRY, INK_MOGO_OP_CHAPTER, INK_MOGO_OP_TOC_PERCENT,
    INK_MOGO_OP_MARKS, INK_MOGO_OP_ADD_MARK, INK_MOGO_OP_REMOVE_MARK, INK_MOGO_OP_GOTO_MARK, INK_MOGO_OP_LAYOUT,
    INK_MOGO_OP_GET_LAYOUT, INK_MOGO_OP_MENU, INK_MOGO_OP_BACK, INK_MOGO_OP_SAVE };
enum { INK_MOGO_LAYOUT_COUNT = 8, INK_MOGO_UNSET = -1000 };
extern const char *const ink_mogo_layout_keys[INK_MOGO_LAYOUT_COUNT];
typedef struct {
    int op, value, layout[INK_MOGO_LAYOUT_COUNT];
    char text[288];          /* a path to open */
    char *json; size_t cap;  /* result for the AI, may be NULL; a result that does not fit comes back in a bigger buffer
                              * (ink_app_alloc) put here instead: the caller frees it when it is not the one it gave */
    char say[160];           /* one sentence for the receipt */
    uint32_t revision;       /* page in the chapter after a page operation */
} ink_mogo_request_t;
/* BG: set the member, the canvas and the font; returns INK_MOGO_R_*. */
int ink_mogo_page_start(const ink_member_identity_t *owner, int w, int h, int px);
/* BG: 0 done, -1 failed (stale, out of range), or INK_MOGO_R_* when the page now explains why. */
int ink_mogo_page_command(ink_mogo_request_t *r);
void ink_mogo_page_draw(uint8_t *pixels);
void ink_mogo_page_close(void);
int ink_mogo_page_snapshot(char *out, size_t cap);
int ink_mogo_page_reason(void);
const char *ink_mogo_page_message(void);
/* The explanation page's text for the current reason, "" when the native view is shown. */
const char *ink_mogo_page_explanation(void);
/* The shelf or an explanation page: BACK leaves the app from here. */
bool ink_mogo_page_root(void);
/* Every page but the bare body shows the hand back button. */
bool ink_mogo_page_back_shown(void);
/* What a tap at native page coordinates asks for: a capability name and its JSON, "" swallowed, NULL not a hit. */
const char *ink_mogo_page_hit(int x, int y, char *args, size_t cap);
/* BG, no font needed: the member's three book places. JSON [{index,title,place,percent}] in *json (a bigger buffer from
 * ink_app_alloc replaces it when it does not fit, as for a request) and the path of `title`/`index`. The index found,
 * -1 none, -2 no card, -3 the list could not be written. */
int ink_mogo_shelf_query(const ink_member_identity_t *owner, char **json, size_t *cap, const char *title, int index, char *path, size_t path_cap);
/* BG: the book the card continues, if it is still there. */
typedef struct { char path[288], title[96], chapter[96]; uint16_t chapter_no; uint8_t pct; uint32_t offset; } ink_mogo_card_t;
bool ink_mogo_card_load(const char *member, ink_mogo_card_t *out);
extern bool ink_mogo_quiet_flush; /* BG: the save in progress is a pause in reading, flushed at background priority */
/* BG: the open book's place for the card, else the record the session wrote last (empty path: none). */
bool ink_mogo_page_card(ink_mogo_card_t *out);
/* BG: which font file the session can use; INK_MOGO_R_*. */
int ink_mogo_font_check(void);
/* BG: move the old reader's data for `owner` (migrate.c). only == NULL: the next book of the whole set, then the
 * marker; otherwise only the last book and `only` (may be ""). 1: more to do, 0: done, -1: not now (no card, a write failed). */
int ink_mogo_migrate_step(const ink_member_identity_t *owner, const char *only);
int ink_mogo_migrate_init(void);
void ink_mogo_book_title(const char *path, char *out, size_t cap);
