/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * Intact reading functions from app_book.c; platform and unused panels supplied by ink.
 */
#include "ink_page_port.h"

static int reader_footer_height(void) {
    return s_chrome_height[1] - (s_reader_progress_bar ? 0 : READER_FOOTER_PROGRESS_SPACE);
}

static EpdRect reader_area(void) {
    const int top = s_reader_fullscreen
        ? (app_settings_reader_statusbar() ? 80 : 0)
        : (app_settings_reader_statusbar() ? 80 : 0) + s_chrome_height[0];
    // 全屏进度线单独差分，翻页时不让正文刷新反复驱动整条横线。
    // Keep the full-screen progress strip out of body updates so turns do not drive the whole bar.
    const int bottom = s_reader_fullscreen
        ? (s_reader_progress_bar ? READER_FULLSCREEN_PROGRESS_TOP : READER_HEIGHT)
        : READER_BAR_TOP + UI_BAR_H - reader_footer_height();
    return (EpdRect){0, top, READER_WIDTH, bottom - top};
}

static EpdRect body_rect_for_tracking(int tracking_index) {
    (void)tracking_index;
    int top = s_reader_fullscreen
        ? (app_settings_reader_statusbar() ? 96 : 24)
        : (app_settings_reader_statusbar() ? 96 : 16) + s_chrome_height[0];
    // 正文底与页脚差分带相接。/ Body bottom meets the footer refresh band.
    int bottom = s_reader_fullscreen
        ? (s_reader_progress_bar ? READER_FULLSCREEN_PROGRESS_TOP : READER_HEIGHT) - 2
        : READER_BAR_TOP + UI_BAR_H - reader_footer_height();
    // 四向留白是屏幕边距，横排竖排同一套：左右缩栏宽，上下缩页高。
    // 不吸附整字格，完整行把余量摊到字间。
    // The four insets are screen-side margins shared by both writing modes: left/right narrow the
    // column, top/bottom shorten the page. They never snap to glyph cells; full lines spread the
    // remainder into the tracking.
    return (EpdRect){s_margin[MARGIN_LEFT], top + s_margin[MARGIN_TOP],
                     READER_WIDTH - s_margin[MARGIN_LEFT] - s_margin[MARGIN_RIGHT],
                     bottom - top - s_margin[MARGIN_TOP] - s_margin[MARGIN_BOTTOM]};
}

static EpdRect body_rect(void) {
    return body_rect_for_tracking(app_settings_book_tracking());
}

static unsigned percent(size_t page) {
    uint32_t total = book_total_bytes();
    if (!total) return 0;
    if (s_reader_session.position.chapter + 1 == book_chapter_count() && page + 1 == book_layout_page_count()) return 100;
    uint32_t off = book_position_bytes(book_chapter_byte_offset(s_reader_session.position.chapter), chapter_end(),
                                       reader_page_offset(page), s_text_len);
    return (unsigned)((uint64_t)off * 100 / total);
}

