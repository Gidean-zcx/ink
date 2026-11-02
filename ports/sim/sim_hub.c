/*
 * Scripted hub for the simulator: answers the device's JSON-RPC messages
 * with timings from a hub script (--hub FILE), standing in for the real
 * family hub (a Python service, D-14) so device-side behaviour (latency,
 * streaming, failures) can be tested without a server. Runs in event (ISR)
 * context and models server time only; the network model carries the bytes.
 *
 * Hub script (JSON):
 *   {"rules": [
 *     {"method": "hello", "think_ms": 40, "result": {"session": "sim"}},
 *     {"method": "ai.ask", "think_ms": 1400,
 *      "stream": {"notify": "ai.delta", "chunks": ["...", "..."], "interval_ms": 450},
 *      "result": {"done": true}},
 *     {"method": "*", "think_ms": 30, "error": {"code": -32601, "message": "method not found"}}
 *   ]}
 * A rule may set "drop": true (no answer: tests timeouts).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "sim.h"

static cJSON *RULES;

static const char *DEFAULT_SCRIPT =
    "{\"rules\":["
    "{\"method\":\"hello\",\"think_ms\":40,\"result\":{\"session\":\"sim\",\"proto\":1}},"
    "{\"method\":\"ping\",\"think_ms\":5,\"result\":{}},"
    "{\"method\":\"ai.ask\",\"think_ms\":1400,\"stream\":{\"notify\":\"ai.delta\",\"interval_ms\":450,\"chunks\":["
    "\"这一页讲的是小满第一次替别人送信。\",\"陈阿姨二十年前和好朋友吵了架，一直没机会说对不起，\","
    "\"小满就在晚上八点对着天狼星，替她把这句话说了出来。\",\"第二天，陈阿姨收到了老朋友的回信：“我也是。”\"]},"
    "\"result\":{\"done\":true,\"source\":\"page\"}},"
    "{\"method\":\"*\",\"think_ms\":30,\"error\":{\"code\":-32601,\"message\":\"method not in hub script\"}}"
    "]}";

int sim_hub_load(const char *path, char *err, size_t errlen) {
    char *txt = NULL;
    if (path) {
        FILE *f = fopen(path, "rb");
        if (!f) {
            snprintf(err, errlen, "cannot read hub script %s", path);
            return -1;
        }
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        txt = malloc((size_t)n + 1);
        size_t got = fread(txt, 1, (size_t)n, f);
        txt[got] = 0;
        fclose(f);
    }
    cJSON *root = cJSON_Parse(txt ? txt : DEFAULT_SCRIPT);
    free(txt);
    if (!root || !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(root, "rules"))) {
        snprintf(err, errlen, "hub script: invalid JSON or no \"rules\" array");
        cJSON_Delete(root);
        return -1;
    }
    RULES = root;
    return 0;
}

static const cJSON *rule_for(const char *method) {
    const cJSON *rules = cJSON_GetObjectItemCaseSensitive(RULES, "rules"), *r, *star = NULL;
    cJSON_ArrayForEach(r, rules) {
        const char *m = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(r, "method"));
        if (!m) continue;
        if (!strcmp(m, method)) return r;
        if (!strcmp(m, "*")) star = r;
    }
    return star;
}

static void send_json(struct hal_ws *ws, uint64_t t, cJSON *msg) {
    char *txt = cJSON_PrintUnformatted(msg);
    sim_net_hub_send(ws, t, txt, strlen(txt), false);
    free(txt);
}

void sim_hub_receive(struct hal_ws *ws, uint64_t t, const uint8_t *data, size_t len, bool binary) {
    if (binary) { /* audio frames: accounted, no reply */
        sim_metric_add("hub.binary_bytes", (double)len);
        return;
    }
    if (!RULES) {
        char err[100];
        sim_hub_load(NULL, err, sizeof err);
    }
    cJSON *req = cJSON_ParseWithLength((const char *)data, len);
    if (!req) {
        sim_warn("hub: device sent invalid JSON (%zu bytes)", len);
        return;
    }
    const char *method = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(req, "method"));
    cJSON *id = cJSON_GetObjectItemCaseSensitive(req, "id");
    char key[80];
    snprintf(key, sizeof key, "hub.requests.%s", method ? method : "?");
    sim_metric_add(key, 1);
    sim_trace_instant("hub", method ? method : "?", t, NULL);
    const cJSON *rule = method ? rule_for(method) : NULL;
    if (!rule || !id || cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(rule, "drop"))) { /* notification or dropped */
        cJSON_Delete(req);
        return;
    }
    const cJSON *think = cJSON_GetObjectItemCaseSensitive(rule, "think_ms");
    uint64_t at = t + SIM_MS(cJSON_IsNumber(think) ? think->valuedouble : 30);
    const cJSON *stream = cJSON_GetObjectItemCaseSensitive(rule, "stream");
    if (stream) {
        const char *notify = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(stream, "notify"));
        const cJSON *iv = cJSON_GetObjectItemCaseSensitive(stream, "interval_ms");
        const cJSON *chunks = cJSON_GetObjectItemCaseSensitive(stream, "chunks"), *c;
        int k = 0;
        cJSON_ArrayForEach(c, chunks) {
            cJSON *n = cJSON_CreateObject();
            cJSON_AddStringToObject(n, "jsonrpc", "2.0");
            cJSON_AddStringToObject(n, "method", notify ? notify : "stream.delta");
            cJSON *params = cJSON_AddObjectToObject(n, "params");
            cJSON_AddItemToObject(params, "id", cJSON_Duplicate(id, 1));
            cJSON_AddNumberToObject(params, "seq", k);
            cJSON_AddStringToObject(params, "text", cJSON_GetStringValue(c) ? cJSON_GetStringValue(c) : "");
            send_json(ws, at, n);
            cJSON_Delete(n);
            at += SIM_MS(cJSON_IsNumber(iv) ? iv->valuedouble : 400);
            k++;
        }
    }
    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "jsonrpc", "2.0");
    cJSON_AddItemToObject(res, "id", cJSON_Duplicate(id, 1));
    const cJSON *err = cJSON_GetObjectItemCaseSensitive(rule, "error");
    const cJSON *result = cJSON_GetObjectItemCaseSensitive(rule, "result");
    if (err) cJSON_AddItemToObject(res, "error", cJSON_Duplicate(err, 1));
    else cJSON_AddItemToObject(res, "result", result ? cJSON_Duplicate(result, 1) : cJSON_CreateObject());
    send_json(ws, at, res);
    cJSON_Delete(res);
    cJSON_Delete(req);
}
