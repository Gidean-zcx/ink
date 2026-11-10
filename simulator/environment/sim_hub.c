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
 * A rule may set "drop": true (no answer: tests timeouts). A stream may add "chunks_<lang>" (e.g.
 * "chunks_en"): the answer when the request's "lang" is that language, like a model told to reply in it.
 *
 * Voice turns ("ai.listen"): the request opens a turn in the device's realtime
 * session ("rt.open", answered here); binary microphone frames [1][sid][seq]...
 * (10-byte header, ink/sys/rt.h) are counted; the "ai.listen.end" notification makes
 * the hub answer like the real one: "ai.stt" (transcript) after stt_ms, the
 * text chunks ("ai.delta") after think_ms, TTS audio as binary frames
 * [2][stream][seq] (stand-in Opus, sent tts_speed x faster than real time,
 * the first tts_first_ms after the thinking), "ai.tts.end", then the result:
 *   {"method": "ai.listen", "listen": {"stt_ms": 300, "transcript": "...",
 *    "tts_first_ms": 500, "tts_chars_per_s": 4.5, "tts_speed": 2},
 *    "think_ms": 1400, "stream": {...}, "result": {...}}
 * A voice rule may list "tools": [{"name": "reader.next_page", "arguments": {},
 * "page_gen_offset": 0, "request_id": "r1", "repeat": 2}] — after the transcript the hub calls them one by one
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

bool sim_backup_frame(struct hal_ws *ws, uint64_t t, const uint8_t *data, size_t len);
bool sim_backup_request(struct hal_ws *ws, uint64_t t, const char *method, const cJSON *params, const cJSON *id);

static cJSON *RULES;

static const char *DEFAULT_SCRIPT =
    "{\"rules\":["
    "{\"method\":\"hello\",\"think_ms\":40,\"result\":{\"session\":\"sim\",\"proto\":1}},"
    "{\"method\":\"ping\",\"think_ms\":5,\"result\":{}},"
    "{\"method\":\"ai.ask\",\"think_ms\":1400,\"stream\":{\"notify\":\"ai.delta\",\"interval_ms\":450,\"chunks\":["
    "\"这一页讲的是小满第一次替别人送信。\",\"陈阿姨二十年前和好朋友吵了架，一直没机会说对不起，\","
    "\"小满就在晚上八点对着天狼星，替她把这句话说了出来。\",\"第二天，陈阿姨收到了老朋友的回信：“我也是。”\"],"
    "\"chunks_en\":[\"This page is about Xiaoman delivering a message for someone for the first time.\"]},"
    "\"result\":{\"done\":true,\"source\":\"page\"}},"
    "{\"method\":\"ai.listen\",\"think_ms\":1400,\"listen\":{\"stt_ms\":300,\"transcript\":\"这一页讲了什么？\","
    "\"tts_first_ms\":500,\"tts_chars_per_s\":4.5,\"tts_speed\":2},"
    "\"stream\":{\"notify\":\"ai.delta\",\"interval_ms\":450,\"chunks\":["
    "\"这一页讲的是小满第一次替别人送信。\",\"陈阿姨二十年前和好朋友吵了架，一直没机会说对不起，\","
    "\"小满就在晚上八点对着天狼星，替她把这句话说了出来。\",\"第二天，陈阿姨收到了老朋友的回信：“我也是。”\"],"
    "\"chunks_en\":[\"This page is about Xiaoman delivering a message for someone for the first time.\"]},"
    "\"result\":{\"done\":true,\"source\":\"page\"}},"
    "{\"method\":\"weather/find\",\"think_ms\":30,\"result\":{\"cities\":[{\"id\":101,\"name\":\"示例市\",\"area\":\"示例省 · 中国\"},"
    "{\"id\":102,\"name\":\"示例市\",\"area\":\"另一省 · 中国\"}]}},"
    "{\"method\":\"weather/get\",\"think_ms\":30,\"result\":{\"city\":\"示例市\",\"at\":0,\"t\":12,\"w\":\"小雨\",\"i\":\"rain\","
    "\"hi\":15,\"lo\":3,\"p\":80,\"days\":[[\"多云\",\"cloudsun\",16,5,20],[\"晴\",\"sun\",18,7,0],[\"小雪\",\"snow\",3,-2,-1]]}},"
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

static int64_t g_hub_clock_offset_s;
static bool g_event_acks = true;
void sim_hub_event_acks(bool enabled) { g_event_acks = enabled; }

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
    sim_net_hub_send_at(ws, t, txt, strlen(txt), false);
    sim_host_free(txt);
}

