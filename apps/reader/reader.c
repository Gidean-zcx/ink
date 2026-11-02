/*
 * Reader: library of the .txt books in /content/books and a paginated reading view.
 *
 * Page turn path: the next page is rendered ahead of time into a second PSRAM
 * buffer while the reader looks at the current one, so a turn is a buffer
 * swap + one TEXT refresh. Pages are found lazily (page start offsets grow as
 * you read). The reading position is saved through the storage service,
 * which waits for an idle panel (flash writes freeze PSRAM and would break a
 * waveform).
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_storage.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/actions.h"
#include "ink/sys/font.h"
#include "ink/sys/storage.h"
#include "ink/sys/text.h"
#include "ink/sys/ui.h"

#define MAX_BOOKS 16
#define MAX_BOOK_BYTES (2u * 1024u * 1024u)
#define SAVE_AFTER_IDLE_MS 8000

typedef struct {
    char name[96];
    char path[160];
} book_t;

static struct {
    book_t books[MAX_BOOKS];
    int nbooks;
    int open; /* index of the open book */
} LIB;

static struct {
    char *text;
    size_t len;
    uint32_t *starts; /* page start offsets found so far */
    int nstarts, cap;
    int page;
    ink_layout_t layout;
    int bw, bh;        /* page buffer (logical) */
    uint8_t *buf[2];   /* [cur, ahead] */
    int buf_page[2];
    lv_image_dsc_t dsc;
    lv_obj_t *img, *footer, *title;
    lv_timer_t *ahead_timer, *save_timer;
    bool dirty_pos;
} RD;

/* ------------------------------------------------------------- library */

static void scan_library(void) {
    LIB.nbooks = 0;
    hal_dir_t *d = hal_fs_opendir("/content/books");
    hal_dirent_t e;
    while (d && LIB.nbooks < MAX_BOOKS && hal_fs_readdir(d, &e) == 1) {
        size_t n = strlen(e.name);
        if (e.is_dir || n < 5 || strcmp(e.name + n - 4, ".txt")) continue;
        book_t *b = &LIB.books[LIB.nbooks++];
        snprintf(b->name, sizeof b->name, "%.*s", (int)(n - 4), e.name);
        snprintf(b->path, sizeof b->path, "/content/books/%s", e.name);
    }
    hal_fs_closedir(d);
    /* stable order */
    for (int i = 0; i < LIB.nbooks; i++)
        for (int j = i + 1; j < LIB.nbooks; j++)
            if (strcmp(LIB.books[j].name, LIB.books[i].name) < 0) {
                book_t t = LIB.books[i];
                LIB.books[i] = LIB.books[j];
                LIB.books[j] = t;
            }
    /* warm the reading positions while the shelf is up: opening a book then needs no flash access
     * (which would have to wait for the tap's refresh to finish) */
    for (int i = 0; i < LIB.nbooks; i++) {
        char pos[200];
        snprintf(pos, sizeof pos, "/user/apps/reader-%s.pos", LIB.books[i].name);
        ink_storage_prefetch(pos);
    }
}

extern const ink_activity_t ink_app_book;

static void book_clicked(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    ink_probe_mark("reader-open-book");
    ink_ui_open(lv_event_get_target_obj(e), &ink_app_book, (void *)(intptr_t)idx);
}

static void library_create(lv_obj_t *scr, void *arg) {
    (void)arg;
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    ink_font_t *title_font = ink_font_for_pt(11, pi.dpi);
    lv_obj_t *t = lv_label_create(scr);
    if (title_font) lv_obj_set_style_text_font(t, ink_font_lvgl(title_font), 0);
    lv_label_set_text(t, "书架");
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, ink_dp(16), ink_dp(12));
    scan_library();
    ink_font_t *f = ink_font_for_pt(9, pi.dpi);
    for (int i = 0; i < LIB.nbooks; i++) {
        lv_obj_t *b = lv_button_create(scr);
        ink_ui_style_button(b);
        lv_obj_set_size(b, lv_pct(90), ink_dp(56));
        lv_obj_align(b, LV_ALIGN_TOP_MID, 0, ink_dp(64 + i * 68));
        lv_obj_add_event_cb(b, book_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *l = lv_label_create(b);
        if (f) lv_obj_set_style_text_font(l, ink_font_lvgl(f), 0);
        lv_label_set_text(l, LIB.books[i].name);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    }
    if (!LIB.nbooks) {
        lv_obj_t *l = lv_label_create(scr);
        if (f) lv_obj_set_style_text_font(l, ink_font_lvgl(f), 0);
        lv_label_set_text(l, "没有书：把 .txt 放进 /content/books");
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, ink_dp(16), ink_dp(64));
    }
}

