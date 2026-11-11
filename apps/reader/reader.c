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
#include "ink/sys/i18n.h"
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

#include <reader_source.h>
#include <ink/ui_theme.h>
#include "ink/sys/ui_list.h"
#include "ink/sys/json.h"

#define LIST_ROWS 4
#define MAX_BOOK_BYTES (2u * 1024u * 1024u)
#define SAVE_AFTER_IDLE_MS 8000
#define SAVE_AT_LEAST_EVERY_MS 60000 /* SR-PL-08: reading on without a pause loses at most this much */

typedef struct {
    char name[96];
    char path[160];
    char key[96];
} book_t;

static struct {
    book_t *books;
    int nbooks;
    int open; /* index of the open book */
} LIB INK_PSRAM_BSS;

static struct { int chapter; uint64_t offset; bool pending; } JUMP;
static struct {
    ink_reader_source_t *source;
    ink_reader_info_t info;
    int chapter;
    int page_base;
    uint64_t chunk;
    bool more;
    ink_reader_error_t error;
    char chapter_title[128];
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
static bool book_ready(void);
static const char *source_error(ink_reader_error_t error);
static void book_object(ink_receipt_t *rc, const char *name, const char *suffix) {
    size_t tail=strlen(suffix), limit=sizeof rc->object-6;
    if (tail > limit) tail=limit;
    size_t n=strlen(name);
    if (n > limit-tail) n=limit-tail;
    while (n && ((unsigned char)name[n]&0xc0)==0x80) n--;
    memcpy(rc->object,"book:",5); memcpy(rc->object+5,name,n);
    memcpy(rc->object+5+n,suffix,tail); rc->object[5+n+tail]=0;
}

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
    char file[96];
    int chapter;
} LAST;

/* A flash read must not hold up drawing home: the file is prefetched into the read cache and read a little
 * later, when it is there; the card shows once it is in. */
static lv_timer_t *last_timer;

static void last_read(lv_timer_t *t) {
    (void)t;
    last_timer = NULL;
    char path[200], data[320] = {0};
    const char *member = ink_member_current()->id;
    if (ink_layout_member(path, sizeof path, member, "reader.last") < 0) return;
    ink_storage_read(path, data, sizeof data - 1);
    memset(&LAST, 0, sizeof LAST);
    snprintf(LAST.member, sizeof LAST.member, "%s", member);
    if (sscanf(data, "page=%d\nbook=%95[^\n]", &LAST.page, LAST.book) != 2) LAST.book[0] = 0;
    const char *offset = strstr(data, "\noffset=");
    if (offset) sscanf(offset, "\noffset=%u", &LAST.offset);
    const char *file = strstr(data, "\nfile=");
    if (file) sscanf(file, "\nfile=%95[^\n]", LAST.file);
    const char *chapter = strstr(data,"\nchapter=");
    if (chapter) sscanf(chapter,"\nchapter=%d",&LAST.chapter);
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
    char path[200], data[320];
    unsigned offset = RD.starts[RD.page];
    int n = snprintf(data, sizeof data, "page=%d\nbook=%.95s\noffset=%u\nfile=%.95s\nchapter=%d\n", page, book, offset, LIB.books[LIB.open].key,RD.chapter);
    if (ink_layout_member(path, sizeof path, member, "reader.last") < 0 || n <= 0 || (size_t)n >= sizeof data) return;
    if (!strcmp(LAST.member,member) && !strcmp(LAST.book,book) && LAST.page==page && LAST.chapter==RD.chapter && !strcmp(LAST.file,LIB.books[LIB.open].key)) return;
    ink_probe_count("reader.last_saved");
    if (ink_storage_write_member_atomic(path, data, (size_t)n) != 0) return;
    snprintf(LAST.member, sizeof LAST.member, "%s", member);
    snprintf(LAST.book, sizeof LAST.book, "%s", book);
    LAST.page = page;
    LAST.offset = offset;
    LAST.chapter = RD.chapter;
    snprintf(LAST.file, sizeof LAST.file, "%s", LIB.books[LIB.open].key);
    ink_app_card_changed("reader");
}

/* ------------------------------------------------------------- library */

