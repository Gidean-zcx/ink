/* Fake NT26 speaks only the vendor framed protocol; no device, IDs or paid traffic. */
#include <assert.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cell_idf.h"
#include "ink/hal_cellular.h"
static bool ink_bh_call;
void ink_s31_bh_call(bool on) { ink_bh_call = on; }
#ifndef INK_TEST_CELL_SOURCE
#define INK_TEST_CELL_SOURCE "../s31_modem.c"
#endif
#include INK_TEST_CELL_SOURCE

struct osal_mutex { bool held; };
struct osal_sem { unsigned count; };
struct osal_queue { size_t size, count, head, tail, capacity; uint8_t *items; };
static struct osal_mutex mutexes[3];
static struct osal_sem done;
static struct osal_queue queues[2];
static unsigned locks, nqueues;
static uint64_t now = 2000000;
static uint8_t rx[16384];
static size_t rx_head, rx_tail, split = 256;
enum { DMA_COUNT = 4, DMA_SIZE = 1600 };
static struct { uint8_t data[DMA_SIZE]; size_t len; bool held, queued; } dma[DMA_COUNT];
static unsigned dma_head, dma_tail, dma_queued, dma_held, loans, returns, starts, stops;
static bool receiving, overflow, recovered;
static int mrdy = 1, srdy = 1;
static bool pumping, no_wake, no_ack, no_response, prompt, missing, unregistered, corrupt_rx;
static uint32_t baud = 2000000, modem_baud = 2000000; /* this side's UART rate; the rate the modem listens at */
static int drop_probes; /* first device run: a just-woken modem missed frames */
static unsigned writes, bad_headers, handshakes, receives, acks, changed, data_tx, radio_commands;
static bool net_started, drop_rx;
static int csq = 23;
static esp_netif_t netif;
static esp_netif_driver_ifconfig_t net_driver;
static void (*srdy_isr)(void *);
static uint8_t ethernet[60];
static void tick(void) { assert(!pumping); pumping = true; ink_step(); ink_s31_cell_sync(); pumping = false; }
uint64_t osal_now_us(void) { return now; }
void osal_task_delay_us(uint64_t us) { now += us; if (!pumping && ink_cell.ready) tick(); }
void esp_rom_delay_us(unsigned us) { assert(us == 50 && mrdy == 1); now += us; acks++; }
osal_mutex_t *osal_mutex_create(void) { assert(locks < 3); return &mutexes[locks++]; }
osal_err_t osal_mutex_lock(osal_mutex_t *m, int64_t us) {
    if (m->held) { if (us > 0) now += (uint64_t)us; return OSAL_TIMEOUT; }
    m->held = true; return OSAL_OK;
}
void osal_mutex_unlock(osal_mutex_t *m) { assert(m->held); m->held = false; }
osal_sem_t *osal_sem_create(uint32_t init, uint32_t max) { assert(!init && max == 1); return &done; }
void osal_sem_give(osal_sem_t *s) { s->count = 1; }
osal_err_t osal_sem_take(osal_sem_t *s, int64_t us) {
    uint64_t end = now + (us > 0 ? (uint64_t)us : 0);
    while (!s->count && now < end) { tick(); now += 1000; }
    if (!s->count) return OSAL_TIMEOUT;
    s->count = 0; return OSAL_OK;
}
osal_queue_t *osal_queue_create(size_t size, size_t n) {
    assert(nqueues < 2); struct osal_queue *q = &queues[nqueues++];
    q->size = size; q->capacity = n; q->items = calloc(n, size); assert(q->items); return q;
}
osal_err_t osal_queue_send(osal_queue_t *q, const void *item, int64_t us) {
    (void)us; if (q->count == q->capacity) return OSAL_FULL;
    memcpy(q->items + q->tail * q->size, item, q->size); q->tail = (q->tail + 1) % q->capacity; q->count++; return OSAL_OK;
}
osal_err_t osal_queue_recv(osal_queue_t *q, void *item, int64_t us) {
    uint64_t end = now + (us > 0 ? (uint64_t)us : 0);
    while (!q->count && now < end) { tick(); now += 1000; }
    if (!q->count) return OSAL_EMPTY;
    memcpy(item, q->items + q->head * q->size, q->size); q->head = (q->head + 1) % q->capacity; q->count--; return OSAL_OK;
}
void *osal_malloc(size_t n, uint32_t mem) { assert(mem == OSAL_MEM_EXTERNAL); return malloc(n); }
void osal_free(void *p) { free(p); }
void osal_panic(const char *fmt, ...) { (void)fmt; abort(); }
static char logged[2048]; /* what the port wrote to the log, line after line */
void hal_log_write(const char *s) { size_t n = strlen(logged); snprintf(logged + n, sizeof logged - n, "%s\n", s); }
osal_err_t osal_task_create(const osal_task_cfg_t *cfg, osal_task_t **out) {
    (void)out; assert(cfg->fn == ink_io && cfg->stack_bytes == 4096 && cfg->flags == OSAL_TASK_STACK_EXTERNAL); return OSAL_OK;
}
void ink_s31_net_changed(void) { changed++; }
uint64_t metalio_board_shared_power_since_us(void) { return 0; }
hal_i2c_status_t metalio_board_set(int port, uint8_t mask, uint8_t value) {
    assert(port == 1 && mask == (MB_P1_SHARED_RST | MB_P1_SHARED_PWR) && mask == value); return HAL_I2C_OK;
}
esp_err_t gpio_config(const gpio_config_t *cfg) { assert(cfg->pin_bit_mask == (1ull << 60) || cfg->pin_bit_mask == (1ull << 3)); return ESP_OK; }
esp_err_t gpio_set_level(int pin, int level) { assert(pin == 60); mrdy = level; if (!level && !no_wake) srdy = 0; return ESP_OK; }
int gpio_get_level(int pin) { assert(pin == 3); return srdy; }
esp_err_t gpio_install_isr_service(int flags) { assert(!flags); return ESP_OK; }
esp_err_t gpio_isr_handler_add(int pin, void (*fn)(void *), void *arg) { assert(pin == 3 && !arg); srdy_isr = fn; return ESP_OK; }
esp_err_t gpio_pullup_en(int pin) { assert(pin == S31_UART2_RX); return ESP_OK; }
esp_err_t uart_driver_install(int p, int rb, int tb, int q, void *queue, int flags) {
    (void)p; (void)rb; (void)tb; (void)q; (void)queue; (void)flags; assert(!"UART driver must not be installed"); return ESP_FAIL;
}
esp_err_t uart_set_baudrate(int p, uint32_t rate) { assert(p == 2 && (rate == 2000000 || rate == 3000000)); baud = rate; return ESP_OK; }
esp_err_t uart_param_config(int p, const uart_config_t *c) { assert(p == 2 && c->baud_rate == 2000000 && c->data_bits == 8); return ESP_OK; }
esp_err_t uart_set_pin(int p, int tx, int r, int cts, int rts) { assert(p == 2 && tx == 58 && r == 59 && cts == -1 && rts == -1); return ESP_OK; }
static void inject(unsigned type, const void *data, size_t n) {
    if (rx_head == rx_tail) rx_head = rx_tail = 0;
    assert(rx_tail + n + 4 <= sizeof rx);
    uint8_t *p = rx + rx_tail;
    p[0] = n & 255; p[1] = (n >> 8) & 15; p[2] = (uint8_t)(type << 2);
    unsigned sum = p[0] + p[1] + p[2]; p[3] = (uint8_t)((sum >> 8) ^ sum ^ 3);
    if (corrupt_rx) p[3] ^= 0x80;
    memcpy(p + 4, data, n); rx_tail += n + 4;
}
static void reply(const char *s) { inject(1, s, strlen(s)); }
bool ink_s31_uhci_transmit(const uint8_t *data, size_t n) {
    assert(receiving && mrdy == 0 && (srdy == 0 || no_wake) && n >= 4) /* a modem that misses the wake gets the frame anyway (vendor) */; writes++;
    const uint8_t *p = data;
    unsigned sum = p[0] + p[1] + p[2];
    if (p[3] != (uint8_t)((sum >> 8) ^ sum ^ 3) || 4 + p[0] + ((p[1] & 15u) << 8) != n) { bad_headers++; return true; }
    if (!no_ack) { srdy = 1; srdy_isr(NULL); srdy = 0; }
    if (no_response || baud != modem_baud) return true; /* at the wrong rate the modem hears noise */
    if (((p[2] >> 2) & 3) == 0) {
        static const uint8_t handshake[] = {0x53, 0x50, 0x49, 0x43, 2, 4, 12, 0, 0, 0, 0, 0};
        if (n == 16 && !memcmp(p + 4, handshake, sizeof handshake)) {
            static const uint8_t ack[] = {0x53, 0x50, 0x49, 0x43, 1, 0x80};
            handshakes++; inject(0, ack, sizeof ack);
        } else { assert(n == sizeof ethernet + 4 && !memcmp(p + 4, ethernet, sizeof ethernet)); data_tx++; }
        return true;
    }
    if (prompt) { assert(p[n - 1] == 26); prompt = false; reply("\r\n+CMGS: 1\r\nOK\r\n"); return true; }
    char cmd[130]; assert(n - 4 < sizeof cmd && p[n - 1] == '\r');
    memcpy(cmd, p + 4, n - 5); cmd[n - 5] = 0;
    if (drop_probes > 0 && !strcmp(cmd, "AT")) { drop_probes--; return true; }
    if (!strcmp(cmd, "AT+CMGS=1")) { prompt = true; reply("\r\n> "); }
    else if (!strcmp(cmd, "AT+ECSIMCFG?")) reply("\r\n+ECSIMCFG: \"SimSlot\",0\r\nOK\r\n");
    else if (!strcmp(cmd, "AT+CEREG?")) reply(unregistered ? "\r\n+CEREG: 0,2\r\nOK\r\n" : "\r\n+CEREG: 0,1\r\nOK\r\n");
    else if (!strcmp(cmd, "AT+CPIN?")) reply(missing ? "\r\n+CME ERROR: 10\r\n" : "\r\n+CPIN: READY\r\nOK\r\n");
    else if (!strcmp(cmd, "AT+CSQ")) { char text[64]; snprintf(text, sizeof text, "\r\n+CSQ: %d,99\r\nOK\r\n", csq); reply(text); }
    else if (!strcmp(cmd, "AT+ECNETCFG?")) reply("\r\n+ECNETCFG: \"nat\",1\r\nOK\r\n");
    else if (!strcmp(cmd, "AT+ECNETDEVCTL?")) reply(net_started ? "\r\n+ECNETDEVCTL: 2,1,1,1\r\nOK\r\n" : "\r\n+ECNETDEVCTL: 2,1,1,0\r\nOK\r\n");
    else if (!strcmp(cmd, "AT+ECNETDEVCTL=2,1,1")) { net_started = true; reply("\r\nOK\r\n"); }
    else if (!strcmp(cmd, "AT+CGMR")) reply("\r\nFAKE_R1 123456789012345\r\nOK\r\n"); /* an IMEI-length run */
    else if (!strcmp(cmd, "ATI")) { reply("\r\nATI\r\nFAKE-NT26\r\n+CMTI: \"SM\",7\r\nOK\r\n"); }
    else if (!strncmp(cmd, "AT+CFUN=", 8)) { radio_commands++; reply("\r\nOK\r\n"); }
    else reply("\r\nOK\r\n");
    return true;
}
bool ink_s31_uhci_init(void) { return true; }
bool ink_s31_uhci_receiving(void) { return receiving; }
void ink_s31_uhci_flush(void) { assert(!receiving && !dma_held); rx_head = rx_tail = 0; }
bool ink_s31_uhci_active(bool active) {
    if (receiving != active) {
        if (active) { assert(!dma_held); starts++; }
        else stops++;
        receiving = active;
    }
    return true;
}
static void dma_fill(void) {
    assert(receiving && !overflow && rx_tail > rx_head);
    if (dma_held == DMA_COUNT) { overflow = true; return; }
    assert(!dma[dma_tail].held);
    size_t n = rx_tail - rx_head;
    if (n > split) n = split;
    if (n > DMA_SIZE) n = DMA_SIZE;
    memcpy(dma[dma_tail].data, rx + rx_head, n);
    dma[dma_tail].len = n;
    dma[dma_tail].held = dma[dma_tail].queued = true;
    rx_head += n;
    dma_tail = (dma_tail + 1) % DMA_COUNT;
    dma_held++; dma_queued++; loans++;
}
bool ink_s31_uhci_take(ink_s31_uhci_buffer_t *out) {
    if (!dma_queued && receiving && !overflow && rx_tail > rx_head) dma_fill();
    if (!dma_queued) return false;
    assert(dma[dma_head].held && dma[dma_head].queued);
    *out = (ink_s31_uhci_buffer_t){dma[dma_head].data, dma[dma_head].len, &dma[dma_head]};
    dma[dma_head].queued = false;
    dma_head = (dma_head + 1) % DMA_COUNT;
    dma_queued--;
    return true;
}
void ink_s31_uhci_return(void *token) {
    unsigned i;
    for (i = 0; i < DMA_COUNT && token != &dma[i]; i++) { }
    assert(i < DMA_COUNT && dma[i].held && !dma[i].queued);
    dma[i].held = false;
    dma_held--; returns++;
    if (overflow && !dma_held) { overflow = false; recovered = true; rx_head = rx_tail; }
}
bool ink_s31_uhci_recovered(void) {
    bool result = recovered;
    recovered = false;
    return result;
}
esp_err_t esp_read_mac(uint8_t *out, int type) { assert(type == ESP_MAC_ETH); memset(out, 0, 6); out[0] = 2; return ESP_OK; }
esp_netif_t *esp_netif_new(const esp_netif_config_t *cfg) {
    assert(!(cfg->base->flags & ESP_NETIF_FLAG_GARP) && cfg->base->route_prio == 10); net_driver = *cfg->driver; return &netif;
}
esp_err_t esp_netif_set_mac(esp_netif_t *n, uint8_t *mac) { assert(n == &netif && mac[0] == 2 && mac[5] == 1); return ESP_OK; }
esp_err_t esp_netif_set_default_netif(esp_netif_t *n) { assert(n == &netif); return ESP_OK; }
/* As esp_netif_config_sanity_check in the tcpip task: no driver handle, transmit or free -> ESP_ERROR_CHECK aborts. */
void esp_netif_action_start(void *n, esp_event_base_t b, int32_t id, void *p) { (void)b; (void)id; (void)p; assert(n == &netif);
    assert(net_driver.handle && net_driver.transmit && net_driver.driver_free_rx_buffer); }