/* Synthetic connection identity, learned only from the device boundary messages. */
static struct { struct hal_ws *ws; char member[16]; uint32_t epoch; } MC[16];
static int member_slot(struct hal_ws *ws) {
    for (int i = 0; i < 16; i++) if (MC[i].ws == ws) return i;
    for (int i = 0; i < 16; i++) if (!MC[i].ws) { MC[i].ws = ws; return i; }
    return 0;
}
static void delivery_meta(cJSON *params, const char *member, uint32_t epoch) {
    cJSON *meta = cJSON_GetObjectItemCaseSensitive(params, "_meta");
    if (!meta) meta = cJSON_AddObjectToObject(params, "_meta");
    if (!cJSON_GetObjectItemCaseSensitive(meta, "member")) cJSON_AddStringToObject(meta, "member", member);
    if (!cJSON_GetObjectItemCaseSensitive(meta, "epoch")) cJSON_AddNumberToObject(meta, "epoch", epoch);
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
    int tool_sends;     /* times the current tool was sent (its "repeat") */
    int64_t tool_rpc;   /* id of the tools/call in flight (0 = none) */
    char say[256];      /* last receipt's "say" */
    char member[16];
    uint32_t epoch;
    char lang[8];       /* the request's "lang" */
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

/* The answer in the request's language: the stream's "chunks_<lang>" when it has them. */
static const cJSON *chunks_for(const cJSON *stream, const char *lang) {
    char key[24];
    snprintf(key, sizeof key, "chunks_%.8s", lang ? lang : "");
    const cJSON *chunks = lang && lang[0] ? cJSON_GetObjectItemCaseSensitive(stream, key) : NULL;
    return chunks ? chunks : cJSON_GetObjectItemCaseSensitive(stream, "chunks");
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
    const cJSON *chunks = chunks_for(stream, LS[i].lang), *c;
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
            uint8_t pkt[10 + 200]; /* realtime frame header (ink/sys/rt.h): kind, session, seq, time, flags */
            uint32_t t_ms = (uint32_t)(tts_at / 1000000u);
            pkt[0] = 2;
            pkt[1] = (uint8_t)LS[i].stream;
            pkt[2] = (uint8_t)seq;
            pkt[3] = (uint8_t)(seq >> 8);
            pkt[4] = (uint8_t)t_ms;
            pkt[5] = (uint8_t)(t_ms >> 8);
            pkt[6] = (uint8_t)(t_ms >> 16);
            pkt[7] = (uint8_t)(t_ms >> 24);
            pkt[8] = pkt[9] = 0;
            double syl = 0.5 + 0.5 * sin(seq * 0.9); /* syllable rhythm */
            int n = sim_opus_fake_packet(pkt + 10, 200, 170.0 + 40.0 * sin(seq * 0.23), 1500.0 + 4000.0 * syl, 180, seq * 2654435761u);
            sim_net_hub_send_at(ws, tts_at, pkt, (size_t)(10 + n), true);
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
    if (++LS[i].tool_sends >= (int)num_or(tool, "repeat", 1)) { /* "repeat": the same request sent again (a retry) */
        LS[i].next_tool++;
        LS[i].tool_sends = 0;
    }
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
    const char *rid = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(tool, "request_id"));
    if (rid) cJSON_AddStringToObject(cJSON_AddObjectToObject(pr, "_meta"), "request_id", rid);
    delivery_meta(pr, LS[i].member, LS[i].epoch);
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

/* For tests: a request to the device, as the real hub makes it, and what it said; what it told, and what it asked with. */
#define TEST_RPC_BASE 1000000000
static struct hal_ws *g_last_ws;
static int64_t g_test_rpc = TEST_RPC_BASE;
static char *g_test_reply;
static int64_t g_test_reply_id, g_test_external_id;
static struct {
    char *text[32];
    char audience[32][16];
    int n;
} g_events;
static char *g_last_ask;

/* Synthetic enrollment peer. No account service or real token is used. */
static struct {
    unsigned faults;
    int hello_error;
    bool wipe, accepted, used;
    char nonce[64], txid[40];
} PAIR;
void sim_hub_pair_faults(unsigned faults, int hello_error, bool wipe) {
    PAIR.faults = faults; PAIR.hello_error = hello_error; PAIR.wipe = wipe;
}
const char *sim_hub_pair_nonce(void) { return PAIR.nonce; }
const char *sim_hub_pair_txid(void) { return PAIR.txid; }
void sim_hub_pair_expire(void) {
    if (!g_last_ws) return;
    cJSON *n = cJSON_Parse("{\"jsonrpc\":\"2.0\",\"method\":\"pair/expired\",\"params\":{}}");
    send_json(g_last_ws, vtk_isr_now() + SIM_MS(1), n); cJSON_Delete(n);
}
static void pair_reply(struct hal_ws *ws, uint64_t at, const cJSON *id, const char *result, int code, bool wipe) {
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "jsonrpc", "2.0");
    cJSON_AddItemToObject(r, "id", cJSON_Duplicate(id, 1));
    if (!code) cJSON_AddItemToObject(r, "result", cJSON_Parse(result ? result : "{}"));
    else {
        cJSON *err = cJSON_AddObjectToObject(r, "error");
        cJSON_AddNumberToObject(err, "code", code);
        cJSON_AddStringToObject(err, "message", "synthetic pairing error");
        if (wipe) cJSON_AddBoolToObject(cJSON_AddObjectToObject(err, "data"), "wipe", true);
    }
    send_json(ws, at, r); cJSON_Delete(r);
}
static void pair_complete(struct hal_ws *ws, uint64_t at) {
    if (PAIR.faults & SIM_PAIR_DROP_COMPLETE) return;
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(r, "id", (double)++g_test_rpc);
    cJSON_AddStringToObject(r, "method", "pair/complete");
    cJSON *p = cJSON_AddObjectToObject(r, "params");
    cJSON_AddStringToObject(p, "txid", "test-tx-1");
    cJSON_AddStringToObject(p, "device", "test-device");
    cJSON_AddStringToObject(p, "token", "test-only-pending-token");
    cJSON_AddItemToObject(p, "urls", cJSON_Parse("[\"wss://example.com/v1/device\"]"));
    cJSON_AddItemToObject(p, "pins", cJSON_Parse("[\"1111111111111111111111111111111111111111111111111111111111111111\"]"));
    send_json(ws, at, r); cJSON_Delete(r);
    sim_metric_add("hub.pair_complete", 1);
}
static bool pairing_request(struct hal_ws *ws, uint64_t at, const char *method, const cJSON *req, const cJSON *id) {
    const cJSON *p = cJSON_GetObjectItemCaseSensitive(req, "params");
    if (!strcmp(method, "hello") && num_or(p, "proto", 1) == 2) {
        if (PAIR.faults & SIM_PAIR_DROP_HELLO) return true;
        const char *token = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(p, "token"));
        int error = PAIR.hello_error ? PAIR.hello_error : token && !strcmp(token, "test-only-pending-token") ? 0 : -32010;
        pair_reply(ws, at + SIM_MS(5), id, "{\"proto\":2,\"session\":\"test-session\"}", error, PAIR.wipe);
        sim_metric_add(error ? "hub.pair_hello_failed" : "hub.pair_hello_ok", 1);
        return true;
    }
    if (strncmp(method, "pair/", 5)) return false;
    if (cJSON_GetObjectItemCaseSensitive(p, "password") || cJSON_GetObjectItemCaseSensitive(p, "ssid")) sim_metric_add("hub.pair_secret_leaked", 1);
    const char *txid = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(p, "txid"));
    if (!strcmp(method, "pair/begin")) {
        const char *nonce = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(p, "nonce"));
        const char *preauth = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(p, "preauth"));
        if (preauth) sim_metric_add("hub.pair_preauth_received", 1);
        if (PAIR.faults & SIM_PAIR_BEGIN_ERROR) {
            pair_reply(ws, at + SIM_MS(5), id, NULL, PAIR.hello_error, PAIR.wipe); return true;
        }
        bool same = nonce && !strcmp(nonce, PAIR.nonce);
        if (preauth && (strcmp(preauth, "test-once") || (PAIR.used && !same))) {
            pair_reply(ws, at + SIM_MS(5), id, NULL, -32030, false); return true;
        }
        snprintf(PAIR.nonce, sizeof PAIR.nonce, "%s", nonce ? nonce : "");
        snprintf(PAIR.txid, sizeof PAIR.txid, "%s", txid ? txid : "");
        if (!(PAIR.faults & SIM_PAIR_DROP_BEGIN)) pair_reply(ws, at + SIM_MS(5), id, "{\"code\":\"K7QF2M9X\",\"expires_s\":600}", 0, false);
        if (preauth || txid) {
            PAIR.used = PAIR.used || preauth != NULL;
            PAIR.accepted = true;
            pair_complete(ws, at + SIM_MS(30));
        }
    } else if (!strcmp(method, "pair/accept")) {
        if (PAIR.faults & SIM_PAIR_ACCEPT_ERROR) {
            pair_reply(ws, at + SIM_MS(5), id, NULL, PAIR.hello_error, false); return true;
        }
        if (!(PAIR.faults & SIM_PAIR_DROP_ACCEPT)) {
            PAIR.accepted = true;
            pair_reply(ws, at + SIM_MS(5), id, "{}", 0, false);
            pair_complete(ws, at + SIM_MS(30));
        }
    } else if (!strcmp(method, "pair/reject")) {
        PAIR.accepted = false;
        pair_reply(ws, at + SIM_MS(5), id, "{}", 0, false);
    } else pair_reply(ws, at + SIM_MS(5), id, NULL, -32601, false);
    return true;
}

