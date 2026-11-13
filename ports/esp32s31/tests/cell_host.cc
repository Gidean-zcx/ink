/* Test-only FreeRTOS, DMA and ESP-netif peers; the vendor source is compiled separately. */
#include "cell_idf.h"
#include "cell_fixture.h"
#include "uart_uhci.h"
#include "ink/osal.h"
#include "ink/hal_net.h"
#include "ink/hal_cellular.h"
#include "ink/hal_device.h"
extern "C" {
#include "metalio_plus/metalio_board.h"
}
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
esp_event_base_t IP_EVENT = "ip";
int GPIO;
using Clock = std::chrono::steady_clock;
static const auto epoch = Clock::now();
static std::atomic<int64_t> offset{2000000};
static constexpr unsigned scale = 1;
static std::mutex clock_mu;
static Clock::duration clock_skipped{};
static Clock::time_point clock_paused_at;
static unsigned clock_holds;
static thread_local bool tx_worker, tx_frame, tx_clock_held;
/* Host descheduling inside a TX transaction is not elapsed modem time. */
static void tx_clock_pause(bool hold) {
    std::lock_guard lock(clock_mu);
    if (hold) {
        if (clock_holds++ == 0) clock_paused_at = Clock::now();
    } else if (--clock_holds == 0) clock_skipped += Clock::now() - clock_paused_at;
}
static void tx_clock_end() {
    if (tx_clock_held) tx_clock_pause(false);
    tx_clock_held = false; tx_frame = false;
}
static auto duration(TickType_t ticks) { return std::chrono::microseconds(uint64_t(ticks) * 1000 / scale); }
struct ink_test_queue { std::mutex m; std::condition_variable cv; size_t size, cap; std::deque<std::vector<uint8_t>> data; };
struct ink_test_event { std::mutex m; std::condition_variable cv; EventBits_t bits = 0; };
struct ink_test_sem { std::mutex m; std::condition_variable cv; bool ready = false; };
struct osal_queue { QueueHandle_t queue; };
static std::vector<osal_queue_t*> osal_queues;
static std::mutex gpio_mu;
static std::condition_variable gpio_cv;
static std::mutex threads_mu, log_mu, event_mu, rx_mu, pool_mu;
static std::condition_variable pool_cv;
static std::vector<std::thread> threads;
static std::string logs;
static std::array<std::atomic<int>, F_CALL + 1> flags;
static std::array<std::atomic<unsigned>, N_DNS + 1> counts;
static std::atomic<int> mrdy{1}, srdy{1}, rate{2000000}, slot{0};
static std::atomic<bool> bh{false}, prompt{false};
static UartUhci *controller;
static void (*isr)(void*);
static void *isr_arg;
static std::array<std::array<uint8_t,1600>,4> memory;
static std::array<UartUhci::RxBuffer,4> buffers;
static std::array<bool,4> held;
struct Handler { esp_event_base_t base; int id; esp_event_handler_t fn; void *arg; };
static std::vector<Handler> handlers;
static esp_netif_t *netif;
static esp_netif_driver_ifconfig_t driver;
static void dispatch(esp_event_base_t base, int id, void *data) {
    std::vector<Handler> copy;
    { std::lock_guard lock(event_mu); copy = handlers; }
    for (auto h : copy) if (!strcmp(base,h.base) && (id == h.id || h.id == ESP_EVENT_ANY_ID)) h.fn(h.arg,base,id,data);
}
extern "C" int64_t esp_timer_get_time(void) {
    if (tx_frame && !tx_clock_held) { tx_clock_pause(true); tx_clock_held = true; }
    std::lock_guard lock(clock_mu);
    auto now = Clock::now();
    auto elapsed = now - epoch - clock_skipped - (clock_holds ? now - clock_paused_at : Clock::duration{});
    return std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()*scale+offset;
}
extern "C" uint64_t osal_now_us(void) { return (uint64_t)esp_timer_get_time(); }
extern "C" void osal_task_delay_us(uint64_t us) { std::this_thread::sleep_for(std::chrono::microseconds(us/scale)); }
extern "C" void vTaskDelay(TickType_t ticks) { std::this_thread::sleep_for(duration(ticks)); }
extern "C" TickType_t xTaskGetTickCount(void) { return (TickType_t)(esp_timer_get_time()/1000); }
extern "C" QueueHandle_t xQueueCreate(unsigned cap, unsigned size) { auto *q=new ink_test_queue; q->size=size; q->cap=cap; return q; }
extern "C" void vQueueDelete(QueueHandle_t q) { delete q; }
extern "C" BaseType_t xQueueSend(QueueHandle_t q, const void *p, TickType_t ticks) {
    std::unique_lock lock(q->m);
    if (!q->cv.wait_for(lock,duration(ticks),[&]{return q->data.size()<q->cap;})) return pdFALSE;
    q->data.emplace_back((const uint8_t*)p,(const uint8_t*)p+q->size); q->cv.notify_all(); return pdTRUE;
}
extern "C" BaseType_t xQueueReceive(QueueHandle_t q, void *p, TickType_t ticks) {
    if (tx_worker) tx_clock_end();
    std::unique_lock lock(q->m);
    if (ticks == portMAX_DELAY) q->cv.wait(lock,[&]{return !q->data.empty();});
    else {
        const int64_t deadline = esp_timer_get_time() + int64_t(ticks) * 1000;
        while (q->data.empty()) {
            int64_t left = deadline - esp_timer_get_time();
            if (left <= 0) return pdFALSE;
            q->cv.wait_for(lock, std::chrono::microseconds(left / scale));
        }
    }
    memcpy(p,q->data.front().data(),q->size); q->data.pop_front(); q->cv.notify_all();
    if (tx_worker) tx_frame = true;
    return pdTRUE;
}
extern "C" BaseType_t xQueueSendFromISR(QueueHandle_t q, const void *p, BaseType_t *wake) { *wake=pdFALSE; return xQueueSend(q,p,0); }
extern "C" EventGroupHandle_t xEventGroupCreate(void) { return new ink_test_event; }
extern "C" void vEventGroupDelete(EventGroupHandle_t e) { delete e; }
extern "C" EventBits_t xEventGroupSetBits(EventGroupHandle_t e, EventBits_t bits) {
    std::lock_guard lock(e->m); e->bits|=bits; e->cv.notify_all(); return e->bits;
}
extern "C" EventBits_t xEventGroupClearBits(EventGroupHandle_t e, EventBits_t bits) {
    std::lock_guard lock(e->m); auto before=e->bits; e->bits&=~bits; return before;
}
extern "C" EventBits_t xEventGroupWaitBits(EventGroupHandle_t e, EventBits_t bits, BaseType_t clear, BaseType_t all, TickType_t ticks) {
    std::unique_lock lock(e->m);
    auto ready=[&]{ return all ? (e->bits&bits)==bits : (e->bits&bits)!=0; };
    if (ticks==portMAX_DELAY) e->cv.wait(lock,ready); else e->cv.wait_for(lock,duration(ticks),ready);
    auto out=e->bits; if (clear && ready()) e->bits&=~bits; return out;
}
extern "C" BaseType_t xEventGroupSetBitsFromISR(EventGroupHandle_t e, EventBits_t bits, BaseType_t *wake) { *wake=pdFALSE; xEventGroupSetBits(e,bits); return pdTRUE; }
extern "C" SemaphoreHandle_t xSemaphoreCreateBinary(void) { return new ink_test_sem; }
extern "C" void vSemaphoreDelete(SemaphoreHandle_t s) { delete s; }
extern "C" BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { std::lock_guard lock(s->m); s->ready=true; s->cv.notify_all(); return pdTRUE; }
extern "C" BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks) {
    std::unique_lock lock(s->m); if (!s->cv.wait_for(lock,duration(ticks),[&]{return s->ready;})) return pdFALSE;
    s->ready=false; return pdTRUE;
}
extern "C" BaseType_t xTaskCreate(void (*fn)(void*), const char *name, unsigned stack, void *arg, unsigned prio, TaskHandle_t *out) {
    assert((!strcmp(name,"uart_eth_main") && stack==4096 && prio==10) ||
           (!strcmp(name,"uart_eth_tx") && stack==3072 && prio==9) || (!strcmp(name,"uart_eth_init") && stack==4096 && prio==4));
    counts[N_TASKS]++; *out=(void*)1;
    std::lock_guard lock(threads_mu);
    bool tx = !strcmp(name, "uart_eth_tx");
    threads.emplace_back([=]{ tx_worker = tx; fn(arg); tx_clock_end(); }); return pdPASS;
}
extern "C" void vTaskDelete(TaskHandle_t task) { assert(!task); }
extern "C" osal_queue_t *osal_queue_create(size_t size, size_t count) { auto *q=new osal_queue{xQueueCreate((unsigned)count,(unsigned)size)}; osal_queues.push_back(q); return q; }
extern "C" osal_err_t osal_queue_send(osal_queue_t *q, const void *p, int64_t us) { return xQueueSend(q->queue,p,(TickType_t)(us/1000)) ? OSAL_OK : OSAL_FULL; }
extern "C" osal_err_t osal_queue_recv(osal_queue_t *q, void *p, int64_t us) { return xQueueReceive(q->queue,p,(TickType_t)(us/1000)) ? OSAL_OK : OSAL_EMPTY; }
extern "C" void osal_panic(const char *,...) { abort(); }
extern "C" void hal_log_write(const char *s) { std::lock_guard lock(log_mu); logs+=s; logs+='\n'; }
extern "C" void ink_test_log(const char *fmt, ...) { char s[1024]; va_list ap; va_start(ap,fmt); vsnprintf(s,sizeof s,fmt,ap); va_end(ap); hal_log_write(s); }
extern "C" const char *esp_err_to_name(esp_err_t) { return "fixture"; }
extern "C" void *heap_caps_malloc(size_t size, unsigned) { return malloc(size); }
extern "C" void ink_s31_net_changed(void) { counts[N_CHANGED]++; }
extern "C" void ink_s31_bh_call(bool on) { bh=on; }
extern "C" bool ink_test_bh(void) { return bh; }
extern "C" uint64_t metalio_board_shared_power_since_us(void) { return 0; }
extern "C" hal_i2c_status_t metalio_board_set(int port, uint8_t mask, uint8_t value) {
    assert(port==1 && mask==(MB_P1_SHARED_RST|MB_P1_SHARED_PWR) && value==mask); return HAL_I2C_OK;
}
extern "C" esp_err_t gpio_config(const gpio_config_t *c) { assert(c->pin_bit_mask==(1ull<<60) || c->pin_bit_mask==(1ull<<3)); return ESP_OK; }
extern "C" esp_err_t gpio_set_level(int pin,int level) { assert(pin==60); std::lock_guard lock(gpio_mu); mrdy=level; gpio_cv.notify_all(); if (!level && !flags[F_NO_WAKE]) srdy=0; return ESP_OK; }
extern "C" int gpio_get_level(int pin) { assert(pin==3); return srdy; }
extern "C" esp_err_t gpio_install_isr_service(int flags_) { assert(!flags_); return ESP_OK; }
extern "C" esp_err_t gpio_isr_handler_add(int pin,void (*fn)(void*),void *arg) { assert(pin==3); isr=fn; isr_arg=arg; return ESP_OK; }
extern "C" esp_err_t gpio_isr_handler_remove(int) { isr=nullptr; return ESP_OK; }
extern "C" esp_err_t gpio_wakeup_enable(int pin,int level) { assert(pin==3 && level==GPIO_INTR_LOW_LEVEL); counts[N_WAKE_LEVEL]=(unsigned)level; return ESP_OK; }
#define GPIO_NOOP(name) extern "C" esp_err_t name(int) { return ESP_OK; }
GPIO_NOOP(gpio_wakeup_disable)
GPIO_NOOP(gpio_sleep_sel_dis)
GPIO_NOOP(gpio_sleep_sel_en)
GPIO_NOOP(gpio_intr_disable)
GPIO_NOOP(gpio_intr_enable)
GPIO_NOOP(gpio_reset_pin)
extern "C" esp_err_t gpio_set_pull_mode(int pin,int mode) { assert(pin==59 && mode==GPIO_PULLUP_ONLY); return ESP_OK; }
extern "C" esp_err_t gpio_set_intr_type(int,int) { return ESP_OK; }
extern "C" esp_err_t uart_param_config(int p,const uart_config_t *c) { assert(p==2 && c->baud_rate==2000000 && c->data_bits==8); rate=c->baud_rate; return ESP_OK; }
extern "C" esp_err_t uart_set_pin(int p,int tx,int rx,int,int) { assert(p==2 && ((tx==58 && rx==59) || (tx==-1 && rx==-1))); return ESP_OK; }
extern "C" esp_err_t uart_set_baudrate(int p,uint32_t baud) { assert(p==2 && (baud==2000000 || baud==3000000)); rate=(int)baud; return ESP_OK; }
extern "C" void esp_rom_delay_us(unsigned us) { assert(us==50 && mrdy==1); counts[N_ACKS]++; }
esp_err_t UartUhci::Init(const Config& c) {
    assert(c.uart_port==2 && c.dma_burst_size==32 && c.rx_pool.buffer_count==4 && c.rx_pool.buffer_size==1600);
    std::lock_guard lock(pool_mu); assert(!counts[N_HELD]); controller=this;
    for (unsigned i=0;i<4;i++) buffers[i]={memory[i].data(),1600,0,i};
    return ESP_OK;
}
void UartUhci::Deinit() { assert(!counts[N_HELD]); running=false; controller=nullptr; }
esp_err_t UartUhci::StartReceive() {
    /* The real driver remounts every buffer, so the consumer must have returned what it borrowed. The vendor's
     * modem task normally has; on a slow host (a CI runner) it may be a little late: wait for it, bounded, and
     * still fail on a buffer that is never returned. */
    { std::unique_lock lock(pool_mu); pool_cv.wait_for(lock, std::chrono::seconds(2), []{ return !counts[N_HELD]; }); }
    assert(!counts[N_HELD]); if (!running.exchange(true)) counts[N_STARTS]++; return ESP_OK;
}
esp_err_t UartUhci::StopReceive() { if (running.exchange(false)) counts[N_STOPS]++; return ESP_OK; }
void UartUhci::ReturnBuffer(RxBuffer *b) {
    std::lock_guard lock(pool_mu); assert(b->index<4 && held[b->index]); held[b->index]=false;
    counts[N_HELD]--; counts[N_RETURNS]++; pool_cv.notify_all();
}
static void deliver(const std::vector<uint8_t>& frame) {
    std::lock_guard serial(rx_mu);
    assert(controller && controller->IsReceiving());
    for (size_t at=0;at<frame.size();) {
        unsigned i=0;
        { std::unique_lock lock(pool_mu);
          bool free=pool_cv.wait_for(lock,std::chrono::seconds(2),[&]{for(i=0;i<4 && held[i];i++) {} return i<4;});
          assert(free); held[i]=true; counts[N_HELD]++; counts[N_LOANS]++; }
        size_t n=std::min({frame.size()-at,(size_t)std::max(1,flags[F_SPLIT].load()),(size_t)1600});
        memcpy(buffers[i].data,frame.data()+at,n); buffers[i].size=n; at+=n;
        controller->callback({&buffers[i],n},controller->context);
    }
}
static void inject(unsigned type,const void *data,size_t size) {
    std::vector<uint8_t> frame(size+4); frame[0]=(uint8_t)size; frame[1]=(uint8_t)(size>>8); frame[2]=(uint8_t)(type<<2);
    unsigned sum=frame[0]+frame[1]+frame[2]; frame[3]=(uint8_t)((sum>>8)^sum^3);
    if (flags[F_CORRUPT]) frame[3]^=0x80;
    memcpy(frame.data()+4,data,size);
    deliver(frame);
}
static void reply(const char *s) { inject(1,s,strlen(s)); }
esp_err_t UartUhci::Transmit(const uint8_t *p,size_t n) {
    { std::unique_lock lock(gpio_mu);
      /* The vendor TX task checks for the active state and then transmits; on a slow host (a CI runner) its
       * 500 ms idle timer can raise MRDY in between. The real link would lose that frame and the task would time
       * out its ACK; this double notes it and carries on instead of aborting the run on scheduling. */
      if (!gpio_cv.wait_for(lock,std::chrono::seconds(2),[]{return mrdy==0;}))
          fprintf(stderr,"cell_host: a frame went out with MRDY high (the host was slow)\n");
      assert(running && (mrdy!=0 || srdy==0 || flags[F_NO_WAKE]) && n>=4); counts[N_WRITES]++; }
    unsigned sum=p[0]+p[1]+p[2];
    if (p[3]!=(uint8_t)((sum>>8)^sum^3) || 4+p[0]+((p[1]&15u)<<8)!=n) { counts[N_BAD_HEADERS]++; return ESP_OK; }
    if (!flags[F_NO_ACK] && isr) { srdy=1; isr(isr_arg); srdy=0; }
    if (n==7 && ((p[2]>>2)&3)==1 && !memcmp(p+4,"AT\r",3)) counts[N_PROBES]++;
    if (flags[F_NO_RESPONSE] || rate!=flags[F_BAUD]) return ESP_OK;
    if (((p[2]>>2)&3)==0) {
        static const uint8_t greeting[]={0x53,0x50,0x49,0x43,2,4,12,0,0,0,0,0};
        if (n==16 && !memcmp(p+4,greeting,sizeof greeting)) {
            counts[N_HANDSHAKES]++; static const uint8_t ack[]={0x53,0x50,0x49,0x43,1,0x80};
            if (!flags[F_SILENT_GREETING]) inject(0,ack,sizeof ack);
        } else counts[N_TX]++;
        return ESP_OK;
    }
    if (prompt) {
        assert(p[n-1]==26); prompt=false;
        if (!flags[F_NO_PDU_FINAL]) reply("\r\n+CMGS: 1\r\nOK\r\n");
        return ESP_OK;
    }
    assert(p[n-1]=='\r'); std::string cmd((const char*)p+4,n-5);
    if (cmd.find("ECPCFG=")!=std::string::npos || cmd.find("ECNETCFG=")!=std::string::npos || cmd.find("XJCFG=")!=std::string::npos || cmd=="AT+ECRST" || cmd.find("ECSCLKEX=")!=std::string::npos || cmd=="AT+CEREG=2") counts[N_FORBIDDEN]++;
    if (cmd=="AT" && flags[F_DROP_PROBES]>0) { flags[F_DROP_PROBES]--; counts[N_DROPPED_PROBES]++; return ESP_OK; }
    if (cmd.find("AT+CMGS=")==0) { prompt=true; reply("\r\n> "); }
    else if (cmd=="AT+ECSIMCFG?") { char s[64]; snprintf(s,sizeof s,"\r\n+ECSIMCFG: \"SimSlot\",%d\r\nOK\r\n",slot.load()); reply(s); }
    else if (cmd.find("AT+ECSIMCFG=SimSlot,")==0) { slot=cmd.back()-'0'; reply("\r\nOK\r\n"); }
    else if (cmd=="AT+CEREG?") {
        if (flags[F_INTERLEAVE].exchange(0)) { reply("\r\n+ECNETDEVCTL: 0\r\n"); reply("\r\n+ECNETDEVCTL: 1\r\n"); }
        reply(flags[F_UNREGISTERED] ? "\r\n+CEREG: 0,2\r\nOK\r\n" : "\r\n+CEREG: 2,1,\"1A2B\",\"0C3D4E5F\",7\r\nOK\r\n");
    }
    else if (cmd=="AT+CPIN?") reply(flags[F_MISSING] ? "\r\n+CME ERROR: 10\r\n" : "\r\n+CPIN: READY\r\nOK\r\n");
    else if (cmd=="AT+CSQ") { char s[64]; snprintf(s,sizeof s,"\r\n+CSQ: %d,99\r\nOK\r\n",flags[F_CSQ].load()); reply(s); }
    else if (cmd=="AT+ECNETCFG?") {
        reply(flags[F_BAD_NAT] ? "\r\n+ECNETCFG: \"nat\",0\r\n" : "\r\n+ECNETCFG: \"nat\",1,\"192.0.2.2\",,\"255.255.255.0\",,\r\n");
        reply("+ECNETCFG: \"ipv6\",1\r\n+ECNETCFG: \"dns\",\"0.0.0.0\",\"0.0.0.0\"\r\n+ECNETCFG: \"mtu\",1500\r\n+ECNETCFG: \"lease\",86400\r\n"); reply("\r\nOK\r\n");
    }
    else if (cmd=="AT+ECNETDEVCTL?") reply(flags[F_STARTED] ? "\r\n+ECNETDEVCTL: 2,1,1,1\r\nOK\r\n" : "\r\n+ECNETDEVCTL: 2,1,1,0\r\nOK\r\n");
    else if (cmd=="AT+ECNETDEVCTL=2,1,1") { flags[F_STARTED]=1; reply("\r\nOK\r\n"); }
    else if (cmd=="AT+CGMR") reply("\r\nFAKE_R1 123456789012345\r\nOK\r\n");
    else if (cmd=="ATI") reply("\r\nATI\r\nFAKE-NT26\r\n+CMTI: \"SM\",7\r\nOK\r\n");
    else if (cmd.find("AT+CFUN=")==0) { counts[N_RADIO]++; reply("\r\nOK\r\n"); }
    else if (cmd.find("ATD")==0 && (flags[F_CALL]==1 || flags[F_CALL]==2)) reply(flags[F_CALL]==1 ? "\r\nBUSY\r\n" : "\r\nNO ANSWER\r\n");
    else if (cmd=="AT+CHUP" && flags[F_CALL]==3) reply("\r\nNO CARRIER\r\n");
    else if (cmd=="AT+CLCC" && (flags[F_CALL]==4 || flags[F_CALL]==5))
        reply(flags[F_CALL]==4 ? "\r\n+CLCC: 1,0,3,0,0,\"00000000000\",129\r\nOK\r\n" : "\r\n+CLCC: 1,0,0,0,0,\"00000000000\",129\r\nOK\r\n");
    else reply("\r\nOK\r\n");
    return ESP_OK;
}
extern "C" esp_err_t esp_read_mac(uint8_t *out,int type) { assert(type==ESP_MAC_ETH); static const uint8_t fake[]={0,0x11,0x22,0x33,0x44,0x54}; memcpy(out,fake,6); return ESP_OK; }
static const esp_netif_inherent_config_t eth_base=ESP_NETIF_INHERENT_DEFAULT_ETH();
const esp_netif_config_t ink_test_eth_config={&eth_base,nullptr,nullptr};
extern "C" esp_netif_t *esp_netif_new(const esp_netif_config_t *c) { assert(!(c->base->flags&ESP_NETIF_FLAG_GARP)); netif=new esp_netif_t{false}; return netif; }
extern "C" void esp_netif_destroy(esp_netif_t *n) { assert(n==netif); delete n; netif=nullptr; }
extern "C" esp_err_t esp_netif_set_mac(esp_netif_t *n,uint8_t *mac) { assert(n==netif && mac[0]==0 && mac[5]==0x55); return ESP_OK; }
extern "C" esp_err_t esp_netif_set_default_netif(esp_netif_t *n) { assert(n==netif); return ESP_OK; }
extern "C" esp_err_t esp_netif_set_driver_config(esp_netif_t *n,const esp_netif_driver_ifconfig_t *c) { assert(n==netif); driver=*c; return ESP_OK; }
extern "C" esp_err_t esp_netif_attach(esp_netif_t *n,void *p) { auto *base=(esp_netif_driver_base_t*)p; return base->post_attach(n,p); }
extern "C" esp_err_t esp_netif_dhcpc_stop(esp_netif_t *) { return ESP_OK; }
extern "C" void esp_netif_action_start(void *n,esp_event_base_t,int32_t,void *) { assert(n==netif && driver.handle && driver.transmit && driver.driver_free_rx_buffer); }
extern "C" void esp_netif_action_stop(void *n,esp_event_base_t,int32_t,void *) { ((esp_netif_t*)n)->connected=false; }
extern "C" void esp_netif_action_connected(void *n,esp_event_base_t,int32_t,void *) {
    ((esp_netif_t*)n)->connected=true;
    if (!flags[F_NO_DHCP]) { ip_event_got_ip_t ip={}; ip.esp_netif=(esp_netif_t*)n; dispatch(IP_EVENT,IP_EVENT_ETH_GOT_IP,&ip); }
}
extern "C" void esp_netif_action_disconnected(void *n,esp_event_base_t,int32_t,void *) { ((esp_netif_t*)n)->connected=false; }
extern "C" esp_err_t esp_netif_receive(esp_netif_t *n,void *p,size_t size,void *ctx) {
    assert(n==netif && !ctx && size==60); if (!flags[F_DROP_RX]) counts[N_RX]++;
    driver.driver_free_rx_buffer(driver.handle,p); return flags[F_DROP_RX] ? ESP_FAIL : ESP_OK;
}
extern "C" esp_err_t esp_event_post(esp_event_base_t base,int32_t id,const void *p,size_t,unsigned) { dispatch(base,id,const_cast<void*>(p)); return ESP_OK; }
extern "C" esp_err_t esp_event_handler_register(esp_event_base_t b,int32_t id,esp_event_handler_t fn,void *p) { std::lock_guard lock(event_mu); handlers.push_back({b,id,fn,p}); return ESP_OK; }
extern "C" esp_err_t esp_event_handler_unregister(esp_event_base_t b,int32_t id,esp_event_handler_t fn) { std::lock_guard lock(event_mu); std::erase_if(handlers,[&](auto h){return h.base==b && h.id==id && h.fn==fn;}); return ESP_OK; }
extern "C" esp_err_t esp_event_handler_instance_register(esp_event_base_t b,int32_t id,esp_event_handler_t fn,void *p,esp_event_handler_instance_t *out) { *out=p; return esp_event_handler_register(b,id,fn,p); }
extern "C" esp_err_t esp_event_handler_instance_unregister(esp_event_base_t b,int32_t id,esp_event_handler_instance_t) {
    std::lock_guard lock(event_mu); std::erase_if(handlers,[&](auto h){return h.base==b && h.id==id;}); return ESP_OK;
}
extern "C" void dns_clear_cache(void) { counts[N_DNS]++; }
extern "C" int tcpip_callback(void (*fn)(void*),void *p) { fn(p); return 0; }
extern "C" void ink_test_set(int key,int value) {
    flags[(size_t)key]=value;
    if (key==F_NO_WAKE) { srdy=value ? 1 : 0; if (!value && isr) isr(isr_arg); }
}
extern "C" unsigned ink_test_count(int key) {
    if (key==N_BAUD) return (unsigned)rate.load();
    if (key==N_SLOT) return (unsigned)slot.load();
    return counts[(size_t)key];
}
extern "C" void ink_test_reply(const char *s) { reply(s); }
extern "C" void ink_test_ethernet(void) { uint8_t data[60]={0}; inject(0,data,sizeof data); }
extern "C" void ink_test_ip(bool up) {
    auto deadline=Clock::now()+std::chrono::seconds(5);
    for (;;) {
        bool ready;
        { std::lock_guard lock(event_mu); ready=netif && std::any_of(handlers.begin(),handlers.end(),[](auto h){return h.base==IP_EVENT && h.id==ESP_EVENT_ANY_ID;}); }
        if (ready) break;
        if (Clock::now()>=deadline) { fprintf(stderr,"%s",ink_test_logs()); assert(false); }
        std::this_thread::yield();
    }
    ip_event_got_ip_t ip={}; ip.esp_netif=netif; dispatch(IP_EVENT,up ? IP_EVENT_ETH_GOT_IP : IP_EVENT_ETH_LOST_IP,&ip);
}
extern "C" void ink_test_advance(unsigned ms) { offset+=int64_t(ms)*1000; }
extern "C" void ink_test_drain(void) {
    auto deadline=Clock::now()+std::chrono::seconds(2);
    while (counts[N_HELD] && Clock::now()<deadline) std::this_thread::yield();
    assert(!counts[N_HELD]);
}
extern "C" const char *ink_test_logs(void) { static std::string copy; std::lock_guard lock(log_mu); copy=logs; return copy.c_str(); }
extern "C" void ink_test_wait_log(const char *s) {
    auto deadline=Clock::now()+std::chrono::seconds(10);
    for (;;) {
        { std::lock_guard lock(log_mu); if (logs.find(s)!=std::string::npos) return; }
        assert(Clock::now()<deadline); std::this_thread::yield();
    }
}
extern "C" void ink_test_finish(void) {
    std::lock_guard lock(threads_mu); for (auto &t:threads) t.join(); threads.clear();
    assert(!clock_holds);
    for (auto *q:osal_queues) { vQueueDelete(q->queue); delete q; } osal_queues.clear();
}

