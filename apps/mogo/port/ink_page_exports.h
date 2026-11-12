bool ink_mogo_page_open(const char *path, const ink_member_identity_t *owner, int w, int h, int px) {
    ink_mogo_owner(owner); ink_mogo_dimensions(w,h,px);
    ink_mogo_page_width = w; ink_mogo_page_height = h;
    s_reader_session = (reader_session_t){0};
    s_margin[0] = s_margin[1] = w * 36 / 684;
    if (ttf_font_open(app_settings_font_path()) != ESP_OK) return false;
    book_layout_set_spacing(150,50);
    return open_book_impl(&ink_mogo_context,path);
}
void ink_mogo_page_turn(int direction) { ink_mogo_context.now_ms = ink_now_us() / 1000; (void)turn_page(&ink_mogo_context,direction); }
void ink_mogo_page_draw(uint8_t *pixels) { if (s_text) draw_reader(pixels,s_reader_session.position.page); }
void ink_mogo_page_close(void) {
    save_progress();
    free_book(); ink_mogo_font_shutdown(); memset(&s_reader_session,0,sizeof s_reader_session);
    s_reader_panel = READER_PANEL_NONE; s_reader_slider = -1;
}
int ink_mogo_page_snapshot(char *out, size_t cap) {
    size_t start = book_layout_page_start_offset(s_reader_session.position.page);
    size_t end = book_layout_page_start_offset(s_reader_session.position.page+1);
    size_t n = s_text && end >= start ? end-start : 0;
    if (n > 600) n = 600;
    while (n && ((unsigned char)s_text[start+n] & 0xc0) == 0x80) --n;
    return snprintf(out,cap,TR_NOOP("新阅读 %s 章节=%zu 页=%zu/%zu 位置=%zu 字号=%d\n%.*s"),
        s_reader_session.book.title,s_reader_session.position.chapter+1,s_reader_session.position.page+1,
        book_layout_page_count(),start,s_px,(int)n,s_text ? s_text+start : s_message);
}
