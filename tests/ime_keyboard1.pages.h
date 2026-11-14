/* keyboard-2 acceptance walk (brief 验收 1): fcitx5-android and Mogo use, by real touches through the touch driver, then
 * by the AI's capability calls; every step is checked and photographed, and both rounds end the same. */
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
    double delivered = sim_metric_value("input.touch_delivery_ms");
    hw(HW_PRESS, pointer_x, pointer_y);
    CHECK(WAIT_FOR(pages_pointer_is(true), 1000), "LVGL received the pointer press");
    hw(HW_RELEASE, pointer_x, pointer_y);
    CHECK(WAIT_FOR(pages_pointer_is(false), 1000), "LVGL received the pointer release");
    CHECK(WAIT_FOR(sim_metric_value("input.touch_delivery_ms") > delivered, 1000), "the touch driver delivered the tap");
    CHECK(WAIT_FOR(settled(), 15000), "the tap settles");
}
static void pages_key(int code) { pointer_code = code; on_ui(pointer_target); pages_tap(); }
static void pages_find(lv_obj_t *o) {
    if (!lv_obj_is_visible(o) || pages_found) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), pages_label)) pages_found = lv_obj_get_parent(o);
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) pages_find(lv_obj_get_child(o, (int)i));
}
static void pages_target(void) {
    pages_found = NULL; lv_obj_update_layout(K.root);
    if (K.popup) pages_find(K.popup); /* what is on top first */
    pages_find(K.root);
    CHECK(pages_found, "the control is on the keyboard: %s", pages_label);
    if (pages_found) {
        lv_area_t a; lv_obj_get_coords(pages_found, &a);
        pointer_x = (a.x1 + a.x2) / 2; pointer_y = (a.y1 + a.y2) / 2;
    }
}
static void pages_press(const char *text) {
    CHECK(WAIT_FOR(settled(), 15000), "page settles before touch");
    pages_label = text; on_ui(pages_target);
    if (pages_found) pages_tap();
}
static void pages_call(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "AI page action: %s %s (%d)", cap, args ? args : "", g_rc.status);
    CHECK(WAIT_FOR(settled(), 15000), "AI page settles");
}
static void pages_ai_key(int code) { char args[40]; snprintf(args, sizeof args, "{\"code\":%d}", code); pages_call("keyboard.key", args); }
static void pages_geometry(lv_obj_t *row) {
    for (uint32_t i = 0; row && i < lv_obj_get_child_count(row); i++) {
        lv_obj_t *o = lv_obj_get_child(row, (int)i);
        if (lv_obj_is_visible(o) && lv_obj_check_type(o, &lv_button_class))
            CHECK(lv_obj_get_width(o) >= 108 && lv_obj_get_height(o) >= 108, "touch targets meet 108: %d x %d", (int)lv_obj_get_width(o), (int)lv_obj_get_height(o));
    }
}
static void pages_inspect(void) {
    CHECK(ink_ui_context_snapshot(snapshot, sizeof snapshot, NULL) > 0, "page fits AI context");
    CHECK(strstr(snapshot, "keyboard.panel") && strstr(snapshot, "候选页="), "snapshot describes page and actions");
    lv_obj_update_layout(K.root);
    if (K.expanded) {
        CHECK(!lv_obj_is_visible(K.board), "expanded candidates replace the keys");
        pages_geometry(K.cands);
    }
    if (K.symbols) { pages_geometry(K.tabs); pages_geometry(K.symbol_grid); pages_geometry(K.picker_nav); }
    pages_geometry(K.popup);
    for (int i = 0; K.readings && i < 3; i++) /* reading cells are keys: the key height (96 or 108 is the user's choice) */
        CHECK(lv_obj_get_height(K.reading[i]) == ink_ui_px(key_height(false)), "a reading cell is one key tall");
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
static int pages_index;
static char pages_word[2][40], pages_value[2][256];
static int pages_find_word(const char *word) {
    int n = ink_text_candidates(0, pages_candidates, 60);
    for (int i = 0; i < n; i++) if (!strcmp(pages_candidates[i].word, word)) return i;
    return -1;
}
static void pages_check_hello(void) {
    CHECK(pages_find_word("你好") >= 0 && !strcmp(ink_text_composing(), "nihao"), "1. nihao offers 你好");
    CHECK(lv_obj_is_visible(K.cand_btn[0]) && !strcmp(lv_label_get_text(K.cand_label[0]), K.cand[0].word), "1. the candidate bar shows them");
}
static int pages_ai;
static void pages_check_page(void) {
    CHECK(K.expanded && K.first == INK_TEXT_PAGE && !strncmp(lv_label_get_text(K.cand_page), "2/", 2), "2. candidate page two says 2/…: %s", lv_label_get_text(K.cand_page));
    if (!pages_ai) snprintf(pages_word[0], sizeof pages_word[0], "%s", lv_label_get_text(K.cand_label[0]));
    pages_index = pages_find_word(pages_word[0]);
}
static void pages_check_picked(void) {
    CHECK(!K.expanded && strstr(ink_text_value(), pages_word[0]), "2. picking %s commits it and folds the grid", pages_word[0]);
}
static void pages_check_symbols(void) { CHECK(K.symbols && K.category == 1, "3. the symbol panel opens on 中文 (%d)", K.category); }
static void pages_check_math(int page) { CHECK(K.category == 3 && K.symbol_page == page, "3. 数学, page %d", page + 1); }
static void pages_check_math_1(void) { pages_check_math(0); }
static void pages_check_math_2(void) { pages_check_math(1); }
static void pages_check_recent(void) {
    CHECK(K.category == RECENT && symbol_at(RECENT, 0) && !strcmp(symbol_at(RECENT, 0), "≈"), "4. 最近 starts with ≈");
    CHECK(strstr(ink_text_value(), "≈"), "3. ≈ was typed");
}
static void pages_check_letters(void) { CHECK(!K.symbols && (K.built_for == 1), "5. back to the letters"); }
static void pages_phone(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PHONE}; ink_text_focus(&cfg, NULL, NULL);
    bool only = true;
    for (int i = 0; i < K.buttons; i++) if (K.codes[i] && !strchr("0123456789+*#\b\n", K.codes[i])) only = false;
    CHECK(K.built_for == 3 && labeled('1', "1") && only && key_index(KEY_SWITCH) < 0 && key_index(KEY_NUMBER) < 0,
          "6. a phone field gets the number pad with only what it takes (no 字母, 符号 or dead keys)");
}
static void pages_plain(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN}; ink_text_focus(&cfg, NULL, NULL);
    if (!ink_text_english()) touch(INK_TEXT_KEY_LANGUAGE);
}
static void pages_check_popup(void) {
    char text[16]; int n = 0;
    while (alternate(K.popup_letter, n, text)) n++;
    CHECK(K.popup && K.popup_letter == 0 && n == 6 && (int)lv_obj_get_child_count(K.popup) == n + 1, "7. holding a shows its whole list and 收起");
}
static void pages_check_alternate(void) { CHECK(!K.popup && !strcmp(ink_text_value(), "á"), "7. á goes in without the a: %s", ink_text_value()); }
static void pages_check_swipe(void) { CHECK(!strcmp(ink_text_value(), "á1"), "8. swiping q up types 1: %s", ink_text_value()); }
static void pages_swipe_q(void) {
    pointer_code = 'q'; on_ui(pointer_target);
    hw(HW_PRESS, pointer_x, pointer_y);
    CHECK(WAIT_FOR(pages_pointer_is(true), 1000), "LVGL received the press on q");
    hw(HW_PRESS, pointer_x, pointer_y - ink_ui_px(key_height(false)) * 3 / 4); delay_ms(60);
    hw(HW_RELEASE, pointer_x, pointer_y - ink_ui_px(key_height(false)) * 3 / 4);
    CHECK(WAIT_FOR(pages_pointer_is(false), 1000), "LVGL received the release above q");
    CHECK(WAIT_FOR(settled(), 15000), "the swipe settles");
}
static void pages_nine(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN}; ink_text_focus(&cfg, NULL, NULL);
    if (ink_text_english()) touch(INK_TEXT_KEY_LANGUAGE);
    ink_receipt_t rc; ink_setting_set("text.nine_key", 1, INK_SRC_TOUCH, &rc); sync();
}
static void pages_check_nine(void) {
    CHECK(!strpbrk(lv_label_get_text(K.composing), "0123456789") && strstr(lv_label_get_text(K.composing), "[wxyz]"), "9. the composition shows no digits: %s", lv_label_get_text(K.composing));
    bool zhong = false;
    for (int i = 0; i < 3; i++) if (!strcmp(lv_label_get_text(lv_obj_get_child(K.reading[i], 0)), "zhong")) zhong = true;
    CHECK(zhong, "9. the reading column offers zhong");
}
static void pages_check_zhong(void) {
    pages_index = -1;
    for (int i = 0; i < INK_TEXT_PAGE && i < K.shown; i++) if (!strcmp(K.cand[i].word, "中国")) pages_index = i;
    CHECK(!strcmp(ink_text_preferred_reading(), "zhong") && pages_index >= 0, "9. with zhong chosen 中国 is on the first page");
    CHECK(!strcmp(lv_label_get_text(K.composing), "zhong"), "9. the composition reads zhong: %s", lv_label_get_text(K.composing));
}
static void pages_check_china(void) { CHECK(!strcmp(ink_text_value(), "中国"), "9. nine-key selection commits 中国"); }
static void pages_check_refused(void) {
    CHECK(!strcmp(ink_text_value(), "中国") && !strcmp(ink_text_composing(), "94") && K.notice && !strcmp(lv_label_get_text(K.composing), TR("先选词或按重输")),
          "10. enter with digits waiting asks for a word and leaves the text alone: %s / %s", ink_text_value(), lv_label_get_text(K.composing));
}
static void pages_check_retyped(void) {
    CHECK(!ink_text_composing()[0] && !strcmp(ink_text_value(), "中国") && !K.notice, "10. 重输 clears the composition only");
    snprintf(pages_value[pages_ai], sizeof pages_value[pages_ai], "%s", ink_text_value());
}
static void keyboard_pages(void) {
    for (int ai = 0; ai < 2; ai++) {
        pages_ai = ai;
        CHECK(BY_HAND("memo.edit", NULL) && WAIT_FOR(ink_text_focused(), 10000), "open real memo"); on_ui(pages_setup);
        /* The words learned in the first round wait up to a minute (INK_DB_LAZY, text.user_freq): write them now, so
         * neither the wait below nor a flash write in the middle of the typing depends on that minute. */
        ink_db_flush_all();
        CHECK(WAIT_FOR(pages_ready(), 15000), "memo setup and initial draft write settle before typing");
        for (const char *p = "nihao"; *p; p++) if (ai) pages_ai_key(*p); else pages_key(*p);
        on_ui(pages_check_hello); pages_capture(ai ? "AI-01-nihao" : "Touch-01-nihao");
        if (ai) pages_call("keyboard.candidates", "{}"); else pages_press("▾");
        pages_capture(ai ? "AI-02-grid" : "Touch-02-grid");
        if (ai) pages_call("keyboard.page", "{\"direction\":1}"); else pages_press("▶");
        on_ui(pages_check_page); pages_capture(ai ? "AI-03-page2" : "Touch-03-page2");
        if (ai) { char args[40]; snprintf(args, sizeof args, "{\"index\":%d}", pages_index); pages_call("keyboard.pick", args); }
        else pages_press(pages_word[0]);
        on_ui(pages_check_picked); pages_capture(ai ? "AI-04-picked" : "Touch-04-picked");
        if (ai) pages_call("keyboard.panel", "{\"panel\":\"symbols\"}"); else pages_key(KEY_SWITCH);
        on_ui(pages_check_symbols); pages_capture(ai ? "AI-05-symbols" : "Touch-05-symbols");
        if (ai) pages_call("keyboard.category", "{\"category\":3}"); else pages_press(TR("数学"));
        on_ui(pages_check_math_1); pages_capture(ai ? "AI-06-math" : "Touch-06-math");
        if (ai) pages_call("keyboard.symbol_page", "{\"direction\":1}"); else pages_press("▶");
        on_ui(pages_check_math_2); pages_capture(ai ? "AI-07-math2" : "Touch-07-math2");
        if (ai) pages_call("keyboard.symbol_page", "{\"direction\":-1}"); else pages_press("◀");
        if (ai) pages_call("keyboard.symbol", "{\"text\":\"≈\"}"); else pages_press("≈");
        if (ai) pages_call("keyboard.category", "{\"category\":0}"); else pages_press(TR("最近"));
        on_ui(pages_check_recent); pages_capture(ai ? "AI-08-recent" : "Touch-08-recent");
        if (ai) pages_call("keyboard.panel", "{\"panel\":\"letters\"}"); else pages_press("26");
        on_ui(pages_check_letters); pages_capture(ai ? "AI-09-letters" : "Touch-09-letters");
        on_ui(pages_phone); pages_capture(ai ? "AI-10-phone" : "Touch-10-phone");
        on_ui(pages_plain); CHECK(WAIT_FOR(settled(), 15000), "plain input settles");
        if (ai) pages_call("keyboard.alternates", "{\"code\":97}"); else {
            pointer_code = 'a'; on_ui(pointer_target); hw(HW_PRESS, pointer_x, pointer_y); delay_ms(700); hw(HW_RELEASE, pointer_x, pointer_y);
            CHECK(WAIT_FOR(pages_pointer_is(false), 1000), "LVGL received the release after holding a");
            CHECK(WAIT_FOR(settled(), 15000), "the long press settles");
        }
        on_ui(pages_check_popup); pages_capture(ai ? "AI-11-alternate" : "Touch-11-alternate");
        if (ai) pages_call("keyboard.symbol", "{\"text\":\"á\"}"); else pages_press("á");
        on_ui(pages_check_alternate); pages_capture(ai ? "AI-12-alternate-picked" : "Touch-12-alternate-picked");
        if (ai) pages_call("keyboard.symbol", "{\"text\":\"1\"}"); else pages_swipe_q();
        on_ui(pages_check_swipe); pages_capture(ai ? "AI-13-swipe" : "Touch-13-swipe");
        on_ui(pages_nine); CHECK(WAIT_FOR(settled(), 15000), "nine-key input settles");
        for (const char *p = "94664"; *p; p++) if (ai) pages_ai_key(*p); else pages_key(*p);
        on_ui(pages_check_nine); pages_capture(ai ? "AI-14-nine" : "Touch-14-nine");
        if (ai) pages_call("keyboard.reading", "{\"text\":\"zhong\"}"); else pages_press("zhong");
        on_ui(pages_check_zhong); pages_capture(ai ? "AI-15-zhong" : "Touch-15-zhong");
        if (ai) { char args[40]; snprintf(args, sizeof args, "{\"index\":%d}", pages_index); pages_call("keyboard.pick", args); }
        else pages_press("中国");
        on_ui(pages_check_china);
        for (const char *p = "94"; *p; p++) if (ai) pages_ai_key(*p); else pages_key(*p);
        if (ai) pages_ai_key(KEY_ENTER); else pages_key(KEY_ENTER);
        on_ui(pages_check_refused); pages_capture(ai ? "AI-16-refused" : "Touch-16-refused");
        if (ai) pages_ai_key(KEY_RETYPE); else pages_key(KEY_RETYPE);
        on_ui(pages_check_retyped); pages_capture(ai ? "AI-17-retyped" : "Touch-17-retyped");
        on_ui(end_scene); CHECK(BY_HAND("system.home", NULL) && WAIT_FOR(!ink_text_focused(), 10000), "leave memo");
    }
    CHECK(!strcmp(pages_value[0], pages_value[1]), "touch and AI end the same: %.60s / %.60s", pages_value[0], pages_value[1]);
}
