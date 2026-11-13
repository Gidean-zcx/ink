/* EPUB and TXT books through the one reader (reader-one-1): what the old C source checks found is now read through
 * reader.chapters and the page snapshot, the same way the AI sees it. Deterministic simulated device. */
#include <stdlib.h>
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include "graft.h"
#include "ink/sys/app.h"
#include "ink/sys/storage.h"
#include "ink/ui_theme.h"

static char view[8192], before[8192];
static volatile bool captured;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(),15000),"reader settles"); }
static void ok(const char *cap, const char *args) {
    CHECK(AI(cap,args) && g_rc.status==INK_RC_DONE,"%s returns done: %.95s / %.95s",cap,g_rc.detail,g_rc.say); quiet();
}
static void graft_view(void) { ink_graft_snapshot(view,sizeof view); }
static void inspect(void) { on_ui(graft_view); }
static void home(void) { ink_ui_home(); }
static void close_book(void) { on_ui(home); quiet(); CHECK(WAIT_FOR(!ink_storage_pending(),15000),"position durable"); }
static void capture(void *name) { sim_display_snapshot(name); captured=true; }
static void frame(const char *name) {
    quiet(); captured=false; vtk_event_at(vtk_now()+SIM_MS(1),capture,(void *)name);
    CHECK(WAIT_FOR(captured,2000),"headless frame captured");
}
static int geometry_top, geometry_h, geometry_w;
static void geometry(void) { geometry_top=ink_ui_px(108); geometry_h=ink_ui_height()-geometry_top; geometry_w=ink_ui_width(); }
static void tap_native(int x,int y) { on_ui(geometry); tap(x*geometry_w/684,geometry_top+y*geometry_h/1216); quiet(); }
static int pages_here(void) { inspect(); const char *p=strstr(view,"页="); return p ? atoi(strchr(p,'/')+1) : 0; }
static void reader_checks(void) {
    ok("app.open","{\"id\":\"reader\"}");
    for (int i=0;i<200 && (inspect(),!strstr(view,"面板=书架"));i++) delay_ms(50); /* the page loads on BG after the app opens */
    CHECK(strstr(view,"面板=书架") && strstr(view,"山间来信 全家的") && strstr(view,"reader.open_book"),"the shelf names books by their own titles: %.150s",view);
    int pages=0;
    while (AI("reader.list_next",NULL) && g_rc.status==INK_RC_DONE) { quiet(); pages++; if (pages>20) break; }
    CHECK(pages>=6,"more than 24 books reachable by pagination (%d)",pages); close_book();
    ok("reader.open_book","{\"title\":\"山间来信\"}");
    ok("reader.chapters",NULL); CHECK(strstr(g_result,"第一章 山路") && strstr(g_result,"中途码头"),"AI sees the NCX contents: %.150s",g_result);
    CHECK(strstr(g_result,"测试作者甲") && strstr(g_result,"\"chapter_count\":2") && strstr(g_result,"\"fallback\":false"),"author, spine count and no fallback: %.200s",g_result);
    CHECK(strstr(g_result,"{\"title\":\"中途码头\",\"chapter\":2,\"entry\":3}"),"the fragment entry maps inside chapter two");
    ok("reader.read_page",NULL); CHECK(strstr(g_rc.say,"山间来信") && strstr(g_rc.say,"第一章 山路"),"AI page includes book and chapter");
    unsigned previous=g_rc.revision; bool crossed=false;
    for (int i=0;i<40;i++) {
        ok("reader.next_page",NULL); ok("reader.read_page",NULL);
        if (strstr(g_rc.say,"第二章 海边")) { crossed=true; break; }
        CHECK(g_rc.revision==previous+1,"chapter page count continues"); previous=g_rc.revision;
    }
    CHECK(crossed && g_rc.revision==1,"next-page flow reaches the next chapter");
    ok("reader.goto_chapter","{\"chapter\":2}"); ok("reader.read_page",NULL);
    CHECK(strstr(g_rc.say,"第二章 海边"),"next chapter body");
    ok("reader.prev_page",NULL); ok("reader.read_page",NULL);
    CHECK(strstr(g_rc.say,"第一章 山路") && g_rc.revision>1,"previous chapter last page");
    ok("reader.next_page",NULL); ok("reader.read_page",NULL);
    CHECK(strstr(g_rc.say,"第二章 海边") && g_rc.revision==1,"forward across chapter returns to first page");
    /* chapter one is the saved place (a close saves it) before the contents jump, which must then be saved itself */
    ok("reader.goto_chapter","{\"chapter\":1}"); close_book(); ok("reader.open_book","{\"title\":\"山间来信\"}"); ok("reader.show_chapters",NULL); inspect();
    CHECK(!strcmp(ink_ui_top_name(),"book") && strstr(view,"面板=目录") && strstr(view,"3 中途码头"),"contents page and AI snapshot agree: %.200s",view);
    frame("epub-contents");
    ok("reader.chapters",NULL); CHECK(strstr(g_result,"中途码头"),"AI can list chapters while the contents page is open");
    tap_native(342,252+2*108+47); ok("reader.read_page",NULL);
    CHECK(strstr(g_rc.say,"第二章 海边") && strstr(g_rc.say,"中途码头"),"TOC tap goes to the fragment in chapter two: %.150s",g_rc.say);
    close_book(); ok("reader.continue",NULL);
    CHECK(!strcmp(ink_ui_top_name(),"book"),"continue finds the EPUB");
    ok("reader.read_page",NULL); CHECK(strstr(g_rc.say,"第二章 海边"),"TOC navigation persists across close and continue");
    ok("reader.goto_chapter","{\"chapter\":1}"); ok("reader.goto_page","{\"page\":2}"); inspect(); snprintf(before,sizeof before,"%s",view);
    CHECK(AI("reader.goto_page","{\"page\":1000000}") && g_rc.status==INK_RC_FAILED,"out-of-range chapter page rejected"); quiet(); inspect();
    CHECK(!strcmp(before,view),"failed seek preserves current chapter page");
    close_book(); ok("reader.open_book","{\"title\":\"山间来信\"}"); inspect();
    CHECK(!strcmp(before,view),"close and reopen restores chapter and offset");
    close_book();
    CHECK(!ink_member_switch("kid"),"child member"); quiet(); ok("reader.open_book","{\"title\":\"山间来信\"}"); ok("reader.goto_chapter","{\"chapter\":2}"); close_book();
    CHECK(!ink_member_switch("owner"),"adult member"); quiet(); ok("reader.open_book","{\"title\":\"山间来信\"}"); inspect();
    CHECK(!strcmp(before,view),"member positions isolated"); close_book();
    ok("reader.open_book","{\"title\":\"星图日记\"}"); ok("reader.chapters",NULL);
    CHECK(strstr(g_result,"中途码头") && strstr(g_result,"测试作者乙"),"EPUB3 NAV is exposed"); close_book();
    /* Without NCX/NAV Mogo builds the contents from the chapter headings; only with neither does it fall back. */
    ok("reader.open_book","{\"title\":\"无目录散记\"}"); ok("reader.chapters",NULL);
    CHECK(strstr(g_result,"{\"title\":\"第一章 山路\",\"chapter\":1,\"entry\":1}") && strstr(g_result,"第二章 海边") && strstr(g_result,"\"fallback\":false"),
          "a book without contents gets them from its headings: %.200s",g_result); close_book();
    ok("reader.open_book","{\"title\":\"无标题散记\"}"); ok("reader.show_chapters",NULL); inspect();
    CHECK(strstr(view,"没有目录，按章节阅读") && strstr(view,"1 第 1 节"),"fallback notice and chapter names are exposed to AI: %.200s",view);
    ok("reader.chapters",NULL); CHECK(strstr(g_result,"第 1 节") && strstr(g_result,"\"fallback\":true"),"fallback chapter labels agree with the contents page");
    frame("epub-fallback-zh");
    ok("settings.set","{\"id\":\"system.language\",\"value\":1}"); close_book(); /* the language is read when the page is laid out */
    ok("reader.open_book","{\"title\":\"无标题散记\"}"); ok("reader.show_chapters",NULL); inspect();
    CHECK(strstr(view,"No contents found"),"English fallback notice in the same snapshot");
    frame("epub-fallback-en");
    ok("settings.set","{\"id\":\"system.language\",\"value\":0}"); close_book();
    CHECK(AI("reader.open_book","{\"title\":\"corrupt\"}") && g_rc.status==INK_RC_FAILED && strstr(g_rc.say,"损坏"),"actionable Chinese corrupt-book receipt: %.100s",g_rc.say);
    inspect(); CHECK(strstr(view,"面板=打不开") && strstr(view,"损坏"),"same Chinese error on page"); frame("epub-corrupt"); close_book();
    CHECK(AI("reader.open_book","{\"title\":\"encrypted\"}") && g_rc.status==INK_RC_FAILED && strstr(g_rc.say,"加密"),"an encrypted EPUB says so: %.100s",g_rc.say); close_book();
    CHECK(AI("reader.open_book","{\"title\":\"not-epub\"}") && g_rc.status==INK_RC_FAILED && g_rc.say[0],"a ZIP that is not an EPUB is refused with a reason: %.100s",g_rc.say); close_book();
    ok("reader.open_book","{\"title\":\"gb18030\"}"); ok("reader.read_page",NULL); CHECK(strstr(g_rc.say,"春天的山路"),"GB18030 decoding"); close_book();
    ok("reader.open_book","{\"title\":\"chapters\"}"); ok("reader.chapters",NULL);
    CHECK(strstr(g_result,"第二章 夜晚") && strstr(g_result,"\"entry\":2"),"TXT chapter rules: %.150s",g_result); close_book();
}
/* The old engine split chapters over 2 MB; Mogo loads the chapter whole. What stays is that a long chapter reads to its
 * end and the next turn enters the next chapter. */
