#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include "graft.h"
#include "ink/sys/appreg.h"
#include "ink/sys/crc.h"
#include <stdlib.h>
static char view[2048], saved[2048], chapter_two[2048];
static uint32_t raster;
static volatile bool captured, cleanup_done;
static ink_exec_job_t cleanup_barrier;
static void inspect(void) { ink_graft_snapshot(view,sizeof view); }
static void locate(lv_obj_t *o) {
    if (lv_obj_check_type(o,&lv_image_class)) {
        const lv_image_dsc_t *d=lv_image_get_src(o);
        if (d && d->header.cf==LV_COLOR_FORMAT_L8) raster=ink_crc32(0,d->data,d->data_size);
    }
    for (uint32_t i=0;i<lv_obj_get_child_count(o);++i) locate(lv_obj_get_child(o,(int32_t)i));
}
static void pixels(void) { raster=0; locate(lv_screen_active()); }
static bool ready(const char *needle) {
    for (int i=0;i<2400;++i) {
        on_ui(inspect);
        if (strstr(view,needle) && !ink_ui_busy() && !ink_display_busy()) { on_ui(pixels); return true; }
        delay_ms(50);
    }
    return false;
}
static void capture(void *name) { sim_display_snapshot(name); captured=true; }
static void frame(const char *name) {
    captured=false; vtk_event_at(vtk_now()+SIM_MS(1),capture,(void *)name);
    CHECK(WAIT_FOR(captured,2000),"capture %s",name);
}
static void after_cleanup(void *arg) { (void)arg; cleanup_done=true; }
static void close_reader(void) { CHECK(!ink_app_close("mogo"),"close native book"); }
static void close_book(void) {
    cleanup_done=false; on_ui(close_reader);
    ink_exec_post(INK_EXEC_BG,&cleanup_barrier,after_cleanup,NULL);
    CHECK(WAIT_FOR(cleanup_done,15000),"native cleanup saves progress");
}
static void open_book(void) {
    CHECK(AI("mogo.open","{\"path\":\"/me/books/星星邮差GBK.txt\"}"),"open current member book");
}
static void action(const char *id,const char *args,const char *needle) {
    CHECK(AI(id,args) && g_rc.status==INK_RC_DONE,"%s completes, rc=%d",id,g_rc.status);
    CHECK(ready(needle),"%s visible: %.150s",id,view);
}
static void tap_native(int x,int y) { tap(x,62+y*(1216-62)/1216); }
static size_t number(const char *key) { const char *p=strstr(view,key); return p?(size_t)strtoul(p+strlen(key),NULL,10):SIZE_MAX; }
static void anchored(size_t offset) { CHECK(number("位置=")<=offset && offset<number("结束="),"reflow keeps source anchor %zu: %.150s",offset,view); }
static void screenshots(const char *prefix,int scale) {
    char name[64];
    action("mogo.shelf",NULL,"面板=书架");
    snprintf(name,sizeof name,"%s-shelf",prefix); frame(name);
    CHECK(strstr(view,"星星邮差GBK.txt"),"shelf snapshot exposes member book");
    open_book(); CHECK(ready("面板=正文"),"body ready for screenshots");
    char expected[32]; snprintf(expected,sizeof expected,"字号=%d",scale);
    CHECK(strstr(view,expected),"reading size follows system size: %.150s",view);
    snprintf(name,sizeof name,"%s-body",prefix); frame(name);
    action("mogo.toc",NULL,"面板=目录"); snprintf(name,sizeof name,"%s-toc",prefix); frame(name);
    action("mogo.bookmarks",NULL,"面板=书签列表"); snprintf(name,sizeof name,"%s-bookmarks",prefix); frame(name);
    action("mogo.settings",NULL,"面板=设置"); snprintf(name,sizeof name,"%s-settings",prefix); frame(name);
    action("mogo.set_font_size","{\"size\":60}","面板=正文");
    snprintf(name,sizeof name,"%s-body-size60",prefix); frame(name);
}
static void run(void *arg) {
    (void)arg; ink_power_hold(INK_NEED_AWAKE); ink_power_hold(INK_NEED_NO_LOCK);
    CHECK(AI("app.open","{\"id\":\"mogo\"}"),"open bookshelf from desktop registration");
    CHECK(ready("面板=书架"),"native shelf opens: %.150s",view); frame("default-shelf");
    int index=0; const char *path=strstr(view,"/me/books/星星邮差GBK.txt");
    if (path) { const char *line=path; while (line>view && line[-1]!='\n') --line; index=atoi(line); }
    CHECK(index>=1 && index<=4,"find book without depending on directory order");
    if (index>=1 && index<=4) tap_native(342,210+(index-1)*190+82);
    CHECK(ready("面板=正文") && strstr(view,"页=1/"),"bookshelf button opens native book: %.150s",view);
    CHECK(strstr(view,"邮筒") && strstr(view,"字号=48"),"GBK body and default size"); frame("default-body");
    for (int i=0;i<5;++i) {
        if (i%2) CHECK(AI("mogo.next",NULL) && g_rc.status==INK_RC_DONE,"AI turns native page"); else tap(600,600);
        char page[24]; snprintf(page,sizeof page,"页=%d/",i+2);
        CHECK(ready(page),"five turns reach page %d: %.150s",i+2,view);
    }
    action("mogo.add_bookmark",NULL,"书签=1");
    snprintf(saved,sizeof saved,"%s",view); uint32_t bookmark_raster=raster;
    action("mogo.toc",NULL,"面板=目录"); frame("default-toc");
    tap_native(342,407);
    CHECK(ready("章节=2"),"TOC button jumps to second chapter: %.150s",view);
    snprintf(chapter_two,sizeof chapter_two,"%s",view); uint32_t chapter_raster=raster;
    close_book(); open_book();
    CHECK(ready("章节=2"),"saved progress restores second chapter: %.150s",view);
    CHECK(!strcmp(view,chapter_two) && raster==chapter_raster,"reopen restores source position, text and pixels");
    action("mogo.bookmarks",NULL,"面板=书签列表"); frame("default-bookmarks");
    tap_native(342,696);
    CHECK(ready("章节=1"),"bookmark list button returns to first chapter");
    CHECK(!strcmp(view,saved) && raster==bookmark_raster,"bookmark restores source position, text and pixels");
    action("mogo.goto_chapter","{\"chapter\":2}","章节=2");
    CHECK(!strcmp(view,chapter_two) && raster==chapter_raster,"AI chapter jump equals TOC button");
    action("mogo.goto_bookmark","{\"index\":1}","章节=1");
    CHECK(!strcmp(view,saved) && raster==bookmark_raster,"AI bookmark jump equals bookmark button");
    action("mogo.settings",NULL,"面板=设置"); frame("default-settings");
    size_t offset=number("位置=");
    action("mogo.set_font_size","{\"size\":60}","字号=60"); anchored(offset); frame("default-body-size60");
    action("mogo.goto_chapter","{\"chapter\":2}","章节=2");
    action("mogo.settings",NULL,"面板=设置"); tap_native(600,421);
    CHECK(ready("字号=61"),"font-size button uses registered setting action");
    snprintf(saved,sizeof saved,"%s",view); uint32_t size_raster=raster;
    action("mogo.set_font_size","{\"size\":60}","字号=60");
    action("mogo.set_font_size","{\"size\":61}","字号=61");
    CHECK(!strcmp(view,saved) && raster==size_raster,"AI font size equals settings button");
    action("mogo.set_font_size","{\"size\":60}","字号=60");
    const char *settings[]={"line_spacing","paragraph_spacing","margin","weight","indent","header","footer"};
    const int values[]={120,25,48,4,3,1,1};
    _Static_assert(sizeof settings/sizeof *settings==sizeof values/sizeof *values,"setting cases");
    for (size_t i=0;i<sizeof values/sizeof *values;++i) {
        char cap[64],args[32]; snprintf(cap,sizeof cap,"mogo.set_%s",settings[i]); snprintf(args,sizeof args,"{\"value\":%d}",values[i]);
        offset=number("位置="); action(cap,args,"面板=正文"); anchored(offset);
    }
    snprintf(saved,sizeof saved,"%s",view); close_book(); open_book();
    CHECK(ready("字号=60") && !strcmp(view,saved),"member preferences and reflow position survive reopen");
    CHECK(!ink_member_switch("kid"),"switch to synthetic child");
    cleanup_done=false; ink_exec_post(INK_EXEC_BG,&cleanup_barrier,after_cleanup,NULL);
    CHECK(WAIT_FOR(cleanup_done,15000),"old member background work drained");
    open_book();
    CHECK(ready("页=1/") && strstr(view,"章节=1") && strstr(view,"书签=0") && strstr(view,"字号=53"),"child sees neither adult progress, marks nor preferences: %.150s",view);
    close_book(); CHECK(!ink_member_switch("owner"),"return to synthetic adult"); open_book();
    CHECK(ready("字号=60") && strstr(view,"书签=1"),"adult settings and bookmark remain isolated");
    const int defaults[]={150,50,36,0,2,0,0};
    _Static_assert(sizeof defaults/sizeof *defaults==sizeof settings/sizeof *settings,"default cases");
    for (size_t i=0;i<sizeof defaults/sizeof *defaults;++i) {
        char cap[64],args[32]; snprintf(cap,sizeof cap,"mogo.set_%s",settings[i]); snprintf(args,sizeof args,"{\"value\":%d}",defaults[i]);
        action(cap,args,"面板=正文");
    }
    action("mogo.set_font_size","{\"size\":48}","字号=48"); close_book();
    CHECK(BY_HAND("settings.set","{\"id\":\"display.font_size\",\"value\":3}") && g_rc.status==INK_RC_DONE,"select largest system text size");
    open_book(); CHECK(ready("字号=58"),"largest system size scales native reader");
    screenshots("maximum",58);
    action("mogo.set_font_size","{\"size\":90}","字号=108"); close_book(); open_book();
    CHECK(ready("字号=108"),"maximum reading preference restores at maximum system size");
    close_book(); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); test_start_home();
    osal_task_cfg_t cfg={.name="test.mogo",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"start complete native reading checks");
}
