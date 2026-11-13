#include "ink/probe.h"
#include "ink/sys/json.h"
#include <zip_reader.h>
#include <book_epub.h>
static book_shelf_entry_t *ink_mogo_shelf;
static int ink_mogo_shelf_count, ink_mogo_shelf_page, ink_mogo_reason;
static char ink_mogo_error[160];
static ink_mogo_card_t ink_mogo_card_written;
static size_t ink_mogo_anchor=SIZE_MAX, ink_mogo_anchor_chapter=SIZE_MAX; /* the reading place a reflow keeps */
static char ink_mogo_last_set[BOOK_STORE_PATH_MAX]; /* the book to continue, as written last */
/* The bookmark rows as last drawn (BG): a tap (UI) finds its mark here, without reading the member's records. */
static struct { int rows; EpdRect rect[BOOKMARK_MAX]; uint32_t id[BOOKMARK_MAX]; } ink_mogo_mark_hits;
enum { INK_MOGO_SHELF_ROWS=4, INK_MOGO_TOOL_COUNT=6 };
enum { INK_MOGO_TOOLS=10, INK_MOGO_MARKS, INK_MOGO_SETTINGS };
const char *const ink_mogo_layout_keys[INK_MOGO_LAYOUT_COUNT]={"font_size","line_spacing","paragraph_spacing","margins","weight","indent","header","footer"};
static const char *const ink_mogo_setting_names[] = {TR_NOOP("字号"),TR_NOOP("行距"),TR_NOOP("段距"),TR_NOOP("边距"),TR_NOOP("字重"),TR_NOOP("缩进"),TR_NOOP("页眉"),TR_NOOP("页脚")};
static const int ink_mogo_layout_range[INK_MOGO_LAYOUT_COUNT][2]={{18,90},{110,150},{0,100},{0,96},{-32,32},{0,3},{0,1},{0,1}};
_Static_assert(sizeof ink_mogo_setting_names/sizeof *ink_mogo_setting_names == INK_MOGO_LAYOUT_COUNT,"setting rows");
static int ink_mogo_setting_value(int index) {
    const ink_mogo_settings_t *p=&ink_mogo_settings;
    int values[]={p->px,p->line,p->paragraph,p->margin[0],p->weight,p->indent,p->header,p->footer};
    _Static_assert(sizeof values/sizeof *values == INK_MOGO_LAYOUT_COUNT,"setting values");
    return values[index];
}
static const char *ink_mogo_place(const char *path) {
    return !strncmp(path,"/family/books/",14)?TR_NOOP("全家的"):!strncmp(path,"/me/books/",10)?TR_NOOP("我的书"):!strncmp(path,"asset:",6)?TR_NOOP("示例"):TR_NOOP("传进来的");
}
/* The name people know a book by: an EPUB's own title, else the file name without its extension. */
void ink_mogo_book_title(const char *path,char *out,size_t cap) {
    char author[8];
    if (!strncmp(path,"asset:",6)) { copy_text(out,cap,TR("夜邮（示例）")); return; }
    const char *leaf=strrchr(path,'/'), *dot=strrchr(path,'.');
    if (dot && !strcasecmp(dot,".epub") && book_epub_metadata(path,out,cap,author,sizeof author)==ESP_OK && out[0]) return;
    leaf=leaf?leaf+1:path; size_t n=dot && dot>leaf?(size_t)(dot-leaf):strlen(leaf);
    if (n>=cap) n=cap-1;
    memcpy(out,leaf,n); out[n]=0;
}
/* No font needed: /me/books, /family/books and /me/收件箱, read last first, then by name. */
static int ink_mogo_shelf_list(const ink_member_identity_t *owner,book_shelf_entry_t **out,bool *no_card) {
    char inbox[64]; snprintf(inbox,sizeof inbox,"/me/%s",TR_NOOP("收件箱"));
    const char *const roots[]={"/me/books","/family/books",inbox};
    ink_mogo_owner(owner); *out=NULL; *no_card=false; int count=0;
    for (int r=0;r<3;++r) {
        uint32_t cursor=0; ink_fs_entry_t e; int got;
        while ((got=ink_fs_index(roots[r],INK_FS_ANY,&cursor,&e))==1) {
            const char *dot=strrchr(e.path,'.');
            if (!dot || (strcasecmp(dot,".txt") && strcasecmp(dot,".epub"))) continue;
            book_shelf_entry_t *next=realloc(*out,(size_t)(count+1)*sizeof *next);
            if (!next) return count;
            *out=next; book_shelf_entry_t *b=&next[count++]; memset(b,0,sizeof *b);
            copy_text(b->path,sizeof b->path,e.path); ink_mogo_book_title(e.path,b->name,sizeof b->name);
            b->search_match=true; b->size=(uint32_t)e.size;
            book_progress_t p;
            if (book_progress_load(e.path,b->size,&p)) { b->has_progress=true; b->pct=p.pct; b->recent=p.last_open_s; }
        }
        if (got==INK_FS_ERR_NO_CARD) *no_card=true;
    }
    book_shelf_query_t query={.recent_sort=true};
    count=book_shelf_sort(*out,count,&query);
    for (int i=0;i<count;++i) for (int j=0;j<count;++j)
        if (i!=j && strncmp((*out)[i].path,"/me/books/",10) && !strcmp((*out)[i].name,(*out)[j].name)) {
            size_t n=strlen((*out)[i].name); snprintf((*out)[i].name+n,sizeof (*out)[i].name-n,"（%s）",TR(ink_mogo_place((*out)[i].path))); break;
        }
    return count;
}
/* A result for the AI: book text is escaped (a quote in a title is a quote, not the end of a field). */
static void ink_mogo_jw_field(ink_jw_t *w,const char *key,const char *text) { ink_jw_raw(w,key); ink_jw_str(w,text); }
static void ink_mogo_jw_number(ink_jw_t *w,const char *key,int64_t v) { ink_jw_raw(w,key); ink_jw_int(w,v); }
static int ink_mogo_jw_end(ink_jw_t *w) { return w->overflow?-2:(int)w->len; }
/* Write a result in `*json` (cap `*cap`), in a bigger buffer from ink_app_alloc when it does not fit (*json then
 * points at it: the caller frees it if it is not its own). A list of hundreds of chapters is still one answer. */
