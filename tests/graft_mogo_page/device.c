#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include "graft.h"
#include "ink/sys/appreg.h"
#include "ink/sys/crc.h"
static char view[1200], first[1200];
static uint32_t raster;
static bool captured;
static volatile bool cleanup_done;
static ink_exec_job_t cleanup_barrier;
static void after_cleanup(void *arg) { (void)arg; cleanup_done = true; }
static void locate(lv_obj_t *o) {
    if (lv_obj_check_type(o,&lv_image_class)) {
        const lv_image_dsc_t *d = lv_image_get_src(o);
        if (d && d->header.cf == LV_COLOR_FORMAT_L8) raster = ink_crc32(0,d->data,d->data_size);
    }
    for (uint32_t i=0;i<lv_obj_get_child_count(o);++i) locate(lv_obj_get_child(o,(int32_t)i));
}
static void inspect(void) { ink_graft_snapshot(view,sizeof view); }
static void pixels(void) { raster = 0; locate(lv_screen_active()); }
static bool page(int n) {
    char expected[32]; snprintf(expected,sizeof expected,"页=%d/",n);
    for (int i=0;i<2400;++i) {
        on_ui(inspect);
        if (strstr(view,expected) && !ink_ui_busy() && !ink_display_busy()) { on_ui(pixels); return true; }
        delay_ms(50);
    }
    return false;
}
static void capture(void *name) { sim_display_snapshot(name); captured = true; }
static void frame(const char *name) {
    captured=false; vtk_event_at(vtk_now()+SIM_MS(1),capture,(void *)name);
    CHECK(WAIT_FOR(captured,2000),"capture native reader frame");
}
static void close_reader(void) { sim_note("Mogo: closing"); CHECK(!ink_app_close("mogo"),"close native reader"); sim_note("Mogo: closed"); }
static void run(void *arg) {
    (void)arg;
    ink_power_hold(INK_NEED_AWAKE); ink_power_hold(INK_NEED_NO_LOCK);
    CHECK(AI("mogo.open","{\"path\":\"/me/books/星星邮差GBK.txt\"}"),"open GBK through new reader");
    CHECK(page(1),"native first page is ready: %.150s",view);
    CHECK(strstr(view,"邮筒") && strstr(view,"字号=48"),"GBK body and default size: %.150s",view);
    snprintf(first,sizeof first,"%s",view); uint32_t first_raster = raster;
    CHECK(first_raster != 0,"native L8 body is presented"); frame("gbk-page-1");
    for (int i=0;i<10;++i) {
        if (i%2) key(HAL_KEY_NEXT); else tap(600,600);
        CHECK(page(i+2),"forward input reaches native page %d: %.100s",i+2,view);
        CHECK(raster != first_raster,"page-turn input changes body pixels");
        if (i==8) frame("gbk-page-10");
    }
    for (int i=0;i<10;++i) {
        if (i%2) tap(80,600); else key(HAL_KEY_PREV);
        CHECK(page(10-i),"previous input reaches native page %d: %.100s",10-i,view);
    }
    CHECK(!strcmp(view,first) && raster==first_raster,"round trip restores first page number, text and pixels"); frame("gbk-return");
    CHECK(AI("mogo.open","{\"path\":\"/me/books/星星邮差.epub\"}") && g_rc.status==INK_RC_DONE,"open EPUB through native path");
    CHECK(page(1) && strstr(view,"邮筒"),"EPUB body is laid out: %.150s",view); frame("epub-page-1");
    sim_note("Mogo: GBK round trip and EPUB assertions completed");
    on_ui(close_reader); sim_note("Mogo: UI close returned");
    ink_exec_post(INK_EXEC_BG,&cleanup_barrier,after_cleanup,NULL);
    CHECK(WAIT_FOR(cleanup_done,15000),"native cleanup finishes on the serialized executor");
    sim_note("Mogo: test finished"); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); test_start_home();
    osal_task_cfg_t cfg={.name="test.mogo",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"start native page checks");
}
