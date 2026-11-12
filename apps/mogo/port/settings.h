#pragma once
#include <stdbool.h>
#include <stdint.h>
const char *app_settings_fonts_dir(void);
const char *app_settings_font_path(void);
int app_settings_book_px(void);
#define app_settings_reader_hide_images() false
#define app_settings_book_vertical() false
#define app_settings_book_tracking() 0
#define app_settings_reader_statusbar() false
#define app_settings_book_fonts() false
#define app_settings_reader_full_pages() 0
#define app_settings_reader_turn_effect() 0
#define app_settings_book_background_path() ""
#define app_settings_system_font_size() 100
#define app_settings_book_title_style() 0
#define app_settings_book_title_size() 48
