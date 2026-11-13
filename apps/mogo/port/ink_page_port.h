#pragma once
#include <runtime.h>
#include <settings.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <reader_open.h>
#include <reader_chapter.h>
#include <reader_turn.h>
#include <book_layout.h>
#include <book_source.h>
#include <book_policy.h>
#include <ttf_font.h>
#include <book_file_path.h>
#include <page.h>
#include <nvs.h>
#include <book_bookmark_format.h>
#include <book_toc.h>
#include <ui_kit.h>
#include <book_shelf_model.h>
#include <book_reader_input.h>
#define READER_WIDTH ink_mogo_page_width
#define READER_HEIGHT ink_mogo_page_height
#define READER_FULLSCREEN_PROGRESS_TOP (READER_HEIGHT - 12)
#define READER_BAR_TOP (READER_HEIGHT - 120)
#define READER_FOOTER_PROGRESS_SPACE 19
#define READER_HEADING_BAND_HEIGHT 214
#define UI_BAR_H 120
#undef BOOK_STORE_FLASH_FILE_MAX
#define BOOK_STORE_FLASH_FILE_MAX (1024 * 1024)
enum { MARGIN_LEFT, MARGIN_RIGHT, MARGIN_TOP, MARGIN_BOTTOM };
enum { SHELF, READING, TOC, READER_PANEL_NONE = 0, APP_REDRAW_NONE = 0, APP_REDRAW_PAGE, APP_REDRAW_AREA,
    MODE_GL16, E0470_TURN_RTL, E0470_TURN_LTR };
