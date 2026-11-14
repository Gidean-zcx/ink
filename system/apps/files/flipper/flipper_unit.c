/* The copied Flipper code as one unit, built with its warnings off (CMakeLists.txt): it opens two static helpers. */
#include "helpers/archive_files.c"
#include "helpers/archive_favorites.c"
#include "views/archive_browser_view.c"
#include "scenes/archive_scene_browser.c"
void ink_flipper_menu(ArchiveBrowserViewModel *model) { render_item_menu(NULL, model); }
bool ink_flipper_rescan(void) { return archive_favourites_rescan(); }
