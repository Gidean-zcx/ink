# The music app: kiikoread's page and library build apart from ink_apps, so their stand-in headers (apps/music/port)
# never meet Mogo's (apps/mogo/port) and their drawing is renamed to ink_music_*. The one shared piece is Mogo's
# TrueType engine (ttf_font in ink_mogo_native, with its font reads in apps/mogo/port/runtime.c); Mogo's drawing.c
# is not used.
function(ink_music_build app_target system_target)
get_filename_component(music_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
set(music_dir ${music_root}/apps/music)
set(music_page ${music_root}/third_party/kiikoread_music)
add_library(ink_music_native STATIC ${music_page}/app_music.c ${music_page}/music_library.c ${music_dir}/port/draw.c)
target_include_directories(ink_music_native BEFORE PRIVATE ${music_dir}/port ${music_page}
  ${music_root}/system/services/audio/vendor/kiikoread ${music_root}/third_party/mogo/main/font ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(ink_music_native PRIVATE ${system_target})
target_compile_options(ink_music_native PRIVATE -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable
  "SHELL:-include ${music_dir}/port/kiikoread.h")
# The S31 apps component globs apps/*.c: only the glue belongs to it, the port builds above.
get_target_property(app_sources ${app_target} SOURCES)
list(FILTER app_sources EXCLUDE REGEX "/apps/music/")
set_property(TARGET ${app_target} PROPERTY SOURCES ${app_sources})
target_sources(${app_target} PRIVATE ${music_dir}/music.c)
target_link_libraries(${app_target} PRIVATE ink_music_native)
if(ESP_PLATFORM)
  # Large tables in PSRAM as in ink_system (the apps component does not define this itself).
  target_compile_options(ink_music_native PRIVATE -Os -fstack-usage "SHELL:-include esp_attr.h")
  target_compile_definitions(ink_music_native PRIVATE INK_PSRAM_BSS=EXT_RAM_BSS_ATTR)
  set_property(SOURCE ${music_dir}/music.c TARGET_DIRECTORY ${app_target} APPEND PROPERTY COMPILE_DEFINITIONS INK_PSRAM_BSS=EXT_RAM_BSS_ATTR)
endif()
endfunction()
if(NOT ESP_PLATFORM)
ink_music_build(ink_apps ink_system)
endif()
