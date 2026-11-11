/* Whole-product inbox and bookmark checks use synthetic books only. */
#include <stdlib.h>
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include "ink/sys/app.h"
#include "ink/sys/data.h"
#include "ink/sys/file.h"
#include "bookmarks.h"

static char view[16384], marks[4096];
static const char *wanted;
static lv_obj_t *found;
static volatile bool captured;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(),15000),"reader settles"); }
static void ok(const char *id,const char *args) {
    CHECK(AI(id,args) && g_rc.status==INK_RC_DONE,"%s returns done: %.95s / %.95s",id,g_rc.detail,g_rc.say); quiet();
}
static void inspect(void) { CHECK(ink_ui_snapshot(view,sizeof view,NULL)>0,"page has an AI snapshot"); }
static void get(void) { on_ui(inspect); }
static void locate(lv_obj_t *obj) {
    if (!lv_obj_is_visible(obj)) return;
    if (lv_obj_check_type(obj,&lv_label_class) && !strcmp(lv_label_get_text(obj),wanted)) found=obj;
    for (uint32_t i=0;i<lv_obj_get_child_count(obj);i++) locate(lv_obj_get_child(obj,(int32_t)i));
}
static void find(void) { found=NULL; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); }
static void touch(const char *text) {
    wanted=text; on_ui(find); CHECK(found,"visible label %s",text); if (!found) return;
    lv_area_t a; lv_obj_get_coords(found,&a); tap((a.x1+a.x2)/2,(a.y1+a.y2)/2); quiet();
}
static void delete_fits(void) {
    wanted="删除"; find(); CHECK(found,"bookmark delete control visible");
    if (found) { lv_point_t size; lv_text_get_size(&size,wanted,lv_obj_get_style_text_font(found,0),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        CHECK(size.x<=lv_obj_get_content_width(found),"bookmark delete label fits at the current system font"); }
}
static void capture(void *name) { sim_display_snapshot(name); captured=true; }
static void frame(const char *name) { quiet(); captured=false; vtk_event_at(vtk_now()+SIM_MS(1),capture,(void *)name); CHECK(WAIT_FOR(captured,2000),"headless frame captured"); }
static void home(void) { ink_ui_home(); }
static void close_book(void) { on_ui(home); quiet(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(),20000),"reader data durable"); }
static void open_book(void) { ok("reader.open_book","{\"title\":\"收件箱来信\"}"); }
static uint64_t number(const char *text,const char *key) { unsigned long long n=0; const char *p=strstr(text,key); if (p) sscanf(p+strlen(key),"%llu",&n); return n; }
static uint32_t first_mark(void) {
    ok("reader.bookmarks",NULL); snprintf(marks,sizeof marks,"%.4095s",g_result); return (uint32_t)number(marks,"\"id\":");
}
static void mark_action(const char *cap,uint32_t id) { char args[64]; snprintf(args,sizeof args,"{\"id\":%u}",(unsigned)id); ok(cap,args); }
static void inbox(void) {
    CHECK(!ink_fs_mkdir("/me/收件箱"),"make own inbox");
    int64_t size=ink_file_size("/content/incoming.epub"); CHECK(size>0,"synthetic epub available");
    char *bytes=osal_malloc((size_t)size,OSAL_MEM_EXTERNAL); ink_file_t *file=ink_file_open("/content/incoming.epub");
    CHECK(bytes && file && ink_file_read(file,bytes,(size_t)size)==size,"read synthetic transfer payload"); ink_file_close(file);
    if (bytes) { CHECK(!ink_fs_write_atomic("/me/收件箱/来信.epub",bytes,(size_t)size),"complete inbox epub"); osal_free(bytes); }
    static char text[20000]; size_t n=(size_t)snprintf(text,sizeof text,"第一章 早晨\n");
    for (int i=0;i<80;i++) { if (i==40) n+=(size_t)snprintf(text+n,sizeof text-n,"第二章 夜晚\n");
        n+=(size_t)snprintf(text+n,sizeof text-n,"山间段落%d。我们沿着石阶上山，记录风声和树影。\n",i); }
    CHECK(!ink_fs_write_atomic("/me/收件箱/随手读.txt",text,n),"complete inbox txt");
    CHECK(!ink_fs_write_atomic("/me/收件箱/不是书.pdf","FAKE",4),"non-book in inbox");
    CHECK(!ink_fs_write_atomic("/me/收件箱/未完成.txt.part","FAKE",4),"unfinished transfer");
    CHECK(!ink_fs_mkdir("/me/收件箱/文件夹.txt"),"book-like directory");
}
static bool shelf(void) {
    ok("app.open","{\"id\":\"reader\"}"); get(); frame("shelf");
    CHECK(strstr(view,"收件箱来信") && strstr(view,"随手读") && strstr(view,"新"),"inbox books appear with new marks");
    CHECK(!strstr(view,"不是书") && !strstr(view,"未完成") && !strstr(view,"文件夹"),"partial, non-book and directories absent");
    return strstr(view,"收件箱来信")!=NULL;
}
static void txt(void) {
    ok("reader.open_book","{\"title\":\"随手读\"}"); ok("reader.goto_chapter","{\"entry\":2}"); get(); uint64_t at=number(view,"offset=");
    ok("reader.add_bookmark",NULL); uint32_t id=first_mark(); CHECK(id && strstr(g_result,"第二章 夜晚"),"TXT bookmark has its current chapter name");
    close_book(); ok("reader.continue",NULL); get(); CHECK(number(view,"offset=")==at,"inbox TXT continues at saved text");
    ok("reader.set_layout","{\"font_size\":72,\"line_spacing\":3}"); ok("reader.goto_page","{\"page\":1}"); mark_action("reader.goto_bookmark",id); get();
    CHECK(number(view,"offset=")<=at && number(view,"end=")>at && strstr(view,"bookmark=true"),"TXT bookmark survives reflow");
    mark_action("reader.remove_bookmark",id); CHECK(first_mark()==0,"TXT bookmark deleted"); close_book();
    ok("reader.set_layout","{\"font_size\":44,\"line_spacing\":1}");
}
static void run(void *arg) {
    (void)arg; ink_power_hold(INK_NEED_AWAKE); ink_power_hold(INK_NEED_NO_LOCK); quiet();
    const char *phase=getenv("INK_READER_MORE_PHASE");
    if (phase && !strcmp(phase,"restart")) {
        open_book(); uint32_t id=first_mark(); CHECK(id && strstr(marks,"\"chapter\":2"),"adult bookmarks survive restart");
        mark_action("reader.goto_bookmark",id); get(); CHECK(strstr(view,"chapter=2 ") && strstr(view,"bookmark=true"),"restarted bookmark returns to chapter two");
        close_book(); CHECK(!ink_member_switch("kid"),"child after restart"); quiet();
        ok("reader.open_book","{\"title\":\"共同读本\"}"); CHECK(first_mark()!=0,"child bookmark survives restart"); close_book();
    } else {
        inbox(); if (!shelf()) { ink_probe_count("test.finished"); return; }
        if (phase && !strcmp(phase,"large")) { ok("settings.set","{\"id\":\"display.font_size\",\"value\":3}"); close_book(); shelf(); }
        open_book(); ok("reader.continue",NULL); ok("reader.goto_chapter","{\"chapter\":2}"); ok("reader.goto_page","{\"page\":2}"); get();
        uint64_t at=number(view,"offset="); CHECK(at>0,"bookmark starts in chapter two body");
        ok("reader.menu",NULL); touch("加书签"); get(); CHECK(strstr(view,"bookmark=true"),"touch adds current page mark visible to AI"); frame("marked");
        uint32_t id=first_mark(); if (!id) { CHECK(false,"bookmark created"); ink_probe_count("test.finished"); return; }
        CHECK(strstr(marks,"\"chapter\":2") && number(marks,"\"block\":")>0 && strstr(marks,"\"added_at\":"),"bookmark exposes source block and time");
        snprintf(marks,sizeof marks,"%.4095s",g_result); ok("reader.add_bookmark",NULL); ok("reader.bookmarks",NULL); CHECK(!strcmp(marks,g_result),"AI add on the same page is idempotent");
        ok("reader.next_page",NULL); get(); CHECK(strstr(view,"bookmark=false"),"following page has no mark");
        ok("reader.set_layout","{\"font_size\":72,\"line_spacing\":3}"); mark_action("reader.goto_bookmark",id); get();
        CHECK(number(view,"offset=")<=at && number(view,"end=")>at && strstr(view,"chapter=2 ") && strstr(view,"bookmark=true"),"EPUB source position survives font and spacing changes");
        ok("reader.menu",NULL); touch("书签"); get(); CHECK(!strcmp(ink_ui_top_name(),"bookmarks") && strstr(view,"海边"),"bookmark list has named chapter and excerpt"); on_ui(delete_fits); frame("bookmarks");
        touch("第二章 海边"); get(); CHECK(strstr(view,"bookmark=true"),"touching a list entry jumps to the same source position");
        ok("reader.menu",NULL); touch("删书签"); get(); CHECK(strstr(view,"bookmark=false") && first_mark()==0,"touch removes bookmark consistently");
        ok("reader.add_bookmark",NULL); id=first_mark(); ok("reader.bookmarks","{\"open\":true}"); touch("删除"); CHECK(first_mark()==0,"list delete uses the shared capability"); ok("shell.back",NULL);
        ok("reader.add_bookmark",NULL); id=first_mark(); close_book();
        ok("reader.continue",NULL); get(); CHECK(strstr(view,"收件箱来信") && strstr(view,"bookmark=true"),"inbox EPUB continues with bookmarks"); close_book();
        if (!phase || strcmp(phase,"large")) {
            txt(); CHECK(!ink_member_switch("kid"),"switch to child"); quiet();
            ok("reader.list_books",NULL); CHECK(!strstr(g_rc.say,"收件箱来信") && !strstr(g_rc.say,"随手读"),"AI cannot see another member inbox from a cached shelf");
            ok("app.open","{\"id\":\"reader\"}"); get(); CHECK(!strstr(view,"收件箱来信") && !strstr(view,"随手读"),"child shelf excludes adult inbox");
            CHECK(AI("reader.open_book","{\"title\":\"收件箱来信\"}") && g_rc.status==INK_RC_FAILED,"child cannot open adult inbox book");
            ok("reader.open_book","{\"title\":\"共同读本\"}"); CHECK(first_mark()==0,"child has independent bookmarks"); ok("reader.add_bookmark",NULL); close_book();
            CHECK(!ink_member_switch("owner"),"return to adult"); quiet(); ok("reader.open_book","{\"title\":\"共同读本\"}"); CHECK(first_mark()==0,"adult cannot see child bookmarks for shared book"); close_book();
            open_book(); CHECK(first_mark()==id,"adult inbox bookmarks retained across member switch"); close_book();
        }
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); test_start_home(); osal_task_cfg_t task={.name="test.reader-more",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&task,NULL)==OSAL_OK,"start reader extension checks");
}
