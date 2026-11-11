/*
 * Reader: library of the .txt books in /content/books and a paginated reading view.
 *
 * Page turn path: the next page is rendered ahead of time into a second PSRAM
 * buffer while the reader looks at the current one, so a turn is a buffer
 * swap + one TEXT refresh. A turn to a page that is not rendered yet only
 * moves the target; the page is rendered when the queued input is handled
 * (settle), so a burst of turns renders its last page only. Pages are found
 * lazily (page start offsets grow as you read). The reading position is saved through the storage service,
 * which waits for an idle panel (flash writes freeze PSRAM and would break a
 * waveform).
 */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "ink/probe.h"
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/cap.h"
#include "ink/sys/crc.h"
#include "ink/sys/font.h"
#include "ink/sys/layout.h"
#include "ink/sys/members.h"
#include "ink/sys/storage.h"
#include "ink/sys/state.h"
#include "ink/sys/text.h"
#include "ink/sys/ui.h"

#define MAX_BOOKS 16
#define MAX_BOOK_BYTES (2u * 1024u * 1024u)
#define SAVE_AFTER_IDLE_MS 8000
#define SAVE_AT_LEAST_EVERY_MS 60000 /* SR-PL-08: reading on without a pause loses at most this much */

typedef struct {
    char name[96];
    char path[160];
} book_t;

static struct {
    book_t books[MAX_BOOKS + 1];
    int nbooks;
    int open; /* index of the open book */
} LIB INK_PSRAM_BSS;

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
    int shown;         /* buffer on screen (-1 none) */
    bool show_pending; /* `page` is not on screen yet: rendered and shown in book_settle */
    lv_image_dsc_t dsc;
    lv_obj_t *img, *footer, *title;
    lv_timer_t *ahead_timer, *save_timer;
    bool dirty_pos;
    uint64_t dirty_since_us; /* first turn not saved yet */
    char member[16];         /* whose reading position this is (the member who opened the book) */
} RD;

/* Reading positions are kept per member (foundation §5.5, D-03): one small file per member and book,
 * in the member's data area, named after the book's checksum (titles can be long). */
static void pos_path(char *out, size_t cap, const char *member, const char *book) {
    char name[32];
    snprintf(name, sizeof name, "reader.%08x.pos", (unsigned)ink_crc32(0, book, strlen(book)));
    if (ink_layout_member(out, cap, member, name) < 0 && cap) out[0] = 0;
}

/* The book each member read last and where (the "continue reading" card), so it outlives the open book:
 * reader.last in the member's data area, mirrored here for the current member (cards are read often). */
static struct {
    char member[16];
    char book[96];
    int page;
    unsigned offset;
} LAST;

/* A flash read must not hold up drawing home: the file is prefetched into the read cache and read a little
 * later, when it is there; the card shows once it is in. */
static lv_timer_t *last_timer;

static void last_read(lv_timer_t *t) {
    (void)t;
    last_timer = NULL;
    char path[200], data[160] = {0};
    const char *member = ink_member_current()->id;
    if (ink_layout_member(path, sizeof path, member, "reader.last") < 0) return;
    ink_storage_read(path, data, sizeof data - 1);
    memset(&LAST, 0, sizeof LAST);
    snprintf(LAST.member, sizeof LAST.member, "%s", member);
    if (sscanf(data, "page=%d\nbook=%95[^\n]", &LAST.page, LAST.book) != 2) LAST.book[0] = 0;
    const char *offset = strstr(data, "\noffset=");
    if (offset) sscanf(offset, "\noffset=%u", &LAST.offset);
    ink_app_card_changed("reader");
}

static void last_load(const char *member) {
    char path[200];
    if (!strcmp(LAST.member, member) || last_timer || ink_layout_member(path, sizeof path, member, "reader.last") < 0) return;
    ink_storage_prefetch(path);
    last_timer = lv_timer_create(last_read, 300, NULL);
    lv_timer_set_repeat_count(last_timer, 1);
}