static void keep_text(char **slot, cJSON *item) {
    sim_host_free(*slot);
    *slot = cJSON_PrintUnformatted(item);
}

static void request_device(const char *method, const char *params_json, bool add_meta) { /* event context */
    if (!g_last_ws) return;
    cJSON *rq = cJSON_CreateObject();
    cJSON_AddStringToObject(rq, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(rq, "id", (double)++g_test_rpc);
    g_test_external_id = g_test_rpc;
    cJSON_AddStringToObject(rq, "method", method);
    cJSON *pr = cJSON_Parse(params_json ? params_json : "{}");
    int m = member_slot(g_last_ws);
    if (add_meta && pr && !strcmp(method, "tools/call")) delivery_meta(pr, MC[m].member, MC[m].epoch);
    if (pr) cJSON_AddItemToObject(rq, "params", pr);
    sim_host_free(g_test_reply);
    g_test_reply = NULL;
    send_json(g_last_ws, vtk_isr_now() + SIM_MS(1), rq);
    cJSON_Delete(rq);
}

void sim_hub_request_device(const char *method, const char *params_json) { request_device(method, params_json, true); }

const char *sim_hub_device_reply(void) { return g_test_reply; }
void sim_hub_request_device_raw(const char *method, const char *params_json) {
    if (!g_last_ws || !method || strlen(method) > 31 || !params_json) return;
    size_t size = strlen(params_json) + 128;
    char *txt = sim_host_malloc(size);
    if (!txt) return;
    g_test_external_id = ++g_test_rpc;
    int n = snprintf(txt, size, "{\"jsonrpc\":\"2.0\",\"id\":%lld,\"method\":\"%s\",\"params\":%s}",
                     (long long)g_test_rpc, method, params_json);
    sim_host_free(g_test_reply); g_test_reply = NULL;
    if (n > 0 && (size_t)n < size) sim_net_hub_send_at(g_last_ws, vtk_isr_now() + SIM_MS(1), txt, (size_t)n, false);
    sim_host_free(txt);
}
bool sim_hub_reply_matches_request(void) { return g_test_reply && g_test_reply_id == g_test_external_id; }
int sim_hub_events_seen(void) { return g_events.n; }
const char *sim_hub_event_at(int i) { return i >= 0 && i < g_events.n && i >= g_events.n - 32 ? g_events.text[i % 32] : NULL; } /* the latest 32 */
const char *sim_hub_event_audience(int i) { return i >= 0 && i < g_events.n && i >= g_events.n - 32 ? g_events.audience[i % 32] : NULL; }
const char *sim_hub_last_ask(void) { return g_last_ask; }

/* ------------------------------------------------------------ dictation (hub protocol §3.11) */

/* What is "said": phrases one after another, a part of each after a while, the whole of it a little later. A test
 * sets them; `sim_hub_asr_script`. */
#define ASR_INTERIM_FRAMES 8
#define ASR_FINAL_FRAMES 16
static struct {
    struct hal_ws *ws;
    int sid;
    bool open, refuse, hide, done, silent;
    int frames, seg, fail_after;
    char phrases[512], fail_code[16], params[128];
} ASR;

void sim_hub_asr_script(const char *phrases, const char *fail_code, int fail_after_frames) {
    snprintf(ASR.phrases, sizeof ASR.phrases, "%s", phrases ? phrases : "");
    snprintf(ASR.fail_code, sizeof ASR.fail_code, "%s", fail_code ? fail_code : "");
    ASR.fail_after = fail_after_frames;
    ASR.silent = fail_code && !strcmp(fail_code, "silent");
}
void sim_hub_asr_refuse(bool refuse) { ASR.refuse = refuse; }
void sim_hub_asr_hide(bool hide) { ASR.hide = hide; }
const char *sim_hub_asr_params(void) { return ASR.params; }

/* The n-th phrase, or NULL. */
static const char *asr_phrase(int n, char *out, size_t cap) {
    const char *p = ASR.phrases;
    while (n-- > 0 && p) {
        p = strchr(p, '|');
        if (p) p++;
    }
    if (!p || !*p) return NULL;
    const char *e = strchr(p, '|');
    size_t len = e ? (size_t)(e - p) : strlen(p);
    snprintf(out, cap, "%.*s", (int)len, p);
    return out;
}

static void asr_send(uint64_t at, const char *method, cJSON *params) {
    cJSON *m = cJSON_CreateObject();
    cJSON_AddStringToObject(m, "jsonrpc", "2.0");
    cJSON_AddStringToObject(m, "method", method);
    cJSON_AddNumberToObject(params, "sid", ASR.sid);
    cJSON_AddItemToObject(m, "params", params);
    send_json(ASR.ws, at, m);
    cJSON_Delete(m);
}

static void asr_result(uint64_t at, int seg, bool final, const char *text) {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "seg", seg);
    cJSON_AddBoolToObject(p, "final", final);
    cJSON_AddStringToObject(p, "text", text);
    asr_send(at, "rt.asr", p);
    sim_metric_add(final ? "hub.asr_finals" : "hub.asr_interims", 1);
}

