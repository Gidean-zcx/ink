/* Observe the receiving connection, independently of the question's contents. */
#define sim_hub_receive fixture_original_receive
#include "../../simulator/environment/sim_hub.c"
#undef sim_hub_receive

void sim_hub_receive(struct hal_ws *ws, uint64_t t, const uint8_t *data, size_t len, bool binary) {
    static bool authed[16];
    if (!binary) {
        cJSON *msg = cJSON_ParseWithLength((const char *)data, len);
        const char *method = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "method"));
        if (method && (!strcmp(method, "hello") || !strcmp(method, "ink/member"))) {
            const cJSON *params = cJSON_GetObjectItemCaseSensitive(msg, "params");
            authed[member_slot(ws)] = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(params, "authed"));
            sim_metric_set("test.wire.context.epoch", num_or(params, "epoch", 0));
            sim_metric_set("test.wire.context.authed", authed[member_slot(ws)]);
            /* A controlled server delay keeps the real handshake window open for UI/PIN work. */
            if (!strcmp(method, "hello")) t += SIM_MS(1000);
        } else if (method && !strcmp(method, "ai.ask")) {
            int m = member_slot(ws);
            char key[64];
            snprintf(key, sizeof key, "test.wire.asks.%s", MC[m].member);
            sim_metric_add(key, 1);
            sim_metric_add("test.wire.asks", 1);
            sim_metric_set("test.wire.ask.epoch", MC[m].epoch);
            sim_metric_set("test.wire.ask.authed", authed[m]);
        }
        cJSON_Delete(msg);
    }
    fixture_original_receive(ws, t, data, len, binary);
}
