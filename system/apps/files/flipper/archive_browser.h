/* Shim for Flipper's archive_browser.h: the copied type table, a canvas that only records menu text, and
 * browser helpers answered by the file pages (files_flipper.c). */
#pragma once
#include "archive_browser_ink.h"
#include "archive_apps.h"
#include "views/archive_browser_view.h"
typedef struct Canvas Canvas;
typedef enum { ColorWhite, ColorBlack } Color;
#define canvas_set_color(canvas, color) ((void)(canvas))
#define canvas_draw_box(canvas, x, y, w, h) ((void)(canvas))
#define elements_slightly_rounded_frame(canvas, x, y, w, h) ((void)(canvas))
#define canvas_draw_icon(canvas, x, y, icon) ((void)(canvas))
void canvas_draw_str(Canvas *canvas, int x, int y, const char *text);
bool archive_is_item_in_array(ArchiveBrowserViewModel *model, uint32_t idx);
ArchiveFile_t *archive_get_current_file(ArchiveBrowserView *browser);
ArchiveTabEnum archive_get_tab(ArchiveBrowserView *browser);
bool archive_is_home(ArchiveBrowserView *browser);
const char *archive_get_name(ArchiveBrowserView *browser);
void archive_show_file_menu(ArchiveBrowserView *browser, bool show);
void archive_file_array_rm_selected(ArchiveBrowserView *browser);
void archive_file_array_swap(ArchiveBrowserView *browser, int8_t dir);
void archive_file_array_load(ArchiveBrowserView *browser, int8_t dir);
void archive_favorites_move_mode(ArchiveBrowserView *browser, bool active);
void archive_update_focus(ArchiveBrowserView *browser, const char *target);
void archive_enter_dir(ArchiveBrowserView *browser, FuriString *name);
void archive_leave_dir(ArchiveBrowserView *browser);
void archive_refresh_dir(ArchiveBrowserView *browser);