static void asr_fail(uint64_t at) {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "code", ASR.fail_code);
    cJSON_AddStringToObject(p, "message", "test");
    asr_send(at, "rt.asr.error", p);
    asr_send(at + SIM_MS(1), "rt.close", cJSON_CreateObject());
    ASR.done = true;
    sim_metric_add("hub.asr_errors", 1);
}

static void asr_frame(uint64_t t) {
    if (ASR.done || ASR.silent) return;
    ASR.frames++;
    sim_metric_add("hub.asr_frames", 1);
    if (ASR.fail_code[0] && ASR.frames >= ASR.fail_after) { /* "mute": no error, it just does not answer any more */
        if (!strcmp(ASR.fail_code, "mute")) ASR.silent = true;
        else asr_fail(t + SIM_MS(10));
        return;
    }
    char text[256];
    if (!asr_phrase(ASR.seg, text, sizeof text)) return;
    int at = ASR.frames % ASR_FINAL_FRAMES;
    if (at == ASR_INTERIM_FRAMES) { /* about half of it, whole characters */
        size_t n = strlen(text) / 2;
        while (n && ((unsigned char)text[n] & 0xC0) == 0x80) n--;
        text[n] = 0;
        asr_result(t + SIM_MS(100), ASR.seg, false, text);
    } else if (at == 0) {
        asr_result(t + SIM_MS(100), ASR.seg, true, text);
        ASR.seg++;
    }
}

