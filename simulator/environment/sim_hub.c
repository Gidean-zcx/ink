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
 * A voice rule may list "tools": [{"name": "reader.next_page", "arguments": {},
 * "page_gen_offset": 0}] — after the transcript the hub calls them one by one
 * ("tools/call", waiting for each receipt) and then answers with the last
 * receipt's "say" (when "say_from_receipt": true) instead of the chunks.
 * {"method": "ai.listen", "turns": [rule, rule, ...]}: the n-th request uses
 * the n-th rule (the last one repeats).
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
        txt = sim_host_malloc((size_t)n + 1);
        size_t got = fread(txt, 1, (size_t)n, f);
        txt[got] = 0;
        fclose(f);
    }
    cJSON *root = cJSON_Parse(txt ? txt : DEFAULT_SCRIPT);
    sim_host_free(txt);
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
    sim_host_free(txt);
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
    int page_gen;
    int next_tool;      /* index of the next tool to call */
    int64_t tool_rpc;   /* id of the tools/call in flight (0 = none) */
    char say[256];      /* last receipt's "say" */
} LS[MAX_LISTEN];

static int64_t g_tool_rpc = 9000;

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

/* Text chunks + TTS audio + end + result, starting at `at`. */
static void finish_listen(int i, uint64_t at) {
    const cJSON *rule = LS[i].rule, *ls = cJSON_GetObjectItemCaseSensitive(rule, "listen");
    struct hal_ws *ws = LS[i].ws;
    cJSON *id = LS[i].id;
    const cJSON *stream = cJSON_GetObjectItemCaseSensitive(rule, "stream");
    const cJSON *chunks = cJSON_GetObjectItemCaseSensitive(stream, "chunks"), *c;
    double iv = num_or(stream, "interval_ms", 400);
    const char *nm = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(stream, "notify"));
    uint64_t text_at = at, tts_at = at + SIM_MS(num_or(ls, "tts_first_ms", 500));
    double cps = num_or(ls, "tts_chars_per_s", 4.5), speed = num_or(ls, "tts_speed", 2);
    uint32_t seq = 0;
    cJSON *say_chunks = NULL;
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(rule, "say_from_receipt")) && LS[i].say[0]) {
        say_chunks = cJSON_CreateArray();
        cJSON_AddItemToArray(say_chunks, cJSON_CreateString(LS[i].say));
        chunks = say_chunks;
    }
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
    cJSON_Delete(say_chunks);
    uint64_t end = text_at > tts_at ? text_at : tts_at;
    { /* names its turn: an end arriving after the user started a new question changes nothing there */
        cJSON *n = cJSON_CreateObject();
        cJSON_AddStringToObject(n, "jsonrpc", "2.0");
        cJSON_AddStringToObject(n, "method", "ai.tts.end");
        cJSON *params = cJSON_AddObjectToObject(n, "params");
        cJSON_AddItemToObject(params, "id", cJSON_Duplicate(id, 1));
        cJSON_AddNumberToObject(params, "stream", LS[i].stream);
        cJSON_AddNumberToObject(params, "frames", seq);
        send_json(ws, tts_at, n);
        cJSON_Delete(n);
    }
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

/* Call the next tool of the rule, or finish the turn. */
static void next_tool_or_finish(int i, uint64_t at) {
    const cJSON *tools = cJSON_GetObjectItemCaseSensitive(LS[i].rule, "tools");
    const cJSON *tool = cJSON_IsArray(tools) ? cJSON_GetArrayItem(tools, LS[i].next_tool) : NULL;
    if (!tool) {
        finish_listen(i, at + SIM_MS(num_or(LS[i].rule, "think_ms", 1400)));
        return;
    }
    LS[i].next_tool++;
    cJSON *rq = cJSON_CreateObject();
    cJSON_AddStringToObject(rq, "jsonrpc", "2.0");
    LS[i].tool_rpc = ++g_tool_rpc;
    cJSON_AddNumberToObject(rq, "id", (double)LS[i].tool_rpc);
    cJSON_AddStringToObject(rq, "method", "tools/call");
    cJSON *pr = cJSON_AddObjectToObject(rq, "params");
    const char *name = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(tool, "name"));
    cJSON_AddStringToObject(pr, "name", name ? name : "?");
    const cJSON *args = cJSON_GetObjectItemCaseSensitive(tool, "arguments");
    cJSON_AddItemToObject(pr, "arguments", args ? cJSON_Duplicate(args, 1) : cJSON_CreateObject());
    cJSON_AddNumberToObject(pr, "page_gen", LS[i].page_gen + (int)num_or(tool, "page_gen_offset", 0));
    send_json(LS[i].ws, at + SIM_MS(num_or(tool, "think_ms", 600)), rq);
    cJSON_Delete(rq);
}