static void scan_library(void) {
    LIB.nbooks = 0;
    ink_dir_t *d = ink_dir_open("/content/books");
    char name[128]; bool is_dir;
    while (d && ink_dir_next(d, name, sizeof name, &is_dir) == 1) {
        char *dot = strrchr(name, '.');
        bool epub = dot && !strcmp(dot, ".epub");
        if (is_dir || !dot || (!epub && strcmp(dot, ".txt"))) continue;
        size_t bytes = (size_t)(LIB.nbooks + 2) * sizeof(book_t);
        book_t *next = LIB.books ? ink_app_realloc(LIB.books,bytes) : ink_app_alloc(bytes,true);
        if (!next) break;
        LIB.books = next;
        book_t *book = &LIB.books[LIB.nbooks++];
        snprintf(book->name, sizeof book->name, "%.*s", (int)(dot-name), name);
        snprintf(book->key, sizeof book->key, "%.*s", epub ? (int)strlen(name) : (int)(dot-name), name);
        snprintf(book->path, sizeof book->path, "/content/books/%s", name);
        if (epub) {
            ink_reader_source_t *source = NULL; ink_reader_info_t info;
            if (ink_reader_source_open(book->path, &source, &info) == INK_READER_OK) snprintf(book->name,sizeof book->name,"%.95s",info.title);
            ink_reader_source_close(source);
        }
    }
    ink_dir_close(d);
    for (int i = 0; i < LIB.nbooks; i++)
        for (int j = i + 1; j < LIB.nbooks; j++)
            if (strcmp(LIB.books[j].name, LIB.books[i].name) < 0) {
                book_t t = LIB.books[i]; LIB.books[i] = LIB.books[j]; LIB.books[j] = t;
            }
    for (int i = 0; i < LIB.nbooks; i++) {
        char pos[200]; pos_path(pos,sizeof pos,ink_member_current()->id,LIB.books[i].key); ink_storage_prefetch(pos);
    }
}