static void asr_end(uint64_t t) { /* rt.turn end: what is left, then done */
    if (ASR.done || ASR.silent) return;
    char text[256];
    uint64_t at = t + SIM_MS(100);
    int ms = ASR.frames * 60;
    /* only what was started: a phrase gets its first frames before it counts */
    while (ASR.frames % ASR_FINAL_FRAMES && asr_phrase(ASR.seg, text, sizeof text)) {
        asr_result(at, ASR.seg++, true, text);
        at += SIM_MS(10);
        ASR.frames = 0;
    }
    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "segs", ASR.seg);
    cJSON_AddNumberToObject(p, "ms", ms);
    asr_send(at + SIM_MS(10), "rt.asr.done", p);
    ASR.done = true;
    sim_metric_add("hub.asr_done", 1);
}

/* What it offers (INK_SIM_UPDATE_PKG, an .inkfw from tools/inkfw): its header says which version; pieces of it in base64. */
static void update_request(struct hal_ws *ws, uint64_t t, const cJSON *req, const cJSON *id, const char *method) {
    const char *pkg = getenv("INK_SIM_UPDATE_PKG");
    FILE *f = pkg ? fopen(pkg, "rb") : NULL;
    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "jsonrpc", "2.0");
    cJSON_AddItemToObject(res, "id", cJSON_Duplicate(id, 1));
    cJSON *r = cJSON_AddObjectToObject(res, "result");
    if (!strcmp(method, "update/check")) {
        char version[25] = "";
        uint32_t code = 0;
        long size = 0;
        if (f) {
            unsigned char h[76];
            if (fread(h, 1, sizeof h, f) == sizeof h) {
                memcpy(version, h + 48, 24);
                memcpy(&code, h + 72, 4);
            }
            fseek(f, 0, SEEK_END);
            size = ftell(f);
        }
        cJSON_AddStringToObject(r, "version", version);
        cJSON_AddNumberToObject(r, "version_code", code);
        cJSON_AddNumberToObject(r, "size", (double)size);
        char blob[48];
        snprintf(blob, sizeof blob, "sha256:sim-%u-%ld", (unsigned)code, size);
        cJSON_AddStringToObject(r, "blob", blob);
    } else if (f) {
        const cJSON *pr = cJSON_GetObjectItemCaseSensitive(req, "params");
        long offset = (long)num_or(pr, "offset", 0), len = (long)num_or(pr, "len", 0);
        /* the whole download is of one package, named by what update/check said (hub protocol §3.10) */
        char want[48];
        unsigned char head[76];
        uint32_t code = 0;
        if (fread(head, 1, sizeof head, f) == sizeof head) memcpy(&code, head + 72, 4);
        fseek(f, 0, SEEK_END);
        snprintf(want, sizeof want, "sha256:sim-%u-%ld", (unsigned)code, ftell(f));
        const cJSON *asked = cJSON_GetObjectItemCaseSensitive(pr, "blob");
        if (!cJSON_IsString(asked) || strcmp(asked->valuestring, want)) {
            sim_metric_add("hub.update_wrong_package", 1);
            len = 0;
        }
        static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        unsigned char *raw = malloc((size_t)len + 3);
        char *text = malloc((size_t)len * 2 + 8);
        fseek(f, offset, SEEK_SET);
        size_t got = fread(raw, 1, (size_t)len, f), o = 0;
        for (size_t i = 0; i < got; i += 3) {
            uint32_t v = (uint32_t)raw[i] << 16 | (uint32_t)(i + 1 < got ? raw[i + 1] : 0) << 8 | (uint32_t)(i + 2 < got ? raw[i + 2] : 0);
            text[o++] = B64[v >> 18 & 63];
            text[o++] = B64[v >> 12 & 63];
            text[o++] = i + 1 < got ? B64[v >> 6 & 63] : '=';
            text[o++] = i + 2 < got ? B64[v & 63] : '=';
        }
        text[o] = 0;
        cJSON_AddStringToObject(r, "data", text);
        free(raw);
        free(text);
    }
    if (f) fclose(f);
    /* the link's speed: Wi-Fi of a few hundred kB/s */
    send_json(ws, t + SIM_MS(20) + SIM_MS(cJSON_GetObjectItemCaseSensitive(r, "data") ? 40 : 0), res);
    cJSON_Delete(res);
}