/* The utterance is complete: transcript, then tools (if any), then the answer. */
static void answer_listen(int i, uint64_t t) {
    const cJSON *rule = LS[i].rule, *ls = cJSON_GetObjectItemCaseSensitive(rule, "listen");
    sim_metric_sample("hub.listen.frames", LS[i].frames);
    uint64_t at = t + SIM_MS(num_or(ls, "stt_ms", 300));
    const char *tr = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(ls, "transcript"));
    notify(LS[i].ws, at, "ai.stt", LS[i].id, tr ? tr : "");
    if (cJSON_GetObjectItemCaseSensitive(rule, "tools")) next_tool_or_finish(i, at);
    else finish_listen(i, at + SIM_MS(num_or(rule, "think_ms", 1400)));
}

/* A response from the device to one of our tools/call requests. */
static bool on_device_response(struct hal_ws *ws, uint64_t t, cJSON *msg) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(msg, "id");
    if (!cJSON_IsNumber(id)) return false;
    for (int i = 0; i < MAX_LISTEN; i++) {
        if (!LS[i].used || LS[i].ws != ws || LS[i].tool_rpc != (int64_t)id->valuedouble) continue;
        LS[i].tool_rpc = 0;
        const cJSON *res = cJSON_GetObjectItemCaseSensitive(msg, "result");
        const char *st = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(res, "status"));
        const char *say = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(res, "say"));
        char key[64];
        snprintf(key, sizeof key, "hub.tool_results.%s", st ? st : "error");
        sim_metric_add(key, 1);
        snprintf(LS[i].say, sizeof LS[i].say, "%s", say ? say : "");
        next_tool_or_finish(i, t);
        return true;
    }
    return false;
}

/* Server capabilities the device calls through its remote proxy ("tools/call" to the hub, the
 * answer an MCP CallToolResult whose structuredContent is a receipt). "echo" returns its arguments
 * (tests of the proxy). Unknown names fall through to the script's rules. */
static bool server_tool(struct hal_ws *ws, uint64_t t, cJSON *req, cJSON *id) {
    const cJSON *pr = cJSON_GetObjectItemCaseSensitive(req, "params");
    const char *name = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(pr, "name"));
    if (!name || strcmp(name, "echo")) return false;
    char *args = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(pr, "arguments"));
    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "jsonrpc", "2.0");
    cJSON_AddItemToObject(res, "id", cJSON_Duplicate(id, 1));
    cJSON *r = cJSON_AddObjectToObject(res, "result");
    cJSON *content = cJSON_AddArrayToObject(r, "content");
    cJSON *text = cJSON_CreateObject();
    cJSON_AddStringToObject(text, "type", "text");
    cJSON_AddStringToObject(text, "text", args ? args : "{}");
    cJSON_AddItemToArray(content, text);
    cJSON *sc = cJSON_AddObjectToObject(r, "structuredContent");
    cJSON_AddStringToObject(sc, "status", "done");
    cJSON_AddStringToObject(sc, "say", args ? args : "{}");
    send_json(ws, t + SIM_MS(20), res);
    cJSON_Delete(res);
    cJSON_free(args);
    sim_metric_add("hub.tools.echo", 1);
    return true;
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
    if (!method) { /* a response from the device (to a tools/call) */
        if (!on_device_response(ws, t, req)) sim_metric_add("hub.unexpected_responses", 1);
        cJSON_Delete(req);
        return;
    }
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
    if (id && !strcmp(method, "tools/call") && server_tool(ws, t, req, id)) {
        cJSON_Delete(req);
        return;
    }
    const cJSON *rule = method ? rule_for(method) : NULL;
    if (!rule || !id || cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(rule, "drop"))) { /* notification or dropped */
        cJSON_Delete(req);
        return;
    }
    const cJSON *turns = cJSON_GetObjectItemCaseSensitive(rule, "turns");
    if (cJSON_IsArray(turns) && cJSON_GetArraySize(turns) > 0) { /* n-th request uses the n-th turn (last repeats) */
        static int nth;
        int k = nth < cJSON_GetArraySize(turns) ? nth : cJSON_GetArraySize(turns) - 1;
        nth++;
        rule = cJSON_GetArrayItem(turns, k);
    }
    if (cJSON_GetObjectItemCaseSensitive(rule, "listen")) { /* voice turn: answer after ai.listen.end */
        const cJSON *pr = cJSON_GetObjectItemCaseSensitive(req, "params");
        for (int i = 0; i < MAX_LISTEN; i++)
            if (!LS[i].used) {
                LS[i] = (typeof(LS[i])){.ws = ws, .id = cJSON_Duplicate(id, 1), .stream = (int)num_or(pr, "stream", 0),
                                        .rule = rule, .used = true, .page_gen = (int)num_or(pr, "page_gen", 0)};
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
