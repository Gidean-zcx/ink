/*
 * System entry point (portable). The platform calls ink_main() from its main
 * task once drivers are up: ESP-IDF app_main(), the simulator's boot task, or
 * main() on POSIX.
 */
#include "ink/hal_device.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/appreg.h"
#include "ink/sys/cap.h"
#include "ink/sys/ai.h"
#include "ink/sys/audio.h"
#include "ink/sys/camera.h"
#include "ink/sys/comm.h"
#include "ink/sys/confirm.h"
#include "ink/sys/data.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/file.h"
#include "ink/sys/power.h"
#include "ink/sys/res.h"
#include "ink/sys/rt.h"
#include "ink/sys/hub.h"
#include "ink/sys/media.h"
#include "ink/sys/members.h"
#include "ink/sys/net.h"
#include "ink/sys/pairing.h"
#include "ink/sys/notify.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "ink/sys/svc.h"
#include "ink/sys/textinput.h"
#include "ink/sys/task.h"
#include "ink/sys/time.h"
#include "ink/sys/ui.h"
#include "ink/sys/file_picker.h"
#include "ink/sys/update.h"

extern const ink_activity_t ink_app_home;
extern const ink_app_def_t ink_appdef_home, ink_appdef_reader;

#include "ink/sys/backup.h"

int ink_system_actions_init(void);
void ink_factory_reset_resume(void);
int ink_reader_actions_init(void);
int ink_remote_init(void);
int ink_ui_lock_mark_init(void);
int ink_shell_interrupt_card_init(void);
int ink_shell_confirm_card_init(void);
int ink_shell_keyboard_init(void);
int ink_bt_service_init(void);
int ink_shell_member_gate_init(void);
int ink_shell_update_card_init(void);
int ink_shell_power_menu_init(void);
int ink_shell_off_picture_init(void);
int ink_contacts_init(void);
int ink_phone_init(void);

void ink_main(void) {
    hal_log_write("ink: starting system services");
    if (ink_display_init() != 0) osal_panic("display service failed to start");
    if (ink_storage_init() != 0) osal_panic("storage service failed to start");
    ink_factory_reset_resume();
    if (ink_cap_init() != 0 || ink_exec_init() != 0 || ink_res_init() != 0 || ink_storage_reclaimable() != 0)
        osal_panic("capability bus or resource manager failed to start");
    if (ink_state_init() != 0 || ink_settings_init() != 0) osal_panic("state and settings tables failed to start");
    if (ink_svc_init() != 0 || ink_confirm_init() != 0 || ink_data_init() != 0) osal_panic("data store failed to start");
    if (ink_task_init() != 0) osal_panic("task list failed to start");
    if (ink_power_init() != 0 || ink_file_service_init() != 0) osal_panic("power or file service failed to start");
    ink_members_init();
    if (ink_members_start() != 0) osal_panic("members failed to start");
    if (ink_net_init() != 0 || ink_wifi_init() != 0 || ink_light_init() != 0) osal_panic("network or light settings failed to start");
    if (ink_pairing_init() != 0) osal_panic("pairing failed to start");
    if (ink_hub_init("wss://hub.local/v1/device") != 0 || ink_rt_init() != 0) osal_panic("hub link failed to start");
    if (ink_audio_init() != 0) osal_panic("audio service failed to start");
    /* before time: an alarm that is due now goes off as soon as the time service is up */
    if (ink_notify_init() != 0) osal_panic("notification service failed to start");
    if (ink_time_init() != 0) osal_panic("time service failed to start");
    if (ink_text_service_init() != 0) osal_panic("text input failed to start");
    if (ink_media_init() != 0 || ink_bt_service_init() != 0) osal_panic("media or Bluetooth failed to start");
    if (ink_backup_init() != 0) osal_panic("backup service failed to start");
    if (ink_update_init() != 0) osal_panic("system upgrades failed to start");
    hal_device_info_t di;
    hal_device_get_info(&di);
    if (ink_camera_init() != 0 || (di.has_modem && ink_comm_init() != 0)) osal_panic("camera or communication service failed to start");
    if (ink_ai_init() != 0) osal_panic("ai service failed to start");
    if (ink_tools_init() != 0 || ink_system_actions_init() != 0 || ink_reader_actions_init() != 0 || ink_remote_init() != 0)
        osal_panic("action registry failed to start");
    if (ink_apps_init() != 0 || ink_app_register(&ink_appdef_home) != 0 || ink_app_register(&ink_appdef_reader) != 0) osal_panic("app directory failed to start");
    if (ink_contacts_init() != 0) osal_panic("contacts failed to start");
    if (ink_phone_init() != 0) osal_panic("phone failed to start");
    if (ink_ui_start(&ink_app_home) != 0 || ink_ui_lock_mark_init() != 0 || ink_shell_interrupt_card_init() != 0 || ink_shell_confirm_card_init() != 0 || ink_shell_keyboard_init() != 0 || ink_shell_member_gate_init() != 0 || ink_shell_update_card_init() != 0 || ink_shell_power_menu_init() != 0 || ink_shell_off_picture_init() != 0) osal_panic("ui failed to start");
    if (ink_file_picker_init() != 0) osal_panic("file picker failed to start");
    ink_fs_refresh_state(); /* blocks on the card: here, not in anyone's way */
    ink_pairing_start();
    ink_backup_start();
    ink_data_refresh_state();
    ink_power_battery_policy_start();
    ink_update_boot_check();
    ink_probe_mark("system-started");
    extern const ink_app_def_t ink_appdef_clock;
    int ink_clock_init(void);
    if (ink_clock_init() != 0 || ink_app_register(&ink_appdef_clock) != 0) osal_panic("clock app failed to start");
    extern int ink_sms_init(void);
    if (ink_sms_init() != 0) osal_panic("SMS failed to start");
    extern int ink_memo_init(void);
    if (ink_memo_init() != 0) osal_panic("memo failed to start");
    extern int ink_settings_family_init(void), ink_member_rename_init(void), ink_wallpaper_init(void);
    if (ink_settings_family_init() || ink_member_rename_init() || ink_wallpaper_init()) osal_panic("family settings failed to start");
    extern int ink_member_pages_init(void);
    if (ink_member_pages_init() != 0) osal_panic("member pages failed to start");
    extern int ink_calendar_init(void);
    if (ink_calendar_init() != 0) osal_panic("calendar failed to start");
    extern int ink_files_init(void);
    if (ink_files_init() != 0) osal_panic("files failed to start");
}
