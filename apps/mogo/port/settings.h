#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { uint8_t px, line, paragraph, margin[4], indent; int8_t weight; uint8_t header, footer; } ink_mogo_settings_t;
extern ink_mogo_settings_t ink_mogo_settings;
void ink_mogo_settings_load(void);
bool ink_mogo_settings_save(void);
int ink_mogo_scaled(int px);
int app_settings_system_font_size(void);
const char *app_settings_fonts_dir(void);
const char *app_settings_font_path(void);
int app_settings_book_px(void);
#define app_settings_book_line_spacing() ink_mogo_settings.line
#define app_settings_book_paragraph_spacing() ink_mogo_settings.paragraph
#define app_settings_book_indent() ink_mogo_settings.indent
#define app_settings_book_indent_adjust() 0
#define app_settings_book_weight_fine() ink_mogo_settings.weight
#define app_settings_reader_hide_images() false
#define app_settings_book_vertical() false
#define app_settings_book_tracking() 0
#define app_settings_reader_statusbar() false
#define app_settings_book_fonts() false
#define app_settings_reader_full_pages() 0
#define app_settings_reader_turn_effect() 0
#define app_settings_book_background_path() ""
#define app_settings_book_title_style() 0
#define app_settings_book_title_size() 48
