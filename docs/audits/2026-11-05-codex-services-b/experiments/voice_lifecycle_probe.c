#include "product_probe.h"
static int started;
static void start(void) {
    ink_text_cfg_t c={.type=INK_TEXT_PLAIN,.initial="base",.max=1024};
    CHECK(ink_text_focus(&c,NULL,NULL)>0,"field");
    started=ink_text_voice_start();
}
static void press_ai(void) { ink_ai_key(true,osal_now_us()); }
static void release_ai(void) { ink_ai_key(false,osal_now_us()); }
static void dismiss_ai(void) { ink_ai_dismiss(); }
static void blur(void) { ink_text_blur(0); }
static void interrupt_twice(void) { ink_text_voice_interrupt(); ink_text_voice_interrupt(); }
static char value[1024], heard[256];
static int vs;
static void look(void) { snprintf(value,sizeof value,"%s",ink_text_value()); snprintf(heard,sizeof heard,"%s",ink_text_voice_heard()); vs=ink_text_voice_state(); }
static void audit_task(void *arg) {
    (void)arg; delay_ms(1800); ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online() && ink_cap_available("asr.stream"),20000),"ASR available");
    sim_hub_asr_script("unfinished dictation", "mute", 12);
    on_ui(start); CHECK(started==0,"dictation starts");
    CHECK(WAIT_FOR((on_ui(look),heard[0]),4000),"interim heard");
    on_ui(press_ai); on_ui(look);
    printf("AUDIT two_sessions voice=%d rt=%d capture=%d focus=%d\n",vs,ink_rt_open_count(),ink_audio_capture_busy(),ink_audio_focus_top());
    CHECK(vs==INK_VOICE_FINISHING && ink_rt_open_count()==2 && ink_audio_capture_busy(),"dictation finishes while AI owns new capture");
    on_ui(blur);
    CHECK(ink_rt_open_count()==1 && ink_audio_capture_busy(),"blur only cancels dictation, AI capture stays");
    on_ui(release_ai); on_ui(dismiss_ai);
    CHECK(WAIT_FOR(!ink_audio_capture_busy() && ink_rt_open_count()==0,3000),"all closed");
    sim_hub_asr_script("second unfinished", "mute", 12);
    on_ui(start); CHECK(started==0,"second dictation");
    CHECK(WAIT_FOR((on_ui(look),heard[0]),4000),"interim heard again");
    on_ui(interrupt_twice); on_ui(look);
    CHECK(vs==INK_VOICE_FINISHING,"repeated interrupt leaves finishing");
    CHECK(WAIT_FOR(!ink_audio_capture_busy(),1000),"microphone free");
    ink_power_lock();
    CHECK(WAIT_FOR((on_ui(look),vs==INK_VOICE_IDLE),2000),"locking cancels finishing");
    CHECK(ink_rt_open_count()==0 && !strcmp(value,"base"),"no late words committed");
    printf("AUDIT locked voice=%d rt=%d value=%s\n",vs,ink_rt_open_count(),value);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c={.name="audit-voice-life",.fn=audit_task,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c,NULL);
}