extern const ink_activity_t ink_app_book;
static const ink_activity_t CHAPTERS;
static struct {
    ink_ui_list_t *view;
    ink_ui_list_position_t position;
    ink_reader_source_t *source;
    ink_reader_info_t info;
    char ids[LIST_ROWS][96], args[LIST_ROWS][640], titles[LIST_ROWS][128], values[LIST_ROWS][64];
    char member[16]; bool toc;
} LIST;
_Static_assert(LIST_ROWS <= 16,"list capacity exceeds the shared component");
static void command(lv_event_t *e) {
    ink_receipt_t rc; ink_cap_invoke(lv_event_get_user_data(e),NULL,INK_SRC_TOUCH,0,&rc);
}
static void bind_row(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned row = (unsigned)(index % LIST_ROWS); ink_reader_entry_t entry = {0};
    if (LIST.toc) ink_reader_source_toc(LIST.source,(int)index,&entry);
    snprintf(LIST.titles[row],sizeof LIST.titles[row],"%s",LIST.toc ? entry.title : LIB.books[index].name);
    const char *path=LIST.toc ? LIB.books[LIB.open].path : LIB.books[index].path;
    uint32_t identity=ink_crc32(0,path,strlen(path));
    snprintf(LIST.ids[row],sizeof LIST.ids[row],"reader:%08x:%d:%llu",(unsigned)identity,LIST.toc ? entry.chapter : 0,(unsigned long long)(LIST.toc ? entry.offset : 0));
    if (LIST.toc) snprintf(LIST.args[row],sizeof LIST.args[row],"{\"entry\":%u}",(unsigned)index+1);
    else {
        ink_jw_t w; ink_jw_init(&w,LIST.args[row],sizeof LIST.args[row]); ink_jw_raw(&w,"{\"title\":"); ink_jw_str(&w,LIB.books[index].name);
        ink_jw_raw(&w,",\"index\":"); ink_jw_int(&w,(int64_t)index); ink_jw_raw(&w,"}");
    }
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_line(slot,"",INK_FONT_LIST,INK_WEIGHT_MEDIUM,INK_GAP,INK_GAP_L,INK_CANVAS_W/2-2*INK_GAP);
        ink_ui_line(slot,"",INK_FONT_NOTE,INK_WEIGHT_REGULAR,INK_CANVAS_W/2,INK_GAP_L,INK_CANVAS_W/2-2*INK_MARGIN-INK_GAP);
    }
    lv_label_set_text(lv_obj_get_child(slot,0),LIST.titles[row]);
    const char *note = LIST.toc ? TR("点按跳到这里") : !strcmp(LAST.book,LIB.books[index].name) && !strcmp(LAST.member,ink_member_current()->id) ? TR("最近在读") : TR("打开阅读");
    if (!LIST.toc && !strcmp(LAST.book,LIB.books[index].name) && !strcmp(LAST.member,ink_member_current()->id)) {
        snprintf(LIST.values[row],sizeof LIST.values[row],TR("第 %d 页"),LAST.page); note=LIST.values[row];
    }
    lv_label_set_text(lv_obj_get_child(slot,1),note);
    *field = (ink_ui_field_spec_t){.id=LIST.ids[row],.role=LIST.toc ? "chapter" : "book",.label=LIST.titles[row],.value=note,.object={LIST.ids[row],ink_member_epoch()}};
    *action = (ink_ui_action_spec_t){.id=LIST.ids[row],.label=TR("打开"),.capability=LIST.toc ? "reader.goto_chapter" : "reader.open_book",.args_json=LIST.args[row],.object=field->object,.enabled=true};
}
static void list_create(lv_obj_t *scr, void *arg) {
    LIST.toc = arg != NULL; LIST.view = NULL;
    const char *member = ink_member_current()->id;
    if (strcmp(LIST.member,member) || LIST.toc) memset(&LIST.position,0,sizeof LIST.position);
    snprintf(LIST.member,sizeof LIST.member,"%s",member);
    if (LIST.toc) ink_reader_source_open(LIB.books[LIB.open].path,&LIST.source,&LIST.info);
    else scan_library();
    const char *title = LIST.toc ? TR("目录") : TR("书架");
    ink_ui_circle(scr,"back",INK_MARGIN,INK_STATUS_BAR,INK_CONTROL,INK_ICON_MEDIUM,INK_SURFACE_WHITE,command,"shell.back");
    ink_ui_line(scr,title,INK_FONT_TITLE,INK_WEIGHT_BOLD,INK_MARGIN+INK_CONTROL+INK_GAP,INK_STATUS_BAR+INK_GAP_S,INK_CANVAS_W-2*INK_MARGIN-INK_CONTROL-INK_GAP);
    int y = INK_STATUS_BAR + INK_CONTROL + INK_GAP_XS;
    lv_obj_t *body = ink_ui_box(scr,0,y,INK_CANVAS_W,INK_CANVAS_H-y,0,INK_SURFACE_WHITE,false);
    ink_ui_action_spec_t actions[] = {{.id="prev",.label=TR("上一页"),.capability="reader.list_prev"},{.id="next",.label=TR("下一页"),.capability="reader.list_next"},
        {.id="files",.label=TR("文件"),.capability="app.open",.args_json="{\"id\":\"files\"}",.enabled=true}};
    _Static_assert(sizeof actions/sizeof actions[0] <= LIST_ROWS,"page actions exceed row storage");
    ink_ui_page_spec_t page = {.id=LIST.toc ? "chapters" : "reader",.title=title,.object={"reader",ink_member_epoch()},.actions=actions,.action_count=LIST.toc ? 2 : 3};
    LIST.view = ink_ui_list_create(body,&page,&LIST.position,LIST_ROWS,1,INK_CONTROL,LIST.toc ? TR("没有目录，按章节阅读") : TR("还没有书，请先从文件传入 TXT 或 EPUB"),bind_row,NULL);
    ink_ui_list_refresh(LIST.view,LIST.toc ? (size_t)LIST.info.entries : (size_t)LIB.nbooks);
    if (LIST.toc && LIST.info.fallback) ink_ui_notice(scr,TR("没有目录，按章节阅读"),INK_STATUS_BAR+INK_PRIMARY);
}
static void list_destroy(void) { ink_reader_source_close(LIST.source); LIST.source=NULL; LIST.view=NULL; }
static int list_snapshot(char *out, size_t cap) { return LIST.view ? ink_ui_list_snapshot(LIST.view,out,cap) : 0; }
static bool list_key(ink_key_t key, bool lng) { return LIST.view && ink_ui_list_key(LIST.view,key,lng); }
static void library_create(lv_obj_t *scr, void *arg) { (void)arg; list_create(scr,NULL); }
static void toc_create(lv_obj_t *scr, void *arg) { (void)arg; list_create(scr,(void *)1); }
const ink_activity_t ink_app_reader = {.name="reader",.app="reader",.create=library_create,.destroy=list_destroy,.snapshot=list_snapshot,.on_key=list_key};
static const ink_activity_t CHAPTERS = {.name="chapters",.app="reader",.create=toc_create,.destroy=list_destroy,.snapshot=list_snapshot,.on_key=list_key};
static void act_list_turn(ink_call_t *c) {
    if (!LIST.view || !ink_ui_list_turn(LIST.view,strstr(c->cap->id,"next") ? 1 : -1)) c->rc.status=INK_RC_FAILED;
}