static void last_save(const char *member, const char *book, int page) {
    char path[200], data[160];
    unsigned offset = RD.starts[RD.page];
    int n = snprintf(data, sizeof data, "page=%d\nbook=%.95s\noffset=%u\n", page, book, offset);
    if (ink_layout_member(path, sizeof path, member, "reader.last") < 0 || n <= 0 || (size_t)n >= sizeof data) return;
    if (!strcmp(LAST.member, member) && !strcmp(LAST.book, book) && LAST.page == page) return; /* as it is on the card */
    ink_probe_count("reader.last_saved");
    if (ink_storage_write_member_atomic(path, data, (size_t)n) != 0) return;
    snprintf(LAST.member, sizeof LAST.member, "%s", member);
    snprintf(LAST.book, sizeof LAST.book, "%s", book);
    LAST.page = page;
    LAST.offset = offset;
    ink_app_card_changed("reader");
}

/* ------------------------------------------------------------- library */

static void scan_library(void) {
    LIB.nbooks = 0;
    ink_dir_t *d = ink_dir_open("/content/books");
    char name[128];
    bool is_dir;
    while (d && LIB.nbooks < MAX_BOOKS && ink_dir_next(d, name, sizeof name, &is_dir) == 1) {
        size_t n = strlen(name);
        if (is_dir || n < 5 || strcmp(name + n - 4, ".txt")) continue;
        book_t *b = &LIB.books[LIB.nbooks++];
        snprintf(b->name, sizeof b->name, "%.*s", (int)(n - 4), name);
        snprintf(b->path, sizeof b->path, "/content/books/%s", name);
    }
    ink_dir_close(d);
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
        pos_path(pos, sizeof pos, ink_member_current()->id, LIB.books[i].name);
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
    int dpi = ink_panel_dpi();
    ink_font_t *title_font = ink_font_for_pt(14, dpi);
    lv_obj_t *t = lv_label_create(scr);
    if (title_font) lv_obj_set_style_text_font(t, ink_font_lvgl(title_font), 0);
    lv_label_set_text(t, "书架");
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, ink_dp(16), ink_dp(12));
    scan_library();
    ink_font_t *f = ink_font_for_pt(10.5, dpi);
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

const ink_activity_t ink_app_reader = {.name = "reader", .app = "reader", .create = library_create};

static void act_list(ink_call_t *c) {
    ink_receipt_t *rc = &c->rc;
    if (!LIB.nbooks) scan_library();
    size_t n = (size_t)snprintf(rc->say, sizeof rc->say, "书架上有：");
    for (int i = 0; i < LIB.nbooks && n < sizeof rc->say; i++)
        n += (size_t)snprintf(rc->say + n, sizeof rc->say - n, "%s%s", i ? "、" : "", LIB.books[i].name);
    snprintf(rc->object, sizeof rc->object, "shelf:books");
}

static bool last_book(char *book, size_t cap, int *page, int *percent);
static void act_sample(ink_call_t *c) {
    snprintf(LIB.books[MAX_BOOKS].name, sizeof LIB.books[MAX_BOOKS].name, "夜邮（示例）");
    snprintf(LIB.books[MAX_BOOKS].path, sizeof LIB.books[MAX_BOOKS].path, "sample/welcome.txt");
    ink_ui_push(&ink_app_book, (void *)(intptr_t)MAX_BOOKS);
    snprintf(c->rc.object, sizeof c->rc.object, "book:sample");
}

/* Straight back to the book read last, at its saved page (the card on home and the AI). */
static void act_continue(ink_call_t *c) {
    ink_receipt_t *rc = &c->rc;
    char book[96];
    int page;
    scan_library();
    bool have = last_book(book, sizeof book, &page, NULL);
    if (have && !strcmp(book, "夜邮（示例）")) { act_sample(c); return; }
    for (int i = 0; have && i < LIB.nbooks; i++)
        if (!strcmp(LIB.books[i].name, book)) {
            ink_ui_push(&ink_app_book, (void *)(intptr_t)i);
            snprintf(rc->object, sizeof rc->object, "book:%s", LIB.books[i].name);
            snprintf(rc->say, sizeof rc->say, "接着读《%s》", LIB.books[i].name);
            return;
        }
    rc->status = INK_RC_FAILED;
    snprintf(rc->detail, sizeof rc->detail, "no book to continue on the shelf");
}

