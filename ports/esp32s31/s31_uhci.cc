/* C HAL bridge to the copied modem and board lifecycle. */
#include "s31_cell.h"
#include "s31_uhci.h"
#include "nt26_lifecycle.h"
#include <cstring>
extern "C" {
#include "metalio_plus/metalio_board.h"
#include "s31_board.h"
#include "ink/osal.h"
}
static Nt26Board ink_cell(static_cast<gpio_num_t>(S31_UART2_TX), static_cast<gpio_num_t>(S31_UART2_RX),
    GPIO_NUM_60, static_cast<gpio_num_t>(S31_GPIO_MODEM_SRDY));
static std::atomic<bool> ink_ready{false}, ink_registered{false};
static std::atomic<int> ink_signal{0};
static std::shared_ptr<UartEthModem> ink_modem(void) {
    std::lock_guard<std::mutex> lock(ink_cell.lifecycle_mu_);
    return ink_cell.modem_;
}
void ink_s31_cell_init(void) {
    ink_s31_urc_init();
    if (metalio_board_set(1, MB_P1_SHARED_RST | MB_P1_SHARED_PWR, MB_P1_SHARED_RST | MB_P1_SHARED_PWR) != HAL_I2C_OK) return;
    const esp_err_t rc = gpio_install_isr_service(0);
    if (rc != ESP_OK && rc != ESP_ERR_INVALID_STATE) return;
    ink_cell.power_restored_us_ = metalio_board_shared_power_since_us();
    ink_ready = true;
}
void ink_s31_cell_netif_init(void) { /* Created by the vendor's InitIotEth. */ }
static hal_at_result_t ink_at(const char *cmd, const char *payload, char *out, size_t cap, uint32_t ms) {
    if (out && cap) *out = 0;
    if (!ink_s31_at_allowed(cmd) || (payload && (strncmp(cmd, "AT+CMGS=", 8) || strlen(payload) > 1024 ||
        strspn(payload, "0123456789ABCDEFabcdef") != strlen(payload)))) return HAL_AT_ERROR;
    if (!ink_ready || !ink_cell.powered_) return HAL_AT_NO_MODEM;
    auto modem = ink_modem();
    if (!modem || (modem->Stopped() && ink_cell.retry_needed_ && esp_timer_get_time() - ink_cell.last_start_us_ >= 5000000)) {
        ink_cell.StartOrRestartModem((int)ms); modem = ink_modem();
    }
    if (!modem || !modem->WaitAtReady(10000)) return HAL_AT_NO_MODEM;
    std::string response;
    esp_err_t rc = modem->SendAt(cmd, response, ms, payload);
    size_t begin = response.find_first_not_of("\r\n "), end = response.size();
    if (end >= 2 && response.compare(end - 2, 2, "OK") == 0) end -= 2;
    while (end && (response[end - 1] == '\r' || response[end - 1] == '\n')) --end;
    size_t n = begin != std::string::npos && end > begin ? end - begin : 0;
    if (out && cap) {
        if (n >= cap) return HAL_AT_ERROR;
        memcpy(out, n ? response.data() + begin : "", n); out[n] = 0;
    }
    return rc == ESP_OK ? HAL_AT_OK : rc == ESP_ERR_TIMEOUT ? HAL_AT_TIMEOUT :
           rc == ESP_ERR_INVALID_STATE ? HAL_AT_NO_MODEM : HAL_AT_ERROR;
}
extern "C" hal_at_result_t hal_modem_at(const char *cmd, char *out, size_t cap, uint32_t ms) { return ink_at(cmd, nullptr, out, cap, ms); }
extern "C" hal_at_result_t hal_modem_at_payload(const char *cmd, const char *body, char *out, size_t cap, uint32_t ms) {
    return body ? ink_at(cmd, body, out, cap, ms) : HAL_AT_ERROR;
}
void ink_s31_cell_use(bool on) {
    bool changed = ink_cell.selected_.exchange(on) != on;
    if (auto modem = ink_modem()) modem->SelectData(on);
    if (changed && on) ink_cell.retry_needed_ = true;
    ink_s31_net_changed();
}
void ink_s31_cell_sync(void) { /* iot_eth owns link events. */ }
void ink_s31_cell_start_data(void) {
    if (!ink_ready || !ink_cell.powered_) return;
    if (ink_cell.selected_ && ink_registered) {
        auto modem = ink_modem();
        if (modem && modem->Netif() && !modem->HasIp() && esp_timer_get_time() - ink_cell.last_start_us_ >= 60000000)
            ink_cell.retry_needed_ = true;
        ink_cell.EnsureNetworkReady(60000);
    }
}
bool ink_s31_cell_available(void) { return ink_ready && ink_registered && ink_cell.powered_; }
bool ink_s31_cell_link(hal_link_status_t *out) {
    auto modem = ink_modem();
    *out = {};
    out->type = HAL_LINK_LTE; out->signal_dbm = ink_signal;
    out->up = modem && modem->HasIp() && modem->HasHandshake() && ink_registered && ink_cell.selected_ && ink_cell.powered_;
    return ink_ready;
}
extern "C" bool hal_net_lte_bytes(uint64_t *rx, uint64_t *tx) {
    auto modem = ink_modem();
    *rx = ink_cell.rx_total_.load() + (modem ? modem->RxBytes() : 0); *tx = ink_cell.tx_total_.load() + (modem ? modem->TxBytes() : 0);
    return ink_ready;
}
void ink_s31_cell_update(bool registered, int signal_dbm) {
    bool was_registered = ink_registered.exchange(registered); ink_signal = signal_dbm;
    if (auto modem = ink_modem()) modem->SelectData(registered && ink_cell.selected_);
    if (registered && !was_registered && ink_cell.selected_) ink_cell.retry_needed_ = true;
    ink_s31_net_changed();
}
bool ink_s31_cell_suspend(void) {
    ink_cell.PreparePowerOff();
    ink_registered = false; ink_signal = 0;
    return true;
}
void ink_s31_cell_resume(void) { ink_cell.OnPowerRestored(); ink_cell.config_readonly_ = false; }
