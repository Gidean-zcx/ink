#include "product.h"
#ifndef INK_TEST_TEXTFIELD_SOURCE
#define INK_TEST_TEXTFIELD_SOURCE "../../system/ui/textfield.c"
#endif
#include INK_TEST_TEXTFIELD_SOURCE
#ifdef INK_TEST_OLD_TEXTFIELD
static size_t field_index(field_t *f,uint32_t letter) {
    if(f->mark_at>=0&&letter>(uint32_t)f->mark_at) letter--;
    return f->window+letter;
}
#endif
static void inspect(void) {
    const char *value="01234567890123456789012345678901234567890123456789012345678901234567890123456789中文";
    ink_text_cfg_t cfg={.type=INK_TEXT_PLAIN,.initial=value,.max=1024};
    lv_obj_t *obj=ink_textfield_create(lv_screen_active(),&cfg,NULL,NULL,true);
    CHECK(obj!=NULL,"field");
    field_t *f=lv_obj_get_user_data(obj);
    ink_text_select(0,75);
    char line[256]; compose_line(f,value,"",line,sizeof line);
    CHECK(f->window==0&&line[0]=='[',"long selection never subtracts pointers before its window");
    ink_text_select(60,65); compose_line(f,value,"",line,sizeof line);
    char *closing=strchr(line,']');
    CHECK(closing!=NULL,"closing selection mark");
    if(closing) CHECK(field_index(f,(uint32_t)(closing-line)+1)==65,"both selection marks map back to original cursor");
    ink_text_select(80,80); compose_line(f,value,"nihao",line,sizeof line);
    char *compose=strstr(line,"nihao");
    CHECK(compose!=NULL,"composition shown");
    if(compose) CHECK(field_index(f,(uint32_t)(compose-line)+3)==80,"tap inside composition stays at insertion cursor");
    ink_text_blur(0); lv_obj_delete(obj);
    ink_probe_count("test.finished");
}
static void run(void *arg) { (void)arg; delay_ms(1800); on_ui(inspect); }
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="text-window",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"test task");
}