/* A response from the device to one of our tools/call requests. */
static bool on_device_response(struct hal_ws *ws, uint64_t t, cJSON *msg) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(msg, "id");
    if (!cJSON_IsNumber(id)) return false;
    if ((int64_t)id->valuedouble > TEST_RPC_BASE) {
        g_test_reply_id = (int64_t)id->valuedouble;
        keep_text(&g_test_reply, msg);
        return true;
    }
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
        const cJSON *pg = cJSON_GetObjectItemCaseSensitive(res, "page_gen");
        if (cJSON_IsNumber(pg)) LS[i].page_gen = (int)pg->valuedouble; /* the next tool is prepared against what is on screen now */
        next_tool_or_finish(i, t);
        return true;
    }
    return false;
}

/* Server capabilities the device calls through its remote proxy ("tools/call" to the hub, the
 * answer an MCP CallToolResult whose structuredContent is a receipt). "echo" returns its arguments
 * (tests of the proxy). Unknown names fall through to the script's rules. */
/* Realtime sessions opened with cap "echo": every frame comes back as it was (tests of the session
 * layer: order, priorities, round trip). */
static struct {
    struct hal_ws *ws;
    int sid;
} ECHO[4];

static bool echo_session(struct hal_ws *ws, int sid) {
    for (int i = 0; i < 4; i++)
        if (ECHO[i].ws == ws && ECHO[i].sid == sid) return true;
    return false;
}

