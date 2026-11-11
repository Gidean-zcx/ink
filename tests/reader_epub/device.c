/* The C boundary and the whole reader, on the deterministic simulated device. */
#include <stdlib.h>
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include <reader_source.h>
#include "ink/sys/app.h"
#include "ink/sys/storage.h"
#include "ink/sys/crc.h"
#include "ink/sys/layout.h"

static char view[8192], before[8192];
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(),15000),"reader settles"); }
static void ok(const char *cap, const char *args) {
    CHECK(AI(cap,args) && g_rc.status==INK_RC_DONE,"%s returns done: %.95s / %.95s",cap,g_rc.detail,g_rc.say); quiet();
}
static void inspect(void) { CHECK(ink_ui_snapshot(view,sizeof view,NULL)>0,"page snapshot available"); }
static void home(void) { ink_ui_home(); }
static void back(void) { ink_ui_pop(); }
static void close_book(void) { on_ui(home); quiet(); CHECK(WAIT_FOR(!ink_storage_pending(),15000),"position durable"); }
static bool cancel_now(void *ctx) { (void)ctx; return true; }
static void source_checks(void) {
    const char *books[]={"epub2.epub","epub3.epub"};
    for (unsigned i=0;i<2;i++) {
        ink_reader_source_t *s=NULL; ink_reader_info_t info; char path[128];
        snprintf(path,sizeof path,"/content/books/%s",books[i]);
        CHECK(ink_reader_source_open(path,&s,&info)==INK_READER_OK,"open %s through C",books[i]);
        if (!s) continue;
        CHECK(!strcmp(info.title,i ? "星图日记" : "山间来信") && !strcmp(info.author,i ? "测试作者乙" : "测试作者甲"),"metadata %s",books[i]);
        CHECK(info.chapters==2 && info.entries==3 && !info.fallback,"spine and contents %s",books[i]);
        ink_reader_entry_t e={0}; CHECK(!ink_reader_source_toc(s,2,&e) && e.chapter==1 && e.offset>0 && !strcmp(e.title,"中途码头"),"fragment maps inside chapter");
        char text[2048]; size_t n=0; bool more=false;
        CHECK(ink_reader_source_read(s,e.chapter,e.offset,text,sizeof text,&n,&more,NULL,NULL)==INK_READER_OK && !strncmp(text,"中途码头",strlen("中途码头")),"fragment body: %.150s",text);
        CHECK(strstr(text,"［图片］"),"image placeholder present");
        CHECK(ink_reader_source_read(s,0,0,text,sizeof text,&n,&more,cancel_now,NULL)==INK_READER_CANCELLED,"cancel bounded read");
        char *whole=ink_app_alloc(32768,true), *joined=ink_app_alloc(32768,true); size_t total=0, full=0;
        CHECK(whole && joined,"chunk test buffers");
        if (whole && joined) {
            CHECK(ink_reader_source_read(s,0,0,whole,32768,&full,&more,NULL,NULL)==INK_READER_OK && !more,"whole short chapter");
            do {
                CHECK(ink_reader_source_read(s,0,total,text,128,&n,&more,NULL,NULL)==INK_READER_OK && n,"bounded paragraph read");
                if (!n || total+n>=32768) break;
                CHECK(!more || text[n-1]=='\n',"cut on a paragraph boundary");
                memcpy(joined+total,text,n); total+=n;
            } while (more);
            CHECK(total==full && !memcmp(joined,whole,full),"chunking loses no UTF-8 bytes");
        }
        ink_app_free(whole); ink_app_free(joined); ink_reader_source_close(s);
    }
    ink_reader_source_t *s=NULL; ink_reader_info_t info; char text[1024]; size_t n; bool more;
    CHECK(ink_reader_source_open("/content/books/chapters.txt",&s,&info)==INK_READER_OK && info.entries==2,"vendor TXT chapter rules");
    ink_reader_entry_t e={0}; CHECK(!ink_reader_source_toc(s,1,&e) && e.offset>0 && !strcmp(e.title,"第二章 夜晚"),"TXT chapter title and byte anchor");
    ink_reader_source_close(s);
    CHECK(ink_reader_source_open("/content/books/gb18030.txt",&s,&info)==INK_READER_OK,"GB18030 opens");
    CHECK(ink_reader_source_read(s,0,0,text,sizeof text,&n,&more,NULL,NULL)==INK_READER_OK && strstr(text,"春天的山路"),"vendor GB18030 decoding"); ink_reader_source_close(s);
    CHECK(ink_reader_source_open("/content/books/no-toc.epub",&s,&info)==INK_READER_OK && info.fallback && info.entries==2,"spine fallback"); ink_reader_source_close(s);
    CHECK(ink_reader_source_open("/content/books/corrupt.epub",&s,&info)==INK_READER_CORRUPT && !s,"damaged ZIP rejected");
    CHECK(ink_reader_source_open("/content/books/not-epub.epub",&s,&info)==INK_READER_FORMAT && !s,"non-EPUB rejected");
    CHECK(ink_reader_source_open("/content/books/encrypted.epub",&s,&info)==INK_READER_ENCRYPTED && !s,"encrypted ZIP rejected");
    CHECK(ink_reader_source_open("/content/books/long.epub",&s,&info)==INK_READER_OK,"long EPUB opens");
    char *large=ink_app_alloc(2u*1024u*1024u+1,true); size_t total=0, parts=0;
    if (large && s) do {
        CHECK(ink_reader_source_read(s,0,total,large,2u*1024u*1024u+1,&n,&more,NULL,NULL)==INK_READER_OK && n,"long bounded read");
        CHECK(!more || large[n-1]=='\n',"long section ends on paragraph"); total+=n; parts++;
    } while (more && parts<4);
    CHECK(parts>1 && total>2u*1024u*1024u && !more,"chapter larger than PSRAM buffer split losslessly");
    ink_app_free(large); ink_reader_source_close(s);
}
static void reader_checks(void) {
    ok("app.open","{\"id\":\"reader\"}"); on_ui(inspect);
    CHECK(strstr(view,"书架") && strstr(view,"reader.open_book"),"semantic bookshelf uses book names");
    int pages=0;
    while (AI("reader.list_next",NULL) && g_rc.status==INK_RC_DONE) { quiet(); pages++; if (pages>20) break; }
    CHECK(pages>=6,"more than 16 books reachable by pagination (%d)",pages); close_book();
    ok("reader.open_book","{\"title\":\"山间来信\"}");
    ok("reader.chapters",NULL); CHECK(strstr(g_result,"第一章 山路") && strstr(g_result,"中途码头"),"AI sees real TOC: %.150s",g_result);
    ok("reader.read_page",NULL); CHECK(strstr(g_rc.say,"山间来信") && strstr(g_rc.say,"第一章 山路"),"AI page includes book and chapter");
    ok("reader.goto_chapter","{\"chapter\":2}"); ok("reader.read_page",NULL);
    CHECK(strstr(g_rc.say,"第二章 海边"),"next chapter body");
    ok("reader.prev_page",NULL); ok("reader.read_page",NULL);
    CHECK(strstr(g_rc.say,"第一章 山路") && g_rc.revision>1,"previous chapter last page");
    ok("reader.next_page",NULL); ok("reader.read_page",NULL);
    CHECK(strstr(g_rc.say,"第二章 海边") && g_rc.revision==1,"forward across chapter returns to first page");
    ok("reader.show_chapters",NULL); on_ui(inspect);
    CHECK(!strcmp(ink_ui_top_name(),"chapters") && strstr(view,"中途码头"),"contents page and AI snapshot agree");
    ok("reader.goto_chapter","{\"entry\":3}"); ok("reader.read_page",NULL);
    CHECK(strstr(g_rc.say,"第二章 海边"),"TOC tap route returns to reading page");
    ok("reader.goto_chapter","{\"chapter\":1}"); ok("reader.goto_page","{\"page\":2}"); on_ui(inspect); snprintf(before,sizeof before,"%s",view);
    close_book(); ok("reader.open_book","{\"title\":\"山间来信\"}"); on_ui(inspect);
    CHECK(!strcmp(before,view),"close and reopen restores chapter and offset"); close_book();
    CHECK(!ink_member_switch("kid"),"child member"); quiet(); ok("reader.open_book","{\"title\":\"山间来信\"}"); ok("reader.goto_chapter","{\"chapter\":2}"); close_book();
    CHECK(!ink_member_switch("owner"),"adult member"); quiet(); ok("reader.open_book","{\"title\":\"山间来信\"}"); on_ui(inspect);
    CHECK(!strcmp(before,view),"member positions isolated"); close_book();
    ok("reader.open_book","{\"title\":\"星图日记\"}"); ok("reader.chapters",NULL); CHECK(strstr(g_result,"中途码头"),"EPUB3 NAV is exposed"); close_book();
    CHECK(AI("reader.open_book","{\"title\":\"corrupt\"}") && g_rc.status==INK_RC_FAILED && strstr(g_rc.say,"损坏"),"actionable Chinese corrupt-book receipt");
    on_ui(inspect); CHECK(strstr(view,"损坏"),"same Chinese error on page"); close_book();
}
static void restart_checks(void) {
    ok("reader.continue",NULL); /* Last readable book is EPUB3. */
    close_book(); ok("reader.open_book","{\"title\":\"山间来信\"}"); ok("reader.read_page",NULL);
    CHECK(strstr(g_rc.say,"第一章 山路") && g_rc.revision==2,"restart restores adult chapter/page"); close_book();
    CHECK(!ink_member_switch("kid"),"child after restart"); quiet(); ok("reader.open_book","{\"title\":\"山间来信\"}"); ok("reader.read_page",NULL);
    CHECK(strstr(g_rc.say,"第二章 海边") && g_rc.revision==1,"restart restores child independently"); close_book();
}
static void run(void *arg) {
    (void)arg; ink_power_hold(INK_NEED_AWAKE); ink_power_hold(INK_NEED_NO_LOCK); quiet();
    const char *phase=getenv("INK_READER_EPUB_PHASE");
    if (phase && !strcmp(phase,"restart")) restart_checks(); else { source_checks(); reader_checks(); }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); test_start_home();
    osal_task_cfg_t task={.name="test.reader-epub",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&task,NULL)==OSAL_OK,"start EPUB integration checks");
}
