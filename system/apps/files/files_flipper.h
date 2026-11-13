/* The file pages and the copied Flipper archive code (vendor/flipper), through the shims in flipper/. UI task unless noted. */
#ifndef INK_FILES_FLIPPER_H
#define INK_FILES_FLIPPER_H
#include <stdbool.h>
#include <stdint.h>
#include "ink/sys/file.h"
enum { FILES_FAV_MAX = 32, FILES_MENU_MAX = 10, FILES_SCENE_RENAME = 1, FILES_SCENE_DELETE = 2 };
/* Flipper's ArchiveBrowserEvent and ArchiveFileTypeEnum numbers (checked in files_flipper.c). */
enum { FILES_EV_MENU = 0, FILES_EV_RUN = 2, FILES_EV_PIN, FILES_EV_RENAME, FILES_EV_DELETE, FILES_EV_ENTER,
       FILES_EV_PREV = 12, FILES_EV_NEXT, FILES_EV_REFRESH, FILES_EV_EXIT };
enum { FILES_TYPE_TEXT, FILES_TYPE_BOOK, FILES_TYPE_AUDIO, FILES_TYPE_IMAGE, FILES_TYPE_ZIP, FILES_TYPE_FOLDER, FILES_TYPE_UNKNOWN };
/* The member's preferences (a record in files.drafts): sort, last destination and the favourites "file" (a path per line). */
typedef struct { int32_t sort, desc; char last[INK_FS_PATH_MAX]; char fav[FILES_FAV_MAX * INK_FS_PATH_MAX + 1]; } files_prefs_t;
int files_fl_init(void);
/** The member's preferences, held (any task) until files_prefs_end, which saves a change. */
files_prefs_t *files_prefs_begin(void);
void files_prefs_end(bool changed);
void files_fl_select(const char *path, bool dir, bool favorites);
/** The menu render_item_menu composes for the selection, without its "---" items. */
int files_fl_menu(const char *out[FILES_MENU_MAX]);
bool files_fl_event(int event);
bool files_fl_is_fav(const char *path, bool dir);
int files_fl_type(const char *path, bool dir);
void files_fl_pin_folder(const char *path);
void files_fl_renamed(const char *from, const char *to);
void files_fl_deleted(const char *path, bool removed);
/** Flipper's rescan (reads the card: BG). */
bool files_fl_rescan(void);
/* Answered by the file pages (files.c). */
void files_fl_show_menu(bool show);
void files_fl_enter(const char *path);
void files_fl_leave(void);
bool files_fl_home(void);
void files_fl_reload(void);
void files_fl_turn(int dir);
void files_fl_removed(void);
void files_fl_open(const char *app);
void files_fl_scene(int scene);
void files_fl_stop(void);
#endif
