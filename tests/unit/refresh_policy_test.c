/* The refresh policy on its own (pure logic). An EPDiy-class panel behaves as before (D-108). A panel whose driver
 * cleans pages itself (the S31 with PQ-BN, D-110) changes pages in one quiet pass and cleans once by count: user
 * 2026-11-11, "肯定不能闪一下……可以定一个统计周期 GC16一下 避免残影 尽量少刷新". */
#undef NDEBUG /* the checks are the test: keep them in every build type */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "../../system/services/display/refresh_policy.h"

enum { W = 1216, H = 684 };
static const uint32_t WAVEFORMS = (1u << HAL_WF_GC16) | (1u << HAL_WF_GL16) | (1u << HAL_WF_DU);

static rp_dispatch_t refresh(rp_state_t *s, hal_rect_t r, ink_refresh_t intent) {
    rp_dispatch_t d = {.cleanup = false};
    rp_add(s, r, intent, 0, NULL);
    bool got = rp_next(s, &d);
    assert(got);
    rp_done(s, &d, true);
    return d;
}

static void cleaning_panel(rp_state_t *s) {
    rp_init(s, W, H, WAVEFORMS, false);
    s->cleans_pages = true;
}

int main(void) {
    const hal_rect_t page = {0, 0, W, H}, bar = {0, 0, 62, H}, word = {100, 100, 80, 40};
    rp_state_t s;
    rp_dispatch_t d;

    /* EPDiy-class, unchanged: a new page flashes, every fourth text refresh cleans */
    rp_init(&s, W, H, WAVEFORMS, false);
    assert(refresh(&s, page, INK_REFRESH_FLASH_UI).wf == HAL_WF_GC16);
    for (int i = 1; i <= 4; i++) assert((refresh(&s, word, INK_REFRESH_TEXT).wf == HAL_WF_GC16) == (i == 4));

    /* cleans_pages: RP_CLEAN_AFTER pages in one quiet pass each, then the next page change cleans the whole panel once */
    cleaning_panel(&s);
    for (int i = 0; i < RP_CLEAN_AFTER; i++) {
        d = refresh(&s, page, INK_REFRESH_FLASH_UI);
        assert(d.wf == HAL_WF_GL16 && !d.cleanup);
    }
    d = refresh(&s, word, INK_REFRESH_UI); /* a small change does not take the clean */
    assert(d.wf == HAL_WF_GL16 && !d.cleanup);
    d = refresh(&s, page, INK_REFRESH_FLASH_UI);
    assert(d.wf == HAL_WF_GC16 && d.cleanup && d.rect.x == 0 && d.rect.y == 0 && d.rect.w == W && d.rect.h == H);
    assert(refresh(&s, page, INK_REFRESH_FLASH_UI).wf == HAL_WF_GL16); /* the count starts again */

    /* text never flashes by itself there: no every-fourth rule */
    cleaning_panel(&s);
    for (int i = 0; i < 12; i++) assert(refresh(&s, word, INK_REFRESH_TEXT).wf == HAL_WF_GL16);

    /* small changes add up: the status bar every minute for about three hours (5 % of the screen each time) is due a
     * clean, and it waits for the next page change */
    cleaning_panel(&s);
    for (int i = 0; i < 200; i++) {
        d = refresh(&s, bar, INK_REFRESH_UI);
        assert(d.wf == HAL_WF_GL16 && !d.cleanup);
    }
    assert(refresh(&s, page, INK_REFRESH_FLASH_UI).wf == HAL_WF_GC16);

    /* clean_after 0 (the refinement pass, the first pass owns cleanup): never a clean */
    cleaning_panel(&s);
    s.clean_after = 0;
    for (int i = 0; i < 3 * RP_CLEAN_AFTER; i++) assert(refresh(&s, page, INK_REFRESH_FLASH_UI).wf == HAL_WF_GL16);

    /* a failed update: the next one is a full clean, and the count starts again from it */
    cleaning_panel(&s);
    for (int i = 0; i < RP_CLEAN_AFTER - 1; i++) (void)refresh(&s, page, INK_REFRESH_FLASH_UI);
    rp_add(&s, word, INK_REFRESH_UI, 0, NULL);
    bool got = rp_next(&s, &d);
    assert(got);
    rp_done(&s, &d, false);
    d = refresh(&s, word, INK_REFRESH_UI);
    assert(d.wf == HAL_WF_GC16 && d.cleanup && d.rect.w == W);
    for (int i = 0; i < RP_CLEAN_AFTER; i++) assert(refresh(&s, page, INK_REFRESH_FLASH_UI).wf == HAL_WF_GL16);

    printf("refresh policy: passed\n");
    return 0;
}
