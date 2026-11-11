/* CloudZao 874538e board lifecycle; MIT, see LICENSE-MIT and SOURCE.md. */
#include "nt26_lifecycle.h"
#include "s31_cell.h"
#include "ink_modem_log.h"
#define TAG "Nt26Board"
namespace {
constexpr uint32_t kWaitNetworkConnected = (1 << 0);
constexpr uint32_t kWaitNetworkFailed = (1 << 1);
constexpr uint32_t kWaitNetworkAll = kWaitNetworkConnected | kWaitNetworkFailed;
constexpr uint32_t kCellularBootSettleMs = 1000;
}
void Nt26Board::OnNetworkEvent(NetworkEvent event, const std::string& data, uint32_t expected_epoch) {
    // Only modem callbacks take this lock; synchronous startup calls already hold it.
    std::unique_lock<std::mutex> epoch_lock(lifecycle_mu_, std::defer_lock);
    if (expected_epoch != 0) {
        epoch_lock.lock();
        if (!powered_.load() || expected_epoch != lifecycle_epoch_.load()) return;
    }
    network_busy_.store(event == NetworkEvent::ModemDetecting || event == NetworkEvent::Connecting);
    if (event == NetworkEvent::Connected) {
        network_ready_.store(true, std::memory_order_release);
    } else if (event == NetworkEvent::Disconnected ||
               event == NetworkEvent::ModemErrorTimeout ||
               event == NetworkEvent::ModemErrorNoSim ||
               event == NetworkEvent::ModemErrorRegDenied ||
               event == NetworkEvent::ModemErrorInitFailed) {
        network_ready_.store(false, std::memory_order_release);
    }
    if (epoch_lock.owns_lock()) epoch_lock.unlock();
    ink_s31_net_changed();
}


void Nt26Board::SignalWaitBits(uint32_t bits) {
    std::lock_guard<std::mutex> lock(lifecycle_mu_);
    if (network_wait_event_) xEventGroupSetBits(network_wait_event_, bits);
}


void Nt26Board::TearDownModem() {
    std::shared_ptr<UartEthModem> doomed;
    {
        std::lock_guard<std::mutex> lock(lifecycle_mu_);
        network_ready_.store(false, std::memory_order_release);
        network_busy_.store(false, std::memory_order_release);
        cached_csq_.store(99, std::memory_order_relaxed);
        lifecycle_epoch_.fetch_add(1, std::memory_order_acq_rel);
        // 唤醒 WaitBits；event group 仍由等待方释放，避免 UAF
        if (network_wait_event_) {
            xEventGroupSetBits(network_wait_event_, kWaitNetworkFailed);
        }
        doomed = std::move(modem_);
    }
    // Stop 在锁外：汇合 modem 任务时回调只 SignalWaitBits，不会与本锁死锁
    if (doomed) {
        ESP_LOGI(TAG, "TearDown modem (P14 / restart)");
        std::lock_guard<std::mutex> operation_lock(modem_operation_mu_);
        doomed->Stop();
        rx_total_ += doomed->RxBytes(); tx_total_ += doomed->TxBytes();
    }
}


void Nt26Board::PreparePowerOff() {
    ESP_LOGI(TAG, "PreparePowerOff: stop cellular before P14 down");
    powered_.store(false, std::memory_order_release);
    TearDownModem();
    ink_s31_net_changed();
}


