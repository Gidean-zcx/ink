#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "script.h"
static ink_script_t script = {.resize = realloc, .width = 684, .height = 1216, .dpi = 297};
static void parse(const char *value) {
    char line[512], error[80] = "";
    snprintf(line, sizeof line, "%s", value);
    assert(ink_script_line(&script, line, 42, error, sizeof error) == 0);
}
int main(void) {
    parse("@1s tap 50% 25%");
    assert(script.count == 2 && script.actions[0].t == 1000000000 && script.actions[1].t == 1080000000);
    assert(script.actions[0].b == 342 && script.actions[0].c == 304 && script.actions[1].a == 1);
    parse("+1s longpress 160dp 40");
    assert(script.actions[2].b == 297 && script.actions[3].t == 2800000000ull);
    parse("+0 swipe 10% 20% 90% 20% 16ms");
    assert(script.count == 8 && script.actions[7].t == 2020000000ull && script.actions[7].a == 1);
    parse("+1us key next 10ms");
    assert(script.actions[8].t == 2000001000ull && script.actions[8].a == HAL_KEY_NEXT);
    parse("+0 press 10 20"); parse("+0 move 20 30"); parse("+0 release");
    assert(script.actions[12].b == -1 && script.actions[12].a == 1);
    parse("+0 cap app.open {\"id\": \"camera\", \"label\": \"FAKE # label\"}");
    assert(script.actions[13].kind == A_CAP && !strcmp(script.actions[13].text, "app.open"));
    assert(!strcmp(script.actions[13].path, "{\"id\": \"camera\", \"label\": \"FAKE # label\"}"));
    assert(script.actions[13].line == 42);
    parse("@1m end"); assert(script.has_end && script.actions[14].t == 60000000000ull);
    const char *denied[] = {"sms.send", "message.send", "phone.dial", "phone.emergency_call", "file.delete", "member.remove",
        "member.set_pin", "factory.reset", "hub/paid", "test.password", "test.payment", "confirm.accept", "remote.open", "remote.submit", "remote.pointer"};
    for (size_t i = 0; i < sizeof denied / sizeof *denied; i++) {
        ink_cap_t cap = {.id = denied[i], .effect = INK_FX_READ};
        assert(!ink_script_cap_allowed(&cap));
    }
    for (int fx = INK_FX_READ; fx <= INK_FX_SYSTEM; fx++) {
        ink_cap_t cap = {.id = "test.value", .effect = fx};
        assert(ink_script_cap_allowed(&cap) == (fx < INK_FX_LOCAL_DESTRUCTIVE));
    }
    ink_cap_t cap = {.id = "app.open", .parent_pin = true}; assert(!ink_script_cap_allowed(&cap));
    cap.parent_pin = false; cap.ask_by_hand = true; assert(!ink_script_cap_allowed(&cap));
    cap.ask_by_hand = false; cap.expose = INK_EXPOSE_OWNER; assert(!ink_script_cap_allowed(&cap));
    assert(!ink_script_cap_allowed(NULL));
    char bad[] = "+0 unknown", error[80]; assert(ink_script_line(&script, bad, 1, error, sizeof error) < 0);
    free(script.actions);
    puts("autotest parser: passed");
}