static void act_list(ink_call_t *c) {
    ink_receipt_t *rc = &c->rc;
    if (!LIB.nbooks) scan_library();
    size_t n = (size_t)snprintf(rc->say, sizeof rc->say, TR("书架上有："));
    for (int i = 0; i < LIB.nbooks && n < sizeof rc->say; i++)
        n += (size_t)snprintf(rc->say + n, sizeof rc->say - n, "%s%s", i ? "、" : "", LIB.books[i].name);
    snprintf(rc->object, sizeof rc->object, "shelf:books");
}

static bool last_book(char *book, size_t cap, int *page, int *percent);
static void act_sample(ink_call_t *c) {
    if (!LIB.books) scan_library();
    book_t *books = ink_app_realloc(LIB.books,(size_t)(LIB.nbooks+1)*sizeof *books);
    if (!books) { c->rc.status=INK_RC_FAILED; return; }
    LIB.books=books;
    snprintf(LIB.books[LIB.nbooks].name,sizeof LIB.books[LIB.nbooks].name,TR("夜邮（示例）"));
    snprintf(LIB.books[LIB.nbooks].key,sizeof LIB.books[LIB.nbooks].key,TR_NOOP("夜邮（示例）"));
    snprintf(LIB.books[LIB.nbooks].path,sizeof LIB.books[LIB.nbooks].path,"sample/welcome.txt");
    ink_ui_push(&ink_app_book,(void *)(intptr_t)LIB.nbooks);
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
            book_object(rc,LIB.books[i].name,"");
            snprintf(rc->say, sizeof rc->say, TR("接着读《%s》"), LIB.books[i].name);
            return;
        }
    rc->status = INK_RC_FAILED;
    snprintf(rc->detail, sizeof rc->detail, "no book to continue on the shelf");
}

static void act_open(ink_call_t *c) {
    ink_receipt_t *rc = &c->rc;
    char title[96] = {0};
    ink_args_get_str(&c->args, "title", title, sizeof title); /* required by the schema */
    if (!LIB.nbooks) scan_library();
    int64_t index = -1; ink_args_get_int(&c->args,"index",&index);
    for (int i = 0; i < LIB.nbooks; i++)
        if (index >= 0 ? i == index : (title[0] && (strstr(LIB.books[i].name, title) || strstr(title, LIB.books[i].name)))) {
            ink_ui_push(&ink_app_book, (void *)(intptr_t)i);
            if (!book_ready()) { rc->status=INK_RC_FAILED; snprintf(rc->say,sizeof rc->say,"%s",source_error(RD.error)); return; }
            book_object(rc,LIB.books[i].name,"");
            snprintf(rc->say, sizeof rc->say, TR("打开《%s》"), LIB.books[i].name);
            return;
        }
    rc->status = INK_RC_FAILED;
    snprintf(rc->detail, sizeof rc->detail, "no such book on the shelf");
}

