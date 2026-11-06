#include "ink/osal.h"
#include "ink/sys/members.h"
static bool handoff_on_alloc;
static void *fixture_calloc(size_t count, size_t size, uint32_t mem) {
    if (handoff_on_alloc) {
        handoff_on_alloc = false;
        ink_member_switch("kid");
        ink_member_switch("owner");
    }
    return osal_calloc(count, size, mem);
}
#define osal_calloc fixture_calloc
#ifndef INK_TEST_CAP_SOURCE
#define INK_TEST_CAP_SOURCE "../../system/policy/cap.c"
#endif
#include INK_TEST_CAP_SOURCE
#undef osal_calloc
#include "../integration/whole_product.h"

static void page_reply(const char *text, int status) {
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    CHECK(j && !ink_json_parse(j, text, strlen(text)), "parse directory fixture");
    if (j) { dir_reply((void *)(uintptr_t)DF.generation, status, j, 0); osal_free(j); }
}

static void begin_directory(void) {
    DF.generation++;
    DF.epoch = ink_hub_member_epoch();
    DF.connection = ink_hub_connection();
    DF.n = DF.pages = 0;
}

void test_cap_directory(void) {
    begin_directory();
    page_reply("{\"tools\":[{\"name\":\"fixture.old\"}],\"rev\":4}", INK_HUB_OK);
    CHECK(ink_cap_available("hub/fixture.old"), "initial complete directory");
    begin_directory();
    page_reply("{\"tools\":[{\"name\":\"fixture.first\",\"offline\":\"local\",\"timeout_ms\":7}],\"nextCursor\":\"opaque/a\",\"rev\":5}", INK_HUB_OK);
    CHECK(ink_cap_available("hub/fixture.old") && !ink_cap_available("hub/fixture.first"), "partial directory does not replace old view");
    page_reply("{\"tools\":[{\"name\":\"fixture.last\",\"form\":\"stream\"}],\"rev\":5}", INK_HUB_OK);
    CHECK(!ink_cap_available("hub/fixture.old") && ink_cap_available("hub/fixture.first") && ink_cap_available("hub/fixture.last"), "all pages atomically replace directory");
    begin_directory();
    page_reply("{\"tools\":[{\"name\":\"fixture.bad\"}],\"nextCursor\":\"opaque/b\",\"rev\":6}", INK_HUB_OK);
    page_reply("{\"tools\":[{\"name\":\"fixture.changed\"}],\"rev\":7}", INK_HUB_OK);
    CHECK(ink_cap_available("hub/fixture.last") && !ink_cap_available("hub/fixture.bad"), "mixed revisions preserve old directory");
    begin_directory();
    page_reply("{\"tools\":[{\"name\":\"fixture.loop\"}],\"nextCursor\":\"same\"}", INK_HUB_OK);
    page_reply("{\"tools\":[{\"name\":\"fixture.loop2\"}],\"nextCursor\":\"same\"}", INK_HUB_OK);
    CHECK(ink_cap_available("hub/fixture.last") && !ink_cap_available("hub/fixture.loop"), "repeating cursor does not publish incomplete directory");
    begin_directory();
    page_reply("{\"tools\":[{\"name\":\"fixture.error\"}],\"nextCursor\":\"next\"}", INK_HUB_OK);
    page_reply("{}", INK_HUB_ERR_TIMEOUT);
    CHECK(ink_cap_available("hub/fixture.last") && !ink_cap_available("hub/fixture.error"), "failed continuation preserves old directory");
    DF.generation++; /* invalidate the synthetic continuation requests */
    dir_fetch(NULL);
}

uint32_t test_event_boot(void) { return CB.boot_tag; }
void test_cap_accept_handoff(void) {
    char text[160];
    snprintf(text, sizeof text, "{\"name\":\"fixture.bump\",\"_meta\":{\"member\":\"owner\",\"epoch\":%u}}", ink_hub_member_epoch());
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    CHECK(j && !ink_json_parse(j, text, strlen(text)), "parse acceptance-race fixture");
    handoff_on_alloc = true;
    on_tools(NULL, 2000000000, "tools/call", j, 0);
    CHECK(!handoff_on_alloc, "handoff happened between validation and job allocation");
    osal_free(j);
}
uint32_t test_event_acked(void) { return ev_acked; }
void test_event_ack(uint32_t boot, uint32_t upto) {
    char text[96];
    snprintf(text, sizeof text, "{\"boot\":%u,\"upto\":%u}", boot, upto);
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (j && !ink_json_parse(j, text, strlen(text))) event_ack(NULL, "device/event_ack", j, 0);
    osal_free(j);
}
