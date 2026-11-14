/* Shim for the copied scene code: the scene manager, view dispatcher and loader become file-page calls. */
#pragma once
#include <furi.h>
#include "archive_browser.h"
typedef struct Loader Loader;
typedef struct ViewDispatcher ViewDispatcher;
typedef struct SceneManager SceneManager;
typedef enum { SceneManagerEventTypeCustom, SceneManagerEventTypeBack } SceneManagerEventType;
typedef struct { SceneManagerEventType type; uint32_t event; } SceneManagerEvent;
typedef enum { ArchiveAppSceneBrowser, ArchiveAppSceneRename, ArchiveAppSceneDelete } ArchiveAppScene;
typedef struct { Loader *loader; ViewDispatcher *view_dispatcher; SceneManager *scene_manager; ArchiveBrowserView *browser;
    void *loader_stop_subscription; FuriString *fav_move_str; } ArchiveApp;
#define scene_manager_set_scene_state(manager, scene, state) ((void)(manager))
#define loader_get_pubsub(loader) ((void *)(loader))
#define furi_pubsub_unsubscribe(pubsub, subscription) ((void)(pubsub))
void scene_manager_next_scene(SceneManager *manager, uint32_t scene);
void view_dispatcher_stop(ViewDispatcher *dispatcher);
void loader_start_with_gui_error(Loader *loader, const char *name, const char *args);
bool archive_scene_browser_on_event(void *context, SceneManagerEvent event);
void ink_flipper_menu(ArchiveBrowserViewModel *model);
bool ink_flipper_rescan(void);
