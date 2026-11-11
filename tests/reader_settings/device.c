/* Reading layout, touch and AI share the actual product path. Synthetic books only. */
#include <stdlib.h>
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include "ink/sys/crc.h"
#include "ink/sys/time.h"
#include "ink/ui_theme.h"

static char view[8192], layout[1024];
static const char *wanted;
static lv_obj_t *found, *image;
static uint32_t raster;
static int image_w, image_h;
static volatile bool captured;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(),15000),"reader settles"); }
static void ok(const char *id, const char *args) {
    CHECK(AI(id,args) && g_rc.status==INK_RC_DONE,"%s returns done: %.95s / %.95s",id,g_rc.detail,g_rc.say); quiet();
}
static void locate(lv_obj_t *obj) {
    if (!lv_obj_is_visible(obj)) return;
    if (lv_obj_check_type(obj,&lv_label_class) && !strcmp(lv_label_get_text(obj),wanted)) found=obj;
    if (lv_obj_check_type(obj,&lv_image_class)) image=obj;
    for (uint32_t i=0;i<lv_obj_get_child_count(obj);i++) locate(lv_obj_get_child(obj,(int32_t)i));
}
static void inspect(void) {
    CHECK(ink_ui_snapshot(view,sizeof view,NULL)>0,"AI sees reader"); found=NULL; image=NULL;
    lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active());
    if (image) { const lv_image_dsc_t *d=lv_image_get_src(image); raster=ink_crc32(0,d->data,d->data_size); image_w=(int)d->header.w; image_h=(int)d->header.h; }
}
static void get(void) { wanted=""; on_ui(inspect); }
static void touch(const char *text) {
    wanted=text; on_ui(inspect); CHECK(found,"visible label %s",text);
    if (!found) return;
    lv_area_t a; lv_obj_get_coords(found,&a); tap((a.x1+a.x2)/2,(a.y1+a.y2)/2); quiet();
}
static void capture(void *name) { sim_display_snapshot(name); captured=true; }
static void frame(const char *name) { quiet(); captured=false; vtk_event_at(vtk_now()+SIM_MS(1),capture,(void *)name); CHECK(WAIT_FOR(captured,2000),"headless frame captured"); }
static void home(void) { ink_ui_home(); }
static void close_book(void) { on_ui(home); quiet(); CHECK(WAIT_FOR(!ink_storage_pending(),15000),"reader position durable"); }
static void values(const char *expect) { ok("reader.get_layout",NULL); CHECK(strstr(g_result,expect),"current layout includes %s: %.150s",expect,g_result); }
static unsigned long long offset(const char *key) { unsigned long long n=0; const char *p=strstr(view,key); if (p) sscanf(p+strlen(key),"%llu",&n); return n; }
static int page(void) { ok("reader.read_page",NULL); return (int)g_rc.revision; }
static void open_book(void) { ok("reader.open_book","{\"title\":\"山间来信\"}"); }
static void position_checks(void) {
    static const char *const keys[]={"font_size","line_spacing","paragraph_spacing","margins"};
    static const int fonts[]={36,44,53,72};
    ok("reader.goto_chapter","{\"chapter\":2}"); ok("reader.goto_page","{\"page\":2}"); get();
    unsigned long long anchor=offset("offset=");
    const char *body=strstr(view,"章内页"); body=body ? strchr(body,'\n') : NULL;
    char sentence[64]={0};
    if (body) { size_t n=LV_MIN(strlen(body+1),36u); while (n && ((unsigned char)body[1+n]&0xc0)==0x80) n--; memcpy(sentence,body+1,n); }
    CHECK(anchor>0 && sentence[0],"test starts in the middle of chapter two");
    for (int key=0;key<4;key++) for (int v=0;v<4;v++) {
        get(); uint32_t previous=raster; int w=image_w,h=image_h;
        ok("reader.get_layout",NULL); snprintf(layout,sizeof layout,"%.1023s",g_result);
        char args[64]; snprintf(args,sizeof args,"{\"%s\":%d}",keys[key],key==0 ? fonts[v] : v);
        ok("reader.set_layout",args); get();
        CHECK(offset("offset=")<=anchor && offset("end=")>anchor,"chapter offset retained for %s=%d",keys[key],v);
        CHECK(strstr(view,"chapter=2 ") && strstr(view,sentence),"same sentence stays on the current page for %s=%d",keys[key],v);
        if (v!=1 || key==0) CHECK(previous!=raster || w!=image_w || h!=image_h,"layout changes body raster for %s=%d",keys[key],v);
    }
    int64_t system_font=ink_setting_get("display.font_size"); ok("reader.set_layout","{\"font_size\":44}");
    CHECK(ink_setting_get("display.font_size")==system_font,"reading font does not change the system font");
    ok("reader.get_layout",NULL); snprintf(layout,sizeof layout,"%.1023s",g_result);
    const char *invalid[]={"{\"font_size\":45}","{\"line_spacing\":4}","{\"paragraph_spacing\":-1}","{\"margins\":4}","{\"footer_mask\":16}","{\"line_spacing\":0,\"margins\":99}","{\"margins\":true}","{\"member\":\"kid\"}"};
    for (unsigned i=0;i<sizeof invalid/sizeof *invalid;i++) {
        CHECK(AI("reader.set_layout",invalid[i]) && g_rc.status==INK_RC_INVALID,"invalid batch rejected: %s",invalid[i]);
        ok("reader.get_layout",NULL); CHECK(!strcmp(layout,g_result),"invalid batch changes nothing");
    }
}
static void touch_checks(void) {
    ok("reader.set_layout","{\"font_size\":44,\"line_spacing\":1,\"paragraph_spacing\":1,\"margins\":1,\"footer_mask\":3}");
    ok("reader.goto_page","{\"page\":3}"); int before=page();
    tap(ink_ui_width()/2,ink_ui_height()/2); quiet(); get();
    CHECK(strstr(view,"menu=menu") && page()==before,"center tap opens menu without turning"); frame("menu");
    touch("阅读设置"); get(); CHECK(strstr(view,"menu=settings") && strstr(view,"\"font_size\":44"),"touch opens settings with current AI values"); frame("settings");
    touch("很大"); values("\"margins\":3");
    touch("电量"); values("\"footer_mask\":7");
    ok("reader.menu","{\"open\":false}"); before=page();
    tap(ink_ui_width()*9/10,ink_ui_height()/2); quiet(); CHECK(page()==before+1,"right tap still turns next");
    tap(ink_ui_width()/10,ink_ui_height()/2); quiet(); CHECK(page()==before,"left tap still turns previous");
    ok("reader.menu","{\"open\":true}"); tap(ink_ui_width()/2,ink_ui_height()/4); quiet(); get(); CHECK(strstr(view,"menu=closed"),"blank body closes menu");
    ok("reader.menu","{\"open\":true}"); ok("reader.menu",NULL); get(); CHECK(strstr(view,"menu=closed"),"AI toggle closes menu");
    ok("reader.menu",NULL); touch("跳到第几页"); ok("text.insert","{\"text\":\"2\"}"); ok("text.submit",NULL); CHECK(page()==2,"page number entry uses goto capability");
    ok("reader.menu",NULL); touch("跳到第几章"); ok("text.insert","{\"text\":\"1\"}"); ok("text.submit",NULL); get(); CHECK(strstr(view,"chapter=1 "),"chapter number entry uses goto capability");
    ok("reader.menu",NULL); touch("目录"); CHECK(!strcmp(ink_ui_top_name(),"chapters"),"menu opens real contents"); ok("shell.back",NULL);
    ok("reader.menu",NULL); touch("返回书架"); CHECK(!strcmp(ink_ui_top_name(),"reader"),"menu returns to shelf"); open_book();
}
static void txt_checks(void) {
    close_book(); ok("reader.open_book","{\"title\":\"chapters\"}");
    ok("reader.menu",NULL); touch("跳到第几章"); ok("text.insert","{\"text\":\"2\"}"); ok("text.submit",NULL); get();
    CHECK(strstr(view,"footer_chapter=第二章 夜晚") && offset("offset=")>0,"TXT numeric chapter entry reaches the named chapter");
    ok("reader.set_layout","{\"font_size\":53,\"line_spacing\":3}"); get();
    CHECK(strstr(view,"footer_chapter=第二章 夜晚"),"TXT reflow retains its chapter title and offset"); close_book(); open_book();
}
static void footer_checks(void) {
    ok("reader.set_layout","{\"footer_mask\":15}"); get(); uint32_t before=raster;
    wanted="时间"; on_ui(inspect);
    for (int bit=0;bit<4;bit++) {
        char args[40]; snprintf(args,sizeof args,"{\"footer_mask\":%d}",1<<bit); ok("reader.set_layout",args); get();
        CHECK(raster==before,"footer-only switch preserves body raster");
        static const char *const names[]={"footer_chapter=","footer_progress=","footer_battery=","footer_time="};
        for (int i=0;i<4;i++) { const char *p=strstr(view,names[i]); CHECK(p && (p[strlen(names[i])]!= '\n')==(i==bit),"only chosen footer item %d visible for bit %d",i,bit); }
    }
    ok("reader.set_layout","{\"footer_mask\":0}"); get(); CHECK(raster!=before,"hidden footer gives space back to the body"); frame("footer-hidden");
    ok("reader.set_layout","{\"footer_mask\":15}"); get(); before=raster;
    ink_time_set(ink_time_now_s()+60,false); quiet(); get(); CHECK(raster==before,"minute change leaves body pixels unchanged");
    int64_t level=0; hw(HW_BATTERY,57,0); ink_power_battery_policy_start();
    CHECK(WAIT_FOR(ink_state_get_int(ink_state_find("battery.level"),&level) && level==57,10000),"battery event reaches the power service");
    quiet(); get(); CHECK(raster==before && strstr(view,"footer_battery=57%"),"battery event updates only its footer label"); frame("footer-all");
}
static void screenshots(void) {
    ok("reader.set_layout","{\"font_size\":36,\"line_spacing\":0,\"paragraph_spacing\":0,\"margins\":0,\"footer_mask\":15}"); frame("compact");
    ok("reader.set_layout","{\"font_size\":72,\"line_spacing\":3,\"paragraph_spacing\":3,\"margins\":3,\"footer_mask\":15}"); frame("large");
}
static void members(void) {
    close_book(); CHECK(!ink_member_switch("kid"),"switch to child"); quiet(); values("\"font_size\":44"); values("\"margins\":1");
    ok("reader.set_layout","{\"font_size\":53,\"line_spacing\":2,\"paragraph_spacing\":0,\"margins\":0,\"footer_mask\":12}"); open_book(); get();
    CHECK(strstr(view,"\"member\":\"kid\"") && strstr(view,"\"font_size\":53"),"child can change their own reader layout"); close_book();
    CHECK(!ink_member_switch("owner"),"switch to adult"); quiet(); values("\"font_size\":72"); values("\"margins\":3");
    CHECK(WAIT_FOR(sim_metric_value("settings.saved")>0 && !ink_storage_pending(),15000),"settings durable");
}
static void restarted(void) {
    values("\"font_size\":72"); values("\"line_spacing\":3"); values("\"paragraph_spacing\":3"); values("\"margins\":3"); values("\"footer_mask\":15");
    open_book(); get(); CHECK(strstr(view,"\"font_size\":72"),"adult layout applies after reboot"); close_book();
    CHECK(!ink_member_switch("kid"),"child after restart"); quiet(); values("\"font_size\":53"); values("\"line_spacing\":2"); values("\"paragraph_spacing\":0"); values("\"margins\":0"); values("\"footer_mask\":12");
    open_book(); get(); CHECK(strstr(view,"\"font_size\":53"),"child layout applies after reboot"); close_book();
}
static void run(void *arg) {
    (void)arg; ink_power_hold(INK_NEED_AWAKE); ink_power_hold(INK_NEED_NO_LOCK); quiet();
    const char *phase=getenv("INK_READER_SETTINGS_PHASE");
    if (phase && !strcmp(phase,"restart")) restarted();
    else if (phase && !strcmp(phase,"large-text")) {
        ok("settings.set","{\"id\":\"display.font_size\",\"value\":3}"); open_book();
        ok("reader.menu",NULL); frame("menu"); ok("reader.menu","{\"view\":\"settings\"}"); frame("settings");
        ok("reader.menu","{\"open\":false}"); screenshots(); close_book();
    } else { open_book(); position_checks(); touch_checks(); txt_checks(); footer_checks(); screenshots(); members(); }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); test_start_home();
    osal_task_cfg_t task={.name="test.reader-settings",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&task,NULL)==OSAL_OK,"start reader layout checks");
}
