/* Observe the actual session request at the synthetic hub boundary. */
#define sim_hub_receive original_hub_receive
#include "../../simulator/environment/sim_hub.c"
#undef sim_hub_receive

static char last_url[180];
const char *test_remote_hub_url(void) { return last_url; }

void sim_hub_receive(struct hal_ws *ws, uint64_t t, const uint8_t *data, size_t len, bool binary) {
    if (!binary) {
        cJSON *msg = cJSON_ParseWithLength((const char *)data, len);
        const char *method = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "method"));
        if (method && !strcmp(method, "rt.open")) {
            const cJSON *params = cJSON_GetObjectItemCaseSensitive(msg, "params");
            const cJSON *args = cJSON_GetObjectItemCaseSensitive(params, "params");
            const char *url = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(args, "url"));
            if (url) snprintf(last_url, sizeof last_url, "%s", url);
        }
        cJSON_Delete(msg);
    }
    original_hub_receive(ws, t, data, len, binary);
}
