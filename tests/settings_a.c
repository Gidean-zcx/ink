/* Real presenters, member policy, settings persistence and F1/F4 snapshots. */
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include <stdlib.h>

static bool restarting;
static const char *wanted, *opening;
static int x, y;
static lv_obj_t *found;
static int page_number, page_offset, page_visible, first_visible;
static char snapshot[4096], bound[128];
static const char *const PAGES[] = {"SetVoice", "SetStorage", "SetAbout", "SetPrivacy"};
static const char *const CAPS[] = {"shell.voice", "shell.storage", "shell.about", "shell.privacy"};
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "display settles"); }
static void save_snapshot(void *arg) {
    (void)arg;
    char path[600];
    int n = snprintf(path, sizeof path, "%s/snapshots.ndjson", g_sim.out_dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "ab");
    if (!f || fputs(snapshot, f) == EOF || fputc('\n', f) == EOF || fclose(f)) abort();
}
static void read_snapshot(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "the production page has a complete snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "the page fits the real AI context buffer");
    CHECK(strstr(snapshot, "\"pagination\"") && strstr(snapshot, "\"fields\"") && strstr(snapshot, "\"actions\""),
        "new pages use both F1 semantics and F4 pagination");
    vtk_host_call(save_snapshot, NULL);
}
static void find_label_at(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) {
        lv_area_t a; lv_obj_update_layout(o); lv_obj_get_coords(o, &a);
        x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
        found = o;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_label_at(lv_obj_get_child(o, i));
}
static void find_label(void) { x = y = -1; found = NULL; find_label_at(lv_screen_active()); }
static bool fully_visible(void) {
    if (!found) return false;
    lv_area_t a, clip; lv_obj_get_coords(found, &a);
    for (lv_obj_t *p = lv_obj_get_parent(found); p; p = lv_obj_get_parent(p)) {
        lv_obj_get_coords(p, &clip);
        if (a.x1 < clip.x1 || a.x2 > clip.x2 || a.y1 < clip.y1 || a.y2 > clip.y2) {
            CHECK(false, "clipped %s: [%d,%d,%d,%d] outside [%d,%d,%d,%d]", wanted,
                (int)a.x1, (int)a.y1, (int)a.x2, (int)a.y2, (int)clip.x1, (int)clip.y1, (int)clip.x2, (int)clip.y2);
            return false;
        }
    }
    return true;
}
static bool row_has_icon(lv_obj_t *slot) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(slot); i++) {
        lv_obj_t *o = lv_obj_get_child(slot, i);
        if (lv_obj_get_width(o) == ink_ui_px(INK_CIRCLE_SMALL) && lv_obj_get_height(o) == ink_ui_px(INK_CIRCLE_SMALL) &&
            lv_color_eq(lv_obj_get_style_bg_color(o, 0), ink_ui_paper()) && lv_obj_get_child_count(o)) {
            CHECK(!lv_obj_get_style_border_width(o, 0) && !lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE),
                "settings presenters use the shared decorative circle");
            lv_obj_t *bg = lv_obj_get_child(slot, 0);
            bool joined = false, divided = false;
            for (uint32_t k = 0; k < lv_obj_get_child_count(bg); k++) {
                lv_obj_t *part = lv_obj_get_child(bg, (int32_t)k);
                if (lv_obj_get_y(part)) continue;
                if (lv_obj_get_width(part) == lv_obj_get_width(bg)) joined = true;
                if (lv_color_eq(lv_obj_get_style_bg_color(part, 0), ink_ui_ink()) &&
                    lv_obj_get_height(part) == ink_ui_px(INK_STROKE)) divided = true;
            }
            CHECK(joined == divided, "every joined settings row uses the shared divider, including later pages");
            if (joined) {
                lv_obj_t *parent = lv_obj_get_parent(slot);
                int bottom = 0;
                for (uint32_t k = 0; k < lv_obj_get_child_count(parent); k++) {
                    lv_obj_t *previous = lv_obj_get_child(parent, (int32_t)k);
                    if (!lv_obj_has_flag(previous, LV_OBJ_FLAG_HIDDEN) && lv_obj_get_y(previous) < lv_obj_get_y(slot))
                        bottom = LV_MAX(bottom, lv_obj_get_y(previous) + lv_obj_get_height(previous));
                }
                CHECK(bottom == lv_obj_get_y(slot), "joined rows have no full-width white gap under the shared divider");
            }
            return true;
        }
    }
    return false;
}
static void inspect_visible_page(void) {
    read_snapshot();
    const char *p = strstr(snapshot, "\"pagination\":");
    int pages, total;
    CHECK(p && sscanf(p, "\"pagination\":{\"page\":%d,\"pages\":%d,\"total\":%d,\"offset\":%d,\"visible\":%d",
        &page_number, &pages, &total, &page_offset, &page_visible) == 5, "pagination has real coordinates");
    p = strstr(snapshot, "\"fields\":[");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    char text[384];
    if (p && j) for (p += strlen("\"fields\":["); *p == '{'; ) {
        lv_obj_t *slot = NULL;
        const char *end = strstr(p, "}}");
        CHECK(end && !ink_json_parse(j, p, (size_t)(end - p) + 2), "visible field parses");
        if (!end) break;
        if (!ink_json_eq(j, ink_json_find(j, 0, "id"), "device") && !ink_json_eq(j, ink_json_find(j, 0, "id"), "update.version")) {
            ink_json_str(j, ink_json_find(j, 0, "label"), text, sizeof text);
            wanted = text; find_label();
            CHECK(fully_visible(), "field title is visible without clipping: %.200s", text);
            if (found) slot = lv_obj_get_parent(found);
            static const char *const GROUPED[] = {"voice.name", "voice.interrupt", "ai.records", "ai.memory", "text.assist", "backup",
                "storage.books", "storage.audio", "storage.photos", "storage.clean", "update.check", "privacy", "device.details", "licenses"};
            for (size_t k = 0; k < sizeof GROUPED / sizeof *GROUPED; k++)
                if (ink_json_eq(j, ink_json_find(j, 0, "id"), GROUPED[k]))
                    CHECK(slot && row_has_icon(slot) && ink_ui_surface_kind(lv_obj_get_child(slot, 0)) == INK_SURFACE_STATIC &&
                        lv_color_eq(lv_obj_get_style_bg_color(lv_obj_get_child(slot, 0), 0), ink_ui_paper()) &&
                        lv_obj_get_style_border_width(lv_obj_get_child(slot, 0), 0) == ink_ui_px(INK_STROKE),
                        "grouped row retains the design's icon and theme background: %s", GROUPED[k]);
        }
        if (!ink_json_eq(j, ink_json_find(j, 0, "role"), "setting")) {
            ink_json_str(j, ink_json_find(j, 0, "value"), text, sizeof text);
            wanted = text;
            if (slot) { x = y = -1; found = NULL; find_label_at(slot); } else find_label();
            CHECK(!text[0] || fully_visible(), "field value is visible: %.200s", text);
        }
        p = end + 2;
        if (*p == ',') p++;
    }
    osal_free(j);
}
static void inspect_layout(void) {
    inspect_visible_page();
    if (!strcmp(ink_ui_page_name(), "SetVoice")) {
        wanted = page_number == 1 ? "没锁屏时才听，不影响待机。" : "AI"; find_label();
        CHECK(fully_visible(), "voice preserves the design's explanation and AI section");
        CHECK(strstr(snapshot, wanted), "the AI snapshot includes the visible explanation or section");
        if (page_number == 2) { wanted = "打字"; find_label(); CHECK(fully_visible(), "voice preserves the typing section"); }
    } else if (!strcmp(ink_ui_page_name(), "SetStorage") && page_number == 1) {
        wanted = "空间"; find_label();
        CHECK(fully_visible() && lv_obj_get_x(found) == 0, "storage restores the space section at the page margin");
        wanted = "还能放 4.0 GB"; find_label();
        CHECK(fully_visible(), "storage presents the real capacity in readable units");
        CHECK(found && lv_obj_get_style_text_font(found, 0) == ink_ui_font_px(INK_FONT_LIST, INK_WEIGHT_REGULAR),
            "space summary uses the current theme's list text size");
        if (found) {
            lv_obj_t *bg = lv_obj_get_child(lv_obj_get_parent(found), 0);
            CHECK(lv_obj_get_style_border_width(bg, 0) == ink_ui_px(INK_STROKE), "space summary has the design's outline");
        }
    } else if (!strcmp(ink_ui_page_name(), "SetAbout") && page_number == 1) {
        char name[96]; snprintf(name, sizeof name, "%s的设备", ink_member_current()->name);
        wanted = name; find_label();
        CHECK(fully_visible() && x == (INK_CANVAS_W - 1) / 2 && lv_obj_get_style_text_align(found, 0) == LV_TEXT_ALIGN_CENTER,
            "about centers the live device name");
        if (found) {
            lv_obj_t *slot = lv_obj_get_parent(found); bool logo = false;
            for (uint32_t i = 0; i < lv_obj_get_child_count(slot); i++) {
                lv_obj_t *o = lv_obj_get_child(slot, i);
                if (lv_obj_get_width(o) == ink_ui_px(INK_CIRCLE_APP) && lv_obj_get_height(o) == ink_ui_px(INK_CIRCLE_APP) &&
                    lv_color_eq(lv_obj_get_style_bg_color(o, 0), ink_ui_ink())) logo = true;
            }
            CHECK(logo, "about restores the centered black identity mark");
        }
    } else if (!strcmp(ink_ui_page_name(), "SetPrivacy") && page_number == 1) {
        wanted = "你的东西放在哪"; find_label();
        CHECK(fully_visible() && lv_obj_get_style_text_font(found, 0) == ink_ui_font_px(INK_FONT_SUBTITLE, INK_WEIGHT_MEDIUM),
            "privacy uses paragraph headings from the current theme");
        if (found) {
            lv_area_t a; lv_obj_get_coords(found, &a);
            CHECK(a.x1 == INK_MARGIN, "privacy paragraphs start at the page margin without a second card inset");
            CHECK(lv_obj_get_style_bg_opa(lv_obj_get_parent(found), 0) == LV_OPA_TRANSP,
                "privacy paragraphs have no card wrapper");
        }
    }
    if (strstr(snapshot, "\"id\":\"reset\"") || strstr(snapshot, "\"id\":\"privacy.records\"")) {
        wanted = !strcmp(ink_ui_page_name(), "SetAbout") ? "清空这台设备" : "查看和删除记录"; find_label();
        CHECK(fully_visible() && y > INK_CANVAS_H - 3 * INK_CONTROL, "the final action sits at the bottom above pagination");
        if (found) CHECK(lv_obj_get_style_border_width(lv_obj_get_child(lv_obj_get_parent(found), 0), 0) ==
            (!strcmp(ink_ui_page_name(), "SetAbout") ? 0 : ink_ui_px(INK_STROKE)),
            "about uses a text footer and privacy uses an outlined footer");
    }
}
static char frame_name[80];
static volatile bool captured;
static void capture_frame(void *arg) { sim_display_snapshot(arg); captured = true; }
static void inspect_all_layouts(void) {
    for (int font = 1; font <= 3; font += 2) {
        char args[80]; snprintf(args, sizeof args, "{\"id\":\"display.font_size\",\"value\":%d}", font);
        CHECK(AI("settings.set", args) && g_rc.status == INK_RC_DONE, "set the real font size for layout checks"); quiet();
        for (int i = 0; i < 4; i++) {
            CHECK(AI(CAPS[i], NULL), "open every layout at both text sizes"); quiet();
            for (int n = 0; n < 4; n++) {
                on_ui(inspect_layout);
                snprintf(frame_name, sizeof frame_name, "font%d-%s-%d", font, PAGES[i], page_number);
                captured = false; vtk_event_at(vtk_now() + SIM_MS(1), capture_frame, frame_name);
                CHECK(WAIT_FOR(captured, 1000), "capture the settled layout page");
                if (!strstr(snapshot, "\"id\":\"next\"")) break;
                CHECK(AI("shell.settings_next", NULL) && g_rc.status == INK_RC_DONE, "visit every F4 layout page"); quiet();
            }
            CHECK(BY_HAND("system.home", NULL), "close the inspected layout"); quiet();
        }
    }
    CHECK(AI("settings.set", "{\"id\":\"display.font_size\",\"value\":1}"), "restore the default layout size"); quiet();
}
static void inspect_second_page(void) {
    inspect_visible_page();
    CHECK(page_number == 2 && page_offset == first_visible && strstr(snapshot, "\"id\":\"ai.memory\""),
        "next actually displays the second voice page and its memory field");
}
static void inspect_first_page(void) {
    inspect_visible_page();
    CHECK(page_number == 1 && page_offset == 0 && strstr(snapshot, "voice.handsfree"),
        "the physical previous key restores the first F4 page");
}
static void outage_event(void *arg) { (void)arg; sim_net_outage(SIM_MS(60000)); }
static void inspect_backup_started(void) {
    read_snapshot();
    CHECK(strstr(snapshot, "备份或恢复正在进行") && !strstr(snapshot, "还没有备份"),
        "accepted backup refreshes the visible page and its AI snapshot");
    char label[160], automatic[64];
    int mode = (int)ink_setting_get("backup.auto");
    snprintf(automatic, sizeof automatic, "自动备份：%s", mode == 1 ? "每天" : mode == 2 ? "每周" : "关");
    snprintf(label, sizeof label, "备份或恢复正在进行 · %s", automatic);
    wanted = label; find_label();
    CHECK(x >= 0 && y >= 0, "the running backup is visible on screen");
}
static void press(const char *label) {
    wanted = label; on_ui(find_label);
    for (int i = 0; x < 0 && i < 3; i++) {
        CHECK(AI("shell.settings_next", NULL), "find the control on a later page"); quiet();
        wanted = label; on_ui(find_label);
    }
    CHECK(x >= 0 && y >= 0, "visible button: %s", label);
    if (x >= 0 && y >= 0) tap(x, y);
    quiet();
}
static void open_direct(void) { ink_shell_open_page(opening); }
static void inspect_backend(void) {
    read_snapshot();
    CHECK(strstr(snapshot, ink_ui_page_name()), "snapshot names its real activity");
    if (!strcmp(ink_ui_page_name(), "SetVoice"))
        CHECK(strstr(snapshot, "voice.handsfree") && strstr(snapshot, "settings.set"), "voice exposes the existing setting capability");
    if (!strcmp(ink_ui_page_name(), "SetStorage")) {
        int64_t bytes = -1;
        CHECK(ink_state_get_int(ink_state_find("storage.free"), &bytes), "storage backend has a real value");
        const char *expected = bytes == 4321 * 1024 ? "还能放 4.2 MB" : "还能放 4.0 GB";
        CHECK(strstr(snapshot, expected), "storage page reads the real service rather than design example sizes");
    }
    if (!strcmp(ink_ui_page_name(), "SetAbout")) {
        char version[64];
        CHECK(ink_state_get_str(ink_state_find("update.version"), version, sizeof version) > 0 && strstr(snapshot, version),
            "about reads the update service's current version");
    }
    if (!strcmp(ink_ui_page_name(), "SetPrivacy"))
        CHECK(strstr(snapshot, "密码框里的字看不到") && strstr(snapshot, "privacy.storage"), "privacy exports its visible explanation");
}
static void extract_action(void) {
    read_snapshot();
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    const char *actions = strstr(snapshot, "\"actions\":[");
    const char *start = actions ? strstr(actions, "{\"id\":\"voice.handsfree\"") : NULL;
    const char *end = start ? strstr(start, "}}") : NULL;
    size_t len = end ? (size_t)(end - start) + 2 : 0;
    CHECK(j && len && !ink_json_parse(j, start, len), "the voice action fits the device JSON parser");
    bound[0] = 0;
    if (j && len) {
        CHECK(ink_json_eq(j, ink_json_find(j, 0, "capability"), "settings.set"), "touch binding and AI both target settings.set");
        CHECK(ink_json_str(j, ink_json_find(j, 0, "args_json"), bound, sizeof bound) > 0, "AI gets the same setting arguments");
    }
    osal_free(j);
    CHECK(bound[0], "the setting action is present");
}
static void backend_changes(void) {
    ink_state_set_int(ink_state_find("storage.free"), 4321 * 1024);
    ink_state_set_str(ink_state_find("update.version"), "0.0.7-test");
}
static void unavailable_card(void) {
    ink_state_set_bool(ink_state_find("storage.card"), false);
    ink_state_set_str(ink_state_find("storage.mode"), "usb");
}
static void check_no_card(void) {
    read_snapshot(); CHECK(strstr(snapshot, "存储卡正借给电脑"), "unavailable storage never displays a stale free-space number");
}
static void check_hidden(void) {
    CHECK(!strcmp(ink_ui_page_name(), "home"), "child cannot open a private settings page through the direct presenter path");
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home(); quiet();
    if (restarting) {
        CHECK(ink_setting_get("voice.handsfree") == 2, "voice setting survives a fresh process and persisted flash");
        CHECK(ink_setting_get("update.auto_download") == 1, "about setting survives restart");
        CHECK(ink_setting_get("ai.read_clipboard") == 1, "confirmed privacy setting survives restart");
    } else {
        for (int i = 0; i < 4; i++) {
            CHECK(AI(CAPS[i], NULL) && g_rc.status == INK_RC_DONE, "AI opens %s", PAGES[i]); quiet();
            CHECK(!strcmp(ink_ui_page_name(), PAGES[i]), "the requested real page opens"); on_ui(inspect_backend);
            CHECK(BY_HAND("shell.back", NULL) && g_rc.status == INK_RC_DONE, "back closes only one page"); quiet();
        }
        inspect_all_layouts();
        CHECK(AI("shell.voice", NULL), "open voice controls"); quiet(); press("什么时候能叫名字");
        CHECK(ink_setting_get("voice.handsfree") == 1, "a real touch uses settings.set");
        on_ui(extract_action);
        CHECK(AI("settings.set", bound) && g_rc.status == INK_RC_DONE && ink_setting_get("voice.handsfree") == 2,
            "AI executes the exact action exported for the same control"); quiet();
        press("使用时"); CHECK(ink_setting_get("voice.handsfree") == 0, "the segment button uses the same settings capability");
        CHECK(AI("settings.set", "{\"id\":\"voice.handsfree\",\"value\":2}") && g_rc.status == INK_RC_DONE, "restore the persisted voice value"); quiet();
        on_ui(inspect_visible_page); first_visible = page_visible;
        CHECK(AI("shell.settings_next", NULL) && g_rc.status == INK_RC_DONE, "AI uses the F4 next capability"); quiet();
        on_ui(inspect_second_page);
        key(INK_KEY_PREV); quiet(); on_ui(inspect_first_page);
        CHECK(AI("settings.set", "{\"id\":\"display.font_size\",\"value\":3}"), "use the largest text setting"); quiet();
        CHECK(AI("shell.voice", NULL), "reopen voice at the largest text size"); quiet();
        on_ui(inspect_visible_page); first_visible = page_visible;
        CHECK(AI("shell.settings_next", NULL), "turn voice at the largest text size"); quiet(); on_ui(inspect_second_page);
        key(INK_KEY_PREV); quiet(); on_ui(inspect_first_page);
        CHECK(AI("settings.set", "{\"id\":\"display.font_size\",\"value\":1}"), "restore normal text size"); quiet();
        CHECK(BY_HAND("system.home", NULL), "close voice before opening about");
        CHECK(AI("shell.about", NULL), "open about controls"); quiet(); press("自动下载新版本");
        CHECK(ink_setting_get("update.auto_download") == 1, "about toggle reaches the existing backend");
        CHECK(AI("shell.privacy", NULL), "open privacy controls"); quiet(); press("AI 可以读剪贴板");
        ink_confirm_view_t confirmation;
        CHECK(ink_confirm_list(&confirmation, 1) == 1 && !strcmp(confirmation.cap, "settings.set") &&
            ink_setting_get("ai.read_clipboard") == 0, "privacy permission waits for the existing human confirmation");
        CHECK(ink_confirm_decide(confirmation.token, true, INK_SRC_TOUCH) == 0, "a person confirms the permission");
        CHECK(WAIT_FOR(ink_setting_get("ai.read_clipboard") == 1, 5000), "confirmed privacy toggle reaches its backend"); quiet();
        on_ui(backend_changes);
        CHECK(BY_HAND("system.home", NULL), "close privacy");
        CHECK(AI("shell.storage", NULL), "open updated storage"); quiet(); on_ui(inspect_backend);
        vtk_event_at(vtk_now() + SIM_MS(1), outage_event, NULL);
        CHECK(WAIT_FOR(!ink_hub_online(), 10000), "ordinary network outage before backup");
        press("备份");
        CHECK(!strcmp(ink_ui_page_name(), "Backup"), "storage row opens the backup page");
        CHECK(AI("backup.status", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"phase\":\"none\""),
            "opening backup does not accept a backup task");
        CHECK(BY_HAND("shell.back", NULL) && g_rc.status == INK_RC_DONE, "return to storage before observing backup events"); quiet();
        CHECK(AI("backup.now", NULL) && g_rc.status == INK_RC_QUEUED, "AI starts a backup while storage stays visible");
        CHECK(AI("backup.status", NULL) && g_rc.status == INK_RC_DONE && !strstr(g_result, "\"phase\":\"none\""),
            "the real backup service accepted the task");
        quiet(); on_ui(inspect_backup_started);
        on_ui(unavailable_card); quiet(); on_ui(check_no_card);
        CHECK(AI("shell.about", NULL), "open updated version"); quiet(); on_ui(inspect_backend);
        CHECK(BY_HAND("system.home", NULL), "return home before switching members"); quiet();
        CHECK(WAIT_FOR(ink_storage_pending() == 0, 10000), "writes settle before member tests");
        CHECK(ink_member_switch("kid") == 0, "enter the child member space"); quiet();
        for (int i = 0; i < 4; i++) {
            CHECK(AI(CAPS[i], NULL) && g_rc.status == INK_RC_REJECTED, "child AI cannot open %s", PAGES[i]);
            opening = PAGES[i]; on_ui(open_direct); quiet(); on_ui(check_hidden);
            on_ui(ink_ui_home); quiet();
        }
        CHECK(AI("settings.set", "{\"id\":\"voice.handsfree\",\"value\":0}") && g_rc.status == INK_RC_REJECTED,
            "child cannot write the adult voice setting via AI");
        CHECK(BY_HAND("settings.set", "{\"id\":\"update.auto_download\",\"value\":0}") && g_rc.status == INK_RC_REJECTED,
            "child cannot write the adult about setting via touch");
        CHECK(AI("settings.set", "{\"id\":\"ai.read_clipboard\",\"value\":0}") && g_rc.status == INK_RC_REJECTED,
            "child cannot change privacy permission");
        CHECK(ink_setting_get("voice.handsfree") == 2 && ink_setting_get("update.auto_download") == 1 && ink_setting_get("ai.read_clipboard") == 1,
            "child attempts leave all adult settings unchanged");
    }
    CHECK(WAIT_FOR(ink_storage_pending() == 0, 10000), "all accepted writes finish");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    restarting = getenv("INK_SETTINGS_RESTART") != NULL;
    product_main();
    osal_task_cfg_t cfg = {.name = "settings-a-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}
