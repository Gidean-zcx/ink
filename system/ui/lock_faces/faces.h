#ifndef INK_LOCK_FACES_H
#define INK_LOCK_FACES_H
#include "ink/ui_shell.h"
#include "ink/sys/members.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_semantics.h"
/* 0..5 keep their meaning across upgrades; styles an app draws come after (lock-styles-1). */
enum { INK_LOCK_SIMPLE = 3, INK_LOCK_STANDBY, INK_LOCK_CALENDAR, INK_LOCK_TICKET, INK_LOCK_COLLAGE, INK_LOCK_DIGITAL,
       INK_LOCK_ALMANAC, INK_LOCK_PERPETUAL, INK_LOCK_LIBRARY, INK_LOCK_FACE_COUNT };
enum { INK_LOCK_DRAWN = INK_LOCK_TICKET, INK_LOCK_FACE_FIELDS = 8 };
extern const char *const ink_lock_face_choices[];
extern const char *const ink_lock_face_notes[];
void ink_lock_face_draw(lv_obj_t *parent, int style);
bool ink_lock_face_update(void);
void ink_lock_face_clear(void);
void ink_lock_face_resume(void);
size_t ink_lock_face_fields(ink_ui_field_spec_t *fields);
void ink_lock_face_settings(lv_obj_t *parent);

/* Lock styles an app draws: a fixed table of clients, registered once at start-up. A client draws a whole style
 * on the background executor into a 684x1216 L8 canvas in PSRAM and says what it shows; the UI task puts the canvas
 * under the lock screen's own status bar and buttons. Until a canvas is ready the default lock screen shows. */
enum { INK_LOCK_CANVAS_W = 684, INK_LOCK_CANVAS_H = 1216, INK_LOCK_CLIENTS = 2, INK_LOCK_NAME_MAX = 16,
       INK_LOCK_SAYS_MAX = 1536, INK_LOCK_WHY_MAX = 128, INK_LOCK_PATH_MAX = 160 };
typedef struct {
    int style, option, select, px, zone; /* option: the style's own setting; select: the focused item; px: text size, 100 = default */
    bool open, child;                 /* the member's space is open (their books may show); a child is drawing */
    ink_member_identity_t owner;
    ink_tm_t now;                     /* local time, valid only with clock */
    bool clock;
} ink_lock_request_t;
typedef struct {
    char says[INK_LOCK_SAYS_MAX];    /* what the canvas shows, one "label: value" a line, for the snapshot */
    char why[INK_LOCK_WHY_MAX];      /* not drawn: the default lock screen shows this line in its place */
    lv_area_t minute;                 /* where the time is drawn; x2 < x1 when nothing on it changes by the minute */
} ink_lock_drawn_t;
typedef enum { INK_LOCK_TAP_NONE, INK_LOCK_TAP_SELECT, INK_LOCK_TAP_OPEN } ink_lock_tap_t;
typedef struct {
    const char *name;                 /* < INK_LOCK_NAME_MAX, checked when registered */
    int first, count;                 /* the styles it draws */
    bool (*draw)(const ink_lock_request_t *r, uint8_t *canvas, ink_lock_drawn_t *out); /* BG; canvas comes white or with the wallpaper */
    /* UI: a tap at canvas (x,y) on what was drawn last; SELECT fills *index, OPEN fills cap and args to call after the unlock */
    ink_lock_tap_t (*tap)(int style, int x, int y, int *index, char *cap, size_t cap_size, char *args, size_t args_size);
    void (*shown)(int style, const uint8_t *canvas); /* BG: the canvas is on the panel and unchanged (caches it) */
    uint32_t (*revision)(int style);  /* any task: what the style reads changed since (a book read, a minute logged) */
} ink_lock_client_t;
int ink_lock_face_register(const ink_lock_client_t *client);
/* lock_mark: a style can be drawn now (a drawn style is ready), else the default lock and *why, if any, explain. */
bool ink_lock_face_ready(int style);
const char *ink_lock_face_why(void);
/* lock_mark: how=1 build the lock again, how=2 only its bottom line changed, how=0 content changed in place. */
void ink_lock_face_hook(void (*changed)(int how));
/* wallpaper.c: the chosen wallpaper scaled to cover a w x h L8 canvas (UI task); false and untouched without one. */
bool ink_wallpaper_fill(uint8_t *canvas, int w, int h);
#endif
