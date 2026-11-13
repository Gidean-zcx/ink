#include <furi.h>
#include "archive_browser_view.h"
#include "archive_browser.h"

static void render_item_menu(Canvas* canvas, ArchiveBrowserViewModel* model) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 71, 17, 57, 46);
    canvas_set_color(canvas, ColorBlack);
    elements_slightly_rounded_frame(canvas, 70, 16, 58, 48);

    FuriString* menu[MENU_ITEMS];

    menu[0] = furi_string_alloc_set("Run in app");
    menu[1] = furi_string_alloc_set("Pin");
    menu[2] = furi_string_alloc_set("Rename");
    menu[3] = furi_string_alloc_set("Delete");
    menu[4] = furi_string_alloc_set("Copy");
    menu[5] = furi_string_alloc_set("Move to");
    menu[6] = furi_string_alloc_set("Info");
    menu[7] = furi_string_alloc_set("Edit");
    menu[8] = furi_string_alloc_set("Read");
    menu[9] = furi_string_alloc_set("---");

    ArchiveFile_t* selected =
        archive_is_item_in_array(model, model->item_idx) ?
            files_array_get(model->files, model->item_idx - model->array_offset) :
            NULL;

    if(!selected) {
        for(size_t i = 0; i < MENU_ITEMS; i++) {
            furi_string_set(menu[i], "---");
        }
    } else {
        if((selected->fav) || (model->tab_idx == ArchiveTabFavorites)) {
            furi_string_set(menu[1], "Unpin");
        }

        if(!archive_is_known_app(selected->type)) {
            furi_string_set(menu[0], "---");
            furi_string_set(menu[1], "---");
        } else {
            if(model->tab_idx == ArchiveTabFavorites) {
                furi_string_set(menu[2], "Move");
                furi_string_set(menu[3], "---");
            } else if(selected->is_app) {
                furi_string_set(menu[2], "---");
            }
        }
    }

    if(selected) {
        if(!furi_string_end_withi(selected->path, ".txt")) {
            furi_string_set(menu[7], "---");
            furi_string_set(menu[8], "---");
        }
        if(furi_string_search(selected->path, "/me/") != 0) {
            furi_string_set(menu[8], "---");
        }
        if(selected->type == ArchiveFileTypeFolder) {
            furi_string_set(menu[9], selected->fav ? "Unpin folder" : "Pin folder");
        }
    }

    for(size_t i = 0; i < MENU_ITEMS; i++) {
        canvas_draw_str(canvas, 82, 27 + i * 11, furi_string_get_cstr(menu[i]));
        furi_string_free(menu[i]);
    }

    canvas_draw_icon(canvas, 74, 20 + model->menu_idx * 11, &I_ButtonRight_4x7);
}
