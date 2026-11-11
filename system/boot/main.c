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
#include "ink/sys/i18n.h"
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
#include "ink/sys/log.h"

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

/* Which step failed, by name: the device log is all there is to go on (log-1). */
#define MUST(call, what) do { if ((call) != 0) osal_panic(what ": " #call); } while (0)

void ink_main(void) {
    if (ink_display_init() != 0) osal_panic("display service failed to start");
    if (ink_storage_init() != 0) osal_panic("storage service failed to start");
    ink_factory_reset_resume();
    MUST(ink_cap_init(), "capability bus failed to start"); MUST(ink_exec_init(), "executors failed to start");
    MUST(ink_res_init(), "resource manager failed to start"); MUST(ink_storage_reclaimable(), "storage reclaim failed to start");
    MUST(ink_state_init(), "state and settings tables failed to start"); MUST(ink_settings_init(), "state and settings tables failed to start"); MUST(ink_i18n_init(), "state and settings tables failed to start");
    MUST(ink_svc_init(), "data store failed to start"); MUST(ink_confirm_init(), "data store failed to start"); MUST(ink_data_init(), "data store failed to start");
    ink_log_init(); /* Logging must not prevent recovery when its partition is unavailable. */
    ink_log(INK_LOG_INFO, "boot", "starting system services");
    if (ink_task_init() != 0) osal_panic("task list failed to start");
    MUST(ink_power_init(), "power or file service failed to start"); MUST(ink_file_service_init(), "power or file service failed to start");
    ink_members_init();
    if (ink_members_start() != 0) osal_panic("members failed to start");
    MUST(ink_net_init(), "network or light settings failed to start"); MUST(ink_wifi_init(), "network or light settings failed to start"); MUST(ink_light_init(), "network or light settings failed to start");
    if (ink_pairing_init() != 0) osal_panic("pairing failed to start");
    MUST(ink_hub_init("wss://hub.local/v1/device"), "hub link failed to start"); MUST(ink_rt_init(), "hub link failed to start");
    if (ink_audio_init() != 0) osal_panic("audio service failed to start");
    /* before time: an alarm that is due now goes off as soon as the time service is up */
    if (ink_notify_init() != 0) osal_panic("notification service failed to start");
    if (ink_time_init() != 0) osal_panic("time service failed to start");
    if (ink_text_service_init() != 0) osal_panic("text input failed to start");
    MUST(ink_media_init(), "media or Bluetooth failed to start"); MUST(ink_bt_service_init(), "media or Bluetooth failed to start");
    if (ink_backup_init() != 0) osal_panic("backup service failed to start");
    if (ink_update_init() != 0) osal_panic("system upgrades failed to start");
    hal_device_info_t di;
    hal_device_get_info(&di);
    if (ink_camera_init() != 0 || (di.has_modem && ink_comm_init() != 0)) osal_panic("camera or communication service failed to start");
    if (ink_ai_init() != 0) osal_panic("ai service failed to start");
    if (ink_tools_init() != 0 || ink_system_actions_init() != 0 || ink_reader_actions_init() != 0 || ink_remote_init() != 0)
        osal_panic("action registry failed to start");
    MUST(ink_apps_init(), "app directory failed to start"); MUST(ink_app_register(&ink_appdef_home), "app directory failed to start"); MUST(ink_app_register(&ink_appdef_reader), "app directory failed to start");
    if (ink_contacts_init() != 0) osal_panic("contacts failed to start");
    if (ink_phone_init() != 0) osal_panic("phone failed to start");
    MUST(ink_ui_start(&ink_app_home), "ui failed to start"); MUST(ink_ui_lock_mark_init(), "ui failed to start"); MUST(ink_shell_interrupt_card_init(), "ui failed to start"); MUST(ink_shell_confirm_card_init(), "ui failed to start"); MUST(ink_shell_keyboard_init(), "ui failed to start"); MUST(ink_shell_member_gate_init(), "ui failed to start"); MUST(ink_shell_update_card_init(), "ui failed to start"); MUST(ink_shell_power_menu_init(), "ui failed to start"); MUST(ink_shell_off_picture_init(), "ui failed to start");
    if (ink_file_picker_init() != 0) osal_panic("file picker failed to start");
    ink_fs_refresh_state(); /* blocks on the card: here, not in anyone's way */
    ink_pairing_start();
    ink_data_refresh_state();
    ink_power_battery_policy_start();
    ink_update_boot_check();
    ink_probe_mark("system-started");
    extern const ink_app_def_t ink_appdef_clock;
    int ink_clock_init(void);
    MUST(ink_clock_init(), "clock app failed to start"); MUST(ink_app_register(&ink_appdef_clock), "clock app failed to start");
    extern int ink_sms_init(void);
    if (ink_sms_init() != 0) osal_panic("SMS failed to start");
    extern int ink_memo_init(void);
    if (ink_memo_init() != 0) osal_panic("memo failed to start");
    extern int ink_settings_family_init(void), ink_wallpaper_init(void);
    MUST(ink_settings_family_init(), "family settings failed to start"); MUST(ink_wallpaper_init(), "family settings failed to start");
    extern int ink_member_pages_init(void);
    if (ink_member_pages_init() != 0) osal_panic("member pages failed to start");
    extern int ink_calendar_init(void);
    if (ink_calendar_init() != 0) osal_panic("calendar failed to start");
    extern int ink_calculator_init(void);
    if (ink_calculator_init() != 0) osal_panic("calculator failed to start");
    extern int ink_weather_init(void);
    if (ink_weather_init() != 0) osal_panic("weather failed to start");
    extern int ink_files_init(void);
    if (ink_files_init() != 0) osal_panic("files failed to start");
    extern int ink_camera_app_init(void), ink_gallery_init(void);
    MUST(ink_camera_app_init(), "camera/gallery failed to start"); MUST(ink_gallery_init(), "camera/gallery failed to start");
    extern int ink_backup_pages_init(void), ink_factory_pages_init(void);
    if (ink_backup_pages_init() != 0) osal_panic("backup pages failed to start");
    if (ink_factory_pages_init() != 0) osal_panic("factory pages failed to start");
    extern int ink_ai_pages_init(void);
    if (ink_ai_pages_init() != 0) osal_panic("AI pages failed to start");
    extern int ink_remote_app_init(void);
    if (ink_remote_app_init() != 0) osal_panic("remote app failed to start");
    if (ink_log_start()) osal_panic("logging capabilities failed to start");
    ink_backup_start(); /* Replay only after all collection definitions are registered. */
    MUST(ink_app_packages_init(), "installed apps failed to start");
}
