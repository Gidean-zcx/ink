#include "product_probe.h"
#include "ink/sys/textfield.h"
static lv_obj_t *field;
static int px, py, entered;
static void submitted(void *ctx, const char *value) { (void)ctx; (void)value; entered++; }
static void page(lv_obj_t *scr, void *arg) {
    (void)arg;
    ink_text_cfg_t c={.type=INK_TEXT_PLAIN,.initial="abcdef",.max=1024};
    field=ink_textfield_create(scr,&c,submitted,NULL,true);
    lv_obj_align(field,LV_ALIGN_TOP_MID,0,ink_dp(40));
}
static const ink_activity_t PAGE={.name="audit-text",.create=page};
static void open_page(void) { ink_ui_push(&PAGE,NULL); }
static void prepare_tap(void) {
    ink_text_select(1,3);
    lv_obj_t *label=lv_obj_get_child(field,0);
    lv_obj_update_layout(field);
    lv_area_t a; lv_point_t p;
    lv_obj_get_coords(label,&a);
    lv_label_get_letter_pos(label,5,&p); /* the d in a[bc]def */
    px=a.x1+p.x+2; py=a.y1+p.y+4;
    printf("AUDIT displayed=%s tap_letter=5\n",lv_label_get_text(label));
}
static void inspect_tap(void) {
    printf("AUDIT after_tap cursor=%zu expected=3\n",ink_text_cursor());
    CHECK(ink_text_cursor()==4,"reproduced: closing bracket shifts the cursor");
    ink_text_commit_utf8("X");
    printf("AUDIT after_insert=%s\n",ink_text_value());
    CHECK(!strcmp(ink_text_value(),"abcdXef"),"reproduced: insertion one character too far");
    char long_text[101]; memset(long_text,'a',100); long_text[100]=0;
    ink_text_select(0,999); ink_text_commit_utf8(long_text); ink_text_select(0,100);
    const char *shown=lv_label_get_text(lv_obj_get_child(field,0));
    const char *bracket=strchr(shown,'[');
    printf("AUDIT long_selection value_chars=100 label_chars=%zu opening_mark_at=%d\n",strlen(shown),bracket?(int)(bracket-shown):-1);
    CHECK(strlen(shown)==132 && bracket==shown+30,"reproduced: tail duplicated before a selection spanning the window");
    ink_ui_pop();
}
static const ink_text_client_t CLIENT={.entered=submitted};
static void hidden_field(void) {
    ink_text_cfg_t c={.type=INK_TEXT_PLAIN,.initial="private draft",.ai_hidden=true};
    CHECK(ink_text_focus(&c,&CLIENT,NULL)>0,"hidden field focused");
}
static void close_field(void) { ink_text_blur(0); }
static void audit_task(void *arg) {
    (void)arg; delay_ms(1800); on_ui(open_page); delay_ms(1000);
    on_ui(prepare_tap); delay_ms(300); tap(px,py); on_ui(inspect_tap);
    delay_ms(400); on_ui(hidden_field);
    CHECK(AI("text.insert","{\"text\":\"x\"}") && g_rc.status==INK_RC_REJECTED,"AI insert refused");
    int before=entered;
    CHECK(AI("text.submit",NULL),"submit answered");
    printf("AUDIT hidden_submit status=%s entered_delta=%d\n",ink_rc_name(g_rc.status),entered-before);
    CHECK(g_rc.status==INK_RC_DONE && entered==before+1,"reproduced: AI submits a hidden field");
    on_ui(close_field); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c={.name="audit-text-boundary",.fn=audit_task,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c,NULL);
}