static const ink_cap_t SHELF_ACTIONS[] = {
    {.id="reader.list_next",.app="reader",.title="书架或目录下一页",.effect=INK_FX_NAVIGATE,.child_ok=true,.run=act_list_turn},
    {.id="reader.list_prev",.app="reader",.title="书架或目录上一页",.effect=INK_FX_NAVIGATE,.child_ok=true,.run=act_list_turn},
    {.id = "reader.open_sample", .app = "reader", .title = "打开离线原创阅读示例", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = act_sample},
    {.id = "reader.list_books", .app = "reader", .title = "书架上有哪些书", .effect = INK_FX_READ, .child_ok = true, .run = act_list},
    {.id = "reader.open_book", .app = "reader",
     .title = "打开一本书",
     .input = "{\"type\":\"object\",\"properties\":{\"title\":{\"type\":\"string\"},\"index\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"title\"]}",
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
    int total_known = !RD.more && RD.nstarts > 0 && RD.starts[RD.nstarts - 1] >= RD.len ? RD.page_base + RD.nstarts - 1 : -1;
    if (total_known > 0) lv_label_set_text_fmt(RD.footer, "%d / %d", RD.page_base + RD.page + 1, total_known);
    else lv_label_set_text_fmt(RD.footer, "%d", RD.page_base + RD.page + 1);
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
    last_save(RD.member, LIB.books[LIB.open].name, RD.page_base + RD.page + 1);
    if (!RD.dirty_pos) return;
    char path[200], data[96];
    pos_path(path,sizeof path,RD.member,LIB.books[LIB.open].key);
    int n = snprintf(data,sizeof data,"offset=%llu\nchapter=%d\n",(unsigned long long)(RD.chunk+RD.starts[RD.page]),RD.chapter);
    if (path[0] && ink_storage_write_member_atomic(path, data, (size_t)n) == 0) RD.dirty_pos = false;
}

static bool change_section(int *target);
static bool go_to(int target) {
    if (RD.info.epub && book_ready() && (target < 0 || find_page_start(target) < 0) && !change_section(&target)) return false;
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
    snprintf(rc->say,sizeof rc->say,"%s",source_error(RD.error));
    return true;
}

static void page_receipt(ink_receipt_t *rc) {
    char suffix[40];
    if (RD.info.epub) snprintf(suffix,sizeof suffix,"#c=%d,p=%d",RD.chapter+1,RD.page_base+RD.page+1);
    else snprintf(suffix,sizeof suffix,"#page=%d",RD.page+1);
    book_object(rc,LIB.books[LIB.open].name,suffix);
    rc->revision = (uint32_t)(RD.page_base + RD.page + 1);
    snprintf(rc->say, sizeof rc->say, TR("现在是第 %d 页"), RD.page_base + RD.page + 1);
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

static bool seek_page(int page);
static void act_goto(ink_call_t *c) {
    ink_receipt_t *rc = &c->rc;
    int64_t page;
    if (not_open(rc)) return;
    if (!ink_args_get_int(&c->args, "page", &page) || page < 1 || page > INT_MAX) {
        rc->status = INK_RC_INVALID;
        snprintf(rc->detail, sizeof rc->detail, "page must be an integer >= 1");
        return;
    }
    if (!seek_page((int)page - 1)) {
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

static void act_chapters(ink_call_t *c);
static void act_chapter(ink_call_t *c);
static void act_toc(ink_call_t *c) { (void)c; ink_ui_push(&CHAPTERS,NULL); }
static const ink_cap_t BOOK_ACTIONS[] = {
    {.id="reader.chapters",.app="reader",.title="列出目录",.effect=INK_FX_READ,.child_ok=true,.activity="book",.run=act_chapters},
    {.id="reader.show_chapters",.app="reader",.title="打开目录",.effect=INK_FX_NAVIGATE,.child_ok=true,.activity="book",.run=act_toc},
    {.id="reader.goto_chapter",.app="reader",.title="跳到章节",.input="{\"type\":\"object\",\"properties\":{\"chapter\":{\"type\":\"integer\",\"minimum\":1},\"entry\":{\"type\":\"integer\",\"minimum\":1}}}",.effect=INK_FX_NAVIGATE,.child_ok=true,.run=act_chapter},
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

static const char *source_error(ink_reader_error_t error) {
    switch (error) {
        case INK_READER_ENCRYPTED: return TR("这本书已加密或压缩方式不支持，请换一本未加密的 EPUB");
        case INK_READER_FORMAT: return TR("这不是 EPUB 或 TXT，请选择支持的书");
        case INK_READER_IO: return TR("读不到这本书，请检查存储卡后重试");
        case INK_READER_MEMORY: return TR("内存不足，请返回书架再试");
        default: return TR("文件损坏或没有正文，请重新传入这本书");
    }
}
static bool load_section(int chapter, uint64_t offset) {
    size_t n=0; bool more=false;
    RD.error=ink_reader_source_read(RD.source,chapter,offset,RD.text,MAX_BOOK_BYTES+1,&n,&more,NULL,NULL);
    if (RD.error != INK_READER_OK || !n) return false;
    RD.chapter=chapter; RD.chunk=offset; RD.len=n; RD.more=more; RD.page=0; RD.page_base=0; RD.nstarts=1;
    if (RD.starts) RD.starts[0]=0;
    RD.buf_page[0]=RD.buf_page[1]=-1; RD.shown=-1;
    snprintf(RD.chapter_title,sizeof RD.chapter_title,TR("第 %d 章"),chapter+1);
    for (int i=0;i<RD.info.entries;i++) { ink_reader_entry_t e; ink_reader_source_toc(RD.source,i,&e);
        if (e.chapter == chapter && e.offset <= offset) snprintf(RD.chapter_title,sizeof RD.chapter_title,"%s",e.title);
    }
    return true;
}
static bool section_at(int chapter, uint64_t offset) {
    uint64_t start=0; int base=0;
    do {
        if (!load_section(chapter,start)) return false;
        RD.page_base=base;
        if (!RD.more || offset < start+RD.len) return true;
        int page=0; while (find_page_start(page+1) >= 0) page++;
        base+=page+1;
        start+=RD.len;
    } while (true);
}
static bool change_section(int *target) {
    if (*target >= 0) {
        if (!RD.more && RD.chapter+1 >= RD.info.chapters) return false;
        int base=RD.more ? RD.page_base+RD.nstarts-1 : 0;
        if (!load_section(RD.more ? RD.chapter : RD.chapter+1,RD.more ? RD.chunk+RD.len : 0)) return false;
        RD.page_base=base;
        *target=0;
    } else {
        if (!RD.chapter && !RD.chunk) return false;
        if (!section_at(RD.chunk ? RD.chapter : RD.chapter-1,RD.chunk ? RD.chunk-1 : UINT64_MAX)) return false;
        while (find_page_start(RD.page+1) >= 0) RD.page++;
        *target=RD.page;
    }
    return true;
}
static bool seek_page(int target) {
    if (!RD.info.epub) return go_to(target);
    uint64_t saved=RD.chunk+RD.starts[RD.page]; int chapter=RD.chapter;
    if (target < RD.page_base && !section_at(chapter,0)) return false;
    while (find_page_start(target-RD.page_base) < 0 && RD.more) {
        int next=0; if (!change_section(&next)) break;
    }
    if (find_page_start(target-RD.page_base) >= 0) return go_to(target-RD.page_base);
    if (section_at(chapter,saved)) {
        int page=0;
        while (find_page_start(page+1) >= 0 && RD.chunk+RD.starts[page+1] <= saved) page++;
        go_to(page);
    }
    return false;
}
static bool ensure_source(void) {
    if (RD.source) return true;
    RD.error=ink_reader_source_open(LIB.books[LIB.open].path,&RD.source,&RD.info);
    return RD.error == INK_READER_OK;
}
static void act_chapters(ink_call_t *c) {
    if (not_open(&c->rc) || !ensure_source()) { c->rc.status=INK_RC_FAILED; return; }
    size_t cap=(size_t)RD.info.entries*320+128; char *json=ink_app_alloc(cap,true);
    if (!json) { c->rc.status=INK_RC_FAILED; return; }
    ink_jw_t w; ink_jw_init(&w,json,cap); ink_jw_raw(&w,"{\"fallback\":"); ink_jw_raw(&w,RD.info.fallback ? "true" : "false"); ink_jw_raw(&w,",\"entries\":[");
    for (int i=0;i<RD.info.entries;i++) { ink_reader_entry_t e; ink_reader_source_toc(RD.source,i,&e);
        if (i) ink_jw_raw(&w,",");
        ink_jw_raw(&w,"{\"title\":"); ink_jw_str(&w,e.title); ink_jw_raw(&w,",\"chapter\":"); ink_jw_int(&w,e.chapter+1); ink_jw_raw(&w,",\"offset\":"); ink_jw_int(&w,(int64_t)e.offset); ink_jw_raw(&w,"}");
    }
    ink_jw_raw(&w,"]}"); if (!w.overflow) ink_cap_result(c,json); else c->rc.status=INK_RC_FAILED;
    snprintf(c->rc.say,sizeof c->rc.say,"%s",RD.info.fallback ? TR("没有目录，按章节阅读") : TR("已列出目录")); ink_app_free(json);
}
static void act_chapter(ink_call_t *c) {
    bool toc=!strcmp(ink_ui_top_name(),"chapters"); int64_t chapter=0, entry=0;
    if ((!toc && !book_ready()) || (toc && !LIST.source) || (!toc && !ensure_source())) { c->rc.status=INK_RC_FAILED; return; }
    ink_reader_info_t *info=toc ? &LIST.info : &RD.info; uint64_t offset=0;
    if (ink_args_get_int(&c->args,"entry",&entry) && entry > 0 && entry <= info->entries) {
        ink_reader_entry_t e; ink_reader_source_toc(toc ? LIST.source : RD.source,(int)entry-1,&e); chapter=e.chapter+1; offset=e.offset;
    } else ink_args_get_int(&c->args,"chapter",&chapter);
    if (chapter < 1 || chapter > info->chapters) { c->rc.status=INK_RC_INVALID; return; }
    if (toc) { JUMP.chapter=(int)chapter-1; JUMP.offset=offset; JUMP.pending=true; ink_ui_pop(); }
    else {
        if (RD.info.epub && !section_at((int)chapter-1,offset)) { c->rc.status=INK_RC_FAILED; return; }
        int page=0; while (find_page_start(page+1) >= 0 && RD.chunk+RD.starts[page+1] <= offset) page++;
        go_to(page);
    }
    if (book_ready()) page_receipt(&c->rc);
}

static bool load_text(const char *path) {
    if (strstr(path,".epub")) {
        if (!ensure_source()) return false;
        const char *tag=ink_app_tag("reader-text"); RD.text=ink_app_alloc(MAX_BOOK_BYTES+1,true); ink_app_tag(tag);
        if (!RD.text) { RD.error=INK_READER_MEMORY; return false; }
        return load_section(0,0);
    }
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
        lv_obj_set_width(l,lv_pct(90)); lv_obj_set_style_text_font(l,ink_ui_font(2),0);
        lv_label_set_text(l,source_error(RD.error)); lv_obj_center(l);
        ink_ui_circle(scr,"back",INK_MARGIN,INK_STATUS_BAR,INK_CONTROL,INK_ICON_MEDIUM,INK_SURFACE_WHITE,command,"shell.back");
        lv_obj_add_flag(scr,LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(scr,page_clicked,LV_EVENT_CLICKED,NULL);
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
    RD.starts = ink_app_alloc((size_t)RD.cap * sizeof(uint32_t), true);
    RD.buf[0] = ink_app_alloc((size_t)RD.bw * RD.bh, true);
    RD.buf[1] = ink_app_alloc((size_t)RD.bw * RD.bh, true);
    ink_app_tag(prev);
    if (!RD.starts || !RD.buf[0] || !RD.buf[1]) ink_app_fail("reader: out of memory for page buffers");
    RD.starts[0] = 0;
    RD.nstarts = 1;

    /* restore the saved position */
    char path[200], data[96] = {0};
    pos_path(path,sizeof path,RD.member,LIB.books[LIB.open].key);
    unsigned long long saved=0; int chapter=0;
    if (path[0] && ink_storage_read(path,data,sizeof data-1) > 0) sscanf(data,"offset=%llu\nchapter=%d",&saved,&chapter);
    if (JUMP.pending) {
        saved=JUMP.offset; chapter=JUMP.chapter; JUMP.pending=false;
        RD.dirty_pos=true; RD.dirty_since_us=ink_now_us();
    }
    if (RD.info.epub && chapter >= 0 && chapter < RD.info.chapters) section_at(chapter,saved);
    if (saved >= RD.chunk) saved-=RD.chunk;
    while (saved && find_page_start(RD.page + 1) >= 0 && RD.starts[RD.page + 1] <= saved) RD.page++;
    ink_probe_value("reader.restored_page", (double)RD.page);

    RD.title = lv_label_create(scr);
    lv_obj_set_style_text_font(RD.title, ink_ui_font(2), 0);
    lv_label_set_text_fmt(RD.title,"%s · %s",LIB.books[LIB.open].name,TR("目录"));
    lv_obj_align(RD.title, LV_ALIGN_TOP_MID, 0, ink_dp(6));
    lv_obj_add_flag(RD.title,LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(RD.title,command,LV_EVENT_CLICKED,"reader.show_chapters");

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
    ink_reader_source_close(RD.source);
    memset(&RD, 0, sizeof RD);
}

static int book_snapshot(char *buf, size_t cap) {
    if (!cap) return 0;
    if (!book_ready()) return snprintf(buf,cap,"%s",source_error(RD.error));
    int header=snprintf(buf,cap,"%s · %s · %s %d\n",LIB.books[LIB.open].name,RD.chapter_title,TR("章内页"),RD.page_base+RD.page+1);
    if (header < 0 || (size_t)header >= cap) { buf[0]=0; return 0; }
    size_t a = RD.starts[RD.page];
    size_t b = RD.page + 1 < RD.nstarts ? RD.starts[RD.page + 1] : RD.len;
    if (b > RD.len) b = RD.len;
    size_t n = b > a ? b - a : 0;
    if (n >= cap-(size_t)header) n = cap-(size_t)header-1;
    while (n && ((unsigned char)RD.text[a + n] & 0xC0) == 0x80) n--; /* do not cut a UTF-8 character */
    memcpy(buf+header,RD.text+a,n);
    buf[header+n]=0;
    return header+(int)n;
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
    if (book_ready() && !strcmp(RD.member, member)) {
        snprintf(book, cap, "%s", LIB.books[LIB.open].name);
        *page = RD.page_base + RD.page + 1;
        if (percent) *percent = (int)(RD.starts[RD.page] * 100ull / RD.len);
        return true;
    }
    last_load(member);
    char path[160];
    if (strcmp(LAST.member, member)) return false; /* still being read */
    if (strstr(LAST.file,".epub")) snprintf(path,sizeof path,"/content/books/%.95s",LAST.file);
    else snprintf(path,sizeof path,"/content/books/%.95s.txt",LAST.book);
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
    snprintf(out->title, sizeof out->title, TR("继续读"));
    snprintf(out->text, sizeof out->text, TR("《%.60s》第 %d 页 · %d%%"), book, page, percent);
    if (book_ready() ? RD.info.epub : strstr(LAST.file,".epub") != NULL)
        snprintf(out->text,sizeof out->text,TR("《%.60s》第 %d 章 · 第 %d 页"),book,(book_ready() ? RD.chapter : LAST.chapter)+1,page);
    snprintf(out->action, sizeof out->action, "reader.continue");
    snprintf(out->member, sizeof out->member, "%s", ink_member_current()->id);
    out->value = page;
    out->has_value = true;
    out->bookmark = true;
    return 1;
}

const ink_app_def_t ink_appdef_reader = {.id = "reader", .title = TR_NOOP("阅读"), .entry = &ink_app_reader, .system_app = true, .child_ok = true, .card = reader_card};

static void card_changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id; ink_app_card_changed("reader");
}
int ink_reader_actions_init(void) {
    int r = ink_cap_register(SHELF_ACTIONS, (int)(sizeof SHELF_ACTIONS / sizeof SHELF_ACTIONS[0]));
    return r ? r : ink_cap_register(BOOK_ACTIONS, (int)(sizeof BOOK_ACTIONS / sizeof BOOK_ACTIONS[0])) ||
        ink_state_watch("storage.card", card_changed, NULL, INK_EXEC_UI);
}
