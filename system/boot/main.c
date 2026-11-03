/*
 * System entry point (portable). The platform calls ink_main() from its main
 * task once drivers are up: ESP-IDF app_main(), the simulator's boot task, or
 * main() on POSIX.
 */
#include "ink/hal_device.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/ai.h"
#include "ink/sys/audio.h"
#include "ink/sys/camera.h"
#include "ink/sys/comm.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/power.h"
#include "ink/sys/res.h"
#include "ink/sys/hub.h"
#include "ink/sys/members.h"
#include "ink/sys/net.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "ink/sys/ui.h"

extern const ink_activity_t ink_app_home;

int ink_system_actions_init(void);
int ink_reader_actions_init(void);
int ink_ui_lock_mark_init(void);

void ink_main(void) {
    hal_log_write("ink: starting system services");
    if (ink_display_init() != 0) osal_panic("display service failed to start");
    if (ink_storage_init() != 0) osal_panic("storage service failed to start");
    if (ink_cap_init() != 0 || ink_exec_init() != 0 || ink_res_init() != 0 || ink_storage_reclaimable() != 0)
        osal_panic("capability bus or resource manager failed to start");
    if (ink_state_init() != 0 || ink_settings_init() != 0) osal_panic("state and settings tables failed to start");
    if (ink_power_init() != 0) osal_panic("power service failed to start");
    ink_members_init();
    if (ink_net_init() != 0 || ink_light_init() != 0) osal_panic("network or light settings failed to start");
    if (ink_hub_init("wss://hub.local/v1/device") != 0) osal_panic("hub link failed to start");
    if (ink_audio_init() != 0) osal_panic("audio service failed to start");
    hal_device_info_t di;
    hal_device_get_info(&di);
    if (ink_camera_init() != 0 || (di.has_modem && ink_comm_init() != 0)) osal_panic("camera or communication service failed to start");
    if (ink_ai_init() != 0) osal_panic("ai service failed to start");
    if (ink_tools_init() != 0 || ink_system_actions_init() != 0 || ink_reader_actions_init() != 0)
        osal_panic("action registry failed to start");
    if (ink_ui_start(&ink_app_home) != 0 || ink_ui_lock_mark_init() != 0) osal_panic("ui failed to start");
    ink_probe_mark("system-started");
}