static void draw_reader_outer(uint8_t *fb,size_t page) {
    ui_text_set_system_scale(false);
    if (app_settings_reader_statusbar()) ui_nav_status(fb);
    if (!s_reader_fullscreen) draw_reader_header(fb);
    EpdRect body=body_rect();
    if (page == 0 && s_chapter_heading_title[0] &&
        (s_chapter_lead_height || s_chapter_lead_width)) {
        if (app_settings_book_vertical()) {
            // 竖排题头站在首页预留的那条列里，正文从它左边接着排。
            // The vertical heading stands in the columns the first page reserved; the body
            // continues to its left.
            draw_vertical_heading(fb, body);
        } else {
            // 样式与字号都来自阅读设置：只改这两个实参，绘制与排版预留仍然同源。
            // The style and the size both come from the reader's settings, so wiring them changes
            // these two arguments while painting stays tied to the reservation.
            draw_reader_title_style(fb, reader_heading_band(body.y),
                                    (int)app_settings_book_title_style(),
                                    (int)app_settings_book_title_size(),
                                    s_chapter_heading_label, s_chapter_heading_title);
        }
    }
    ui_reader_background_paint(fb, (EpdRect){0,0,READER_WIDTH,READER_HEIGHT}, true);
    draw_reader_images(fb, page, body);
    if (s_reader_fullscreen) {
        // Full-book progress uses the last few screen rows and no footer text.
        EpdRect track = {36, READER_FULLSCREEN_PROGRESS_TOP + 3, READER_WIDTH - 72, 3};
        if (s_reader_progress_bar) {
            ui_fill_round_rect(fb, track, 1, 0x90);
            track.width = track.width * (int)percent(page) / 100;
            if (track.width) ui_fill_round_rect(fb, track, 1, UI_GRAY_BLACK);
        }
    } else {
        EpdRect track = progress_rect();
        // 显示时文字顶为带顶+27；隐藏后带顶下移19px，文字顶仍保持原位置。
        // Text starts 27px below the shown band; hiding moves the band down 19px and keeps the text fixed.
        if (s_reader_progress_bar) {
            EpdRect bar = {track.x, track.y + 8, track.width, 6};
            ui_fill_round_rect(fb, bar, 3, UI_GRAY_LIGHT);
            bar.width = bar.width * (int)percent(page) / 100;
            if (bar.width) ui_fill_round_rect(fb, bar, 3, UI_GRAY_BLACK);
        }
        // 页脚文字行三段槽位：字号不随正文设置，布局与截短在 reader_chrome 内完成。
        // The footer text row's three slots: the size never follows body settings; layout and
        // trimming live in reader_chrome.
        reader_chrome_data_t data;
        reader_chrome_data(&data);
        const int text_top = track.y + 27 - (s_reader_progress_bar ? 0 : READER_FOOTER_PROGRESS_SPACE);
        reader_chrome_draw_footer_row(fb, &data, text_top, 28, track.x, track.x + track.width);
    }
    draw_reader_panel(fb);
    if (s_dictionary_notice_until) ui_reader_dictionary_notice(fb);
    if (s_dictionary_selection.active) {
        size_t count = 0; const book_layout_glyph_t *glyphs = book_layout_glyphs(page, &count);
        ui_reader_dictionary_selection(fb, &s_dictionary_selection, glyphs, count);
        ui_reader_dictionary_draw(fb, &s_dictionary_view, &s_dictionary_result, s_dictionary_result.query[0] ? s_dictionary_result.query : s_dictionary_selection.query);
    }
    ui_reader_notice_draw(fb, s_reader_notice, app_settings_system_font_size());
}

static void draw_reader(uint8_t *fb,size_t page) {
    ui_text_set_system_scale(false);ui_clear_page(fb);
    book_layout_draw_page(fb,page,body_rect(),s_px);
    draw_reader_outer(fb,page);
}

static void reader_bind_chapter(void) {
    const html_text_t *content = reader_session_content(&s_reader_session);
    s_text = content->utf8;
    s_text_len = content->len;
    s_blocks = content->blocks;
    s_block_count = content->count;
    s_links = content->links;
    s_link_count = content->link_count;
    s_anchors = content->anchors;
    s_anchor_count = content->anchor_count;
    s_link_strings = content->strings;
    s_images = content->images;
    s_image_count = content->image_count;
}

static void chapter_restore_images(void *ctx) {
    const chapter_layout_undo_t *undo = ctx;
    // 候选章释放前恢复图片借用。/ Restore image borrows before freeing the candidate.
    s_reader_dims_ctx = undo->dims;
}

static void chapter_restore_lead(void *ctx) {
    const chapter_layout_undo_t *undo = ctx;
    s_chapter_lead_skip = undo->skip;
    s_chapter_lead_height = undo->height;
    s_chapter_lead_width = undo->width;
    book_layout_set_chapter_lead(undo->skip, undo->height);
    book_layout_set_chapter_lead_width(undo->width);
}