static void act_open(ink_call_t *c) {
    ink_receipt_t *rc = &c->rc;
    char title[96];
    ink_args_get_str(&c->args, "title", title, sizeof title); /* required by the schema */
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

static const ink_cap_t SHELF_ACTIONS[] = {
    {.id = "reader.open_sample", .app = "reader", .title = "打开离线原创阅读示例", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = act_sample},
    {.id = "reader.list_books", .app = "reader", .title = "书架上有哪些书", .effect = INK_FX_READ, .child_ok = true, .run = act_list},
    {.id = "reader.open_book", .app = "reader",
     .title = "打开一本书",
     .input = "{\"type\":\"object\",\"properties\":{\"title\":{\"type\":\"string\"}},\"required\":[\"title\"]}",
     .effect = INK_FX_NAVIGATE,
     .child_ok = true,
     .run = act_open},
    {.id = "reader.continue", .app = "reader", .title = "接着读上次的书", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = act_continue},
};



/* ---------------------------------------------------------------- book */

/* The text is loaded and paginated from the start (an empty or unreadable file leaves the book closed). */
static bool book_ready(void) { return RD.text && RD.starts && RD.nstarts > 0; }

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
            uint32_t *n = ink_app_realloc(RD.starts, (size_t)ncap * sizeof(uint32_t));
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
    uint64_t t0 = ink_now_us();
    ink_page_t pg;
    ink_layout_page(&RD.layout, RD.text, RD.len, (size_t)start, &pg);
    memset(RD.buf[slot], 0xff, (size_t)RD.bw * RD.bh);
    ink_app_cost("psram.copy_byte", (uint64_t)RD.bw * RD.bh / 2); /* clear (write-only) */
    ink_render_page(&RD.layout, RD.text, &pg, RD.buf[slot], RD.bw, RD.bw, RD.bh, 0, 0);
    RD.buf_page[slot] = page;
    ink_probe_value("reader.page_render_ms", (double)(ink_now_us() - t0) / 1000.0);
    return true;
}

static void show_slot(int slot) {
    RD.shown = slot;
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
    if (RD.show_pending) return;
    if (ink_ui_queue_pending()) { /* input first (more turns may follow): render ahead when it is quiet */
        arm_once(RD.ahead_timer);
        return;
    }
    int want = RD.page + 1;
    int slot = RD.shown == 0 ? 1 : 0; /* the buffer not on screen */
    if (RD.buf_page[slot] != want) render_into(slot, want);
}

static void save_cb(lv_timer_t *t) {
    (void)t;
    if (!ink_member_exists(RD.member)) return;
    last_save(RD.member, LIB.books[LIB.open].name, RD.page + 1); /* also the book just opened */
    if (!RD.dirty_pos) return;
    char path[200], data[64];
    pos_path(path, sizeof path, RD.member, LIB.books[LIB.open].name);
    int n = snprintf(data, sizeof data, "offset=%u\n", (unsigned)RD.starts[RD.page]);
    if (path[0] && ink_storage_write_member_atomic(path, data, (size_t)n) == 0) RD.dirty_pos = false;
}

static bool go_to(int target) {
    if (target < 0 || !book_ready() || find_page_start(target) < 0) return false; /* past the end of the book */
    int slot = RD.buf_page[0] == target ? 0 : RD.buf_page[1] == target ? 1 : -1;
    if (RD.show_pending) ink_probe_count("reader.turns_coalesced"); /* the page waiting to be drawn is skipped */
    RD.page = target;
    ink_ui_content_changed(); /* a tool call prepared for the old page no longer applies (F-6) */
    if (slot >= 0) {
        RD.show_pending = false;
        show_slot(slot);
        ink_probe_count("reader.turns_prerendered");
    } else {
        RD.show_pending = true; /* rendered once the queued input is handled (book_settle) */
    }
    if (!RD.dirty_pos) RD.dirty_since_us = ink_now_us();
    RD.dirty_pos = true;
    ink_probe_count("reader.page_turns");
    arm_once(RD.ahead_timer);
    /* saved after a pause in reading; the idle timer restarts on every turn, so reading on without
     * a pause saves at least every SAVE_AT_LEAST_EVERY_MS (the write itself waits for an idle panel) */
    if (ink_now_us() - RD.dirty_since_us >= SAVE_AT_LEAST_EVERY_MS * 1000ull) save_cb(NULL);
    else arm_once(RD.save_timer);
    return true;
}