static int ink_mogo_json_fit(char **json,size_t *cap,size_t guess,int (*write)(char *,size_t,const void *),const void *arg) {
    char *own=*json;
    for (size_t size=guess>*cap?guess:*cap;size<=(size_t)4<<20;size*=2) {
        char *buf=size<=*cap?own:ink_app_alloc(size,true);
        if (!buf) return -1;
        int n=write(buf,size,arg);
        if (n>=0) { *json=buf; *cap=size; return n; }
        if (buf!=own) ink_app_free(buf);
        if (n!=-2) return -1;
    }
    return -1;
}
typedef struct { const book_shelf_entry_t *b; int count; } ink_mogo_shelf_rows_t;
static int ink_mogo_shelf_json(char *json,size_t cap,const void *arg) {
    const ink_mogo_shelf_rows_t *r=arg; ink_jw_t w; ink_jw_init(&w,json,cap); ink_jw_raw(&w,"[");
    for (int i=0;i<r->count;++i) {
        ink_mogo_jw_number(&w,i?",{\"index\":":"{\"index\":",i); ink_mogo_jw_field(&w,",\"title\":",r->b[i].name);
        ink_mogo_jw_field(&w,",\"place\":",TR(ink_mogo_place(r->b[i].path))); ink_mogo_jw_number(&w,",\"percent\":",r->b[i].pct); ink_jw_raw(&w,"}");
    }
    ink_jw_raw(&w,"]");
    return ink_mogo_jw_end(&w);
}
int ink_mogo_shelf_query(const ink_member_identity_t *owner,char **json,size_t *cap,const char *title,int index,char *path,size_t path_cap) {
    book_shelf_entry_t *books; bool no_card; int count=ink_mogo_shelf_list(owner,&books,&no_card), found=-1;
    for (int i=0;i<count && found<0;++i)
        if (index>=0 ? i==index : title && title[0] && (strstr(books[i].name,title) || strstr(title,books[i].name))) found=i;
    ink_mogo_shelf_rows_t rows={books,count};
    if (json && ink_mogo_json_fit(json,cap,(size_t)count*(sizeof books->name*2+96)+16,ink_mogo_shelf_json,&rows)<0) found=-3;
    if (path && found>=0) copy_text(path,path_cap,books[found].path);
    free(books);
    return no_card?-2:found;
}
/* The font lives on the card (runtime.c); a missing or broken one is explained, never a blank page. */
int ink_mogo_page_start(const ink_member_identity_t *owner,int w,int h,int px) {
    memset(&ink_mogo_card_written,0,sizeof ink_mogo_card_written); memset(&s_progress_saved,0,sizeof s_progress_saved); ink_mogo_last_set[0]=0;
    ink_mogo_owner(owner); ink_mogo_dimensions(w,h,px); ink_mogo_settings_load();
    ink_mogo_page_width=w; ink_mogo_page_height=h; s_reader_session=(reader_session_t){0};
    for (int i=0;i<4;++i) s_margin[i]=ink_mogo_scaled(ink_mogo_settings.margin[i]);
    s_margin[2]+=ink_mogo_settings.header?ink_mogo_scaled(64):0; s_margin[3]+=ink_mogo_settings.footer?ink_mogo_scaled(64):0;
    ink_mogo_reason=ink_mogo_font_check();
    if (!ink_mogo_reason && ttf_font_open(app_settings_font_path()) != ESP_OK) ink_mogo_reason=INK_MOGO_R_BAD_FONT;
    book_layout_set_spacing(ink_mogo_settings.line,ink_mogo_settings.paragraph);
    book_layout_set_first_line_indent(ink_mogo_settings.indent);
    ttf_set_stroke_adjust_fine(ink_mogo_settings.weight);
    return ink_mogo_reason;
}
static int ink_mogo_scan(void) {
    save_progress(); s_view=SHELF; s_reader_panel=READER_PANEL_NONE;
    free(ink_mogo_shelf); ink_mogo_shelf_page=0; bool no_card;
    ink_mogo_shelf_count=ink_mogo_shelf_list(ink_mogo_current_owner(),&ink_mogo_shelf,&no_card);
    if (!ink_mogo_reason || ink_mogo_reason>=INK_MOGO_R_BAD_BOOK) ink_mogo_reason=no_card?INK_MOGO_R_NO_CARD:ink_mogo_shelf_count?0:INK_MOGO_R_EMPTY;
    return ink_mogo_reason;
}
static bool ink_mogo_encrypted(const char *path) {
    const char *dot=strrchr(path,'.'); zip_reader_t *zip=NULL;
    if (!dot || strcasecmp(dot,".epub")) return false;
    esp_err_t err=zip_open(path,&zip);
    bool locked=err==ESP_ERR_NOT_SUPPORTED || (!err && zip_find(zip,"META-INF/encryption.xml")>=0);
    zip_close(zip); return locked;
}
static int ink_mogo_open_book(const char *path) {
    if (ink_mogo_reason && ink_mogo_reason<INK_MOGO_R_BAD_BOOK) return ink_mogo_reason;
    bool locked=ink_mogo_encrypted(path);
    s_turning=true; bool opened=!locked && open_book_impl(&ink_mogo_context,path); s_turning=false; /* opening writes nothing either */
    if (!opened) {
        const char *why=locked||strstr(s_message,"压缩方式")?TR_NOOP("这本书已加密或压缩方式不支持，请换一本未加密的 EPUB"):strstr(s_message,"仅支持")?TR_NOOP("这不是 EPUB 或 TXT，请选择支持的书"):
            strstr(s_message,"不可读")?TR_NOOP("读不到这本书，请检查存储卡后重试"):strstr(s_message,"内存不足")?TR_NOOP("内存不足，请返回书架再试"):TR_NOOP("文件损坏或没有正文，请重新传入这本书");
        free_book(); copy_text(ink_mogo_error,sizeof ink_mogo_error,TR(why)); s_view=SHELF;
        return ink_mogo_reason=INK_MOGO_R_BAD_BOOK;
    }
    ink_mogo_book_title(path,s_reader_session.book.title,sizeof s_reader_session.book.title);
    ink_mogo_reason=0;
    return 0;
}
static unsigned ink_mogo_mark_percent(const reader_bookmark_entry_t *m) {
    uint32_t total=book_total_bytes(), start=book_chapter_byte_offset(m->chapter);
    uint32_t end=(size_t)m->chapter+1<book_chapter_count()?book_chapter_byte_offset(m->chapter+1):total;
    uint32_t at=m->chapter==s_reader_session.position.chapter?book_position_bytes(start,end,m->byte_off,s_text_len):start;
    return total?(unsigned)((uint64_t)at*100/total):0;
}
static uint32_t ink_mogo_mark_id(const reader_bookmark_entry_t *m) { return m->saved_s?m->saved_s:1; }
/* Add the page's mark or remove one by its stable id (saved_s), keeping at most BOOKMARK_MAX. */
static bool ink_mogo_mark_write(bool add,uint32_t id) {
    nvs_handle_t h; reader_bookmarks_t marks; char key[11];
    if (!s_text || nvs_open("rp_marks",NVS_READWRITE,&h)!=ESP_OK) return false;
    bookmark_key(s_reader_session.book.path,key);
    if (!bookmark_read_handle(h,&marks)) {
        memset(&marks,0,sizeof marks); marks.magic=UINT32_C(0x52504d32); marks.file_size=s_reader_session.book.file_size;
        copy_text(marks.path,sizeof marks.path,s_reader_session.book.path);
    }
    uint32_t offset=(uint32_t)book_layout_page_start_offset(s_reader_session.position.page), now=(uint32_t)time(NULL);
    size_t found=marks.count;
    for (size_t i=0;i<marks.count;++i) {
        if (add ? marks.entries[i].chapter==s_reader_session.position.chapter && marks.entries[i].byte_off==offset : ink_mogo_mark_id(&marks.entries[i])==id) found=i;
        if (add && marks.entries[i].saved_s>=now) now=marks.entries[i].saved_s+1;
    }
    esp_err_t err=ESP_OK;
    if (add && found==marks.count) {
        if (marks.count==BOOKMARK_MAX) { memmove(&marks.entries[0],&marks.entries[1],(BOOKMARK_MAX-1)*sizeof marks.entries[0]); --marks.count; }
        marks.entries[marks.count++]=(reader_bookmark_entry_t){.chapter=(uint16_t)s_reader_session.position.chapter,.byte_off=offset,.saved_s=now};
    } else if (!add && found<marks.count) {
        memmove(&marks.entries[found],&marks.entries[found+1],(marks.count-found-1)*sizeof marks.entries[0]); --marks.count;
    } else { nvs_close(h); return add; }
    err=marks.count?nvs_set_blob(h,key,&marks,sizeof marks):nvs_erase_key(h,key);
    if (!err) err=nvs_commit(h);
    nvs_close(h); return !err;
}
/* The first bytes at a mark, read from the book now (up to 30 bytes, whole characters). */
static void ink_mogo_mark_excerpt(const reader_bookmark_entry_t *m,char *out,size_t cap) {
    html_text_t loaded={0}; const char *text=s_text; size_t len=s_text_len; out[0]=0;
    if (m->chapter!=s_reader_session.position.chapter) {
        if (book_chapter_load_blocks(m->chapter,&loaded)!=ESP_OK) return;
        text=loaded.utf8; len=loaded.len;
    }
    size_t n=m->byte_off<len?len-m->byte_off:0;
    if (n>30) n=30;
    if (n>=cap) n=cap-1;
    while (n && text && ((unsigned char)text[m->byte_off+n]&0xc0)==0x80) --n;
    if (text && n) { memcpy(out,text+m->byte_off,n); out[n]=0; }
    for (char *c=out;*c;++c) if (*c=='\n' || *c=='"' || *c=='\\') *c=' ';
    html_text_free(&loaded);
}
/* Where the open book is, for the card. */
static bool ink_mogo_card_now(ink_mogo_card_t *c) {
    memset(c,0,sizeof *c);
    if (!s_text) return false;
    copy_text(c->path,sizeof c->path,s_reader_session.book.path); copy_text(c->title,sizeof c->title,s_reader_session.book.title);
    copy_text(c->chapter,sizeof c->chapter,s_reader_session.book.chapter_title);
    c->chapter_no=(uint16_t)s_reader_session.position.chapter; c->pct=(uint8_t)percent(s_reader_session.position.page);
    c->offset=(uint32_t)book_layout_page_start_offset(s_reader_session.position.page);
    return true;
}
static void ink_mogo_card_save(void) {
    ink_mogo_card_t c;
    if (!ink_mogo_card_now(&c) || s_save_failed) return;
    nvs_handle_t h;
    if (!memcmp(&c,&ink_mogo_card_written,sizeof c) || nvs_open("ink_card",NVS_READWRITE,&h)!=ESP_OK) return;
    if (!nvs_set_blob(h,"last",&c,sizeof c) && !nvs_commit(h)) { ink_mogo_card_written=c; ink_probe_count("reader.last_saved"); }
    nvs_close(h);
}
/* The open book's place (shown at once; written after a pause), else the one written last. */
/* After a pause and at the close: the place, the book to continue, the card. */
static void ink_mogo_save_all(void) {
    save_progress();
    if (s_text && strcmp(ink_mogo_last_set,s_reader_session.book.path) && book_progress_set_last_path(s_reader_session.book.path)==ESP_OK)
        copy_text(ink_mogo_last_set,sizeof ink_mogo_last_set,s_reader_session.book.path);
    ink_mogo_card_save();
}
bool ink_mogo_page_card(ink_mogo_card_t *out) { if (!ink_mogo_card_now(out)) *out=ink_mogo_card_written; return out->path[0]; }
static bool ink_mogo_layout(const int *values) {
    ink_mogo_settings_t old=ink_mogo_settings, *p=&ink_mogo_settings;
    size_t offset=book_layout_page_start_offset(s_reader_session.position.page), next=book_layout_page_start_offset(s_reader_session.position.page+1);
    /* Reflow from where the reader is, not from the page start the last reflow left, so repeated changes do not drift back. */
    if (ink_mogo_anchor_chapter==s_reader_session.position.chapter && ink_mogo_anchor>=offset && ink_mogo_anchor<next) offset=ink_mogo_anchor;
    else { ink_mogo_anchor=offset; ink_mogo_anchor_chapter=s_reader_session.position.chapter; }
    int v[INK_MOGO_LAYOUT_COUNT];
    for (int i=0;i<INK_MOGO_LAYOUT_COUNT;++i) v[i]=values[i]==INK_MOGO_UNSET?ink_mogo_setting_value(i):values[i];
    p->px=(uint8_t)v[0]; p->line=(uint8_t)v[1]; p->paragraph=(uint8_t)v[2]; memset(p->margin,v[3],sizeof p->margin);
    p->margin[2]=p->margin[3]=old.margin[2]==old.margin[0]?(uint8_t)v[3]:old.margin[2];
    p->weight=(int8_t)v[4]; p->indent=(uint8_t)v[5]; p->header=(uint8_t)v[6]; p->footer=(uint8_t)v[7];
    for (int attempt=0;attempt<2;++attempt) {
        s_px=app_settings_book_px();
        for (int i=0;i<4;++i) s_margin[i]=ink_mogo_scaled(p->margin[i]);
        s_margin[2]+=p->header?ink_mogo_scaled(64):0; s_margin[3]+=p->footer?ink_mogo_scaled(64):0;
        book_layout_set_spacing(p->line,p->paragraph); book_layout_set_first_line_indent(p->indent); ttf_set_stroke_adjust_fine(p->weight);
        if ((!s_text || load_chapter(&ink_mogo_context,s_reader_session.position.chapter,offset,false)) && !attempt && ink_mogo_settings_save()) {
            if (s_text) { s_reader_panel=READER_PANEL_NONE; s_view=READING; }
            return true;
        }
        *p=old;
    }
    return false;
}
static int ink_mogo_json_layout(char *json,size_t cap) {
    const ink_member_identity_t *o=ink_mogo_current_owner();
    return snprintf(json,cap,"{\"member\":\"%s\",\"font_size\":%d,\"line_spacing\":%d,\"paragraph_spacing\":%d,\"margins\":%d,\"weight\":%d,\"indent\":%d,\"header\":%d,\"footer\":%d}",
        o->member,ink_mogo_setting_value(0),ink_mogo_setting_value(1),ink_mogo_setting_value(2),ink_mogo_setting_value(3),ink_mogo_setting_value(4),ink_mogo_setting_value(5),ink_mogo_setting_value(6),ink_mogo_setting_value(7));
}
/* No NCX/NAV and no headings to build one from: Mogo names the spine "第 N 节", and the page says so like the old reader. */
static bool ink_mogo_fallback_toc(void) {
    if (book_kind()!=BOOK_KIND_EPUB) return false;
    for (size_t i=0;i<book_navigation_count();++i) {
        char title[96], fallback[32]; snprintf(fallback,sizeof fallback,TR_NOOP("第 %u 节"),(unsigned)i+1);
        if (book_navigation_title(i,title,sizeof title)!=ESP_OK || strcmp(title,fallback)) return false;
    }
    return true;
}
static int ink_mogo_json_chapters(char *json,size_t cap,const void *arg) {
    (void)arg; char name[96]="", author[96]=""; ink_jw_t w; ink_jw_init(&w,json,cap);
    if (book_kind()==BOOK_KIND_EPUB) (void)book_epub_metadata(s_reader_session.book.path,name,sizeof name,author,sizeof author);
    ink_mogo_jw_field(&w,"{\"title\":",s_reader_session.book.title); ink_mogo_jw_field(&w,",\"author\":",author);
    ink_mogo_jw_number(&w,",\"chapter_count\":",(int64_t)book_chapter_count()); ink_jw_raw(&w,ink_mogo_fallback_toc()?",\"fallback\":true,\"entries\":[":",\"fallback\":false,\"entries\":[");
    for (size_t i=0;i<book_navigation_count() && !w.overflow;++i) {
        char title[96]=""; (void)book_navigation_title(i,title,sizeof title);
        ink_mogo_jw_field(&w,i?",{\"title\":":"{\"title\":",title); ink_mogo_jw_number(&w,",\"chapter\":",(int64_t)book_navigation_chapter(i)+1);
        ink_mogo_jw_number(&w,",\"entry\":",(int64_t)i+1); ink_jw_raw(&w,"}");
    }
    ink_jw_raw(&w,"]}");
    return ink_mogo_jw_end(&w);
}
static int ink_mogo_json_marks(char *json,size_t cap,const void *arg) {
    (void)arg; reader_bookmarks_t marks; ink_jw_t w; ink_jw_init(&w,json,cap); ink_jw_raw(&w,"{\"bookmarks\":[");
    if (!bookmark_load(&marks)) marks.count=0;
    for (int i=(int)marks.count-1;i>=0 && !w.overflow;--i) {
        const reader_bookmark_entry_t *m=&marks.entries[i]; char label[96], excerpt[40];
        if (book_chapter_title(m->chapter,label,sizeof label)!=ESP_OK) snprintf(label,sizeof label,TR_NOOP("第 %u 节"),(unsigned)m->chapter+1);
        ink_mogo_mark_excerpt(m,excerpt,sizeof excerpt);
        ink_mogo_jw_number(&w,i+1<(int)marks.count?",{\"id\":":"{\"id\":",ink_mogo_mark_id(m)); ink_mogo_jw_number(&w,",\"chapter\":",m->chapter+1);
        ink_mogo_jw_number(&w,",\"percent\":",ink_mogo_mark_percent(m)); ink_mogo_jw_number(&w,",\"added_at\":",m->saved_s);
        ink_mogo_jw_field(&w,",\"label\":",label); ink_mogo_jw_field(&w,",\"excerpt\":",excerpt); ink_jw_raw(&w,"}");
    }
    ink_jw_raw(&w,"]}");
    return ink_mogo_jw_end(&w);
}
static bool ink_mogo_list_turn(int direction) {
    int *page=s_view==TOC?NULL:s_reader_panel==INK_MOGO_MARKS?&s_bookmark_page:s_view==SHELF?&ink_mogo_shelf_page:NULL;
    int last=s_view==TOC?book_toc_pages(book_navigation_count())-1:s_reader_panel==INK_MOGO_MARKS?bookmark_pages(bookmark_count())-1:(ink_mogo_shelf_count?(ink_mogo_shelf_count-1)/INK_MOGO_SHELF_ROWS:0);
    int at=page?*page:(int)ink_mogo_context.leaf, next=at+direction;
    if ((s_view==READING && s_reader_panel!=INK_MOGO_MARKS) || next<0 || next>last) return false;
    if (page) *page=next; else ink_mogo_context.leaf=(size_t)next;
    return true;
}
static bool ink_mogo_turn(int pages,ink_mogo_request_t *r) {
    if (!s_text || s_view!=READING) return false;
    ink_mogo_context.now_ms=ink_now_us()/1000; s_reader_panel=READER_PANEL_NONE;
    reader_position_t before=s_reader_session.position;
    /* Mogo saves every 8th turn; here the place is written after a pause instead, so no flash write lands on a waveform. */
    s_turning=true;
    for (int i=0;i<(pages<0?-pages:pages);++i) { s_unsaved=0; (void)turn_page(&ink_mogo_context,pages<0?-1:1); }
    s_turning=false;
    for (int i=1;i<(pages<0?-pages:pages);++i) ink_probe_count("reader.turns_coalesced");
    r->revision=(uint32_t)s_reader_session.position.page+1;
    return s_text && (before.chapter!=s_reader_session.position.chapter || before.page!=s_reader_session.position.page);
}
int ink_mogo_page_command(ink_mogo_request_t *r) {
    int v=r->value; bool ok=true;
    r->revision=0; r->say[0]=0;
    if (r->op==INK_MOGO_OP_SAVE) { /* a pause in reading: written behind sound and touch, not ahead of them */
        ink_mogo_quiet_flush=true; ink_mogo_save_all(); ink_mogo_quiet_flush=false; return s_save_failed?-1:0;
    }
    if (r->op==INK_MOGO_OP_LOAD || r->op==INK_MOGO_OP_OPEN) {
        int status=r->op==INK_MOGO_OP_OPEN || r->text[0]?ink_mogo_open_book(r->text):ink_mogo_scan();
        if (!status && s_text) { /* reader.restored_page: about which page of the whole book (the old reader numbered TXT pages so) */
            size_t pages=book_layout_page_count(); uint32_t start=book_chapter_byte_offset(s_reader_session.position.chapter), end=chapter_end();
            size_t per=pages && end>start?(end-start)/pages:0;
            size_t before=0; /* each earlier chapter: its bytes at this chapter's bytes per page, at least one page */
            for (size_t c=0;c<s_reader_session.position.chapter;++c) { uint32_t bytes=book_chapter_byte_offset(c+1)-book_chapter_byte_offset(c); before+=per?(bytes+per-1)/per:1; if (per && !bytes) before++; }
            r->revision=(uint32_t)(before+s_reader_session.position.page+1); (void)start;
        }
        return status;
    }
    if (r->op==INK_MOGO_OP_SHELF) return ink_mogo_scan();
    if (r->op==INK_MOGO_OP_GET_LAYOUT) return r->json && ink_mogo_json_layout(r->json,r->cap)>0?0:-1;
    if (r->op==INK_MOGO_OP_LAYOUT) {
        if (!ink_mogo_layout(r->layout)) return -1;
        save_progress();
        return r->json && ink_mogo_json_layout(r->json,r->cap)<=0?-1:0;
    }
    if (r->op==INK_MOGO_OP_LIST) return ink_mogo_list_turn(v)?0:-1;
    if (r->op==INK_MOGO_OP_BACK) {
        if (s_view==TOC || s_reader_panel!=READER_PANEL_NONE) { s_reader_panel=READER_PANEL_NONE; s_view=s_text?READING:SHELF; return 0; }
        return ink_mogo_scan();
    }
    if (r->op==INK_MOGO_OP_MENU && v==3) return ink_mogo_scan();
    if (!s_text) return -1;
    switch (r->op) {
    case INK_MOGO_OP_TURN: ok=ink_mogo_turn(v,r); break;
    case INK_MOGO_OP_READ: r->revision=(uint32_t)s_reader_session.position.page+1; ok=s_view==READING; break;
    case INK_MOGO_OP_GOTO_PAGE:
        ok=v>=1 && (size_t)v<=book_layout_page_count();
        if (ok) { s_reader_session.position.page=(size_t)v-1; skip_hidden_image_pages(&ink_mogo_context,1); s_view=READING; s_reader_panel=READER_PANEL_NONE; r->revision=(uint32_t)s_reader_session.position.page+1; }
        break;
    case INK_MOGO_OP_CHAPTERS: /* about 6 bytes a title character at worst; grown when a list does not fit */
        return r->json && ink_mogo_json_fit(&r->json,&r->cap,book_navigation_count()*160+640,ink_mogo_json_chapters,NULL)>0?0:-1;
    case INK_MOGO_OP_MARKS:
        if (v) { s_view=READING; s_reader_panel=INK_MOGO_MARKS; s_bookmark_page=0; return 0; }
        return r->json && ink_mogo_json_fit(&r->json,&r->cap,BOOKMARK_MAX*400+64,ink_mogo_json_marks,NULL)>0?0:-1;
    case INK_MOGO_OP_TOC: s_view=TOC; ink_mogo_context.leaf=current_toc_position()/BOOK_TOC_ROWS; s_reader_panel=READER_PANEL_NONE; break;
    case INK_MOGO_OP_TOC_PERCENT: ink_mogo_context.leaf=(size_t)book_toc_page_from_percent(v,book_toc_pages(book_navigation_count())); break;
    case INK_MOGO_OP_ENTRY: case INK_MOGO_OP_CHAPTER: {
        size_t target=(size_t)v-1;
        if (r->op==INK_MOGO_OP_ENTRY ? v<1 || (size_t)v>book_navigation_count() : v<1 || (size_t)v>book_chapter_count()) return -1;
        ok=r->op==INK_MOGO_OP_ENTRY ? load_chapter_at(&ink_mogo_context,book_navigation_chapter(target),0,false,book_navigation_anchor(target),book_navigation_source_offset(target)) :
            load_chapter(&ink_mogo_context,target,0,false);
        if (ok) { skip_hidden_image_pages(&ink_mogo_context,1); s_selected_toc=r->op==INK_MOGO_OP_ENTRY?target:SIZE_MAX; s_view=READING; s_reader_panel=READER_PANEL_NONE; r->revision=(uint32_t)s_reader_session.position.page+1; }
        break;
    }
    case INK_MOGO_OP_ADD_MARK: ok=ink_mogo_mark_write(true,0); break;
    case INK_MOGO_OP_REMOVE_MARK: {
        reader_bookmarks_t marks; uint32_t id=(uint32_t)v;
        if (!id && bookmark_load(&marks)) for (size_t i=0;i<marks.count;++i)
            if (marks.entries[i].chapter==s_reader_session.position.chapter && marks.entries[i].byte_off==book_layout_page_start_offset(s_reader_session.position.page)) id=ink_mogo_mark_id(&marks.entries[i]);
        ok=id && ink_mogo_mark_write(false,id);
        break;
    }
    case INK_MOGO_OP_GOTO_MARK: {
        reader_bookmarks_t marks; ok=false;
        if (bookmark_load(&marks)) for (size_t i=0;i<marks.count && !ok;++i)
            if (ink_mogo_mark_id(&marks.entries[i])==(uint32_t)v) ok=load_chapter(&ink_mogo_context,marks.entries[i].chapter,marks.entries[i].byte_off,false);
        if (ok) { skip_hidden_image_pages(&ink_mogo_context,1); s_view=READING; s_reader_panel=READER_PANEL_NONE; r->revision=(uint32_t)s_reader_session.position.page+1; }
        break;
    }
    case INK_MOGO_OP_MENU: s_view=READING; s_reader_panel=v==2?INK_MOGO_SETTINGS:v==1?INK_MOGO_TOOLS:READER_PANEL_NONE; break;
    default: return -1;
    }
    if (!ok) return -1;
    if (r->op==INK_MOGO_OP_READ) {
        size_t start=book_layout_page_start_offset(s_reader_session.position.page), end=book_layout_page_start_offset(s_reader_session.position.page+1);
        int head=snprintf(r->say,sizeof r->say,"%s · %s · %s %u\n",s_reader_session.book.title,s_reader_session.book.chapter_title,TR("章内页"),(unsigned)r->revision);
        size_t n=end>start?end-start:0;
        if (head<0 || (size_t)head>=sizeof r->say) head=0;
        if (n>=sizeof r->say-(size_t)head) n=sizeof r->say-(size_t)head-1;
        while (n && ((unsigned char)s_text[start+n]&0xc0)==0x80) --n;
        memcpy(r->say+head,s_text+start,n); r->say[head+(int)n]=0;
    } else if (r->revision) snprintf(r->say,sizeof r->say,TR("现在是第 %u 页"),(unsigned)r->revision);
    return 0; /* saved after a pause in reading (OP_SAVE), by Mogo's own turns and chapter changes, and at the close */
}
int ink_mogo_page_reason(void) { return ink_mogo_reason; }
const char *ink_mogo_page_message(void) { return ink_mogo_error; }
bool ink_mogo_page_root(void) { return ink_mogo_reason || s_view==SHELF || !s_text; }
bool ink_mogo_page_back_shown(void) { return ink_mogo_page_root() || s_view==TOC || s_reader_panel!=READER_PANEL_NONE; }
void ink_mogo_page_draw(uint8_t *pixels) {
    ink_mogo_ui_coordinates(false);
    if (ink_mogo_reason) { ui_clear_page(pixels); return; }
    if (s_text && s_view!=SHELF && s_view!=TOC) draw_reader(pixels,s_reader_session.position.page);
    else ui_clear_page(pixels);
    ink_mogo_ui_coordinates(true);
    if (s_view==READING && s_text) {
        if (ink_mogo_settings.header) ui_text_box(pixels,(EpdRect){36,8,612,64},24,s_reader_session.book.title,EPD_DRAW_ALIGN_CENTER,false);
        if (ink_mogo_settings.footer) { char text[80]; snprintf(text,sizeof text,"%zu / %zu · %u%%",s_reader_session.position.page+1,book_layout_page_count(),percent(s_reader_session.position.page)); ui_text_box(pixels,(EpdRect){36,1130,612,64},24,text,EPD_DRAW_ALIGN_CENTER,false); }
    }
    if (s_view==TOC) book_toc_render(pixels,s_reader_session.book.title,book_navigation_count(),current_toc_position(),(int)ink_mogo_context.leaf,s_message);
    else if (s_view==SHELF) {
        ui_text_box(pixels,(EpdRect){36,80,612,90},42,TR("书架"),EPD_DRAW_ALIGN_CENTER,false);
        for (int row=0;row<INK_MOGO_SHELF_ROWS && ink_mogo_shelf_page*INK_MOGO_SHELF_ROWS+row<ink_mogo_shelf_count;++row)
            ui_draw_button(pixels,(EpdRect){36,210+row*190,612,164},ink_mogo_shelf[ink_mogo_shelf_page*INK_MOGO_SHELF_ROWS+row].name,false);
        ui_text_box(pixels,(EpdRect){36,1000,612,80},28,TR("‹ 上一页                     下一页 ›"),EPD_DRAW_ALIGN_CENTER,false);
    } else if (s_reader_panel==INK_MOGO_MARKS) {
        draw_reader_bookmarks(pixels);
        reader_bookmarks_t marks; ink_mogo_mark_hits.rows=0;
        if (bookmark_load(&marks)) for (int i=0;i<bookmark_rows() && i<BOOKMARK_MAX;++i) {
            int index=(int)marks.count-1-(s_bookmark_page*bookmark_rows()+i);
            if (index<0) break;
            ink_mogo_mark_hits.rect[i]=bookmark_row_rect(i); ink_mogo_mark_hits.id[i]=ink_mogo_mark_id(&marks.entries[index]); ink_mogo_mark_hits.rows=i+1;
        }
    }
    else if (s_reader_panel==INK_MOGO_SETTINGS) {
        draw_sheet(pixels,180,TR("阅读设置"));
        ui_draw_button(pixels,(EpdRect){36,280,612,72},TR("返回正文"),false);
        for (int i=0;i<INK_MOGO_LAYOUT_COUNT;++i) {
            char text[80]; snprintf(text,sizeof text,"-     %s  %d     +",TR(ink_mogo_setting_names[i]),ink_mogo_setting_value(i));
            ui_draw_button(pixels,(EpdRect){36,380+i*94,612,82},text,false);
        }
    } else if (s_reader_panel==INK_MOGO_TOOLS) {
        draw_sheet(pixels,640,TR("阅读工具"));
        const char *labels[]={TR_NOOP("目录"),TR_NOOP("加书签"),TR_NOOP("书签列表"),TR_NOOP("阅读设置"),TR_NOOP("书架"),TR_NOOP("返回正文")};
        for (int i=0;i<INK_MOGO_TOOL_COUNT;++i) ui_draw_button(pixels,(EpdRect){36+(i%2)*318,750+(i/2)*136,294,112},TR(labels[i]),false);
    }
    ink_mogo_ui_coordinates(false);
}
/* The corner under the shell's back button: no content and no target of Mogo's own. */
static bool ink_mogo_corner(int x,int y) { return ink_mogo_page_back_shown() && x<170 && y<150; }
const char *ink_mogo_page_hit(int x,int y,char *args,size_t cap) {
    x=x*684/ink_mogo_page_width; y=y*1216/ink_mogo_page_height; args[0]=0;
    if (ink_mogo_reason || ink_mogo_corner(x,y)) return "";
    if (s_view==SHELF) {
        int index=ink_mogo_shelf_page*INK_MOGO_SHELF_ROWS+(y-210)/190;
        if (y>=210 && y<970 && index<ink_mogo_shelf_count) { snprintf(args,cap,"{\"title\":\"\",\"index\":%d}",index); return "reader.open_book"; }
        if (y>=1000) return x<342?"reader.list_prev":"reader.list_next";
    } else if (s_view==TOC) {
        int hit=book_toc_hit((uint16_t)x,(uint16_t)y,(int)ink_mogo_context.leaf,book_navigation_count());
        if (hit>=0) { snprintf(args,cap,"{\"entry\":%d}",hit+1); return "reader.goto_chapter"; }
        if (hit==BOOK_TOC_BACK) return "reader.back";
        if (hit==BOOK_TOC_PREV || hit==BOOK_TOC_SKIP_PREV) return "reader.list_prev";
        if (hit==BOOK_TOC_NEXT || hit==BOOK_TOC_SKIP_NEXT) return "reader.list_next";
        if (hit==BOOK_TOC_JUMP) { snprintf(args,cap,"{\"value\":%d}",x*100/684); return "reader.toc_percent"; }
    } else if (s_reader_panel==INK_MOGO_MARKS) { /* UI: the rows as drawn, no flash read here */
        if (y<650) return "reader.back";
        if (y>=1120) return x<342?"reader.list_prev":"reader.list_next";
        for (int i=0;i<ink_mogo_mark_hits.rows;++i)
            if (ui_rect_hit(ink_mogo_mark_hits.rect[i],x,y)) { snprintf(args,cap,"{\"id\":%u}",(unsigned)ink_mogo_mark_hits.id[i]); return "reader.goto_bookmark"; }
    } else if (s_reader_panel==INK_MOGO_SETTINGS) {
        if (y<180 || (y>=280 && y<352)) return "reader.back";
        if (y>=380 && y<1132) {
            int i=(y-380)/94, delta=x<342?-1:1, v=ink_mogo_setting_value(i);
            if (i==1 || i==2) delta*=5;
            if (i==3) delta*=12;
            if (i>=6) v=!v; else v+=delta;
            if (v<ink_mogo_layout_range[i][0] || v>ink_mogo_layout_range[i][1]) return "";
            snprintf(args,cap,"{\"%s\":%d}",ink_mogo_layout_keys[i],v); return "reader.set_layout";
        }
    } else if (s_reader_panel==INK_MOGO_TOOLS) {
        static const char *const actions[]={"reader.show_chapters","reader.add_bookmark","reader.bookmarks","reader.menu","reader.menu","reader.back"};
        static const char *const values[]={"","","{\"open\":true}","{\"view\":\"settings\"}","{\"view\":\"shelf\"}",""};
        if (y<640) return "reader.back";
        if (y>=750 && y<1158) { int i=((y-750)/136)*2+(x>=342); snprintf(args,cap,"%s",values[i]); return actions[i]; }
    } else if (s_view==READING && s_text) {
        if (x<228) return "reader.prev_page";
        if (x>=456) return "reader.next_page";
        snprintf(args,cap,"{\"view\":\"menu\"}"); return "reader.menu";
    }
    return "";
}
void ink_mogo_page_close(void) {
    ink_mogo_save_all(); free_book(); ink_mogo_font_shutdown(); memset(&s_reader_session,0,sizeof s_reader_session);
    free(ink_mogo_shelf); ink_mogo_shelf=NULL; ink_mogo_shelf_count=ink_mogo_shelf_page=0; ink_mogo_reason=0; ink_mogo_error[0]=0; ink_mogo_anchor=SIZE_MAX;
    memset(&ink_mogo_settings,0,sizeof ink_mogo_settings); /* the card written stays readable until the next session starts */
    s_reader_panel=READER_PANEL_NONE; s_reader_slider=-1; s_view=SHELF;
    s_bookmark_page=0; s_bookmark_selected=0; ink_mogo_mark_hits.rows=0; s_bookmark_edit=s_bookmark_delete_confirm=s_bookmark_delete_error=false;
    s_save_failed=false; s_message[0]=s_reader_notice[0]=0;
}
static const char *ink_mogo_panel(void) {
    static const char *const reasons[]={"",TR_NOOP("没有卡"),TR_NOOP("缺字体"),TR_NOOP("缺字体"),TR_NOOP("打不开"),TR_NOOP("书架空")};
    return ink_mogo_reason?reasons[ink_mogo_reason]:s_view==SHELF||!s_text?TR_NOOP("书架"):s_view==TOC?TR_NOOP("目录"):s_reader_panel==INK_MOGO_MARKS?TR_NOOP("书签列表"):
        s_reader_panel==INK_MOGO_SETTINGS?TR_NOOP("设置"):s_reader_panel==INK_MOGO_TOOLS?TR_NOOP("工具"):TR_NOOP("正文");
}
static int ink_mogo_put(char *out,size_t cap,int len,const char *line) {
    if (len<0 || (size_t)len>=cap) return len;
    size_t n=strlen(line);
    if (n>=cap-(size_t)len) { n=cap-(size_t)len-1; while (n && ((unsigned char)line[n]&0xc0)==0x80) --n; }
    memcpy(out+len,line,n); out[len+(int)n]=0;
    return len+(int)n;
}
#define INK_MOGO_LINE(...) (snprintf(line,sizeof line,__VA_ARGS__),len=ink_mogo_put(out,cap,len,line))
/* The explanation page's words (interface spec section 0 vocabulary). */
const char *ink_mogo_page_explanation(void) {
    switch (ink_mogo_reason) {
    case INK_MOGO_R_NO_CARD: return TR("没有插存储卡。书和阅读字体都放在存储卡上，插好后再打开阅读。");
    case INK_MOGO_R_NO_FONT: return TR("还缺阅读用的字体，暂时不能显示书。字体文件 InkSC.ttf 应放在存储卡的 fonts 文件夹里；连着电脑装机时会自动放进去。");
    case INK_MOGO_R_BAD_FONT: return TR("阅读字体（存储卡 fonts/InkSC.ttf）读不出来，请连着电脑重新装机补上。");
    case INK_MOGO_R_BAD_BOOK: return ink_mogo_error;
    case INK_MOGO_R_EMPTY: return ink_member_current()->role==INK_ROLE_CHILD?TR("请家长用‘传书’把书传进来。"):
        TR("书架上还没有书。用‘传书’从电脑或手机传进来，会放在‘传进来的’；全家的书放在存储卡的 books 文件夹。");
    default: return "";
    }
}
/* What is shown, for the AI: the first line, machine fields, the text, the lists and the actions here. */
int ink_mogo_page_snapshot(char *out,size_t cap) {
    char line[400];
    const char *panel=ink_mogo_panel(); size_t page=s_reader_session.position.page, pages=s_text?book_layout_page_count():0;
    size_t start=s_text?book_layout_page_start_offset(page):0, end=s_text?book_layout_page_start_offset(page+1):0;
    int len=0; out[0]=0;
    if (s_text && !ink_mogo_reason) INK_MOGO_LINE(TR_NOOP("阅读 《%s》 · %s · 章内页 %zu/%zu · %u%%\n"),s_reader_session.book.title,s_reader_session.book.chapter_title,page+1,pages,percent(page));
    else INK_MOGO_LINE(TR_NOOP("阅读 %s\n"),TR(panel));
    INK_MOGO_LINE(TR_NOOP("章节=%zu 页=%zu/%zu 位置=%zu 结束=%zu 字号=%d 面板=%s 书签=%zu 本页书签=%d\n"),s_text?s_reader_session.position.chapter+1:0,s_text?page+1:0,pages,start,end,s_px,panel,s_text?bookmark_count():0,s_text && bookmark_is_current());
    if (ink_mogo_reason) INK_MOGO_LINE("%s\n",ink_mogo_page_explanation());
    else if (s_view==SHELF || !s_text) for (int i=0;i<ink_mogo_shelf_count;++i)
        INK_MOGO_LINE("%d %s %s %u%%\n",i,ink_mogo_shelf[i].name,TR(ink_mogo_place(ink_mogo_shelf[i].path)),ink_mogo_shelf[i].pct);
    else if (s_view==TOC) {
        if (ink_mogo_fallback_toc()) INK_MOGO_LINE("%s\n",TR("没有目录，按章节阅读"));
        for (size_t i=0;i<book_navigation_count();++i) { char title[96]=""; (void)book_navigation_title(i,title,sizeof title); INK_MOGO_LINE("%zu %s\n",i+1,title); }
    } else if (s_reader_panel==INK_MOGO_MARKS) {
        reader_bookmarks_t marks;
        if (bookmark_load(&marks)) for (int i=(int)marks.count-1;i>=0;--i) {
            char label[96]; if (book_chapter_title(marks.entries[i].chapter,label,sizeof label)!=ESP_OK) label[0]=0;
            INK_MOGO_LINE("%u %s %u%%\n",(unsigned)ink_mogo_mark_id(&marks.entries[i]),label,ink_mogo_mark_percent(&marks.entries[i]));
        }
    } else if (s_reader_panel==INK_MOGO_SETTINGS) for (int i=0;i<INK_MOGO_LAYOUT_COUNT;++i) INK_MOGO_LINE("%s=%d\n",ink_mogo_layout_keys[i],ink_mogo_setting_value(i));
    if (s_text && !ink_mogo_reason && s_view==READING) {
        size_t n=end>=start?end-start:0;
        if (n>600) n=600;
        while (n && ((unsigned char)s_text[start+n]&0xc0)==0x80) --n;
        if (len>=0 && (size_t)len+n+1<cap) { memcpy(out+len,s_text+start,n); len+=(int)n; out[len++]='\n'; out[len]=0; }
    }
    static const char *const actions[]={"reader.open_book reader.list_next reader.list_prev reader.list_books reader.continue reader.open_sample reader.get_layout reader.set_layout reader.back",
        "reader.next_page reader.prev_page reader.goto_page reader.read_page reader.chapters reader.show_chapters reader.goto_chapter reader.bookmarks reader.add_bookmark reader.remove_bookmark reader.goto_bookmark reader.get_layout reader.set_layout reader.menu reader.back",
        "reader.goto_chapter reader.toc_percent reader.list_next reader.list_prev reader.chapters reader.back",
        "reader.goto_bookmark reader.remove_bookmark reader.bookmarks reader.list_next reader.list_prev reader.back",
        "reader.set_layout reader.get_layout reader.menu reader.back", "reader.show_chapters reader.add_bookmark reader.bookmarks reader.menu reader.back",
        "reader.back", "reader.back transfer.open"};
    int which=ink_mogo_reason==INK_MOGO_R_EMPTY && ink_member_current()->role!=INK_ROLE_CHILD?7:ink_mogo_reason?6:s_view==SHELF||!s_text?0:s_view==TOC?2:
        s_reader_panel==INK_MOGO_MARKS?3:s_reader_panel==INK_MOGO_SETTINGS?4:s_reader_panel==INK_MOGO_TOOLS?5:1;
    return INK_MOGO_LINE(TR_NOOP("动作：%s"),actions[which]);
}