static bool load_chapter_at(app_ctx_t* ctx, size_t chapter, size_t offset,
                            bool last_page, const char *anchor, size_t source_offset) {
    dictionary_cancel();
    bool clear_toc = chapter != s_reader_session.position.chapter || (!anchor && source_offset == SIZE_MAX);
    reader_chapter_t candidate = {0};
    esp_err_t err = reader_chapter_read(&candidate, chapter, anchor, source_offset,
                                        body_rect().height, READER_HEADING_BAND_HEIGHT);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "chapter load failed: %s", esp_err_to_name(err));
        copy_text(s_message, sizeof(s_message), "加载失败，请检查存储卡后重试");
        return false;
    }
    chapter_layout_undo_t undo = {
        .skip = s_chapter_lead_skip, .height = s_chapter_lead_height,
        .width = s_chapter_lead_width, .dims = s_reader_dims_ctx,
    };
    reader_chapter_rollback_t rollback = {
        .ctx = &undo, .restore_images = chapter_restore_images,
        .restore_lead = chapter_restore_lead,
    };
    lock_draw();
    invalidate_prep();
    // 题头几何按当前模式重算，横排留顶部行高、竖排留右侧列宽，绘制与预留不会错开。
    // Recompute the heading geometry for the current mode -- rows at the top horizontally,
    // columns at the right vertically -- so drawing and reservation cannot drift apart.
    apply_chapter_lead(candidate.heading, candidate.label, candidate.lead_skip, candidate.lead_height);
    s_reader_dims_ctx = (reader_dims_ctx_t){chapter, candidate.content.images, candidate.content.image_count};
    book_layout_set_image_dims(reader_image_dims, &s_reader_dims_ctx);
    reader_chapter_result_t result = reader_chapter_apply_locked(&s_reader_session, &candidate,
                                                                body_rect(), s_px, &rollback);
    if (result != READER_CHAPTER_APPLIED) {
        unlock_draw();
        if (result == READER_CHAPTER_LOST_LAYOUT) {
            free_book();
            s_view = SHELF;
            ctx->leaf = 0;
        }
        copy_text(s_message, sizeof(s_message), "排版失败：内存不足或章节过长");
        return false;
    }
    reader_bind_chapter();
    release_page_images();
    s_reader_session.position.chapter = chapter;
    if (clear_toc) s_selected_toc = SIZE_MAX;
    s_chapter_lead_skip = candidate.lead_skip;
    s_chapter_lead_row_height = candidate.lead_height;
    copy_text(s_chapter_heading_title, sizeof(s_chapter_heading_title), candidate.heading);
    copy_text(s_chapter_heading_label, sizeof(s_chapter_heading_label), candidate.label);
    s_reader_session.position.page = last_page ? book_layout_page_count() - 1 :
             book_layout_page_for_offset(anchor || source_offset != SIZE_MAX ? candidate.anchor_offset : offset);
    s_jump_offset = anchor || source_offset != SIZE_MAX ? candidate.anchor_offset : SIZE_MAX;
    s_jump_page = s_reader_session.position.page;
    if (book_chapter_title(chapter, s_reader_session.book.chapter_title, sizeof(s_reader_session.book.chapter_title)) != ESP_OK)
        snprintf(s_reader_session.book.chapter_title, sizeof(s_reader_session.book.chapter_title), "第 %u 节", (unsigned)chapter + 1);
    copy_text(s_font_path, sizeof(s_font_path), ttf_font_path());
    s_message[0] = 0;
    unlock_draw();
    prepare_inline_image();
    // 开书这条链上有解析、解压和字体装载，栈曾经在这里溢出过；留一个可观测的水位。
    // 顺带量一下章节排完后的 PSRAM 余量：书内字体的装载预算要靠这个数来定，不能靠估。
    // Parsing, inflate and face loading all ride this chain, and it overflowed once before;
    // keep the headroom observable. The PSRAM left after a chapter is laid out is also the
    // number the face budget should be derived from rather than guessed.
    ESP_LOGI(TAG, "chapter %u stack free %u psram free %u largest %u", (unsigned)chapter,
             (unsigned)uxTaskGetStackHighWaterMark(NULL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    return true;
}

static bool load_chapter(app_ctx_t* ctx, size_t chapter, size_t offset, bool last_page) {
    return load_chapter_at(ctx, chapter, offset, last_page, NULL, SIZE_MAX);
}

static void skip_hidden_image_pages(app_ctx_t *ctx, int direction) {
    if (!app_settings_reader_hide_images()) return;
    for (int guard = 0; guard < 128 && book_layout_page_image(s_reader_session.position.page) >= 0; ++guard) {
        if (direction > 0 && s_reader_session.position.page + 1 < book_layout_page_count()) ++s_reader_session.position.page;
        else if (direction < 0 && s_reader_session.position.page > 0) --s_reader_session.position.page;
        else if (direction > 0 && s_reader_session.position.chapter + 1 < book_chapter_count()) {
            if (!load_chapter(ctx, s_reader_session.position.chapter + 1, 0, false)) break;
        } else if (direction < 0 && s_reader_session.position.chapter > 0) {
            if (!load_chapter(ctx, s_reader_session.position.chapter - 1, 0, true)) break;
        } else break;
    }
}

static void reader_open_replace(void) {
    save_progress();
    free_book();
    s_reader_fullscreen = s_requested_fullscreen;
    app_font_activate_reading();
}

static void reader_open_describe(const char *path) {
    // 书源打开后再把书内字体开关接上：EPUB 才有效，TXT 是空操作。
    // Wire the embedded-face switch only after the source is open: it only applies to EPUB,
    // and is a no-op for TXT.
    s_book_fonts_applied = app_settings_book_fonts();
    book_set_embedded_fonts(s_book_fonts_applied);
    copy_text(s_reader_session.book.path, sizeof(s_reader_session.book.path), path);
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    if (!book_title_from_path(path, s_reader_session.book.title, sizeof(s_reader_session.book.title)))
        clean_filename(s_reader_session.book.title, sizeof(s_reader_session.book.title), filename);
}

static bool reader_open_load(void *ctx, size_t chapter, size_t offset) {
    if (!load_chapter(ctx, chapter, offset, false)) return false;
    skip_hidden_image_pages(ctx, 1);
    return s_text != NULL;
}

static bool open_book_impl(app_ctx_t* ctx, const char* path) {
#if CONFIG_READER_PERF
    reader_perf_begin("open");
    READER_PERF_SCOPE(READER_PERF_ACTION);
#endif
    if (!book_file_path_supported(path)) {
        copy_text(s_message, sizeof(s_message), "此版本仅支持 TXT、EPUB 和 MOBI 书籍");
        return false;
    }
    s_reader_return_home = false;
    if (!strncmp(path, "/sdcard/", 8)) {
        board_sd_info_t sd = {0};
        board_sd_get_info(&sd);
        if (!sd.present || !sd.mounted) {
            copy_text(s_message, sizeof(s_message), "TF 卡不可用，请重新挂载后打开");
            return false;
        }
    }
    // 自动续读也不能绕过同路径旧文件的清理重试。
    // Automatic resume must also finish cleanup of the old file at this path.
    if (delete_retry_find(path)) {
        copy_text(s_message, sizeof(s_message), "文件已删除，进度清理失败，请重试");
        return false;
    }
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 || (uint64_t)st.st_size > UINT32_MAX) {
        copy_text(s_message, sizeof(s_message), "文件不可读，请检查 TF 卡和书籍文件");
        return false;
    }
    if (strncmp(path, "/flash/", 7) == 0 && st.st_size > BOOK_STORE_FLASH_FILE_MAX) {
        copy_text(s_message, sizeof(s_message), "内置存储单本限 1 MB");
        return false;
    }
    wifi_transfer_status_t network = {0};
    wifi_transfer_get_status(&network);
    s_shelf_key_mode=s_shelf_key_paint_pending=false;s_shelf_key_delta=s_shelf_key_pressed=0;
    // 书架封面和上一本书的预渲染缓冲可以重建；先释放，再解析 EPUB。
    // Cover and previous-page caches are rebuildable; release them before EPUB parsing.
    invalidate_covers();
    lock_draw();
    reader_prefetch_discard();
    unlock_draw();
    if (network.network_ready) ttf_font_cache_clear();
    // 开书前把字形缓存全还回去并量一下能腾多少：书内字体要的正是这块 PSRAM，而系统字体
    // 的缓存是里面最容易回收的。字体本身保持装载，翻页时按需重新缓存。
    // Hand every glyph cache back before opening and measure what that was worth: embedded
    // faces need exactly this PSRAM, and the system face's cache is the easiest part to
    // reclaim. The faces stay loaded and re-cache on demand.
    size_t psram_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    size_t reclaimed = ttf_font_cache_clear_all();
    size_t psram_after = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "open start wifi=%d internal=%u/%u psram=%u/%u cache=%uK freed=%uK",
             network.network_ready,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)psram_after,
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
             (unsigned)(reclaimed / 1024),
             (unsigned)((psram_after - psram_before) / 1024));
    reader_open_t open = {
        .session = &s_reader_session, .px = &s_px,
        .reserve = pending_reserve, .replace = reader_open_replace,
        .describe = reader_open_describe, .pending_restore = pending_restore,
        .load = reader_open_load, .close = free_book, .load_ctx = ctx,
    };
    esp_err_t err = ESP_OK;
    reader_open_result_t result = reader_open_run(&open, path, (uint32_t)st.st_size, &err);
    if (result == READER_OPEN_RESERVE_OLD) {
        copy_text(s_message, sizeof(s_message), "内存不足，无法保留待保存进度");
        return false;
    }
    if (result == READER_OPEN_RESERVE_NEW) {
        copy_text(s_message, sizeof(s_message), "内存不足，无法打开图书");
        app_font_activate_system();
        return false;
    }
    if (result == READER_OPEN_SOURCE_FAILED) {
        pending_progress_t* pending = pending_find(path);
        if (pending && !pending->dirty) pending_discard(path);
        copy_text(s_message, sizeof(s_message),
                  err == ESP_ERR_NOT_SUPPORTED ? "文件格式或压缩方式暂不支持" :
                  err == ESP_ERR_NO_MEM ? "内存不足，请重启后重试" :
                  err == ESP_ERR_INVALID_SIZE ? "图书资源过大或章节过多" :
                  "无法打开图书，请检查文件");
        ESP_LOGW(TAG, "open failed: %s internal=%u/%u psram=%u/%u", esp_err_to_name(err),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        app_font_activate_system();
        return false;
    }
    if (result == READER_OPEN_CONTENT_FAILED) return false;
    book_excerpt_begin(s_reader_session.book.path);
    s_view = READING;
    s_reader_entry_full = true;
    s_reader_image_refresh_pending = !app_settings_reader_hide_images() &&
        book_layout_page_image_count(s_reader_session.position.page) > 0 && s_page_images;
    s_stats_last_ms = s_stats_activity_ms = ctx->now_ms;
    app_reading_journal_record(s_reader_session.book.path,s_reader_session.book.title,
        s_reader_session.book.file_size,(unsigned)s_reader_session.position.chapter,0,0);
    s_reader_panel = READER_PANEL_NONE;
    s_clear_confirm = s_batch_confirm = false;
    s_session_read_ms = s_session_turns = 0;
    s_reader_notice[0] = 0;
    s_turns = s_unsaved = 0;
    pending_mark_latest(s_reader_session.book.path);
    save_progress();
    // 打开即重新上书架：移出书架只隐藏条目，重新读这本书就是它该在架上的信号。
    // Opening puts the book back on the shelf: removal only hides the entry, and reading it
    // again is the signal that it belongs there.
    if (shelf_hidden_load(s_reader_session.book.path)) (void)shelf_hidden_save(s_reader_session.book.path, false);
    ESP_LOGI(TAG, "opened kind=%d chapters=%u pages=%u px=%d", book_kind(), (unsigned)book_chapter_count(), (unsigned)book_layout_page_count(), s_px);
    return true;
}

