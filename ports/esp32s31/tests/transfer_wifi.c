/* Exercise the existing station fixture through the transfer pause/restore glue. */
#define main ink_station_fixture
#include "wifi.c"
#undef main
int main(void) {
    assert(ink_station_fixture() == 0);
    assert(retry_timer.active && !radio_on);
    ink_transfer_wifi_pause();
    assert(!retry_timer.active && !radio_on && ink_wifi.mode == INK_PAUSE);
    ink_transfer_wifi_restore(true);
    assert(retry_timer.active && !radio_on && ink_wifi.mode == INK_AUTO);
    ink_transfer_wifi_pause();
    ink_transfer_wifi_restore(false);
    assert(!retry_timer.active && !radio_on);
    response = 1; hal_net_use(HAL_LINK_WIFI); pump_all(); link_is(HAL_LINK_WIFI, true);
    ink_transfer_wifi_pause(); assert(!radio_on);
    response = 1; ink_transfer_wifi_restore(true); pump_all(); link_is(HAL_LINK_WIFI, true);
    assert(radio_on);
    puts("transfer Wi-Fi restore: prior scan timer, off radio, prior station restored");
    return 0;
}