/* The input queued up is handled (the UI task draws next): render the page the turns ended on. */
static void book_settle(void) {
    if (!RD.show_pending) return;
    RD.show_pending = false;
    int slot = RD.shown == 0 ? 1 : 0; /* the buffer not on screen */
    if (!render_into(slot, RD.page)) return;
    show_slot(slot);
    ink_probe_count("reader.turns_rendered_on_demand");
}

/* ---- actions: keys, swipes, taps and the AI all turn pages through these ---- */

static bool not_open(ink_receipt_t *rc) {
    if (book_ready()) return false;
    rc->status = INK_RC_FAILED;
    snprintf(rc->detail, sizeof rc->detail, "the book could not be opened");
    return true;
}

static void page_receipt(ink_receipt_t *rc) {
    snprintf(rc->object, sizeof rc->object, "book:%s#page=%d", LIB.books[LIB.open].name, RD.page + 1);
    rc->revision = (uint32_t)RD.page + 1;
    snprintf(rc->say, sizeof rc->say, "现在是第 %d 页", RD.page + 1);
}

static void act_next(ink_call_t *c) {
    ink_receipt_t *rc = &c->rc;
    if (not_open(rc)) return;
    if (!go_to(RD.page + 1)) {
        rc->status = INK_RC_FAILED;
        snprintf(rc->detail, sizeof rc->detail, "already on the last page");
    }
    page_receipt(rc);
}

static void act_prev(ink_call_t *c) {
    ink_receipt_t *rc = &c->rc;
    if (not_open(rc)) return;
    if (!go_to(RD.page - 1)) {
        rc->status = INK_RC_FAILED;
        snprintf(rc->detail, sizeof rc->detail, "already on the first page");
    }
    page_receipt(rc);
}

static void act_goto(ink_call_t *c) {
    ink_receipt_t *rc = &c->rc;
    int64_t page;
    if (not_open(rc)) return;
    if (!ink_args_get_int(&c->args, "page", &page) || page < 1 || page > INT_MAX) {
        rc->status = INK_RC_INVALID;
        snprintf(rc->detail, sizeof rc->detail, "page must be an integer >= 1");
        return;
    }
    if (!go_to((int)page - 1)) {
        rc->status = INK_RC_FAILED;
        snprintf(rc->detail, sizeof rc->detail, "the book has fewer pages");
    }
    page_receipt(rc);
}

static int book_snapshot(char *buf, size_t cap);

static void act_read(ink_call_t *c) {
    ink_receipt_t *rc = &c->rc;
    if (not_open(rc)) return;
    page_receipt(rc);
    book_snapshot(rc->say, sizeof rc->say); /* the start of the page; the full text is in the page snapshot */
}