const ink_activity_t ink_app_reader = {.name = "reader", .create = library_create};

static void act_list(const ink_json_t *a, int t, ink_receipt_t *rc) {
    (void)a;
    (void)t;
    if (!LIB.nbooks) scan_library();
    size_t n = (size_t)snprintf(rc->say, sizeof rc->say, "书架上有：");
    for (int i = 0; i < LIB.nbooks && n < sizeof rc->say; i++)
        n += (size_t)snprintf(rc->say + n, sizeof rc->say - n, "%s%s", i ? "、" : "", LIB.books[i].name);
    snprintf(rc->object, sizeof rc->object, "shelf:books");
}

static void act_open(const ink_json_t *a, int t, ink_receipt_t *rc) {
    char title[96];
    if (!a || ink_json_str(a, ink_json_find(a, t, "title"), title, sizeof title) < 0) {
        rc->status = INK_RC_INVALID;
        snprintf(rc->detail, sizeof rc->detail, "title required");
        return;
    }
    if (!LIB.nbooks) scan_library();
    for (int i = 0; i < LIB.nbooks; i++)
        if (strstr(LIB.books[i].name, title) || strstr(title, LIB.books[i].name)) {
            ink_ui_push(&ink_app_book, (void *)(intptr_t)i);
            snprintf(rc->object, sizeof rc->object, "book:%s", LIB.books[i].name);
            snprintf(rc->say, sizeof rc->say, "打开《%s》", LIB.books[i].name);
            return;
        }
    rc->status = INK_RC_FAILED;
    snprintf(rc->detail, sizeof rc->detail, "no such book on the shelf");
}

static const ink_action_t SHELF_ACTIONS[] = {
    {.id = "reader.list_books", .title = "书架上有哪些书", .effect = INK_FX_READ, .child_ok = true, .run = act_list},
    {.id = "reader.open_book",
     .title = "打开一本书",
     .args = "{\"type\":\"object\",\"properties\":{\"title\":{\"type\":\"string\"}},\"required\":[\"title\"]}",
     .effect = INK_FX_NAVIGATE,
     .child_ok = true,
     .run = act_open},
};



/* ---------------------------------------------------------------- book */

/* starts[k] = byte offset of page k; a final entry == len marks the end of the book. */
static int find_page_start(int page) {
    while (RD.nstarts <= page) {
        uint32_t s0 = RD.starts[RD.nstarts - 1];
        if (s0 >= RD.len) return -1;
        ink_page_t pg;
        size_t next = ink_layout_page(&RD.layout, RD.text, RD.len, s0, &pg);
        if (next <= s0) return -1;
        if (RD.nstarts == RD.cap) {
            int ncap = RD.cap * 2;
            uint32_t *n = osal_realloc(RD.starts, (size_t)ncap * sizeof(uint32_t), OSAL_MEM_DEFAULT);
            if (!n) return -1;
            RD.starts = n;
            RD.cap = ncap;
        }
        RD.starts[RD.nstarts++] = (uint32_t)next;
    }
    return RD.starts[page] < RD.len ? (int)RD.starts[page] : -1;
}

static void arm_once(lv_timer_t *t) {
    lv_timer_set_repeat_count(t, 1);
    lv_timer_reset(t);
    lv_timer_resume(t);
}

static bool render_into(int slot, int page) {
    int start = find_page_start(page);
    if (start < 0) return false;
    uint64_t t0 = osal_now_us();
    ink_page_t pg;
    ink_layout_page(&RD.layout, RD.text, RD.len, (size_t)start, &pg);
    memset(RD.buf[slot], 0xff, (size_t)RD.bw * RD.bh);
    osal_cost_units("psram.copy_byte", (uint64_t)RD.bw * RD.bh / 2); /* clear (write-only) */
    ink_render_page(&RD.layout, RD.text, &pg, RD.buf[slot], RD.bw, RD.bw, RD.bh, 0, 0);
    RD.buf_page[slot] = page;
    ink_probe_value("reader.page_render_ms", (double)(osal_now_us() - t0) / 1000.0);
    return true;
}

