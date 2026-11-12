/* Preserve the original AP name, parameters and station restore through fake SDK calls. */
#include "fake.h"
#include <cassert>
static std::string configured;
static int pauses, restores;
static int capture_config(int mode, wifi_config_t *config) {
    assert(mode == WIFI_IF_AP);
    configured = reinterpret_cast<char *>(config->ap.ssid);
    assert(config->ap.channel == 1 && config->ap.max_connection == 4 && config->ap.authmode == WIFI_AUTH_OPEN);
    return 0;
}
#define esp_wifi_set_config capture_config
#ifndef INK_RADIO_SOURCE
#define INK_RADIO_SOURCE "../vendor/read_pico_transfer/ink_radio.cc"
#endif
#include INK_RADIO_SOURCE
#undef esp_wifi_set_config
extern "C" void ink_transfer_wifi_pause(void) { pauses++; }
extern "C" void ink_transfer_wifi_restore(bool resume) { assert(resume); restores++; }
int main() {
    assert(ink_pico_radio_start() == 0);
    assert(configured == "Metalio-0000" && pauses == 1);
    ink_pico_radio_stop(); assert(restores == 1);
    ink_pico_radio_stop(); assert(restores == 1);
    puts("original AP name, channel, capacity, authentication, pause/restore: passed");
}
