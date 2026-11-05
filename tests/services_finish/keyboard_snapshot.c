#include "product.h"
static char snapshot[4096];
static int page_snapshot(char *out,size_t cap) { return snprintf(out,cap,"合成表单内容"); }
static void create(lv_obj_t *screen,void *ctx) { (void)screen; (void)ctx; }
static const ink_activity_t PAGE={.name="synthetic-form",.create=create,.snapshot=page_snapshot};
static void inspect(void) {
    ink_ui_push(&PAGE,NULL);
    const ink_text_client_t client={0};
    const ink_text_type_t types[]={INK_TEXT_PLAIN,INK_TEXT_NUMBER,INK_TEXT_PASSWORD};
    for (unsigned i=0;i<3;i++) {
        ink_text_cfg_t cfg={.type=types[i],.initial=i==2?"SYNTHETIC_SECRET":i==1?"12345":"SYNTHETIC_TEXT",.hint="合成输入框"};
        CHECK(ink_text_focus(&cfg,&client,NULL)>0,"focus keyboard");
        const char *name=NULL;
        CHECK(ink_ui_snapshot(snapshot,sizeof snapshot,&name)>0,"combined snapshot fits");
        CHECK(strstr(snapshot,"合成表单内容")&&strstr(snapshot,"\"keyboard\""),"page and input share one snapshot");
        CHECK(strstr(snapshot,"合成输入框"),"input target identified");
        if(i==2) CHECK(!strstr(snapshot,"SYNTHETIC_SECRET")&&strstr(snapshot,"\"hidden\":true"),"password remains masked");
        else CHECK(strstr(snapshot,i==1?"12345":"SYNTHETIC_TEXT")&&strstr(snapshot,"\"session\"")&&strstr(snapshot,"\"revision\""),"plain and numeric keyboard expose content and target versions");
        ink_text_blur(0);
    }
    ink_probe_count("test.finished");
}
static void run(void *arg) { (void)arg; delay_ms(1800); on_ui(inspect); }
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="keyboard-snapshot",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"test task");
}