bool Nt26Board::StartOrRestartModem(int timeout_ms) {
    // 整段单飞：EnsureNetworkReady 与 P14 上升沿恢复不得并行 TearDown 彼此
    const int budget_ms = timeout_ms > 0 ? timeout_ms : 1;
    const int64_t deadline_us = esp_timer_get_time() + int64_t(budget_ms) * 1000;
    std::unique_lock<std::timed_mutex> start_lock(start_mu_, std::defer_lock);
    if (!start_lock.try_lock_for(std::chrono::milliseconds(budget_ms))) return IsNetworkReady();

    if (!powered_.load(std::memory_order_acquire)) return false;
    const int64_t settle_us = int64_t(kCellularBootSettleMs) * 1000 - (esp_timer_get_time() - power_restored_us_.load());
    if (settle_us > 0) {
        if (timeout_ms <= 0) return false;
        const int64_t available = deadline_us - esp_timer_get_time();
        if (available <= 0) return false;
        vTaskDelay(pdMS_TO_TICKS((std::min(settle_us, available) + 999) / 1000));
        if (esp_timer_get_time() >= deadline_us) return false;
    }
    if (!powered_.load(std::memory_order_acquire)) return false;
    if (IsNetworkReady()) {
        return true;
    }

    bool existing = false;
    {
        std::lock_guard<std::mutex> lock(lifecycle_mu_);
        existing = modem_ != nullptr;
    }
    if (existing && (!retry_needed_.load() || esp_timer_get_time() - last_start_us_.load() < 5000000)) {
        while (powered_.load() && !IsNetworkReady() && !retry_needed_.load() && esp_timer_get_time() < deadline_us)
            vTaskDelay(pdMS_TO_TICKS(10));
        return IsNetworkReady();
    }
    TearDownModem();
    retry_needed_.store(false);
    last_start_us_.store(esp_timer_get_time());

    EventGroupHandle_t wait_eg = nullptr;
    uint32_t my_epoch = 0;
    std::shared_ptr<UartEthModem> started;

    {
        std::lock_guard<std::mutex> lock(lifecycle_mu_);

        if (network_ready_.load(std::memory_order_acquire) && modem_ &&
            modem_->IsInitialized()) {
            return true;
        }

        if (!powered_.load(std::memory_order_acquire)) return false;
        my_epoch = lifecycle_epoch_.load(std::memory_order_acquire);
        OnNetworkEvent(NetworkEvent::ModemDetecting);

        wait_eg = xEventGroupCreate();
        if (!wait_eg) {
            ESP_LOGE(TAG, "Failed to create network wait event group");
            OnNetworkEvent(NetworkEvent::ModemErrorInitFailed);
            return false;
        }
        network_wait_event_ = wait_eg;

        UartEthModem::Config config = {
            .uart_num = UART_NUM_2,
            .baud_rate = 2000000,
            .tx_pin = tx_pin_,
            .rx_pin = rx_pin_,
            .mrdy_pin = dtr_pin_,
            .srdy_pin = ri_pin_
        };

        modem_ = std::make_shared<UartEthModem>(config);
        modem_->SelectData(selected_);
        modem_->SetNetworkEventCallback([this, my_epoch](UartEthModem::UartEthModemEvent event,
                                                const std::string& detail) {
            if (!powered_.load(std::memory_order_acquire) || my_epoch != lifecycle_epoch_.load(std::memory_order_acquire)) return;
            if (detail == "config_readonly") config_readonly_ = true;
            if (detail.empty()) {
                ESP_LOGI(TAG, "Modem event: %s", UartEthModem::GetNetworkEventName(event));
            } else {
                ESP_LOGI(TAG, "Modem event: %s (%s)",
                         UartEthModem::GetNetworkEventName(event), detail.c_str());
            }
            switch (event) {
                case UartEthModem::UartEthModemEvent::Connected:
                    OnNetworkEvent(NetworkEvent::Connected, "", my_epoch);
                    SignalWaitBits(kWaitNetworkConnected);
                    break;
                case UartEthModem::UartEthModemEvent::Disconnected:
                    OnNetworkEvent(NetworkEvent::Disconnected, "", my_epoch);
                    break;
                case UartEthModem::UartEthModemEvent::ErrorNoSim:
                    SignalWaitBits(kWaitNetworkFailed);
                    OnNetworkEvent(NetworkEvent::ModemErrorNoSim, "", my_epoch);
                    break;
                case UartEthModem::UartEthModemEvent::ErrorRegistrationDenied:
                    SignalWaitBits(kWaitNetworkFailed);
                    OnNetworkEvent(NetworkEvent::ModemErrorRegDenied, "", my_epoch);
                    break;
                case UartEthModem::UartEthModemEvent::Connecting:
                    OnNetworkEvent(NetworkEvent::Connecting, "", my_epoch);
                    break;
                case UartEthModem::UartEthModemEvent::ErrorInitFailed:
                    retry_needed_.store(detail != "config_readonly");
                    SignalWaitBits(kWaitNetworkFailed);
                    OnNetworkEvent(NetworkEvent::ModemErrorInitFailed, detail, my_epoch);
                    break;

            }
        });
        started = modem_;
    }

    // Serialize resource creation with power-off; Stop must not race Start.
    esp_err_t start_result;
    {
        std::lock_guard<std::mutex> operation_lock(modem_operation_mu_);
        start_result = powered_.load(std::memory_order_acquire) ? started->Start() : ESP_ERR_INVALID_STATE;
    }
    if (start_result != ESP_OK) {
        {
            std::lock_guard<std::mutex> lock(lifecycle_mu_);
            if (network_wait_event_ == wait_eg) {
                network_wait_event_ = nullptr;
            }
            if (modem_ == started) {
                modem_.reset();
            }
        }
        vEventGroupDelete(wait_eg);
        OnNetworkEvent(NetworkEvent::ModemErrorInitFailed);
        return false;
    }

    ESP_LOGI(TAG, "Waiting for network ready (timeout=%d ms)...", timeout_ms);
    const int wait_ms = std::max(1, int((deadline_us - esp_timer_get_time()) / 1000));
    EventBits_t bits = xEventGroupWaitBits(wait_eg, kWaitNetworkAll, pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(timeout_ms > 0 ? wait_ms : 0));

    {
        std::lock_guard<std::mutex> lock(lifecycle_mu_);
        if (network_wait_event_ == wait_eg) {
            network_wait_event_ = nullptr;
        }
    }
    vEventGroupDelete(wait_eg);

    if (my_epoch != lifecycle_epoch_.load(std::memory_order_acquire)) {
        ESP_LOGW(TAG, "StartOrRestart aborted (P14 / newer lifecycle)");
        return false;
    }

    if (bits & kWaitNetworkConnected) {
        ESP_LOGI(TAG, "Network ready");
        return true;
    }

    if (timeout_ms > 0 && !(bits & kWaitNetworkFailed) && !config_readonly_) retry_needed_ = true;
    ESP_LOGW(TAG, "Network wait failed or timed out (bits=0x%lx)", (unsigned long)bits);
    return false;
}


bool Nt26Board::IsNetworkReady() {
    // 仅看 network_ready_：TearDown 先清标志再 reset modem_，避免无锁读 modem_ UAF
    return network_ready_.load(std::memory_order_acquire);
}


bool Nt26Board::EnsureNetworkReady(int timeout_ms) {
    if (IsNetworkReady()) return true;
    if (!powered_.load(std::memory_order_acquire)) return false;
    return StartOrRestartModem(timeout_ms);
}


void Nt26Board::OnPowerRestored() {
    power_restored_us_.store(esp_timer_get_time());
    powered_.store(true, std::memory_order_release);
}