static const ink_cap_t BOOK_ACTIONS[] = {
    {.id = "reader.next_page", .app = "reader", .title = "下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "book", .run = act_next},
    {.id = "reader.prev_page", .app = "reader", .title = "上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "book", .run = act_prev},
    {.id = "reader.goto_page", .app = "reader",
     .title = "跳到第几页",
     .input = "{\"type\":\"object\",\"properties\":{\"page\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"page\"]}",
     .effect = INK_FX_NAVIGATE,
     .child_ok = true,
     .activity = "book",
     .run = act_goto},
    {.id = "reader.read_page", .app = "reader", .title = "读出这一页", .effect = INK_FX_READ, .child_ok = true, .activity = "book", .run = act_read},
};

static void turn(int delta, ink_source_t src) {
    ink_receipt_t rc;
    ink_cap_invoke(delta > 0 ? "reader.next_page" : "reader.prev_page", NULL, src, 0, &rc);
}

/* Pages turn with the cover keys; the volume keys only change the volume (D-19). */
static bool book_key(ink_key_t key, bool long_press) {
    if (long_press) return false;
    if (key == INK_KEY_NEXT) turn(+1, INK_SRC_KEY);
    else if (key == INK_KEY_PREV) turn(-1, INK_SRC_KEY);
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
    bool sample = !strcmp(path, "sample/welcome.txt");
    int64_t size = sample ? ink_app_asset_read(path, NULL, 0) : ink_file_size(path);
    if (size <= 0 || size > MAX_BOOK_BYTES) return false;
    const char *prev = ink_app_tag("reader-text");
    RD.text = ink_app_alloc((size_t)size + 1, true);
    ink_app_tag(prev);
    if (!RD.text) return false;
    if (sample) {
        if (ink_app_asset_read(path, RD.text, (size_t)size) != size) return false;
        RD.len = (size_t)size; RD.text[size] = 0;
        return true;
    }
    ink_file_t *f = ink_file_open(path);
    if (!f) return false;
    size_t got = 0;
    while (got < (size_t)size) {
        int64_t n = ink_file_read(f, RD.text + got, (size_t)size - got > 32768 ? 32768 : (size_t)size - got);
        if (n <= 0) break;
        got += (size_t)n;
    }
    ink_file_close(f);
    RD.len = got;
    RD.text[got] = 0;
    return got > 0;
}

static void book_destroy(void);

static void book_create(lv_obj_t *scr, void *arg) {
    LIB.open = (int)(intptr_t)arg;
    int dpi = ink_panel_dpi();
    uint64_t t0 = ink_now_us();
    memset(&RD, 0, sizeof RD);
    RD.buf_page[0] = RD.buf_page[1] = -1;
    RD.shown = -1;
    snprintf(RD.member, sizeof RD.member, "%s", ink_member_current()->id);
    
    ink_font_t *body = ink_font_for_pt(10.5, dpi); /* wu hao, about 3.8 mm on this panel */
    if (!body || !load_text(LIB.books[LIB.open].path)) {
        lv_obj_t *l = lv_label_create(scr);
        lv_label_set_text(l, "cannot open book");
        return;
    }
    int lw = lv_display_get_horizontal_resolution(NULL), lh = lv_display_get_vertical_resolution(NULL);
    int margin = ink_dp(20), header = lv_font_get_line_height(ink_ui_font(2)) + 2 * ink_dp(6), footer = header;
    RD.bw = lw - 2 * margin;
    RD.bh = lh - header - footer;
    RD.layout.font = body;
    RD.layout.width = RD.bw - ink_font_px(body) / 2; /* room for hanging punctuation */
    RD.layout.height = RD.bh;
    RD.layout.line_gap = ink_font_px(body) / 3;
    RD.layout.para_gap = ink_font_px(body) / 4;
    RD.layout.indent_em = 2;
    const char *prev = ink_app_tag("reader-pages");
    RD.cap = 256;
    RD.starts = ink_app_alloc((size_t)RD.cap * sizeof(uint32_t), false);
    RD.buf[0] = ink_app_alloc((size_t)RD.bw * RD.bh, true);
    RD.buf[1] = ink_app_alloc((size_t)RD.bw * RD.bh, true);
    ink_app_tag(prev);
    if (!RD.starts || !RD.buf[0] || !RD.buf[1]) ink_app_fail("reader: out of memory for page buffers");
    RD.starts[0] = 0;
    RD.nstarts = 1;

    /* restore the saved position */
    char path[200], data[64] = {0};
    pos_path(path, sizeof path, RD.member, LIB.books[LIB.open].name);
    unsigned saved = 0;
    if (path[0] && ink_storage_read(path, data, sizeof data - 1) > 0) sscanf(data, "offset=%u", &saved);
    while (saved && find_page_start(RD.page + 1) >= 0 && RD.starts[RD.page + 1] <= saved) RD.page++;
    ink_probe_value("reader.restored_page", (double)RD.page);

    RD.title = lv_label_create(scr);
    lv_obj_set_style_text_font(RD.title, ink_ui_font(2), 0);
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
    lv_obj_set_style_text_font(RD.footer, ink_ui_font(2), 0);
    lv_obj_align(RD.footer, LV_ALIGN_BOTTOM_MID, 0, -ink_dp(6));

    render_into(0, RD.page);
    show_slot(0);
    RD.ahead_timer = lv_timer_create(ahead_cb, 60, NULL);
    lv_timer_set_repeat_count(RD.ahead_timer, 1);
    lv_timer_set_auto_delete(RD.ahead_timer, false);
    RD.save_timer = lv_timer_create(save_cb, SAVE_AFTER_IDLE_MS, NULL);
    lv_timer_set_repeat_count(RD.save_timer, 1);
    lv_timer_set_auto_delete(RD.save_timer, false);
    arm_once(RD.save_timer); /* this book is now the one to continue: written after a pause, not here */
    ink_probe_value("reader.open_ms", (double)(ink_now_us() - t0) / 1000.0);
}

static void book_destroy(void) {
    if (RD.save_timer) save_cb(NULL); /* queued; written when the panel is idle */
    if (RD.ahead_timer) lv_timer_delete(RD.ahead_timer);
    if (RD.save_timer) lv_timer_delete(RD.save_timer);
    ink_app_free(RD.buf[0]);
    ink_app_free(RD.buf[1]);
    ink_app_free(RD.starts);
    ink_app_free(RD.text);
    memset(&RD, 0, sizeof RD);
}

static int book_snapshot(char *buf, size_t cap) {
    if (!book_ready() || !cap) return 0;
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
                                     .app = "reader",
                                     .create = book_create,
                                     .destroy = book_destroy,
                                     .on_key = book_key,
                                     .on_swipe = book_swipe,
                                     .snapshot = book_snapshot,
                                     .settle = book_settle};

/* The current member's book to continue: the open one, else the saved one if it is still on the card. */
static bool last_book(char *book, size_t cap, int *page, int *percent) {
    const char *member = ink_member_current()->id;
    if (RD.text && !strcmp(RD.member, member)) {
        snprintf(book, cap, "%s", LIB.books[LIB.open].name);
        *page = RD.page + 1;
        if (percent) *percent = (int)(RD.starts[RD.page] * 100ull / RD.len);
        return true;
    }
    last_load(member);
    char path[160];
    if (strcmp(LAST.member, member)) return false; /* still being read */
    snprintf(path, sizeof path, "/content/books/%.95s.txt", LAST.book);
    int64_t bytes = LAST.book[0] ? ink_file_size(path) : 0;
    if (!strcmp(LAST.book, "夜邮（示例）")) bytes = ink_app_asset_read("sample/welcome.txt", NULL, 0);
    if (bytes <= 0) return false; /* deleted, or the card is out */
    snprintf(book, cap, "%s", LAST.book);
    *page = LAST.page;
    if (percent) *percent = (int)((LAST.offset < (uint64_t)bytes ? LAST.offset : (uint64_t)bytes) * 100ull / (uint64_t)bytes);
    return true;
}

/* What is being read: the card on home, and for the AI. */
static int reader_card(int index, ink_card_t *out) {
    char book[96];
    int page, percent;
    if (index != 0 || !last_book(book, sizeof book, &page, &percent)) return 0;
    snprintf(out->id, sizeof out->id, "reading");
    snprintf(out->title, sizeof out->title, "继续读");
    snprintf(out->text, sizeof out->text, "《%.60s》第 %d 页 · %d%%", book, page, percent);
    snprintf(out->action, sizeof out->action, "reader.continue");
    snprintf(out->member, sizeof out->member, "%s", ink_member_current()->id);
    out->value = page;
    out->has_value = true;
    out->bookmark = true;
    return 1;
}

const ink_app_def_t ink_appdef_reader = {.id = "reader", .title = "阅读", .entry = &ink_app_reader, .system_app = true, .child_ok = true, .card = reader_card};

static void card_changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id; ink_app_card_changed("reader");
}
int ink_reader_actions_init(void) {
    int r = ink_cap_register(SHELF_ACTIONS, (int)(sizeof SHELF_ACTIONS / sizeof SHELF_ACTIONS[0]));
    return r ? r : ink_cap_register(BOOK_ACTIONS, (int)(sizeof BOOK_ACTIONS / sizeof BOOK_ACTIONS[0])) ||
        ink_state_watch("storage.card", card_changed, NULL, INK_EXEC_UI);
}