static void long_chapter(void) {
    ok("reader.open_book","{\"title\":\"长章读完\"}"); ok("reader.goto_chapter","{\"chapter\":1}");
    int last=pages_here(); CHECK(last>100,"the long chapter is many pages (%d)",last);
    char args[32]; snprintf(args,sizeof args,"{\"page\":%d}",last); ok("reader.goto_page",args);
    ok("reader.read_page",NULL); inspect(); CHECK(strstr(view,"第4999段") && g_rc.revision==(unsigned)last,"on the long chapter's last page: %.100s",g_rc.say);
    ok("reader.next_page",NULL); ok("reader.read_page",NULL);
    CHECK(strstr(g_rc.say,"第二章 海边") && g_rc.revision==1,"the next turn enters chapter two: %.100s",g_rc.say); close_book();
}
static void restart_checks(void) {
    ok("reader.continue",NULL); close_book();
    ok("reader.open_book","{\"title\":\"山间来信\"}"); ok("reader.read_page",NULL);
    CHECK(strstr(g_rc.say,"第一章 山路") && g_rc.revision==2,"restart restores adult chapter/page"); close_book();
    CHECK(!ink_member_switch("kid"),"child after restart"); quiet(); ok("reader.open_book","{\"title\":\"山间来信\"}"); ok("reader.read_page",NULL);
    CHECK(strstr(g_rc.say,"第二章 海边") && g_rc.revision==1,"restart restores child independently"); close_book();
}
static void run(void *arg) {
    (void)arg; ink_power_hold(INK_NEED_AWAKE); ink_power_hold(INK_NEED_NO_LOCK); quiet();
    const char *phase=getenv("INK_READER_EPUB_PHASE");
    if (phase && !strcmp(phase,"restart")) restart_checks();
    else if (phase && !strcmp(phase,"segments")) long_chapter();
    else reader_checks();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); test_start_home();
    osal_task_cfg_t task={.name="test.reader-epub",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&task,NULL)==OSAL_OK,"start EPUB integration checks");
}
