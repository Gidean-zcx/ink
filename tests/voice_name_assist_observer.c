/* Observe actual serialized payloads in the simulated hub's host context. */
#define sim_hub_receive original_receive
#include "../simulator/environment/sim_hub.c"
#undef sim_hub_receive
void sim_hub_receive(struct hal_ws *ws, uint64_t t, const uint8_t *data, size_t len, bool binary) {
    if (!binary) {
        cJSON *j = cJSON_ParseWithLength((const char *)data, len);
        const char *method = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j, "method"));
        if (method && !strcmp(method, "text/suggest")) {
            const cJSON *params = cJSON_GetObjectItemCaseSensitive(j, "params");
            const char *prefix = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(params, "prefix"));
            int chars = 0;
            for (const unsigned char *p = (const unsigned char *)(prefix ? prefix : ""); *p; p++)
                if ((*p & 0xc0) != 0x80) chars++;
            sim_metric_set("test.prefix.chars", chars);
            sim_metric_set("test.prefix.suffix", prefix && strstr(prefix, "TAIL_SECRET"));
            if (cJSON_GetArraySize(params) != 2) sim_warn("unexpected private suggestion fields");
        }
        cJSON_Delete(j);
    }
    original_receive(ws, t, data, len, binary);
}