extern "C" void ink_test_wait_count(int key,unsigned min) {
    auto deadline=Clock::now()+std::chrono::seconds(5);
    while (ink_test_count(key)<min && Clock::now()<deadline) std::this_thread::yield();
    assert(ink_test_count(key)>=min);
}
extern "C" bool ink_test_transmit(void) {
    uint8_t data[60]={0}; return driver.transmit && driver.transmit(driver.handle,data,sizeof data)==ESP_OK;
}
extern "C" bool ink_test_gap_during_at(void) {
    unsigned before=counts[N_WRITES]; flags[F_NO_RESPONSE]=1;
    hal_at_result_t result=HAL_AT_TIMEOUT;
    std::thread caller([&]{ char out[128]; result=hal_modem_at("ATI",out,sizeof out,1000); });
    ink_test_wait_count(N_WRITES,before+1);
    reply("\r\nPARTIAL"); deliver({0,0});
    assert(controller->lost); controller->lost(controller->lost_context);
    caller.join(); flags[F_NO_RESPONSE]=0;
    return result==HAL_AT_ERROR;
}

extern "C" void ink_test_rebuild_status(void) {
    unsigned tasks=counts[N_TASKS]; bool ok=false;
    std::thread caller([&]{ hal_cell_status_t status; ok=hal_cell_status(&status); });
    ink_test_wait_count(N_TASKS,tasks+3);
    caller.join(); assert(ok);
}
