/* Synthetic enrollment faults, with real product services and persistent flash. */
#include "../services_finish/product.h"
#include "ink/hal_power.h"
#include "ink/sys/pairing.h"

#define CONFIG "{\"url\":\"wss://example.com/v1/device\",\"console\":\"https://example.com\",\"pin\":\"1111111111111111111111111111111111111111111111111111111111111111\"}"
#define TX "{\"txid\":\"test-tx-1\"}"
#define OFFER "{\"txid\":\"test-tx-1\",\"family\":\"测试家庭\",\"by\":\"测试大人\",\"name\":\"测试设备\"}"
static const char *phase;
static unsigned faults;
static int hello_error;
static bool wipe;
static const char *request_method, *request_params;
static volatile int request_dispatched;
static char ap_password[65];
static const char *web_json;
static volatile int barrier_done;
static ink_exec_job_t barrier_job;
static void barrier(void *arg) { (void)arg; barrier_done = 1; }
static void fault_event(void *arg) { (void)arg; sim_hub_pair_faults(faults, hello_error, wipe); }
static void request_event(void *arg) { (void)arg; sim_hub_request_device_raw(request_method, request_params); request_dispatched = 1; }
static void web_event(void *arg) { (void)arg; CHECK(sim_provision_submit(ap_password, web_json) > 0, "local page accepts an authenticated submission"); }
static void outage_event(void *arg) { (void)arg; sim_net_outage(SIM_MS(900)); }
static void wifi_event(void *arg) { (void)arg; sim_wifi_ap_add("test-wifi", -42, "test-password"); }
static void wifi_wrong(void *arg) { (void)arg; sim_wifi_ap_add("test-wifi", -42, "test-different-password"); }
static void expired_event(void *arg) { (void)arg; sim_hub_pair_expire(); }
static void set_fault(unsigned f, int err, bool erase) {
    faults = f; hello_error = err; wipe = erase;
    vtk_event_at(vtk_now() + SIM_MS(1), fault_event, NULL); delay_ms(10);
}
static void request(const char *method, const char *params) {
    request_method = method; request_params = params;
    request_dispatched = 0;
    vtk_event_at(vtk_now() + SIM_MS(1), request_event, NULL);
    CHECK(WAIT_FOR(request_dispatched, 1000), "pair request dispatched");
    if (!strncmp(method, "pair/", 5)) CHECK(WAIT_FOR(sim_hub_reply_matches_request(), 3000), "pair request answered");
}
static bool status_has(const char *value) {
    return AI("pairing.status", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, value);
}
static void wait_claim(void) {
    CHECK(WAIT_FOR(ink_pairing_state() == INK_PAIR_PAIRING, 18000), "waiting for claim code");
    CHECK(WAIT_FOR(status_has("K7QF2M9X"), 3000), "claim code is visible");
    const char *claim = !strcmp(phase, "revoked") ? "https://moved.example.com/claim/K7QF2M9X#s=EREREREREREREREREREREQ" : "https://example.com/claim/K7QF2M9X#s=EREREREREREREREREREREQ";
    CHECK(strstr(g_result, claim) && strstr(g_result, "CEIRCEIRCEIR"), "claim URL and verification code derive from observed peer key");
    CHECK(!strstr(g_result, "test-only-pending-token") && !strstr(g_result, "preauth") && !strstr(g_result, "password"), "public snapshot excludes credentials");
}
static void offer(void) {
    request("pair/offer", OFFER);
    CHECK(WAIT_FOR(status_has("测试家庭"), 2000), "offer is persisted and readable for future boot page");
}
static void checkpoint(void) {
    CHECK(ink_storage_pending() == 0 || WAIT_FOR(ink_storage_pending() == 0, 5000), "checkpoint durable");
    ink_probe_count("test.checkpoint");
    hal_power_reboot();
}
static void sentinel(void) {
    static const char value[] = "synthetic private data";
    CHECK(ink_storage_write_atomic("/user/m/owner/test.pairing", value, sizeof value) == 0 && WAIT_FOR(ink_storage_pending() == 0, 5000), "private flash fixture durable");
    CHECK(hal_fs_mkdir("/content/.m") == 0 && hal_fs_mkdir("/content/.m/owner") == 0, "personal card fixture directory");
    hal_file_t *f = hal_fs_open("/content/.m/owner/private.txt", HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    CHECK(f && hal_fs_write(f, value, sizeof value) == sizeof value, "personal card fixture");
    if (f) CHECK(hal_fs_close(f) == 0, "card fixture closed");
}
static bool data_present(void) { return ink_storage_size("/user/m/owner/test.pairing") > 0 && hal_fs_size("/content/.m/owner/private.txt") > 0; }
static bool wifi_failed(void) { char s[16]; return ink_state_get_str(ink_state_find("wifi.state"), s, sizeof s) && !strcmp(s, "failed"); }

static void run(void *arg) {
    (void)arg;
    if (!strcmp(phase, "network")) {
        set_fault(SIM_PAIR_DROP_BEGIN, 0, false);
        CHECK(AI("pairing.configure", CONFIG) && g_rc.status != INK_RC_DONE, "AI cannot provision identity");
        CHECK(BY_HAND("pairing.configure", "{\"url\":\"ws://example.com/v1/device\"}") && g_rc.status != INK_RC_DONE, "plaintext endpoint rejected");
        CHECK(BY_HAND("pairing.configure", CONFIG) && g_rc.status == INK_RC_DONE, "configuration durable");
        CHECK(WAIT_FOR(sim_metric_value("hub.requests.pair/begin") > 0, 12000), "begin sent before response is lost");
        CHECK(ink_pairing_state() == INK_PAIR_UNPAIRED && !ink_hub_online(), "no token, no hello or online state");
        const char *nonce = sim_hub_pair_nonce();
        CHECK(strlen(nonce) == 32 && ink_storage_write_atomic("/user/sys/test.nonce", nonce, 33) == 0, "nonce checkpoint");
        sentinel(); checkpoint(); return;
    }
    if (!strcmp(phase, "claim")) {
        wait_claim();
        CHECK(ink_hub_respond(424242, "{\"private\":\"synthetic old response\"}") == INK_HUB_ERR_OFFLINE, "ordinary responses cannot leave on a preauth connection");
        char nonce[33];
        CHECK(ink_storage_read("/user/sys/test.nonce", nonce, sizeof nonce) == sizeof nonce && !strcmp(nonce, sim_hub_pair_nonce()), "same nonce after lost begin and reboot");
        double old_begin = sim_metric_value("hub.requests.pair/begin");
        vtk_event_at(vtk_now() + SIM_MS(1), expired_event, NULL);
        CHECK(WAIT_FOR(sim_metric_value("hub.requests.pair/begin") > old_begin && strcmp(nonce, sim_hub_pair_nonce()), 12000), "expired code starts a new nonce without touching Wi-Fi");
        wait_claim();
        request("pair/complete", "{\"txid\":\"wrong\",\"device\":\"test-device\",\"token\":\"test-forged\"}");
        CHECK(sim_hub_device_reply() && strstr(sim_hub_device_reply(), "error") && ink_pairing_state() == INK_PAIR_PAIRING && !ink_hub_online(), "unsolicited completion rejected");
        int64_t volume = ink_setting_get("audio.volume");
        request("tools/call", "{\"name\":\"system.set_volume\",\"arguments\":{\"percent\":99}}");
        offer();
        CHECK(!ink_hub_online() && ink_setting_get("audio.volume") == volume, "unauthenticated tools do not run");
        request("pair/offer", "{\"txid\":\"test-tx-1\\u0000forged\",\"family\":\"测试家庭\",\"by\":\"测试大人\",\"name\":\"测试设备\"}");
        CHECK(sim_hub_device_reply() && strstr(sim_hub_device_reply(), "error"), "embedded NUL transaction rejected");
        CHECK(AI("pairing.accept", TX) && g_rc.status != INK_RC_DONE, "AI cannot confirm a family");
        CHECK(BY_HAND("pairing.accept", "{\"txid\":\"stale\"}") && g_rc.status != INK_RC_DONE, "stale transaction cannot confirm");
        double unconfirmed_begin = sim_metric_value("hub.requests.pair/begin");
        ink_hub_reconnect();
        CHECK(WAIT_FOR(sim_metric_value("hub.requests.pair/begin") > unconfirmed_begin, 12000), "reconnect while offer is unconfirmed");
        CHECK(!sim_hub_pair_txid()[0], "unconfirmed offer is not resumed as accepted");
        CHECK(BY_HAND("pairing.reject", TX) && g_rc.status == INK_RC_DONE, "person cancels offer");
        CHECK(WAIT_FOR(sim_metric_value("hub.requests.pair/reject") > 0, 3000), "cancel sent on preauth socket");
        vtk_event_at(vtk_now() + SIM_MS(1), outage_event, NULL);
        delay_ms(1100);
        double begins = sim_metric_value("hub.requests.pair/begin");
        ink_hub_reconnect();
        CHECK(WAIT_FOR(sim_metric_value("hub.requests.pair/begin") > begins, 18000), "pairing reconnects after claim-stage outage");
        wait_claim(); offer();
        set_fault(SIM_PAIR_DROP_ACCEPT, 0, false);
        CHECK(BY_HAND("pairing.accept", TX) && g_rc.status == INK_RC_DONE, "confirmation stored before send");
        CHECK(ink_pairing_state() == INK_PAIR_COMMITTING && sim_metric_value("pairing.token_durable") == 0, "accept loss keeps transaction without token");
        checkpoint(); return;
    }
    if (!strcmp(phase, "commit")) {
        CHECK(WAIT_FOR(sim_metric_value("pairing.token_durable") > 0, 15000), "missing complete recovered after restart");
        CHECK(!strcmp(sim_hub_pair_txid(), "test-tx-1") && ink_pairing_state() == INK_PAIR_COMMITTING, "resume carries committed transaction");
        CHECK(WAIT_FOR(sim_metric_value("hub.requests.hello") > 0, 8000), "saved token used despite missing completion acknowledgement");
        CHECK(sim_metric_value("hub.pair_ack_dropped") > 0, "completion acknowledgement explicitly lost at peer");
        checkpoint(); return;
    }
    if (!strcmp(phase, "wipe-string")) {
        CHECK(WAIT_FOR(ink_pairing_state() == INK_PAIR_BOUND && ink_hub_online(), 18000), "bound before malformed wipe field");
        ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
        const char *error = "{\"error\":{\"code\":-32011,\"data\":{\"wipe\":\"true\"}}}";
        CHECK(j && ink_json_parse(j, error, strlen(error)) == 0, "malformed wipe fixture");
        if (j) { ink_pairing_reply(true, INK_HUB_ERR_REMOTE, j, ink_json_find(j, 0, "error")); osal_free(j); }
        CHECK(WAIT_FOR(ink_pairing_state() == INK_PAIR_REVOKED, 5000), "malformed wipe still revokes");
        ink_exec_post(INK_EXEC_BG, &barrier_job, barrier, NULL);
        CHECK(WAIT_FOR(barrier_done, 5000) && data_present() && ink_storage_size("/user/sys/reset.pending") < 0, "string true cannot authorize irreversible wipe");
        ink_probe_count("test.finished"); return;
    }
    if (!strcmp(phase, "bound") || !strcmp(phase, "wipe")) {
        CHECK(WAIT_FOR(ink_pairing_state() == INK_PAIR_BOUND && ink_hub_online(), 18000), "persisted pending token reaches bound");
        CHECK(data_present(), "binding retains local data");
        CHECK(BY_HAND("pairing.configure", "{\"url\":\"wss://elsewhere.example.com/v1/device\",\"pin\":\"2222222222222222222222222222222222222222222222222222222222222222\"}") && g_rc.status != INK_RC_DONE, "new public key cannot steal bound identity");
        CHECK(BY_HAND("pairing.configure", "{\"url\":\"wss://moved.example.com/v1/device\",\"pin\":\"1111111111111111111111111111111111111111111111111111111111111111\"}") && g_rc.status == INK_RC_DONE, "same key can change the address without losing identity");
        char url[128]; unsigned char pin[32]; bool pinned;
        CHECK(ink_pairing_endpoint(url, pin, &pinned) && pinned && !strcmp(url, "wss://moved.example.com/v1/device"), "same-key move actually updates saved endpoint");
        CHECK(WAIT_FOR(ink_hub_online(), 15000), "same-key move reconnects");
        set_fault(0, -32010, false); ink_hub_reconnect();
        CHECK(WAIT_FOR(sim_metric_value("hub.pair_hello_failed") > 0, 12000), "bound invalid-token error observed");
        CHECK(ink_pairing_state() == INK_PAIR_BOUND && !ink_pairing_blocked() && data_present(), "bound auth failure keeps identity and data");
        set_fault(0, -32011, !strcmp(phase, "wipe")); ink_hub_reconnect();
        if (!strcmp(phase, "wipe")) { CHECK(WAIT_FOR(ink_pairing_state() == INK_PAIR_REVOKED, 12000), "wipe revocation persisted"); return; }
        CHECK(WAIT_FOR(ink_pairing_state() == INK_PAIR_REVOKED && ink_pairing_blocked(), 12000), "revocation persisted and reconnect stopped");
        CHECK(data_present(), "ordinary revocation preserves local data");
        checkpoint(); return;
    }
    if (!strcmp(phase, "revoked")) {
        CHECK(ink_pairing_state() == INK_PAIR_REVOKED && ink_pairing_blocked(), "revocation survives reboot");
        ink_hub_demand(); delay_ms(500);
        CHECK(sim_metric_value("hub.requests.hello") == 0 && sim_metric_value("hub.requests.pair/begin") == 0 && data_present(), "revoked device remains offline and local data survives");
        CHECK(BY_HAND("pairing.restart", NULL) && g_rc.status == INK_RC_DONE, "person returns to unpaired");
        wait_claim();
    } else if (!strcmp(phase, "wiped")) {
        CHECK(!ink_pairing_enabled() && ink_pairing_state() == INK_PAIR_UNPAIRED, "explicit wipe returns to unpaired");
        CHECK(!data_present() && ink_storage_size("/user/m/owner/test.pairing") < 0 && hal_fs_size("/content/.m/owner/private.txt") < 0, "explicit wipe clears private flash and card");
    } else if (!strcmp(phase, "expired") || !strcmp(phase, "invalid")) {
        CHECK(WAIT_FOR(sim_metric_value("hub.pair_hello_failed") > 0, 18000), "pending token rejection observed");
        CHECK(WAIT_FOR(ink_pairing_state() == INK_PAIR_PAIRING, 18000), "expired or invalid pending token is discarded and enrollment resumes");
        CHECK(data_present(), "pending rejection preserves local data");
    } else if (!strcmp(phase, "hotspot")) {
        vtk_event_at(vtk_now() + SIM_MS(1), wifi_event, NULL); delay_ms(10);
        CHECK(BY_HAND("pairing.hotspot_start", NULL) && g_rc.status == INK_RC_DONE, "hotspot started by person");
        const char *p = strstr(g_result, "P:");
        CHECK(p && strlen(p) > 18, "owner receives random hotspot password");
        if (p) { memcpy(ap_password, p + 2, 16); ap_password[16] = 0; }
        CHECK(sim_provision_page() && strstr(sim_provision_page(), "type=\"password\"") && !strstr(sim_provision_page(), "<script src="), "small local page has no external script");
        web_json = "{\"url\":\"wss://example.com/v1/device\",\"console\":\"https://example.com\",\"pin\":\"1111111111111111111111111111111111111111111111111111111111111111\",\"ssid\":\"test-wifi\",\"password\":\"test-password\"}";
        vtk_event_at(vtk_now() + SIM_MS(1), web_event, NULL);
        CHECK(WAIT_FOR(sim_provision_status() == 200 && !sim_provision_active(), 15000), "local page saves setup and closes hotspot");
        wait_claim();
        CHECK(sim_metric_value("hub.pair_secret_leaked") == 0, "Wi-Fi password never sent to hub");
        CHECK(AI("wifi.list", NULL) && strstr(g_result, "test-wifi") && !strstr(g_result, "test-password"), "Wi-Fi saved independently without exposing password");
    } else if (!strcmp(phase, "hotspot-reboot")) {
        CHECK(ink_pairing_enabled() && !sim_provision_active(), "identity configuration survives, setup hotspot stays off after reboot");
        CHECK(AI("wifi.list", NULL) && strstr(g_result, "test-wifi"), "Wi-Fi survives its own restart independently of device token");
    } else if (!strcmp(phase, "hotspot-handoff")) {
        CHECK(BY_HAND("pairing.hotspot_start", NULL) && g_rc.status == INK_RC_DONE, "adult starts setup session");
        const char *p = strstr(g_result, "P:");
        if (p) { memcpy(ap_password, p + 2, 16); ap_password[16] = 0; }
        CHECK(p && ink_member_switch("kid") == 0, "switch to child while hotspot remains open");
        web_json = CONFIG;
        vtk_event_at(vtk_now() + SIM_MS(1), web_event, NULL);
        CHECK(WAIT_FOR(sim_provision_status() == 403, 5000) && !ink_pairing_enabled(), "setup session cannot cross member boundary");
        CHECK(ink_member_switch("owner") == 0 && BY_HAND("pairing.hotspot_stop", NULL) && g_rc.status == INK_RC_DONE, "owner ends setup");
    } else if (!strcmp(phase, "card")) {
        CHECK(WAIT_FOR(hal_fs_size("/content/ink-pair.json") < 0, 5000), "one-use card file automatically imported and removed after durable save");
        CHECK(WAIT_FOR(ink_pairing_state() == INK_PAIR_BOUND, 18000), "card authorization completes without phone or device confirmation");
        CHECK(sim_metric_value("hub.pair_ack_received") == 1, "normal completion acknowledgement reaches peer before reconnect");
    } else if (!strcmp(phase, "card-wifi")) {
        CHECK(WAIT_FOR(wifi_failed(), 18000), "optional card Wi-Fi failure is explicit");
        CHECK(WAIT_FOR(ink_pairing_state() == INK_PAIR_BOUND, 18000), "household enrollment is independent of optional card Wi-Fi");
        CHECK(hal_fs_size("/content/ink-pair.json") > 0, "card stays until optional Wi-Fi is saved");
    } else if (!strcmp(phase, "card-wifi-retry")) {
        CHECK(WAIT_FOR(hal_fs_size("/content/ink-pair.json") < 0, 18000), "restart finishes optional Wi-Fi then deletes card file");
        CHECK(ink_pairing_state() == INK_PAIR_BOUND && sim_metric_value("hub.requests.pair/begin") == 0, "same-key card retry preserves household token");
        CHECK(AI("wifi.list", NULL) && strstr(g_result, "test-wifi"), "optional card Wi-Fi is durable");
    } else if (!strcmp(phase, "escaped-token")) {
        CHECK(BY_HAND("pairing.configure", CONFIG) && g_rc.status == INK_RC_DONE, "configure for escaped identity boundary");
        wait_claim(); offer(); set_fault(SIM_PAIR_DROP_COMPLETE, 0, false);
        CHECK(BY_HAND("pairing.accept", TX) && g_rc.status == INK_RC_DONE, "confirm escaped identity fixture");
        char token[128], params[1024];
        memcpy(token, "test-", 5); memset(token + 5, 1, 122); token[127] = 0;
        ink_jw_t w; ink_jw_init(&w, params, sizeof params);
        ink_jw_raw(&w, "{\"txid\":\"test-tx-1\",\"device\":\"test-device\",\"token\":");
        ink_jw_str(&w, token); ink_jw_raw(&w, "}");
        CHECK(!w.overflow, "max decoded token fixture fits incoming JSON budget");
        request("pair/complete", params);
        CHECK(WAIT_FOR(sim_metric_value("hub.requests.hello") > 0 && sim_metric_value("hub.pair_hello_failed") > 0, 15000), "escaped token within limits is sent to hello");
        CHECK(WAIT_FOR(ink_pairing_state() == INK_PAIR_PAIRING, 10000), "invalid escaped synthetic token returns to enrollment");
    } else if (!strcmp(phase, "pin")) {
        CHECK(BY_HAND("pairing.configure", "{\"url\":\"wss://example.com/v1/device\",\"pin\":\"2222222222222222222222222222222222222222222222222222222222222222\"}") && g_rc.status == INK_RC_DONE, "configured independent trust anchor");
        CHECK(WAIT_FOR(sim_metric_value("net.pin_rejected") > 0, 10000), "incorrect TLS public key rejected before pair begin");
        CHECK(sim_metric_value("hub.requests.pair/begin") == 0 && !ink_hub_online(), "wrong peer never receives pairing credentials");
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    phase = getenv("INK_PAIR_PHASE");
    if (!phase) phase = "network";
    if (!strcmp(phase, "commit")) sim_hub_pair_faults(SIM_PAIR_DROP_HELLO | SIM_PAIR_DROP_ACK, 0, false);
    if (!strcmp(phase, "expired")) sim_hub_pair_faults(0, -32013, false);
    if (!strcmp(phase, "invalid")) sim_hub_pair_faults(0, -32010, false);
    if (!strcmp(phase, "card-wifi")) vtk_event_at(vtk_now() + SIM_MS(1), wifi_wrong, NULL);
    if (!strcmp(phase, "card-wifi-retry")) vtk_event_at(vtk_now() + SIM_MS(1), wifi_event, NULL);
    product_main();
    osal_task_cfg_t cfg = {.name="pairing-test", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
