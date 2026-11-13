/* Additional keyboard pages use the actual pointer driver and the same AI capabilities. */
static lv_obj_t *pages_found;
static const char *pages_label;
static bool pages_down;
static void pages_pointer_state(void) {
    pages_down = false;
    for (lv_indev_t *d = lv_indev_get_next(NULL); d; d = lv_indev_get_next(d))
        if (lv_indev_get_type(d) == LV_INDEV_TYPE_POINTER && lv_indev_get_state(d) == LV_INDEV_STATE_PRESSED) pages_down = true;
}
static bool pages_pointer_is(bool down) { on_ui(pages_pointer_state); return pages_down == down; }
static void pages_tap(void) {
    hw(HW_PRESS, pointer_x, pointer_y);
    CHECK(WAIT_FOR(pages_pointer_is(true), 1000), "LVGL received the pointer press");
    hw(HW_RELEASE, pointer_x, pointer_y);
    CHECK(WAIT_FOR(pages_pointer_is(false), 1000), "LVGL received the pointer release");
}
static void pages_key(int code) { pointer_code = code; on_ui(pointer_target); pages_tap(); }
static void pages_find(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), pages_label)) pages_found = lv_obj_get_parent(o);
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) pages_find(lv_obj_get_child(o, (int)i));
}
static void pages_target(void) {
    pages_found = NULL; lv_obj_update_layout(K.root); pages_find(K.root);
    CHECK(pages_found, "page control exists: %s", pages_label);
    if (pages_found) {
        lv_area_t a; lv_obj_get_coords(pages_found, &a);
        pointer_x = (a.x1 + a.x2) / 2; pointer_y = (a.y1 + a.y2) / 2;
    }
}
static void pages_press(const char *text) {
    CHECK(WAIT_FOR(settled(), 15000), "page settles before touch");
    pages_label = text; on_ui(pages_target);
    double delivered = sim_metric_value("input.touch_delivery_ms");
    if (pages_found) pages_tap();
    CHECK(WAIT_FOR(sim_metric_value("input.touch_delivery_ms") > delivered, 1000), "driver received page touch: %s", text);
    CHECK(WAIT_FOR(settled(), 15000), "page settles after touch");
}
static void pages_call(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "AI page action: %s (%d)", cap, g_rc.status);
    CHECK(WAIT_FOR(settled(), 15000), "AI page settles");
}
static void pages_geometry(lv_obj_t *row) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(row); i++) {
        lv_obj_t *o = lv_obj_get_child(row, (int)i);
        if (lv_obj_is_visible(o)) CHECK(lv_obj_get_width(o) >= 108 && lv_obj_get_height(o) >= 108, "page targets meet 108: %d x %d", (int)lv_obj_get_width(o), (int)lv_obj_get_height(o));
    }
}
static void pages_inspect(void) {
    CHECK(ink_ui_context_snapshot(snapshot, sizeof snapshot, NULL) > 0, "page fits AI context");
    CHECK(strstr(snapshot, "keyboard.panel") && strstr(snapshot, "候选页="), "snapshot describes page and actions");
    lv_obj_update_layout(K.root);
    if (K.expanded) {
        CHECK(!lv_obj_is_visible(K.board), "expanded candidates replace the keys");
        for (int i = 0; i < INK_TEXT_PAGE; i++) if (lv_obj_is_visible(K.cand_btn[i]))
            CHECK(lv_obj_get_width(K.cand_btn[i]) >= 108 && lv_obj_get_height(K.cand_btn[i]) >= 108, "grid cell meets minimum");
        CHECK(lv_obj_get_width(K.more) >= 108 && lv_obj_get_height(K.more) >= 108, "candidate paging meets minimum");
    }
    if (K.symbols) { pages_geometry(K.tabs); pages_geometry(K.symbol_grid); pages_geometry(K.picker_nav); }
    if (K.popup) pages_geometry(K.popup);
}
static void pages_capture(const char *name) {
    CHECK(WAIT_FOR(settled(), 15000), "acceptance frame settles");
    on_ui(pages_inspect);
    captured = false; vtk_event_at(vtk_now() + SIM_US(1), frame, (void *)name);
    CHECK(WAIT_FOR(captured, 2000), "acceptance screenshot: %s", name);
}
static void pages_setup(void) {
    ink_receipt_t rc; ink_setting_set("text.nine_key", 0, INK_SRC_TOUCH, &rc);
    ink_cap_invoke_json("keyboard.panel", "{\"panel\":\"letters\"}", INK_SRC_TOUCH, 0, &rc);
    if (ink_text_english()) touch(INK_TEXT_KEY_LANGUAGE);
    ink_text_select(0, ink_text_char_offset(ink_text_value(), strlen(ink_text_value()))); ink_text_backspace();
}
static bool pages_durable;
static void pages_read_durable(void) { pages_durable = ink_text_draft_saved() != 0; }
static bool pages_ready(void) {
    on_ui(pages_read_durable);
    return pages_durable && settled() && !ink_storage_pending() && ink_db_idle();
}
static ink_text_cand_t pages_candidates[60];
static int pages_china;
static void pages_check_hello(void) {
    int n = ink_text_candidates(0, pages_candidates, 60); bool hello = false;
    for (int i = 0; i < n; i++) if (!strcmp(pages_candidates[i].word, "你好")) hello = true;
    CHECK(hello && !strcmp(ink_text_composing(), "nihao"), "nihao offers 你好");
}
static void pages_check_page(void) {
    CHECK(K.expanded && K.first == INK_TEXT_PAGE, "candidate page two");
}
static void pages_check_picked(void) { CHECK(!K.expanded && ink_text_value()[0], "picking commits and automatically folds candidates"); }
static void pages_check_recent(void) {
    char text[8]; int id = recent_id(0) - 1;
    CHECK(id >= 0 && symbol_at(id / 256, id % 256, text) && !strcmp(text, "≈"), "recent symbols start with ≈");
    CHECK(strstr(ink_text_value(), "≈"), "math symbol committed");
}
static void pages_phone(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PHONE}; ink_text_focus(&cfg, NULL, NULL);
    CHECK(K.built_for == 3 && labeled('1', "1"), "phone field automatically opens the number pad");
}
static void pages_plain(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN}; ink_text_focus(&cfg, NULL, NULL);
    if (!ink_text_english()) touch(INK_TEXT_KEY_LANGUAGE);
}
static void pages_check_popup(void) { CHECK(K.popup && !strcmp(K.alternate, "@"), "holding a opens its upstream alternate"); }
static void pages_check_alternate(void) { CHECK(!K.popup && !strcmp(ink_text_value(), "@"), "alternate commits without the original a"); }
static void pages_nine(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN}; ink_text_focus(&cfg, NULL, NULL);
    if (ink_text_english()) touch(INK_TEXT_KEY_LANGUAGE);
    ink_receipt_t rc; ink_setting_set("text.nine_key", 1, INK_SRC_TOUCH, &rc); sync();
}
static void pages_find_china(void) {
    int n = ink_text_candidates(0, pages_candidates, 60); pages_china = -1;
    for (int i = 0; i < n; i++) if (!strcmp(pages_candidates[i].word, "中国")) pages_china = i;
    CHECK(pages_china >= 0, "94664 has 中国");
    CHECK(!strstr(lv_label_get_text(K.composing), "94664"), "internal T9 digits are not printed");
}
static void pages_check_china(void) { CHECK(!strcmp(ink_text_value(), "中国"), "nine-key selection commits 中国"); }
static void pages_track(void) {
    lv_obj_update_layout(K.root); lv_area_t a; lv_obj_get_coords(K.board, &a);
    hal_panel_info_t p; hal_display_get_info(&p); int x0, y0, x1, y1;
    ink_logical_to_native(p.width, p.height, p.rotation_deg, a.x1, a.y1, &x0, &y0);
    ink_logical_to_native(p.width, p.height, p.rotation_deg, a.x2, a.y2, &x1, &y1);
    keyboard_rows = (hal_rect_t){x0 < x1 ? x0 : x1, y0 < y1 ? y0 : y1, abs(x1 - x0) + 1, abs(y1 - y0) + 1};
}
static void pages_input_setup(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN}; ink_text_focus(&cfg, NULL, NULL); pages_setup();
}
static void pages_delete_setup(void) {
    pages_plain(); ink_text_commit_utf8("abcdefghijklmnopqrstuvwxyz");
}
static size_t pages_length;
static void pages_read_length(void) { pages_length = strlen(ink_text_value()); }
static void pages_password(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PASSWORD}; ink_text_focus(&cfg, NULL, NULL);
    ink_text_key('s'); ink_text_key('e'); ink_text_key('c');
    ink_receipt_t rc;
    CHECK(ink_cap_invoke_json("keyboard.symbol", "{\"text\":\"≈\"}", INK_SRC_TOUCH, 0, &rc) == INK_RC_DONE, "human may type a password symbol");
    CHECK(ink_cap_invoke_json("keyboard.symbol", "{\"text\":\"≈\"}", INK_SRC_AI, 0, &rc) == INK_RC_REJECTED, "AI cannot type password symbols");
    CHECK(!lv_obj_is_visible(K.cands), "password has no candidates");
    CHECK(ink_ui_context_snapshot(snapshot, sizeof snapshot, NULL) > 0 && !strstr(snapshot, "sec") && !strstr(snapshot, "≈"), "password snapshot has no content or recent symbols");
}
static void keyboard_pages(void) {
    for (int ai = 0; ai < 2; ai++) {
        CHECK(BY_HAND("memo.edit", NULL) && WAIT_FOR(ink_text_focused(), 10000), "open real memo"); on_ui(pages_setup);
        CHECK(WAIT_FOR(pages_ready(), 15000), "memo setup and initial draft write settle before typing");
        for (const char *p = "nihao"; *p; p++) {
            if (ai) { char args[40]; snprintf(args, sizeof args, "{\"code\":%d}", *p); pages_call("keyboard.key", args); }
            else { pages_key(*p); CHECK(WAIT_FOR(settled(), 15000), "pinyin key settles"); }
        }
        on_ui(pages_check_hello); pages_capture(ai ? "AI-01-nihao" : "Touch-01-nihao");
        if (ai) pages_call("keyboard.candidates", "{}"); else pages_press("▾");
        pages_capture(ai ? "AI-02-grid" : "Touch-02-grid");
        if (ai) pages_call("keyboard.page", "{\"direction\":1}"); else pages_press("▶");
        on_ui(pages_check_page); pages_capture(ai ? "AI-03-page2" : "Touch-03-page2");
        if (ai) pages_call("keyboard.pick", "{\"index\":6}"); else {
            const char *word = pages_candidates[6].word; pages_press(word);
        }
        on_ui(pages_check_picked); pages_capture(ai ? "AI-04-picked" : "Touch-04-picked");
        if (ai) pages_call("keyboard.panel", "{\"panel\":\"symbols\"}"); else { pages_key(KEY_SWITCH); CHECK(WAIT_FOR(settled(), 15000), "symbol panel settles"); }
        pages_capture(ai ? "AI-05-symbols" : "Touch-05-symbols");
        if (ai) pages_call("keyboard.category", "{\"category\":1}"); else pages_press(TR("数学"));
        pages_capture(ai ? "AI-06-math" : "Touch-06-math");
        if (ai) pages_call("keyboard.symbol_page", "{\"direction\":1}"); else pages_press("▶");
        pages_capture(ai ? "AI-07-math2" : "Touch-07-math2");
        if (ai) pages_call("keyboard.symbol_page", "{\"direction\":-1}"); else pages_press("◀");
        if (ai) pages_call("keyboard.symbol", "{\"text\":\"≈\"}"); else pages_press("≈");
        on_ui(pages_check_recent); pages_capture(ai ? "AI-08-recent" : "Touch-08-recent");
        if (ai) pages_call("keyboard.panel", "{\"panel\":\"letters\"}"); else pages_press("26");
        pages_capture(ai ? "AI-09-letters" : "Touch-09-letters");
        on_ui(pages_phone); pages_capture(ai ? "AI-10-phone" : "Touch-10-phone");
        on_ui(pages_plain); CHECK(WAIT_FOR(settled(), 15000), "plain input settles");
        if (ai) pages_call("keyboard.alternates", "{\"code\":97}"); else {
            pointer_code = 'a'; on_ui(pointer_target); hw(HW_PRESS, pointer_x, pointer_y); delay_ms(700); hw(HW_RELEASE, pointer_x, pointer_y);
        }
        on_ui(pages_check_popup); pages_capture(ai ? "AI-11-alternate" : "Touch-11-alternate");
        if (ai) pages_call("keyboard.symbol", "{\"text\":\"@\"}"); else pages_press("@");
        on_ui(pages_check_alternate); pages_capture(ai ? "AI-12-alternate-picked" : "Touch-12-alternate-picked");
        on_ui(pages_nine); CHECK(WAIT_FOR(settled(), 15000), "nine-key input settles");
        for (const char *p = "94664"; *p; p++) {
            if (ai) { char args[40]; snprintf(args, sizeof args, "{\"code\":%d}", *p); pages_call("keyboard.key", args); }
            else { pages_key(*p); CHECK(WAIT_FOR(settled(), 15000), "nine-key key settles"); }
        }
        on_ui(pages_find_china); pages_capture(ai ? "AI-13-nine" : "Touch-13-nine");
        if (ai) { char args[40]; snprintf(args, sizeof args, "{\"index\":%d}", pages_china); pages_call("keyboard.pick", args); }
        else {
            pages_press("▾");
            for (int i = 0; i < pages_china / INK_TEXT_PAGE; i++) pages_press("▶");
            pages_press("中国");
        }
        on_ui(pages_check_china); pages_capture(ai ? "AI-14-china" : "Touch-14-china");
        on_ui(end_scene); CHECK(BY_HAND("system.home", NULL) && WAIT_FOR(!ink_text_focused(), 10000), "leave memo");
    }
    on_ui(pages_input_setup); CHECK(WAIT_FOR(settled(), 15000), "refresh setup settles"); on_ui(pages_track);
    keyboard_updates = keyboard_outside = keyboard_full = 0; keyboard_tracking = true;
    for (const char *p = "nihaozhongg"; *p; p++) { pages_key(*p); CHECK(WAIT_FOR(settled(), 15000), "local key settles"); }
    keyboard_tracking = false;
    CHECK(keyboard_updates && !keyboard_outside && !keyboard_full, "letters only refresh composition and candidates: updates=%u keys=%u full=%u", keyboard_updates, keyboard_outside, keyboard_full);
    keyboard_tracking = true; pages_key(KEY_SWITCH); CHECK(WAIT_FOR(settled(), 15000), "layout refresh settles"); keyboard_tracking = false;
    CHECK(keyboard_outside, "layout switch refreshes the keys area");
    pages_call("keyboard.panel", "{\"panel\":\"letters\"}"); on_ui(pages_delete_setup); CHECK(WAIT_FOR(settled(), 15000), "deletion setup settles");
    on_ui(pages_read_length); size_t before = pages_length;
    pointer_code = KEY_BACKSPACE; on_ui(pointer_target); hw(HW_PRESS, pointer_x, pointer_y); delay_ms(1100); hw(HW_RELEASE, pointer_x, pointer_y);
    on_ui(pages_read_length); CHECK(pages_length + 1 < before, "held delete repeats"); size_t after = pages_length;
    delay_ms(500); on_ui(pages_read_length); CHECK(pages_length == after, "delete stops after release");
    hw(HW_PRESS, pointer_x, pointer_y); delay_ms(650); hw(HW_PRESS, 0, 0); on_ui(pages_read_length); after = pages_length;
    delay_ms(500); on_ui(pages_read_length); CHECK(pages_length == after, "delete stops after leaving the key"); hw(HW_RELEASE, 0, 0);
    int64_t recent0 = ink_setting_get(RECENT_SETTINGS[0]), recent1 = ink_setting_get(RECENT_SETTINGS[1]);
    on_ui(pages_password);
    CHECK(ink_setting_get(RECENT_SETTINGS[0]) == recent0 && ink_setting_get(RECENT_SETTINGS[1]) == recent1, "password does not learn recent symbols");
    on_ui(end_scene);
    CHECK(ink_member_switch("kid") == 0 && WAIT_FOR(!ink_text_focused(), 5000), "switch to child for recent isolation");
    CHECK(!ink_setting_get(RECENT_SETTINGS[0]) && !ink_setting_get(RECENT_SETTINGS[1]), "child has separate recent symbols");
    CHECK(ink_member_switch("owner") == 0 && WAIT_FOR(!ink_text_focused(), 5000), "restore owner");
    CHECK(ink_setting_get(RECENT_SETTINGS[0]) == recent0 && ink_setting_get(RECENT_SETTINGS[1]) == recent1, "owner's symbols return");
}
