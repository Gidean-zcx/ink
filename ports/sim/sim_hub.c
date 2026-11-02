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
 *
 * Voice turns ("ai.listen"): the request opens a session; binary microphone
 * frames [1][stream][seq] are counted; the "ai.listen.end" notification makes
 * the hub answer like the real one: "ai.stt" (transcript) after stt_ms, the
 * text chunks ("ai.delta") after think_ms, TTS audio as binary frames
 * [2][stream][seq] (stand-in Opus, sent tts_speed x faster than real time,
 * the first tts_first_ms after the thinking), "ai.tts.end", then the result:
 *   {"method": "ai.listen", "listen": {"stt_ms": 300, "transcript": "...",
 *    "tts_first_ms": 500, "tts_chars_per_s": 4.5, "tts_speed": 2},
 *    "think_ms": 1400, "stream": {...}, "result": {...}}
 */
#include <math.h>
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
    "{\"method\":\"ai.listen\",\"think_ms\":1400,\"listen\":{\"stt_ms\":300,\"transcript\":\"这一页讲了什么？\","
    "\"tts_first_ms\":500,\"tts_chars_per_s\":4.5,\"tts_speed\":2},"
    "\"stream\":{\"notify\":\"ai.delta\",\"interval_ms\":450,\"chunks\":["
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

/* ------------------------------------------------------------ voice turns */

#define MAX_LISTEN 4
static struct {
    struct hal_ws *ws;
    cJSON *id;
    int stream;
    int frames;
    const cJSON *rule;
    bool used;
} LS[MAX_LISTEN];

static void notify(struct hal_ws *ws, uint64_t t, const char *method, cJSON *id, const char *text) {
    cJSON *n = cJSON_CreateObject();
    cJSON_AddStringToObject(n, "jsonrpc", "2.0");
    cJSON_AddStringToObject(n, "method", method);
    cJSON *params = cJSON_AddObjectToObject(n, "params");
    cJSON_AddItemToObject(params, "id", cJSON_Duplicate(id, 1));
    if (text) cJSON_AddStringToObject(params, "text", text);
    send_json(ws, t, n);
    cJSON_Delete(n);
}

static size_t utf8_chars(const char *s) {
    size_t n = 0;
    for (; *s; s++)
        if (((unsigned char)*s & 0xC0) != 0x80) n++;
    return n;
}

static double num_or(const cJSON *o, const char *k, double def) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    return cJSON_IsNumber(v) ? v->valuedouble : def;
}

/* The utterance is complete: transcript, answer text, TTS audio, end, result. */
static void answer_listen(int i, uint64_t t) {
    const cJSON *rule = LS[i].rule, *ls = cJSON_GetObjectItemCaseSensitive(rule, "listen");
    struct hal_ws *ws = LS[i].ws;
    cJSON *id = LS[i].id;
    sim_metric_sample("hub.listen.frames", LS[i].frames);
    uint64_t at = t + SIM_MS(num_or(ls, "stt_ms", 300));
    const char *tr = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(ls, "transcript"));
    notify(ws, at, "ai.stt", id, tr ? tr : "");
    at += SIM_MS(num_or(rule, "think_ms", 1400));
    const cJSON *stream = cJSON_GetObjectItemCaseSensitive(rule, "stream");
    const cJSON *chunks = cJSON_GetObjectItemCaseSensitive(stream, "chunks"), *c;
    double iv = num_or(stream, "interval_ms", 400);
    const char *nm = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(stream, "notify"));
    uint64_t text_at = at, tts_at = at + SIM_MS(num_or(ls, "tts_first_ms", 500));
    double cps = num_or(ls, "tts_chars_per_s", 4.5), speed = num_or(ls, "tts_speed", 2);
    uint32_t seq = 0;
    cJSON_ArrayForEach(c, chunks) {
        const char *txt = cJSON_GetStringValue(c) ? cJSON_GetStringValue(c) : "";
        notify(ws, text_at, nm ? nm : "ai.delta", id, txt);
        text_at += SIM_MS(iv);
        /* this chunk's speech: chars / cps seconds of 60 ms frames, sent `speed` x real time */
        int frames = (int)(utf8_chars(txt) / cps / 0.06) + 1;
        for (int f = 0; f < frames; f++) {
            uint8_t pkt[4 + 200];
            pkt[0] = 2;
            pkt[1] = (uint8_t)LS[i].stream;
            pkt[2] = (uint8_t)seq;
            pkt[3] = (uint8_t)(seq >> 8);
            double syl = 0.5 + 0.5 * sin(seq * 0.9); /* syllable rhythm */
            int n = sim_opus_fake_packet(pkt + 4, 200, 170.0 + 40.0 * sin(seq * 0.23), 1500.0 + 4000.0 * syl, 180, seq * 2654435761u);
            sim_net_hub_send(ws, tts_at, pkt, (size_t)(4 + n), true);
            tts_at += (uint64_t)(SIM_MS(60) / speed);
            seq++;
        }
    }
    uint64_t end = text_at > tts_at ? text_at : tts_at;
    notify(ws, tts_at, "ai.tts.end", id, NULL);
    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "jsonrpc", "2.0");
    cJSON_AddItemToObject(res, "id", cJSON_Duplicate(id, 1));
    const cJSON *result = cJSON_GetObjectItemCaseSensitive(rule, "result");
    cJSON_AddItemToObject(res, "result", result ? cJSON_Duplicate(result, 1) : cJSON_CreateObject());
    send_json(ws, end, res);
    cJSON_Delete(res);
    cJSON_Delete(LS[i].id);
    LS[i].used = false;
}

void sim_hub_receive(struct hal_ws *ws, uint64_t t, const uint8_t *data, size_t len, bool binary) {
    if (binary) { /* microphone frames of a voice turn */
        sim_metric_add("hub.binary_bytes", (double)len);
        if (len >= 4 && data[0] == 1)
            for (int i = 0; i < MAX_LISTEN; i++)
                if (LS[i].used && LS[i].ws == ws && LS[i].stream == data[1]) LS[i].frames++;
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
    if (method && !strcmp(method, "ai.listen.end")) { /* the device finished uploading an utterance */
        const cJSON *pr = cJSON_GetObjectItemCaseSensitive(req, "params");
        int stream = (int)num_or(pr, "stream", -1);
        for (int i = 0; i < MAX_LISTEN; i++)
            if (LS[i].used && LS[i].ws == ws && LS[i].stream == stream) answer_listen(i, t);
        cJSON_Delete(req);
        return;
    }
    const cJSON *rule = method ? rule_for(method) : NULL;
    if (!rule || !id || cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(rule, "drop"))) { /* notification or dropped */
        cJSON_Delete(req);
        return;
    }
    if (cJSON_GetObjectItemCaseSensitive(rule, "listen")) { /* voice turn: answer after ai.listen.end */
        const cJSON *pr = cJSON_GetObjectItemCaseSensitive(req, "params");
        for (int i = 0; i < MAX_LISTEN; i++)
            if (!LS[i].used) {
                LS[i] = (typeof(LS[i])){.ws = ws, .id = cJSON_Duplicate(id, 1), .stream = (int)num_or(pr, "stream", 0),
                                        .rule = rule, .used = true};
                break;
            }
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
