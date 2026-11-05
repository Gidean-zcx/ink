#include "product.h"
#include "ink/ui_shell.h"
static char snapshot[4096];
static char context[3072]; /* The production AI request's SNAPSHOT_CAP. */
static int page_snapshot(char *out,size_t cap) { return snprintf(out,cap,"合成表单内容"); }
static int ai_snapshot(char *out,size_t cap) { return snprintf(out,cap,"SYNTHETIC_AI_REPLY"); }
static int prompt_snapshot(char *out,size_t cap) { return snprintf(out,cap,"SYNTHETIC_PROMPT"); }
static void create(lv_obj_t *screen,void *ctx) { (void)screen; (void)ctx; }
static const ink_activity_t PAGE={.name="synthetic-form",.create=create,.snapshot=page_snapshot};
static const ink_ui_overlay_t AI_PANEL={.name="Ai",.level=-1,.snapshot=ai_snapshot};
static const ink_ui_overlay_t PROMPT={.name="synthetic-prompt",.level=1,.snapshot=prompt_snapshot};
static void inspect_context(void) {
    const char *name=NULL;
    CHECK(ink_ui_context_snapshot(context,sizeof context,&name)>0&&!strcmp(context,snapshot)&&strstr(context,"合成表单内容")&&strstr(context,"\"keyboard\""),"AI request retains the same page and keyboard snapshot");
    CHECK(name&&!strcmp(name,"Keyboard"),"combined snapshot identifies the visible keyboard");
    char small[8]="filled";
    CHECK(ink_ui_snapshot(small,sizeof small,NULL)<0&&!small[0],"small snapshot buffer fails without partial JSON");
    CHECK(ink_ui_context_snapshot(small,sizeof small,NULL)<0&&!small[0],"small AI context buffer fails without partial JSON");
    lv_obj_t *ai=lv_obj_create(lv_layer_sys());
    ink_ui_overlay_add(&AI_PANEL,ai);
    CHECK(ink_ui_context_snapshot(context,sizeof context,&name)>0&&!strcmp(context,snapshot)&&strstr(context,"合成表单内容")&&strstr(context,"\"keyboard\""),"AI panel keeps the underlying page and input context");
    CHECK(ink_ui_snapshot(context,sizeof context,&name)>0&&!strcmp(context,"SYNTHETIC_AI_REPLY")&&name&&!strcmp(name,"Ai"),"visible snapshot still describes the AI panel");
    lv_obj_t *prompt=lv_obj_create(lv_layer_sys());
    ink_ui_overlay_add(&PROMPT,prompt);
    CHECK(ink_ui_context_snapshot(context,sizeof context,&name)>0&&!strcmp(context,"SYNTHETIC_PROMPT"),"blocking overlay hides page and input from AI context");
    CHECK(ink_ui_snapshot(context,sizeof context,&name)>0&&!strcmp(context,"SYNTHETIC_PROMPT"),"blocking overlay owns the visible snapshot");
    ink_ui_overlay_remove(&PROMPT);
    lv_obj_delete(prompt);
    ink_ui_overlay_remove(&AI_PANEL);
    lv_obj_delete(ai);
}
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
        CHECK(strstr(snapshot,"\"keyboard_ui\"")&&strstr(snapshot,i==2?"密码输入":"键盘"),"combined snapshot retains visible keyboard details");
        inspect_context();
        ink_text_blur(0);
    }
    ink_text_cfg_t hidden={.type=INK_TEXT_PLAIN,.initial="SYNTHETIC_HIDDEN",.hint="合成敏感输入框",.ai_hidden=true};
    CHECK(ink_text_focus(&hidden,&client,NULL)>0,"focus AI-hidden input");
    CHECK(ink_ui_snapshot(snapshot,sizeof snapshot,NULL)>0&&strstr(snapshot,"合成表单内容")&&strstr(snapshot,"\"hidden\":true")&&strstr(snapshot,"******")&&!strstr(snapshot,"SYNTHETIC_HIDDEN")&&!strstr(snapshot,"\"session\"")&&!strstr(snapshot,"候选["),"both parts of AI-hidden input remain masked");
    inspect_context();
    ink_text_blur(0);
    CHECK(ink_ui_snapshot(snapshot,sizeof snapshot,NULL)>0&&!strcmp(snapshot,"合成表单内容"),"blur restores the page snapshot");
    CHECK(ink_ui_context_snapshot(context,sizeof context,NULL)>0&&!strcmp(context,snapshot),"AI context returns to the page after blur");
    ink_probe_count("test.finished");
}
static void run(void *arg) { (void)arg; delay_ms(1800); on_ui(inspect); }
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="keyboard-snapshot",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"test task");
}