typedef int app_redraw_t;
typedef struct { uint64_t now_ms; size_t leaf; bool user_activity; } app_ctx_t;
typedef struct { bool active; } ink_mogo_selection_t;
typedef struct { bool dirty; } pending_progress_t;
typedef struct { bool present, mounted; } board_sd_info_t;
typedef struct { bool network_ready; } wifi_transfer_status_t;
typedef struct { size_t chapter; char **images; size_t count; } reader_dims_ctx_t;
typedef struct { size_t skip; unsigned height, width; reader_dims_ctx_t dims; } chapter_layout_undo_t;
static const char *TAG = "mogo";
static reader_session_t s_reader_session;
static const char *s_text;
static size_t s_text_len, s_block_count, s_link_count, s_anchor_count, s_image_count;
static const blk_t *s_blocks;
static const html_link_t *s_links;
static const html_anchor_t *s_anchors;
static const char *s_link_strings;
static char **s_images;
static int ink_mogo_page_width, ink_mogo_page_height, s_px, s_margin[4] = {36,36,0,0}, s_chrome_height[2] = {80,80};
static bool s_reader_fullscreen = true, s_requested_fullscreen = true, s_reader_progress_bar = true;
static int s_reader_panel, s_reader_slider = -1, s_view;
static bool s_clear_confirm, s_batch_confirm, s_save_failed, s_book_fonts_applied;
static bool s_reader_return_home, s_reader_entry_full, s_reader_image_refresh_pending, s_reader_footer_strong;
static bool s_reader_turn_pending, s_reader_text_frame, s_reader_stable_frame, s_reader_cleanup;
static bool s_excerpt_forward, s_water_turn_pending, s_shelf_key_mode, s_shelf_key_paint_pending;
static int s_shelf_key_delta, s_shelf_key_pressed, s_water_turn_dir, s_dictionary_notice_until;
static size_t s_chapter_lead_skip, s_selected_toc, s_jump_offset, s_jump_page;
static unsigned s_chapter_lead_height, s_chapter_lead_width, s_chapter_lead_row_height;
static unsigned s_turns, s_unsaved, s_session_turns, s_stats_pending_turns;
static uint64_t s_stats_last_ms, s_stats_activity_ms, s_last_turn_ms, s_session_read_ms;
static char s_message[160], s_font_path[256], s_reader_notice[160];
static char s_chapter_heading_title[128], s_chapter_heading_label[32];
static void *s_page_images;
static reader_dims_ctx_t s_reader_dims_ctx;
static ink_mogo_selection_t s_dictionary_selection;
static app_ctx_t ink_mogo_context;
static int s_bookmark_page;
static bool s_bookmark_edit, s_bookmark_delete_confirm, s_bookmark_delete_error;
static uint32_t s_bookmark_selected;
static int64_t s_reader_notice_until;
static void fit_text(char *s,int px,int width) {
    size_t n=strlen(s);
    while (n && ttf_text_width_px(ui_text_effective_px(px),s)>width) { do { --n; } while (n && ((unsigned char)s[n]&0xc0)==0x80); s[n]=0; }
}
static void free_book(void) { reader_session_close(&s_reader_session); s_text = NULL; }
static unsigned percent(size_t page);
/* The place written last in this session: the same place is not written to the flash again (a close after a pause). */
static struct { char path[288]; uint16_t chapter; uint32_t byte_off; } s_progress_saved;
static bool s_turning; /* page turns write nothing: the place is written after a pause (and at least every minute) */
static void save_progress(void) {
    if (s_turning) return;
    if (!s_text || !book_layout_page_count()) return;
    book_progress_t p = {.file_size=s_reader_session.book.file_size,
        .chapter=(uint16_t)s_reader_session.position.chapter,
        .byte_off=(uint32_t)book_layout_page_start_offset(s_reader_session.position.page),.px=ink_mogo_settings.px,.pct=(uint8_t)percent(s_reader_session.position.page)};
    if (!s_save_failed && s_progress_saved.chapter == p.chapter && s_progress_saved.byte_off == p.byte_off && !strcmp(s_progress_saved.path, s_reader_session.book.path)) return;
    s_save_failed = book_progress_save(s_reader_session.book.path,&p) != ESP_OK;
    if (!s_save_failed) { snprintf(s_progress_saved.path, sizeof s_progress_saved.path, "%s", s_reader_session.book.path); s_progress_saved.chapter = p.chapter; s_progress_saved.byte_off = p.byte_off; }
}
static void copy_text(char *out, size_t cap, const char *s) {
    size_t n = strlen(s);
    if (n >= cap) { n = cap - 1; while (n && ((unsigned char)s[n] & 0xc0) == 0x80) --n; }
    memcpy(out, s, n); out[n] = 0;
}
static void apply_chapter_lead(const char *t, const char *l, size_t skip, unsigned height) {
    (void)t; (void)l; s_chapter_lead_skip = skip; s_chapter_lead_height = height;
    book_layout_set_chapter_lead(skip, height); book_layout_set_chapter_lead_width(0);
}
static bool reader_image_dims(void *ctx, int image, int *w, int *h) { (void)ctx; (void)image; (void)w; (void)h; return false; }
static bool pending_restore(const char *p, uint32_t size, book_progress_t *v) { (void)p; (void)size; (void)v; return false; }
static bool pending_reserve(const char *p) { (void)p; return true; }
#define pending_find(p) ((pending_progress_t *)NULL)
#define pending_discard(p) ((void)0)
#define pending_mark_latest(p) ((void)0)
#define delete_retry_find(p) false
#define invalidate_covers() ((void)0)
#define lock_draw() ((void)0)
#define unlock_draw() ((void)0)
#define invalidate_prep() ((void)0)
#define reader_prefetch_discard() ((void)0)
#define dictionary_cancel() ((void)0)
#define dictionary_dismiss() APP_REDRAW_NONE
#define release_page_images() ((void)0)
#define prepare_inline_image() ((void)0)
#define book_title_from_path(p,o,c) false
#define clean_filename(o,c,p) copy_text(o,c,p)
#define book_excerpt_begin(p) ((void)0)
#define reader_excerpt_reposition() ((void)0)
#define app_font_activate_reading() ((void)0)
#define app_font_activate_system() ((void)0)
#define app_reading_journal_record(...) ((void)0)
#define shelf_hidden_load(p) false
#define shelf_hidden_save(p,b) true
#define set_reader_view(v) (s_view = (v))
#define board_sd_get_info(p) (*(p) = (board_sd_info_t){true,true})
#define wifi_transfer_get_status(p) (*(p) = (wifi_transfer_status_t){false})
#define uxTaskGetStackHighWaterMark(p) 0
#define TD_LOGI(...) ((void)0)
#define TD_TIME(n,c) (c)
#define ui_text_set_system_scale(b) ((void)0)
#define ui_nav_status(fb) ((void)0)
#define draw_reader_header(fb) ((void)0)
#define draw_vertical_heading(fb,b) ((void)0)
#define draw_reader_title_style(...) ((void)0)
#define ui_reader_background_paint(...) ((void)0)
#define draw_reader_images(...) ((void)0)
#define reader_chrome_data_t int
#define reader_chrome_data(p) (*(p) = 0)
#define reader_chrome_draw_footer_row(...) ((void)0)
#define draw_reader_panel(fb) ((void)0)
#define ui_reader_dictionary_notice(fb) ((void)0)
#define ui_reader_dictionary_selection(...) ((void)0)
#define ui_reader_dictionary_draw(...) ((void)0)
#define ui_reader_notice_draw(...) ((void)0)
static EpdRect progress_rect(void) { return (EpdRect){36,READER_HEIGHT-70,READER_WIDTH-72,60}; }
static uint32_t chapter_end(void) { return s_reader_session.position.chapter + 1 < book_chapter_count() ? book_chapter_byte_offset(s_reader_session.position.chapter + 1) : book_total_bytes(); }
static size_t reader_page_offset(size_t p) { return book_layout_page_start_offset(p); }
static app_redraw_t paint_reading(app_ctx_t *ctx, int mode) { (void)ctx; (void)mode; return APP_REDRAW_AREA; }