static void echo_set(struct hal_ws *ws, int sid, bool on) {
    if (on && echo_session(ws, sid)) return; /* announced again after a reconnect */
    for (int i = 0; i < 4; i++)
        if (on ? !ECHO[i].ws : ECHO[i].ws == ws && ECHO[i].sid == sid) {
            ECHO[i].ws = on ? ws : NULL;
            ECHO[i].sid = on ? sid : 0;
            return;
        }
}

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
        if (sim_backup_frame(ws, t, data, len)) return;
        if (sim_remote_frame(ws, t, data, len)) return; /* credits and input of a remote screen */
        if (len >= 10 && echo_session(ws, data[1])) {
            sim_metric_add("hub.rt_echoed", 1);
            sim_net_hub_send(ws, t + SIM_MS(5), data, len, true);
            return;
        }
        if (len >= 10 && data[0] == 1 && ASR.open && ASR.ws == ws && ASR.sid == data[1]) {
            asr_frame(t);
            return;
        }
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
        const cJSON *result = cJSON_GetObjectItemCaseSensitive(req, "result");
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(result, "ok"))) {
            if (PAIR.faults & SIM_PAIR_DROP_ACK) { sim_metric_add("hub.pair_ack_dropped", 1); cJSON_Delete(req); return; }
            sim_metric_add("hub.pair_ack_received", 1);
        }
        if (!on_device_response(ws, t, req)) sim_metric_add("hub.unexpected_responses", 1);
        cJSON_Delete(req);
        return;
    }
    char key[80];
    snprintf(key, sizeof key, "hub.requests.%s", method ? method : "?");
    sim_metric_add(key, 1);
    sim_trace_instant("hub", method ? method : "?", t, NULL);
    g_last_ws = ws;
    if (id && sim_backup_request(ws, t, method, cJSON_GetObjectItemCaseSensitive(req, "params"), id)) { cJSON_Delete(req); return; }
    if (!strcmp(method, "hello") || !strcmp(method, "ink/member")) {
        int m = member_slot(ws);
        const cJSON *pr = cJSON_GetObjectItemCaseSensitive(req, "params");
        const char *member = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(pr, "member"));
        uint32_t epoch = (uint32_t)num_or(pr, "epoch", 0);
        if (member && epoch && (!strcmp(method, "hello") || epoch > MC[m].epoch)) {
            snprintf(MC[m].member, sizeof MC[m].member, "%s", member);
            MC[m].epoch = epoch;
        }
        if (!strcmp(method, "ink/member")) {
            for (int i = 0; i < MAX_LISTEN; i++) if (LS[i].used && LS[i].ws == ws) {
                cJSON_Delete(LS[i].id);
                LS[i].used = false;
                sim_metric_add("hub.member_turns_cancelled", 1);
            }
            if (ASR.ws == ws) ASR.open = false;
            cJSON *changed = cJSON_CreateObject();
            cJSON_AddStringToObject(changed, "jsonrpc", "2.0");
            cJSON_AddStringToObject(changed, "method", "caps/changed");
            cJSON_AddObjectToObject(changed, "params");
            send_json(ws, t + SIM_MS(1), changed);
            cJSON_Delete(changed);
            cJSON_Delete(req);
            return;
        }
    }
    if (id && pairing_request(ws, t, method, req, id)) { cJSON_Delete(req); return; }
    if (!strcmp(method, "device/event")) {
        keep_text(&g_events.text[g_events.n % 32], req);
        snprintf(g_events.audience[g_events.n % 32], sizeof g_events.audience[0], "%s", MC[member_slot(ws)].member);
        g_events.n++;
        const cJSON *pr = cJSON_GetObjectItemCaseSensitive(req, "params");
        if (g_event_acks && cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(pr, "seq")) && cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(pr, "boot"))) {
            cJSON *ack = cJSON_CreateObject();
            cJSON_AddStringToObject(ack, "jsonrpc", "2.0");
            cJSON_AddStringToObject(ack, "method", "device/event_ack");
            cJSON *ap = cJSON_AddObjectToObject(ack, "params");
            cJSON_AddNumberToObject(ap, "boot", num_or(pr, "boot", 0));
            cJSON_AddNumberToObject(ap, "upto", num_or(pr, "seq", 0));
            send_json(ws, t + SIM_MS(1), ack);
            cJSON_Delete(ack);
        }
        cJSON_Delete(req);
        return;
    }
    if (!strncmp(method, "ai.ask", 6) || !strcmp(method, "ai.listen")) keep_text(&g_last_ask, req);
    if (method && !strcmp(method, "ai.listen.end")) { /* the device finished uploading an utterance */
        const cJSON *pr = cJSON_GetObjectItemCaseSensitive(req, "params");
        int stream = (int)num_or(pr, "stream", -1);
        for (int i = 0; i < MAX_LISTEN; i++)
            if (LS[i].used && LS[i].ws == ws && LS[i].stream == stream) answer_listen(i, t);
        cJSON_Delete(req);
        return;
    }
    if (!strcmp(method, "rt.turn") && ASR.open && ASR.ws == ws && (int)num_or(cJSON_GetObjectItemCaseSensitive(req, "params"), "sid", 0) == ASR.sid) {
        const char *op = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(req, "params"), "op"));
        if (op && !strcmp(op, "end")) {
            sim_metric_add("hub.asr_turn_end", 1);
            asr_end(t);
        }
        cJSON_Delete(req);
        return;
    }
    if (!strcmp(method, "rt.close")) { /* a realtime session ended (device side) */
        sim_metric_add("hub.rt_closed", 1);
        if (ASR.open && ASR.ws == ws && (int)num_or(cJSON_GetObjectItemCaseSensitive(req, "params"), "sid", 0) == ASR.sid) {
            ASR.open = false;
            if (!ASR.done) sim_metric_add("hub.asr_cancelled", 1);
        }
        echo_set(ws, (int)num_or(cJSON_GetObjectItemCaseSensitive(req, "params"), "sid", 0), false);
        sim_remote_close(ws, (int)num_or(cJSON_GetObjectItemCaseSensitive(req, "params"), "sid", 0));
        cJSON_Delete(req);
        return;
    }
    if (id && !strcmp(method, "caps/list")) { /* the hub's directory of server capabilities (architecture §5.3) */
        static const char *DIR_ASR = "{\"tools\":[{\"name\":\"asr.stream\",\"title\":\"听写\",\"form\":\"session\"},";
        static const char *DIR =
            "{\"tools\":[{\"name\":\"echo\",\"title\":\"回声（测试）\"},{\"name\":\"asr.transcribe\",\"title\":\"语音识别\"},"
            "{\"name\":\"tts.speak\",\"title\":\"朗读\"},{\"name\":\"llm.ask\",\"title\":\"问大模型\"},"
            "{\"name\":\"voice.chat\",\"title\":\"语音对话\",\"form\":\"session\"},"
            "{\"name\":\"browser.session\",\"title\":\"浏览器（网页）\",\"form\":\"session\"}]}";
        cJSON *res = cJSON_CreateObject();
        cJSON_AddStringToObject(res, "jsonrpc", "2.0");
        cJSON_AddItemToObject(res, "id", cJSON_Duplicate(id, 1));
        char dir[768];
        snprintf(dir, sizeof dir, "%s%s", ASR.hide ? "{\"tools\":[" : DIR_ASR, DIR + strlen("{\"tools\":["));
        const cJSON *pr = cJSON_GetObjectItemCaseSensitive(req, "params");
        const cJSON *cursor_item = cJSON_GetObjectItemCaseSensitive(pr, "cursor");
        const char *cursor = cJSON_GetStringValue(cursor_item);
        int start = !cursor_item ? 0 : cursor && !strcmp(cursor, "sim:3") ? 3 : cursor && !strcmp(cursor, "sim:6") ? 6 : -1;
        cJSON *all = cJSON_Parse(dir);
        const cJSON *tools = cJSON_GetObjectItemCaseSensitive(all, "tools");
        int count = cJSON_GetArraySize(tools), end = start + 3 < count ? start + 3 : count;
        if (start < 0 || (start && start >= count)) {
            cJSON *error = cJSON_AddObjectToObject(res, "error");
            cJSON_AddNumberToObject(error, "code", -32602);
            cJSON_AddStringToObject(error, "message", "invalid directory cursor");
        } else {
            cJSON *result = cJSON_AddObjectToObject(res, "result");
            cJSON *page = cJSON_AddArrayToObject(result, "tools");
            for (int k = start; k < end; k++) cJSON_AddItemToArray(page, cJSON_Duplicate(cJSON_GetArrayItem(tools, k), 1));
            cJSON_AddNumberToObject(result, "rev", 1);
            if (end < count) cJSON_AddStringToObject(result, "nextCursor", end == 3 ? "sim:3" : "sim:6");
        }
        cJSON_Delete(all);
        send_json(ws, t + SIM_MS(10), res);
        cJSON_Delete(res);
        sim_metric_add("hub.caps_list", 1);
        cJSON_Delete(req);
        return;
    }
    if (id && !strncmp(method, "update/", 7)) { /* system upgrades: the package a test put there */
        update_request(ws, t, req, id, method);
        cJSON_Delete(req);
        return;
    }
    if (id && !strcmp(method, "rt.open")) { /* a realtime session (ink/sys/rt.h): the device chose its id */
        const cJSON *pr = cJSON_GetObjectItemCaseSensitive(req, "params");
        cJSON *res = cJSON_CreateObject();
        cJSON_AddStringToObject(res, "jsonrpc", "2.0");
        cJSON_AddItemToObject(res, "id", cJSON_Duplicate(id, 1));
        cJSON *r = cJSON_AddObjectToObject(res, "result");
        cJSON_AddNumberToObject(r, "sid", num_or(pr, "sid", 0));
        const char *cap = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(pr, "cap"));
        if (cap && !strcmp(cap, "asr.stream")) {
            sim_metric_add("hub.asr_opened", 1);
            char *pt = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(pr, "params"));
            snprintf(ASR.params, sizeof ASR.params, "%s", pt ? pt : "");
            sim_host_free(pt);
            if (ASR.refuse) {
                cJSON_DeleteItemFromObject(res, "result");
                cJSON *er = cJSON_AddObjectToObject(res, "error");
                cJSON_AddNumberToObject(er, "code", -32001);
                cJSON_AddStringToObject(er, "message", "not offered");
                send_json(ws, t + SIM_MS(5), res);
                cJSON_Delete(res);
                cJSON_Delete(req);
                return;
            }
            ASR.ws = ws;
            ASR.sid = (int)num_or(pr, "sid", 0);
            ASR.open = true;
            ASR.done = false;
            ASR.frames = ASR.seg = 0;
        }
        if (cap && !strcmp(cap, "echo")) echo_set(ws, (int)num_or(pr, "sid", 0), true);
        if (cap && !strcmp(cap, "browser.session")) {
            const cJSON *pp = cJSON_GetObjectItemCaseSensitive(pr, "params");
            sim_remote_open(ws, t, (int)num_or(pr, "sid", 0), (int)num_or(pp, "w", 684), (int)num_or(pp, "h", 1216));
        }
        send_json(ws, t + SIM_MS(5), res);
        cJSON_Delete(res);
        sim_metric_add("hub.rt_opened", 1);
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
                int m = member_slot(ws);
                snprintf(LS[i].member, sizeof LS[i].member, "%s", MC[m].member);
                LS[i].epoch = MC[m].epoch;
                snprintf(LS[i].lang, sizeof LS[i].lang, "%.7s", cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(pr, "lang")) ?
                         cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(pr, "lang")) : "");
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
        const cJSON *chunks = chunks_for(stream, cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(
            cJSON_GetObjectItemCaseSensitive(req, "params"), "lang"))), *c;
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
    cJSON *fetched = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(res, "result"), "at");
    if (!err && !strcmp(method, "weather/get") && cJSON_IsNumber(fetched) && fetched->valuedouble == 0) /* "at": 0 is now */
        cJSON_SetNumberValue(fetched, (double)(g_sim.wall_start_us / 1000000 + (int64_t)(at / 1000000000ull) + g_hub_clock_offset_s));
    if (!err && !strcmp(method, "hello")) /* the hub's clock (time service): right unless a script moved it */
        cJSON_AddNumberToObject(cJSON_GetObjectItemCaseSensitive(res, "result"), "now_ms",
                                (double)(g_sim.wall_start_us / 1000 + (int64_t)(at / 1000000ull) + g_hub_clock_offset_s * 1000));
    send_json(ws, at, res);
    cJSON_Delete(res);
    cJSON_Delete(req);
}

void sim_hub_clock_offset(int64_t seconds) { g_hub_clock_offset_s = seconds; }
