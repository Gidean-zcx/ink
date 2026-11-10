/* C entry points for the vendor sleep module. */
#include "vendor/s31_sleep/s31_sleep.h"

extern "C" int ink_s31_sleep_init(void) {
    s31_sleep::CaptureReset();
    return s31_sleep::Init();
}
extern "C" int ink_s31_light_sleep(void) {
    s31_sleep::BeginCycle();
    return s31_sleep::LightSleep(0);
}