void esp_netif_action_connected(void *n, esp_event_base_t b, int32_t id, void *p) { (void)b; (void)id; (void)p; ((esp_netif_t *)n)->connected = true; }
void esp_netif_action_disconnected(void *n, esp_event_base_t b, int32_t id, void *p) { (void)b; (void)id; (void)p; ((esp_netif_t *)n)->connected = false; }
esp_err_t esp_netif_receive(esp_netif_t *n, void *data, size_t len, void *ctx) {
    assert(n == &netif && !ctx && len == sizeof ethernet && !memcmp(data, ethernet, len));
    if (!drop_rx) receives++;
    net_driver.driver_free_rx_buffer(NULL, data); return drop_rx ? ESP_FAIL : ESP_OK;
}
esp_err_t esp_event_handler_register(esp_event_base_t b, int32_t id, esp_event_handler_t fn, void *p) { (void)b; (void)id; (void)fn; (void)p; return ESP_OK; }
static void drain(void) { for (unsigned i = 0; i < 200 && (rx_head < rx_tail || ink_cell.tx->count); i++) tick(); }
int main(void) {
    ink_s31_cell_init(); ink_s31_cell_netif_init(); assert(ink_cell.ready);
    char response[256];
    split = 1;
    drop_probes = 2; unsigned first = writes;
    assert(hal_modem_at("ATI", response, sizeof response, 1000) == HAL_AT_OK && !strcmp(response, "FAKE-NT26"));
    assert(!drop_probes && writes == first + 4 && ink_cell.synced && baud == 2000000 && strstr(logged, "answers AT at 2000000"));
    assert(hal_modem_urc(response, sizeof response, 0) > 0 && !strcmp(response, "+CMTI: \"SM\",7"));
    unsigned two_buffer_loans = loans;
    split = 5; /* The six-byte OK payload and its header span exactly two buffers. */
    assert(hal_modem_at("AT", response, sizeof response, 1000) == HAL_AT_OK && loans == two_buffer_loans + 2);
    split = 1;
    assert(hal_modem_at_payload("AT+CMGS=1", "00", response, sizeof response, 1000) == HAL_AT_OK && !strcmp(response, "+CMGS: 1"));
    assert(loans == returns && !dma_held && starts == 1);
    /* Overflow after four owned buffers, with both a partial header and AT line in flight. */
    split = 1;
    reply("\r\nRING\r\n");
    for (unsigned i = 0; i <= DMA_COUNT; i++) dma_fill();
    assert(overflow && dma_held == DMA_COUNT);
    ink_cell.line_n = 2;
    memcpy(ink_cell.line, "BA", 2);
    ink_cell.waiting = ink_cell.at_sent = true;
    ink_receive();
    assert(!overflow && !ink_cell.frame_n && !ink_cell.line_n && !ink_cell.waiting && ink_cell.desynced);
    assert(ink_cell.result == HAL_AT_ERROR && returns == loans && !dma_held);
    /* The first post-gap buffer contains discarded tail bytes, then an intact short EOF frame. */
    split = DMA_SIZE;
    rx_head = rx_tail = 0;
    memset(rx, 0xFF, 7); rx_tail = 7;
    reply("\r\n+CMTI: \"SM\",8\r\nOK\r\n");
    ink_receive();
    assert(!ink_cell.desynced && hal_modem_urc(response, sizeof response, 0) > 0 && !strcmp(response, "+CMTI: \"SM\",8"));
    assert(loans == returns && !dma_held);
    no_wake = true; srdy = 1; now += 500001; tick();
    assert(!receiving && stops == 1);
    no_wake = false; srdy = 0; tick();
    assert(receiving && starts == 2 && mrdy == 0);
    now += 500001; tick();
    assert(receiving && mrdy == 1);
    tick(); assert(receiving && mrdy == 1);
    split = 256;
    const char *forbidden[] = {"AT+ECSIMCFG=SimSlot,1", "AT+CGDCONT=1,\"IP\",\"example.com\"", "AT+CGAUTH=1,0", "AT+CMGD=7", "AT&W", "AT+ECRST", "AT+ECNETCFG=1", "AT+XJCFG=netPortBaudRate,1", "AT+IMS=1", "AT+CFUN=1;&W", "AT\r&W", "ATD1;&W", "AT+VTS=\"1\";&W"};
    unsigned before = writes;
    for (unsigned i = 0; i < sizeof forbidden / sizeof *forbidden; i++) assert(hal_modem_at(forbidden[i], response, sizeof response, 1000) == HAL_AT_ERROR);
    hal_cell_apn_t apn = {.name = "example.com"};
    assert(hal_cell_apn(&apn) < 0 && hal_cell_select_sim(HAL_CELL_SLOT_INTERNAL, 0) < 0);
    assert(hal_modem_at_payload("AT+CMGS=1", "00\032AT&W", response, sizeof response, 1000) == HAL_AT_ERROR);
    assert(writes == before);
    assert(hal_cell_dial("00000000000") == 0 && ink_bh_call);
    assert(hal_cell_answer() == 0 && ink_bh_call);
    assert(hal_cell_hangup() == 0 && !ink_bh_call && hal_cell_dtmf('5') == 0);
    hal_cell_event_t call_ev;
    hal_cell_wait_event(&call_ev, 0); /* consume the scheduled registration refresh */
    ink_bh_call = true;
    reply("\r\nNO CARRIER\r\n");
    drain();
    assert(hal_cell_wait_event(&call_ev, 0) == 1 && call_ev.type == HAL_CELL_EV_CALL_ENDED && !ink_bh_call);
    inject(0, ethernet, sizeof ethernet); drain(); assert(!receives && !ink_cell.handshake);
    ink_s31_cell_use(true);
    hal_cell_status_t st;
    assert(hal_cell_status(&st) && st.registered && st.signal_dbm == -67 && st.config_readonly && !st.sim_missing);
    /* the once-per-boot modem summary: read-only queries, long digit runs masked */
    assert(strstr(logged, "cell: AT+CGMR -> OK") && strstr(logged, "FAKE_R1 ###############") && !strstr(logged, "123456789012345"));
    assert(strstr(logged, "cell: AT+CIREG? -> OK") && !strstr(logged, "cell: ATI"));
    size_t summary = strlen(logged); assert(hal_cell_status(&st) && strlen(logged) == summary); /* once */
    assert(handshakes == 1 && ink_cell.handshake && netif.connected);
    ip_event_got_ip_t ip = {.esp_netif = &netif}; ink_ip(NULL, IP_EVENT, IP_EVENT_ETH_GOT_IP, &ip);
    hal_link_status_t link; assert(ink_s31_cell_link(&link) && link.up && link.signal_dbm == -67);
    assert(ink_s31_cell_available()); ink_s31_cell_use(true);
    assert(net_driver.transmit(NULL, ethernet, sizeof ethernet) == ESP_OK); drain(); assert(data_tx == 1);
    inject(0, ethernet, sizeof ethernet); drain(); assert(receives == 1);
    unsigned ack_before = acks;
    corrupt_rx = true; inject(0, ethernet, sizeof ethernet); corrupt_rx = false; drain(); assert(receives == 1 && acks == ack_before);
    uint64_t in, out; assert(hal_net_lte_bytes(&in, &out) && in == sizeof ethernet && out == sizeof ethernet);
    drop_rx = true; inject(0, ethernet, sizeof ethernet); drain(); drop_rx = false;
    assert(receives == 1 && hal_net_lte_bytes(&in, &out) && in == sizeof ethernet);
    reply("\r\nRING\r\n+CLIP: \"00000000000\",129\r\nNO CARRIER\r\n"); drain();
    assert(hal_modem_urc(response, sizeof response, 0) > 0 && !strcmp(response, "RING"));
    assert(hal_modem_urc(response, sizeof response, 0) > 0 && strstr(response, "+CLIP:"));
    assert(hal_modem_urc(response, sizeof response, 0) > 0 && !strcmp(response, "NO CARRIER"));
    before = writes; no_ack = true;
    assert(hal_modem_at("AT", response, sizeof response, 1000) == HAL_AT_OK && writes == before + 1); no_ack = false;
    no_wake = true; srdy = 1; before = writes;
    /* as the vendor (HandleIdleTimeout): after the 200 ms wake window the frame goes anyway */
    assert(hal_modem_at("AT", response, sizeof response, 1000) == HAL_AT_OK && writes == before + 1 && ink_cell.forced >= 1);
    no_wake = false; srdy = 0;
    assert(hal_modem_at("AT", response, sizeof response, 1000) == HAL_AT_OK);
    reply("\r\nECRDY\r\n"); drain();
    assert(!ink_cell.handshake && !ink_cell.ip && !ink_s31_cell_available()); net_started = false;
    assert(hal_cell_status(&st) && ink_cell.handshake && handshakes == 2);
    no_response = true; before = writes;
    assert(hal_modem_at("ATI", response, sizeof response, 200) == HAL_AT_TIMEOUT && writes == before + 1); no_response = false;
    assert(hal_modem_at("AT", response, sizeof response, 200) == HAL_AT_NO_MODEM);
    reply("\r\nOK\r\n"); drain(); assert(hal_modem_at("AT", response, sizeof response, 1000) == HAL_AT_OK);
    assert(ink_s31_cell_suspend()); before = writes; tick(); assert(writes == before);
    assert(hal_modem_at("AT", response, sizeof response, 200) == HAL_AT_NO_MODEM);
    ink_s31_cell_resume();
    assert(!receiving && ink_cell.ready_after == now + 1000000);
    now = ink_cell.ready_after;
    assert(!ink_cell.synced); modem_baud = 3000000; before = writes;
    assert(hal_modem_at("AT", response, sizeof response, 1000) == HAL_AT_OK && baud == 3000000 && writes == before + 10); /* 8 at 2, 1 at 3, then the command */
    assert(strstr(logged, "answers AT at 3000000") && !strstr(logged, "XJCFG"));
    /* a sent command never answered desyncs; past 30 s with no late reply a probe brings the channel back */
    no_response = true; assert(hal_modem_at("ATI", response, sizeof response, 200) == HAL_AT_TIMEOUT && ink_cell.desynced); no_response = false;
    assert(hal_modem_at("AT", response, sizeof response, 200) == HAL_AT_NO_MODEM);
    now += 30000001; assert(hal_modem_at("AT", response, sizeof response, 1000) == HAL_AT_OK && !ink_cell.desynced);
    net_started = false; assert(hal_cell_status(&st) && handshakes == 3);
    unregistered = missing = true; csq = 99;
    assert(hal_cell_status(&st) && !st.registered && st.sim_missing && st.signal_dbm == 0 && !ink_s31_cell_available());
    assert(!ink_cell.ip && !ink_cell.handshake);
    assert(!bad_headers && changed && radio_commands == 0);
    drain(); assert(loans == returns && !dma_held);
    for (unsigned i = 0; i < nqueues; i++) free(queues[i].items);
    puts("cell: PASS handshake, AT/payload, URC, timeout, Ethernet, reset, storage guard");
    return 0;
}
