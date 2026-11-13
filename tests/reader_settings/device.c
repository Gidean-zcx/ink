/* Reading layout through the one reader (reader-one-1): touch and AI share reader.set_layout, the place in the chapter
 * survives every change, values are each member's and survive a restart. Synthetic books only. */
#include <stdlib.h>
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include "graft.h"
#include "ink/sys/crc.h"
#include "ink/ui_theme.h"

static char view[8192], layout[1024];
static uint32_t raster, raster_size;
static volatile bool captured;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(),15000),"reader settles"); }
static void ok(const char *id, const char *args) {
    CHECK(AI(id,args) && g_rc.status==INK_RC_DONE,"%s returns done (%d): %.95s / %.95s",id,g_rc.status,g_rc.detail,g_rc.say); quiet();
}
static void locate(lv_obj_t *o) {
    if (lv_obj_check_type(o,&lv_image_class)) { const lv_image_dsc_t *d=lv_image_get_src(o); if (d && d->header.cf==LV_COLOR_FORMAT_L8 && d->data_size>raster_size) { raster=ink_crc32(0,d->data,d->data_size); raster_size=d->data_size; } }
    for (uint32_t i=0;i<lv_obj_get_child_count(o);i++) locate(lv_obj_get_child(o,(int32_t)i));
}
static void inspect(void) { ink_graft_snapshot(view,sizeof view); raster=raster_size=0; locate(lv_screen_active()); }
static void get(void) { on_ui(inspect); }
static bool ready(const char *needle) { for (int i=0;i<300;i++) { get(); if (strstr(view,needle) && settled()) return true; delay_ms(50); } return false; }
static int geometry_top, geometry_h, geometry_w;
static void geometry(void) { geometry_top=ink_ui_px(INK_STATUS_BAR); geometry_h=ink_ui_height()-geometry_top; geometry_w=ink_ui_width(); }
static void tap_native(int x,int y) { on_ui(geometry); tap(x*geometry_w/684,geometry_top+y*geometry_h/1216); quiet(); }
static void capture(void *name) { sim_display_snapshot(name); captured=true; }
static void frame(const char *name) { quiet(); captured=false; vtk_event_at(vtk_now()+SIM_MS(1),capture,(void *)name); CHECK(WAIT_FOR(captured,2000),"headless frame captured"); }
static void home(void) { ink_ui_home(); }
static void close_book(void) { on_ui(home); quiet(); CHECK(WAIT_FOR(!ink_storage_pending(),15000),"reader position durable"); }
static void values(const char *expect) { ok("reader.get_layout",NULL); CHECK(strstr(g_result,expect),"current layout includes %s: %.150s",expect,g_result); }
static unsigned long long number(const char *key) { unsigned long long n=0; const char *p=strstr(view,key); if (p) sscanf(p+strlen(key),"%llu",&n); return n; }
static int page(void) { ok("reader.read_page",NULL); return (int)g_rc.revision; }
static void open_book(void) { ok("reader.open_book","{\"title\":\"山间来信\"}"); }
static void position_checks(void) {
    static const char *const keys[]={"font_size","line_spacing","paragraph_spacing","margins","weight","indent","header","footer"};
    static const int choices[8][4]={{36,60,90,48},{110,120,135,150},{0,25,100,50},{0,24,96,36},{-16,16,32,0},{0,1,3,2},{1,0,1,0},{1,0,1,0}};
    ok("reader.goto_chapter","{\"chapter\":2}"); ok("reader.goto_page","{\"page\":4}"); get();
    unsigned long long anchor=number("位置=");
    const char *body=strstr(view,"本页书签="); body=body ? strchr(body,'\n') : NULL;
    char sentence[64]={0};
    if (body) { size_t n=LV_MIN(strlen(body+1),36u); while (n && ((unsigned char)body[1+n]&0xc0)==0x80) n--; memcpy(sentence,body+1,n); }
    CHECK(anchor>0 && sentence[0],"test starts in the middle of chapter two");
    for (int key=0;key<8;key++) for (int v=0;v<4;v++) {
        get(); uint32_t previous=raster;
        char args[64]; snprintf(args,sizeof args,"{\"%s\":%d}",keys[key],choices[key][v]);
        ok("reader.set_layout",args); get();
        CHECK(number("位置=")<=anchor && number("结束=")>anchor,"chapter offset retained for %s (%llu): %.120s",args,anchor,view);
        CHECK(strstr(view,"章节=2 ") && (anchor-number("位置=")>400 || strstr(view,sentence)),"same sentence stays on the current page for %s",args); /* the snapshot holds 600 bytes of the page */
        CHECK(previous!=raster,"layout changes body raster for %s (%08x %u): %.120s",args,(unsigned)raster,(unsigned)raster_size,view);
    }
    int64_t system_font=ink_setting_get("display.font_size"); ok("reader.set_layout","{\"font_size\":44}");
    CHECK(ink_setting_get("display.font_size")==system_font,"reading font does not change the system font");
    ok("reader.get_layout",NULL); snprintf(layout,sizeof layout,"%.1023s",g_result);
    const char *invalid[]={"{\"font_size\":17}","{\"line_spacing\":151}","{\"paragraph_spacing\":-1}","{\"margins\":97}","{\"footer_mask\":1}","{\"line_spacing\":120,\"margins\":99}","{\"margins\":true}","{\"member\":\"kid\"}"};
    for (unsigned i=0;i<sizeof invalid/sizeof *invalid;i++) {
        CHECK(AI("reader.set_layout",invalid[i]) && g_rc.status==INK_RC_INVALID,"invalid batch rejected: %s",invalid[i]);
        ok("reader.get_layout",NULL); CHECK(!strcmp(layout,g_result),"invalid batch changes nothing");
    }
}
static void touch_checks(void) {
    ok("reader.set_layout","{\"font_size\":48,\"line_spacing\":150,\"paragraph_spacing\":50,\"margins\":36}");
    ok("reader.goto_page","{\"page\":3}"); int before=page();
    tap_native(342,600); CHECK(ready("面板=工具") && page()==before,"center tap opens the tools without turning"); frame("menu");
    tap_native(501,750+136+56); CHECK(ready("面板=设置") && strstr(view,"font_size=48"),"touch opens settings with the current values"); frame("settings");
    tap_native(600,380+41); CHECK(ready("字号=49"),"the + button changes the size"); values("\"font_size\":49");
    ok("reader.set_layout","{\"font_size\":48}"); get(); uint32_t by_ai=raster;
    ok("reader.menu","{\"view\":\"settings\"}"); tap_native(600,380+41); CHECK(ready("字号=49"),"the + button again"); ok("reader.set_layout","{\"font_size\":48}"); get();
    CHECK(raster==by_ai,"the settings button and the AI give the same page");
    ok("reader.menu","{\"open\":false}"); before=page();
    tap_native(620,600); CHECK(ready("面板=正文") && page()==before+1,"right tap still turns next");
    tap_native(60,600); CHECK(ready("面板=正文") && page()==before,"left tap still turns previous");
    ok("reader.menu",NULL); ok("reader.menu","{\"open\":false}"); get(); CHECK(strstr(view,"面板=正文"),"the AI closes the tools");
    ok("reader.menu",NULL); tap_native(183,750+56); CHECK(ready("面板=目录"),"the tools open the real contents"); ok("reader.back",NULL);
    ok("reader.menu",NULL); tap_native(183,750+272+56); CHECK(ready("面板=书架"),"the tools return to the shelf"); open_book();
}
static void txt_checks(void) {
    close_book(); ok("reader.open_book","{\"title\":\"chapters\"}"); ok("reader.goto_chapter","{\"entry\":2}"); get();
    CHECK(strstr(view,"第二章 夜晚"),"TXT chapter entry reaches the named chapter: %.150s",view);
    ok("reader.set_layout","{\"font_size\":60,\"line_spacing\":150}"); get();
    CHECK(strstr(view,"第二章 夜晚"),"TXT reflow keeps its chapter"); close_book(); open_book();
}
static void screenshots(void) {
    ok("reader.set_layout","{\"font_size\":36,\"line_spacing\":110,\"paragraph_spacing\":0,\"margins\":0,\"footer\":1,\"header\":1}"); frame("compact");
    ok("reader.set_layout","{\"font_size\":72,\"line_spacing\":150,\"paragraph_spacing\":100,\"margins\":56,\"footer\":1,\"header\":0}"); frame("large");
}
static void members(void) {
    close_book(); CHECK(!ink_member_switch("kid"),"switch to child"); quiet(); values("\"font_size\":48"); values("\"margins\":36");
    ok("reader.set_layout","{\"font_size\":60,\"line_spacing\":120,\"paragraph_spacing\":0,\"margins\":0,\"footer\":0}"); open_book(); get();
    ok("reader.get_layout",NULL); CHECK(strstr(g_result,"\"member\":\"kid\"") && strstr(g_result,"\"font_size\":60"),"child can change their own reader layout"); close_book();
    CHECK(!ink_member_switch("owner"),"switch to adult"); quiet(); values("\"font_size\":72"); values("\"margins\":56");
    CHECK(WAIT_FOR(!ink_storage_pending(),15000),"layout durable");
}
static void restarted(void) {
    values("\"font_size\":72"); values("\"line_spacing\":150"); values("\"paragraph_spacing\":100"); values("\"margins\":56"); values("\"footer\":1");
    open_book(); get(); CHECK(strstr(view,"字号=72"),"adult layout applies after reboot: %.150s",view); close_book();
    CHECK(!ink_member_switch("kid"),"child after restart"); quiet(); values("\"font_size\":60"); values("\"line_spacing\":120"); values("\"paragraph_spacing\":0"); values("\"margins\":0"); values("\"footer\":0");
    open_book(); get(); CHECK(strstr(view,"字号=66"),"child layout applies after reboot (children read at 110%%: 60 is 66 px): %.120s",view); close_book();
}
static void run(void *arg) {
    (void)arg; ink_power_hold(INK_NEED_AWAKE); ink_power_hold(INK_NEED_NO_LOCK); quiet();
    const char *phase=getenv("INK_READER_SETTINGS_PHASE");
    if (phase && !strcmp(phase,"restart")) restarted();
    else if (phase && !strcmp(phase,"large-text")) {
        ok("settings.set","{\"id\":\"display.font_size\",\"value\":3}"); open_book();
        ok("reader.menu",NULL); frame("menu"); ok("reader.menu","{\"view\":\"settings\"}"); frame("settings");
        ok("reader.menu","{\"open\":false}"); screenshots(); close_book();
    } else { open_book(); position_checks(); touch_checks(); txt_checks(); screenshots(); members(); }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); test_start_home();
    osal_task_cfg_t task={.name="test.reader-settings",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&task,NULL)==OSAL_OK,"start reader layout checks");
}