static void show_slot(int slot) {
    RD.dsc.data = RD.buf[slot];
    lv_image_set_src(RD.img, &RD.dsc);
    ink_ui_hint_obj(RD.img, INK_REFRESH_TEXT);
    lv_obj_invalidate(RD.img);
    int total_known = RD.nstarts > 0 && RD.starts[RD.nstarts - 1] >= RD.len ? RD.nstarts - 1 : -1;
    if (total_known > 0) lv_label_set_text_fmt(RD.footer, "%d / %d", RD.page + 1, total_known);
    else lv_label_set_text_fmt(RD.footer, "%d", RD.page + 1);
    ink_ui_hint_obj(RD.footer, INK_REFRESH_TEXT);
}

static void ahead_cb(lv_timer_t *t) {
    (void)t;
    int want = RD.page + 1;
    int slot = RD.buf_page[0] == RD.page ? 1 : 0;
    if (RD.buf_page[slot] != want) render_into(slot, want);
}

static void save_cb(lv_timer_t *t) {
    (void)t;
    if (!RD.dirty_pos) return;
    char path[200], data[64];
    snprintf(path, sizeof path, "/user/apps/reader-%s.pos", LIB.books[LIB.open].name);
    int n = snprintf(data, sizeof data, "offset=%u\n", (unsigned)RD.starts[RD.page]);
    if (ink_storage_write_atomic(path, data, (size_t)n) == 0) RD.dirty_pos = false;
}

static bool go_to(int target) {
    if (target < 0) return false;
    int slot = RD.buf_page[0] == target ? 0 : RD.buf_page[1] == target ? 1 : -1;
    if (slot < 0) {
        slot = RD.buf_page[0] == RD.page ? 1 : 0; /* reuse the buffer not on screen */
        if (!render_into(slot, target)) return false; /* past the end of the book */
        ink_probe_count("reader.turns_rendered_on_demand");
    } else {
        ink_probe_count("reader.turns_prerendered");
    }
    RD.page = target;
    show_slot(slot);
    RD.dirty_pos = true;
    ink_probe_count("reader.page_turns");
    arm_once(RD.ahead_timer);
    arm_once(RD.save_timer);
    return true;
}

/* ---- actions: keys, swipes, taps and the AI all turn pages through these ---- */

static void page_receipt(ink_receipt_t *rc) {
    snprintf(rc->object, sizeof rc->object, "book:%s#page=%d", LIB.books[LIB.open].name, RD.page + 1);
    rc->revision = (uint32_t)RD.page + 1;
    snprintf(rc->say, sizeof rc->say, "现在是第 %d 页", RD.page + 1);
}

static void act_next(const ink_json_t *a, int t, ink_receipt_t *rc) {
    (void)a;
    (void)t;
    if (!go_to(RD.page + 1)) {
        rc->status = INK_RC_FAILED;
        snprintf(rc->detail, sizeof rc->detail, "already on the last page");
    }
    page_receipt(rc);
}

static void act_prev(const ink_json_t *a, int t, ink_receipt_t *rc) {
    (void)a;
    (void)t;
    if (!go_to(RD.page - 1)) {
        rc->status = INK_RC_FAILED;
        snprintf(rc->detail, sizeof rc->detail, "already on the first page");
    }
    page_receipt(rc);
}

static void act_goto(const ink_json_t *a, int t, ink_receipt_t *rc) {
    int64_t page;
    if (!a || !ink_json_int(a, ink_json_find(a, t, "page"), &page) || page < 1) {
        rc->status = INK_RC_INVALID;
        snprintf(rc->detail, sizeof rc->detail, "page must be >= 1");
        return;
    }
    if (!go_to((int)page - 1)) {
        rc->status = INK_RC_FAILED;
        snprintf(rc->detail, sizeof rc->detail, "the book has fewer pages");
    }
    page_receipt(rc);
}

static int book_snapshot(char *buf, size_t cap);

static void act_read(const ink_json_t *a, int t, ink_receipt_t *rc) {
    (void)a;
    (void)t;
    page_receipt(rc);
    book_snapshot(rc->say, sizeof rc->say); /* the start of the page; the full text is in the page snapshot */
}

