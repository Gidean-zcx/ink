/* Measure the production text service on the simulated device, without learning. */
#include <stdlib.h>
#include "ink/sys/res.h"
#include "ink/sys/textinput.h"
#include "../../system/services/text/text_internal.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include "ime_cases.h"

static int session, index_now, nc;
static bool fallback;
static volatile bool background_done;
static ink_exec_job_t barrier;
static ink_text_cand_t candidates[5];
static char value[64];
static ink_text_cand_t all_candidates[60];
static void host_mode(void *arg) { (void)arg; fallback = getenv("INK_IME_FALLBACK") != NULL; }
static void bg_done(void *arg) { (void)arg; background_done = true; }
static void focus(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .hint = "合成输入"};
    session = ink_text_focus(&cfg, NULL, NULL);
}
static void query(void) {
    while (*ink_text_composing()) ink_text_backspace();
    for (const char *p = CASES[index_now].key; *p; p++) ink_text_key(*p);
    nc = ink_text_candidates(0, candidates, 5);
}
static void host_row(void *arg) {
    (void)arg;
    printf("IME\t%d", index_now);
    for (int i = 0; i < nc; i++) printf("\t%s:%u", candidates[i].word, candidates[i].consumed);
    printf("\n");
}
static void commit(void) {
    while (*ink_text_composing()) ink_text_backspace();
    ink_text_key('n'); ink_text_key('h');
    nc = ink_text_candidates(0, all_candidates, 60);
    int selected = -1;
    for (int i = 0; i < nc; i++)
        if (!strcmp(all_candidates[i].word, "你好") && all_candidates[i].consumed == 2) selected = i;
    CHECK(selected >= 0, "nh offers nihao as a selectable candidate");
    if (selected >= 0) ink_text_pick(selected);
    snprintf(value, sizeof value, "%.60s", ink_text_value());
    ink_text_blur(session);
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home();
    uint64_t start = osal_now_us();
    py_want_dict(NULL);
    if (fallback) {
        CHECK(ink_exec_post(INK_EXEC_BG, &barrier, bg_done, NULL) == 0 && WAIT_FOR(background_done, 5000), "missing dictionary lookup completes");
        CHECK(sim_metric_value("text.dict_loaded") == 0, "fallback has no loaded dictionary");
    } else {
        CHECK(WAIT_FOR(sim_metric_value("text.dict_loaded") == 1, 5000), "shipping dictionary loads");
    }
    ink_probe_value("ime.load_ms", (osal_now_us() - start) / 1000.0);
    ink_probe_value("ime.external_bytes", (double)ink_mem_used("ime", INK_MEM_PSRAM));
    on_ui(focus);
    for (index_now = 0; index_now < (int)(sizeof CASES / sizeof CASES[0]); index_now++) {
        on_ui(query);
        bool first = false, five = false;
        for (int i = 0; i < nc; i++) {
            bool match = !strcmp(candidates[i].word, CASES[index_now].word) && candidates[i].consumed == strlen(CASES[index_now].key);
            five |= match;
            if (i == 0) first = match;
        }
        ink_probe_count("ime.cases");
        if (first) ink_probe_count("ime.first");
        if (five) ink_probe_count("ime.five");
        vtk_host_call(host_row, NULL);
    }
    if (!fallback) {
        on_ui(commit);
        CHECK(!strcmp(value, "你好"), "initials candidate commits through the public text service");
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(host_mode, NULL);
    product_main();
    osal_task_cfg_t cfg = {.name = "ime-quality-test", .fn = run, .stack_bytes = 16384,
                           .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start bounded quality driver");
}