static bool reader_turn_chrome(app_ctx_t *ctx) {
    // 翻页只收起面板与滑块，全屏状态保持不变；全屏切换走工具栏按钮与按键。
    // Turns collapse panels and sliders only; full screen stays untouched and is
    // toggled from the toolbar button or keys.
    (void)ctx;
    bool changed = s_reader_panel != READER_PANEL_NONE || s_reader_slider >= 0;
    s_reader_panel = READER_PANEL_NONE;
    s_reader_slider = -1;
    if (changed) { invalidate_prep(); s_reader_text_frame = false; }
    return changed;
}

static void reader_turn_save(void *context) {
    (void)context;
    save_progress();
}

static bool reader_turn_load(void *context, size_t chapter, bool last) {
    return load_chapter(context, chapter, 0, last);
}

static app_redraw_t turn_page(app_ctx_t* ctx, int dir) {
    s_excerpt_forward = false;
    if (s_dictionary_selection.active) return dictionary_dismiss();
#if CONFIG_READER_PERF
    reader_perf_begin("turn");
    READER_PERF_SCOPE(READER_PERF_ACTION);
#endif
    // 翻页就是用户操作：音量键、上键和蓝牙翻页器都不经过触摸帧，只有在这里登记才能重置自动休眠计时。
    // A turn is user activity: the side keys, the up key and the page turner never pass a touch frame,
    // so only noting it here restarts the idle-lock clock.
    ctx->user_activity = true;
    TD_LOGI("TURN dir=%d ch=%u pg=%u", dir, (unsigned)s_reader_session.position.chapter, (unsigned)s_reader_session.position.page);
    s_reader_footer_strong = false;
    if (!s_text || s_clear_confirm) return APP_REDRAW_NONE;
    const size_t prior_chapter = s_reader_session.position.chapter;
    // 页面图片表可能随跨章加载替换，旧页资格以实际成功显示的画面为准。
    // Chapter loads may replace the image table; the old-page eligibility comes from the displayed frame.
    bool from_text_frame = s_reader_text_frame;
    bool from_stable_frame = s_reader_stable_frame;
    const reader_turn_ops_t turn_ops = {
        .context = ctx, .save = reader_turn_save, .load = reader_turn_load,
    };
    reader_turn_result_t moved = reader_turn_move(&s_reader_session.position, dir,
        book_layout_page_count(), book_chapter_count(), &turn_ops);
    if (moved == READER_TURN_EDGE) return reader_turn_chrome(ctx) ? APP_REDRAW_PAGE : APP_REDRAW_NONE;
    if (moved == READER_TURN_LOAD_FAILED) { set_reader_view(s_text ? TOC : SHELF); ctx->leaf = s_text ? s_reader_session.position.chapter / BOOK_TOC_ROWS : 0; return APP_REDRAW_PAGE; }
    skip_hidden_image_pages(ctx, dir);
    if (!s_text) { s_view = SHELF; return APP_REDRAW_PAGE; }
    // 先选择旧布局的目标页，再切全屏，保留目标位置而不是跨过未读段落。
    // Choose the destination in the old layout first, then reflow to retain its reading position.
    bool chrome_changed = reader_turn_chrome(ctx);
    if (!s_text) return APP_REDRAW_PAGE;
    s_jump_offset = SIZE_MAX;
    if (dir < 0) reader_excerpt_reposition();
    s_last_turn_ms = s_stats_activity_ms = ctx->now_ms;
    ++s_stats_pending_turns;
    ++s_session_turns;
    app_reading_journal_record(s_reader_session.book.path,s_reader_session.book.title,
        s_reader_session.book.file_size,(unsigned)s_reader_session.position.chapter,0,1);
    uint8_t full_pages = app_settings_reader_full_pages();
    if (full_pages) ++s_turns;
    else s_turns = 0;
    s_reader_cleanup = full_pages > 0 && s_turns >= full_pages;
    if (s_reader_cleanup) s_turns = 0;
    if (s_unsaved < 8) ++s_unsaved;
    if (s_unsaved >= 8 && !s_save_failed) TD_TIME("save_progress", save_progress());
    ESP_LOGI(TAG, "turn chapter=%u page=%u/%u pct=%u", (unsigned)s_reader_session.position.chapter, (unsigned)s_reader_session.position.page + 1, (unsigned)book_layout_page_count(), percent(s_reader_session.position.page));
    // 预渲染未命中时也使用 GL16；到达设定页数后在本次翻页整屏全刷。
    // A cache miss still uses GL16; the selected turn count triggers a full-screen refresh on this turn.
    app_redraw_t redraw = paint_reading(ctx, MODE_GL16);
    const reader_turn_frame_t frame = {
        .chrome_changed = chrome_changed, .area_redraw = redraw == APP_REDRAW_AREA,
        .from_text = from_text_frame, .from_stable = from_stable_frame,
        .images = book_layout_page_image_count(s_reader_session.position.page) != 0,
        .background = app_settings_book_background_path()[0] != 0,
        .cleanup = s_reader_cleanup, .water_effect = app_settings_reader_turn_effect() == 1,
        .chapter_changed = s_reader_session.position.chapter != prior_chapter,
    };
    reader_turn_effects_t effects = reader_turn_effects(&frame);
    s_reader_turn_pending = effects.text;
    // 水波纹走原厂 GL16 表，灰阶转换逐对校准，阅读背景页同样适用；插图页仍排除。
    // The ripple rides the vendor GL16 table with per-pair calibrated transitions, so
    // reading backgrounds qualify; image pages stay excluded.
    s_water_turn_pending = effects.water;
    s_water_turn_dir = dir > 0 ? E0470_TURN_RTL : E0470_TURN_LTR;
    // 换章页脚恢复默认 GL16 清理；普通页脚仍走短 DU，整屏清理不重复输出页脚。
    // Restore default GL16 cleanup for chapter footers; ordinary footers use short DU and full cleanup never replays them.
    s_reader_footer_strong = effects.footer_strong;
    s_excerpt_forward = dir > 0;
    return chrome_changed ? APP_REDRAW_PAGE : redraw;
}

#include "ink_page_exports.h"