static const ink_action_t BOOK_ACTIONS[] = {
    {.id = "reader.next_page", .title = "下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "book", .run = act_next},
    {.id = "reader.prev_page", .title = "上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "book", .run = act_prev},
    {.id = "reader.goto_page",
     .title = "跳到第几页",
     .args = "{\"type\":\"object\",\"properties\":{\"page\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"page\"]}",
     .effect = INK_FX_NAVIGATE,
     .child_ok = true,
     .activity = "book",
     .run = act_goto},
    {.id = "reader.read_page", .title = "读出这一页", .effect = INK_FX_READ, .child_ok = true, .activity = "book", .run = act_read},
};

static void turn(int delta, ink_source_t src) {
    ink_receipt_t rc;
    ink_action_invoke(delta > 0 ? "reader.next_page" : "reader.prev_page", NULL, src, 0, &rc);
}

static bool book_key(hal_key_t key, bool long_press) {
    if (long_press) return false;
    if (key == HAL_KEY_NEXT || key == HAL_KEY_VOL_DOWN) turn(+1, INK_SRC_KEY);
    else if (key == HAL_KEY_PREV || key == HAL_KEY_VOL_UP) turn(-1, INK_SRC_KEY);
    else return false;
    return true;
}

static bool book_swipe(ink_swipe_dir_t dir) {
    if (dir == INK_SWIPE_LEFT) turn(+1, INK_SRC_TOUCH);
    else if (dir == INK_SWIPE_RIGHT) turn(-1, INK_SRC_TOUCH);
    else return false;
    return true;
}

static void page_clicked(lv_event_t *e) {
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    lv_area_t a;
    lv_obj_get_coords(lv_event_get_target_obj(e), &a);
    int w = lv_area_get_width(&a);
    if (p.x - a.x1 < w / 3) turn(-1, INK_SRC_TOUCH);
    else turn(+1, INK_SRC_TOUCH);
}

static bool load_text(const char *path) {
    int64_t size = hal_fs_size(path);
    if (size <= 0 || size > MAX_BOOK_BYTES) return false;
    const char *prev = osal_mem_tag("reader-text");
    RD.text = osal_malloc((size_t)size + 1, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!RD.text) return false;
    hal_file_t *f = hal_fs_open(path, HAL_O_READ);
    if (!f) return false;
    size_t got = 0;
    while (got < (size_t)size) {
        ssize_t n = hal_fs_read(f, RD.text + got, (size_t)size - got > 32768 ? 32768 : (size_t)size - got);
        if (n <= 0) break;
        got += (size_t)n;
    }
    hal_fs_close(f);
    RD.len = got;
    RD.text[got] = 0;
    return got > 0;
}

static void book_destroy(void);

static void book_create(lv_obj_t *scr, void *arg) {
    LIB.open = (int)(intptr_t)arg;
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    uint64_t t0 = osal_now_us();
    memset(&RD, 0, sizeof RD);
    RD.buf_page[0] = RD.buf_page[1] = -1;
    ink_font_t *body = ink_font_for_pt(8, pi.dpi);
    ink_font_t *small = ink_font_for_pt(6, pi.dpi);
    if (!body || !load_text(LIB.books[LIB.open].path)) {
        lv_obj_t *l = lv_label_create(scr);
        lv_label_set_text(l, "cannot open book");
        return;
    }
    int lw = lv_display_get_horizontal_resolution(NULL), lh = lv_display_get_vertical_resolution(NULL);
    int margin = ink_dp(14), header = ink_dp(28), footer = ink_dp(28);
    RD.bw = lw - 2 * margin;
    RD.bh = lh - header - footer;
    RD.layout.font = body;
    RD.layout.width = RD.bw - ink_font_px(body) / 2; /* room for hanging punctuation */
    RD.layout.height = RD.bh;
    RD.layout.line_gap = ink_font_px(body) / 3;
    RD.layout.para_gap = ink_font_px(body) / 4;
    RD.layout.indent_em = 2;
    const char *prev = osal_mem_tag("reader-pages");
    RD.cap = 256;
    RD.starts = osal_malloc((size_t)RD.cap * sizeof(uint32_t), OSAL_MEM_DEFAULT);
    RD.buf[0] = osal_malloc((size_t)RD.bw * RD.bh, OSAL_MEM_EXTERNAL);
    RD.buf[1] = osal_malloc((size_t)RD.bw * RD.bh, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!RD.starts || !RD.buf[0] || !RD.buf[1]) osal_panic("reader: out of memory for page buffers");
    RD.starts[0] = 0;
    RD.nstarts = 1;

    /* restore the saved position */
    char path[200], data[64] = {0};
    snprintf(path, sizeof path, "/user/apps/reader-%s.pos", LIB.books[LIB.open].name);
    unsigned saved = 0;
    if (ink_storage_read(path, data, sizeof data - 1) > 0) sscanf(data, "offset=%u", &saved);
    while (saved && find_page_start(RD.page + 1) >= 0 && RD.starts[RD.page + 1] <= saved) RD.page++;
    ink_probe_value("reader.restored_page", (double)RD.page);

    RD.title = lv_label_create(scr);
    if (small) lv_obj_set_style_text_font(RD.title, ink_font_lvgl(small), 0);
    lv_label_set_text(RD.title, LIB.books[LIB.open].name);
    lv_obj_align(RD.title, LV_ALIGN_TOP_MID, 0, ink_dp(6));

    RD.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    RD.dsc.header.cf = LV_COLOR_FORMAT_L8;
    RD.dsc.header.w = (uint32_t)RD.bw;
    RD.dsc.header.h = (uint32_t)RD.bh;
    RD.dsc.header.stride = (uint32_t)RD.bw;
    RD.dsc.data_size = (uint32_t)(RD.bw * RD.bh);
    RD.img = lv_image_create(scr);
    lv_obj_set_pos(RD.img, margin, header);
    lv_obj_add_flag(RD.img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(RD.img, page_clicked, LV_EVENT_CLICKED, NULL);

    RD.footer = lv_label_create(scr);
    if (small) lv_obj_set_style_text_font(RD.footer, ink_font_lvgl(small), 0);
    lv_obj_align(RD.footer, LV_ALIGN_BOTTOM_MID, 0, -ink_dp(6));

    render_into(0, RD.page);
    show_slot(0);
    RD.ahead_timer = lv_timer_create(ahead_cb, 60, NULL);
    lv_timer_set_repeat_count(RD.ahead_timer, 1);
    lv_timer_set_auto_delete(RD.ahead_timer, false);
    RD.save_timer = lv_timer_create(save_cb, SAVE_AFTER_IDLE_MS, NULL);
    lv_timer_set_repeat_count(RD.save_timer, 1);
    lv_timer_set_auto_delete(RD.save_timer, false);
    lv_timer_pause(RD.save_timer);
    ink_probe_value("reader.open_ms", (double)(osal_now_us() - t0) / 1000.0);
}

static void book_destroy(void) {
    if (RD.dirty_pos) save_cb(NULL); /* queued; written when the panel is idle */
    if (RD.ahead_timer) lv_timer_delete(RD.ahead_timer);
    if (RD.save_timer) lv_timer_delete(RD.save_timer);
    osal_free(RD.buf[0]);
    osal_free(RD.buf[1]);
    osal_free(RD.starts);
    osal_free(RD.text);
    memset(&RD, 0, sizeof RD);
}

static int book_snapshot(char *buf, size_t cap) {
    if (!RD.text || !cap) return 0;
    size_t a = RD.starts[RD.page];
    size_t b = RD.page + 1 < RD.nstarts ? RD.starts[RD.page + 1] : RD.len;
    if (b > RD.len) b = RD.len;
    size_t n = b > a ? b - a : 0;
    if (n >= cap) n = cap - 1;
    while (n && ((unsigned char)RD.text[a + n] & 0xC0) == 0x80) n--; /* do not cut a UTF-8 character */
    memcpy(buf, RD.text + a, n);
    buf[n] = 0;
    return (int)n;
}

const ink_activity_t ink_app_book = {.name = "book",
                                     .create = book_create,
                                     .destroy = book_destroy,
                                     .on_key = book_key,
                                     .on_swipe = book_swipe,
                                     .snapshot = book_snapshot};

int ink_reader_actions_init(void) {
    int r = ink_actions_register(SHELF_ACTIONS, (int)(sizeof SHELF_ACTIONS / sizeof SHELF_ACTIONS[0]));
    return r ? r : ink_actions_register(BOOK_ACTIONS, (int)(sizeof BOOK_ACTIONS / sizeof BOOK_ACTIONS[0]));
}
