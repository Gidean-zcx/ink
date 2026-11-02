/*
 * System entry point (portable). The platform calls ink_main() from its main
 * task once drivers are up: ESP-IDF app_main(), the simulator's boot task, or
 * main() on POSIX.
 */
#include "ink/hal_device.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/ai.h"
#include "ink/sys/audio.h"
#include "ink/sys/display.h"
#include "ink/sys/hub.h"
#include "ink/sys/storage.h"
#include "ink/sys/ui.h"

extern const ink_activity_t ink_app_home;

void ink_main(void) {
    hal_log_write("ink: starting system services");
    if (ink_display_init() != 0) osal_panic("display service failed to start");
    if (ink_storage_init() != 0) osal_panic("storage service failed to start");
    if (ink_hub_init("wss://hub.local/v1/device") != 0) osal_panic("hub link failed to start");
    if (ink_audio_init() != 0) osal_panic("audio service failed to start");
    if (ink_ai_init() != 0) osal_panic("ai service failed to start");
    if (ink_ui_start(&ink_app_home) != 0) osal_panic("ui failed to start");
    ink_probe_mark("system-started");
}
