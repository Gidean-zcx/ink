/* Between the kiikoread page (third_party/kiikoread_music/app_music.c) and the glue (apps/music/music.c).
 * Everything here runs on the BG executor. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "epdiy.h"
#include "../../../third_party/kiikoread_music/music_player.h"

enum { APP_REDRAW_NONE = 0, APP_REDRAW_PAGE, APP_REDRAW_AREA };
typedef int app_redraw_t;
typedef struct { int index, count; char folder[72]; } music_row_t; /* index -1: a folder */

/* Glue, for the page: a tap or key becomes the capability AI uses, with the touch source. */
bool music_request(const char *cap, const char *fmt, ...);
bool music_request_path(const char *cap, const char *path);
bool music_playable(const char *path);

/* Page and drawing, for the glue. */
void ink_music_canvas(int w, int h);
void music_page_enter(int width, int height);
void music_page_reset(void);
bool music_page_tap(int x, int y);
bool music_page_key(int key);
app_redraw_t music_page_tick(void);
void music_page_message(const char *text);
app_redraw_t music_page_list(int view, int page, const char *dir);
app_redraw_t music_page_changed(const char *what, const char *path);
EpdRect music_page_draw(uint8_t *fb);
void music_page_presented(void);
bool music_page_all(void);
const char *music_page_dir(void);
const char *music_page_notice(void);
int music_page_rows(music_row_t *out, int max, int *page, int *pages);
int music_page_targets(EpdRect *out, int max);
