/* apps/mogo/port/settings.h plus the settings the lock styles read; each answers from our own settings. */
#pragma once
#include_next <settings.h>
typedef enum { APP_TICKET_BACKGROUND_TRANSPARENT = 0, APP_TICKET_BACKGROUND_COVER = 1 } app_ticket_background_t;
app_ticket_background_t app_settings_ticket_background(void);
const char *app_settings_device_name(void);
bool app_settings_main_fast_refresh(void);
uint8_t app_settings_system_contrast(void);
const char *app_settings_books_dir(void);
