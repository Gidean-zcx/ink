/* Adapted board lifecycle, CloudZao 874538e; MIT, see LICENSE-MIT and SOURCE.md. */
#pragma once
#include "uart_eth_modem.h"
#include <chrono>
#include <algorithm>
#include "esp_timer.h"
enum class NetworkEvent {
    Scanning, Connecting, Connected, Disconnected, WifiConfigModeEnter, WifiConfigModeExit,
    ModemDetecting, ModemErrorNoSim, ModemErrorRegDenied, ModemErrorInitFailed, ModemErrorTimeout
};
class Nt26Board {
public:
    // shared_ptr：SendAt 与 TearDown 并发时延长 lifetime，避免 Stop 后 UAF
    std::shared_ptr<UartEthModem> modem_;
    gpio_num_t tx_pin_;
    gpio_num_t rx_pin_;
    gpio_num_t dtr_pin_; // mrdy_pin
    gpio_num_t ri_pin_;  // srdy_pin


    EventGroupHandle_t network_wait_event_ = nullptr;

    // 生命周期：PreparePowerOff / StartOrRestart / Ensure 互斥
    mutable std::mutex lifecycle_mu_;
    /** 串行化 StartOrRestart 整段（含 wait），避免 Ensure 与上升沿恢复互相 TearDown */
    std::timed_mutex start_mu_;
    std::atomic<bool> retry_needed_{false};
    std::atomic<int64_t> last_start_us_{0};
    std::mutex modem_operation_mu_;
    std::atomic<bool> powered_{true};
    std::atomic<int64_t> power_restored_us_{0};
    std::atomic<int> cached_csq_{99};
    std::atomic<uint32_t> lifecycle_epoch_{0};
    std::atomic<bool> network_ready_{false};
    std::atomic<bool> network_busy_{false};

    Nt26Board(gpio_num_t tx, gpio_num_t rx, gpio_num_t mrdy, gpio_num_t srdy)
        : tx_pin_(tx), rx_pin_(rx), dtr_pin_(mrdy), ri_pin_(srdy) {}
    std::atomic<uint64_t> rx_total_{0}, tx_total_{0};
    std::atomic<bool> selected_{false}, config_readonly_{false};
    void OnNetworkEvent(NetworkEvent event, const std::string& data = "", uint32_t expected_epoch = 0);
    void SignalWaitBits(uint32_t bits);
    void TearDownModem();
    void PreparePowerOff();
    bool StartOrRestartModem(int timeout_ms);
    bool IsNetworkReady();
    bool EnsureNetworkReady(int timeout_ms);
    void OnPowerRestored();
};
