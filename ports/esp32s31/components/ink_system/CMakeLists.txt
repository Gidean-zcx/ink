set(root ${CMAKE_CURRENT_LIST_DIR}/../../../..)
file(GLOB_RECURSE sources ${root}/system/*.c)
list(FILTER sources EXCLUDE REGEX "/services/notify/vendor/pebble/")
set(quirc_sources ${root}/third_party/quirc/quirc.c ${root}/third_party/quirc/identify.c ${root}/third_party/quirc/decode.c ${root}/third_party/quirc/version_db.c)
list(APPEND sources ${quirc_sources})
list(REMOVE_ITEM sources ${root}/system/boot/main.c ${root}/system/ui/lv_mem_osal.c)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)
idf_component_register(SRCS ${sources} INCLUDE_DIRS ${root}/system/include
    PRIV_INCLUDE_DIRS ${root}/system/services/display ${root}/third_party/jsmn ${root}/third_party/stb ${root}/third_party/quirc ${CMAKE_BINARY_DIR}/generated
    ${root}/system/services/time/vendor/mogo-calendar/main
    REQUIRES ink_platform ink_lvgl)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage -fcallgraph-info=su -Wextra -Wno-unused-parameter -include esp_attr.h)
target_compile_definitions(${COMPONENT_LIB} PRIVATE INK_PSRAM_BSS=EXT_RAM_BSS_ATTR)
# The two codec translation units include upstream stb implementations.
set_source_files_properties(${root}/system/services/media/image_codec.c ${root}/system/apps/camera/png.c
    PROPERTIES COMPILE_OPTIONS -w)
# The copied ZSWatch recording screen and its glue only: Zephyr-shaped shim headers (system/apps/memo/vendor/zswatch/SOURCE.md).
set_source_files_properties(${root}/system/apps/memo/memo_voice.c ${root}/system/apps/memo/vendor/zswatch/voice_memo_ui.c PROPERTIES INCLUDE_DIRECTORIES
    "${root}/system/apps/memo/vendor/zswatch/port;${root}/system/apps/memo/vendor/zswatch")
set_source_files_properties(${quirc_sources} PROPERTIES COMPILE_OPTIONS "-include;${root}/system/apps/camera/quirc_alloc.h;-w")
file(GLOB_RECURSE ui_sources ${root}/system/ui/*.c ${root}/system/apps/*.c ${root}/system/apps/*.cpp)
# Keep the installer at -O2: -Os exposes existing path truncation diagnostics.
list(REMOVE_ITEM ui_sources ${root}/system/ui/app_packages.c)
set_property(SOURCE ${ui_sources} APPEND PROPERTY COMPILE_OPTIONS -Os)
execute_process(COMMAND ${PYTHON} ${root}/tools/icongen/check.py --header ${CMAKE_BINARY_DIR}/generated/ink/ui_icon_names.h
    WORKING_DIRECTORY ${root} COMMAND_ERROR_IS_FATAL ANY)
file(GLOB_RECURSE i18n_services ${root}/system/services/*.c ${root}/system/policy/*.c)
file(GLOB i18n_tables ${root}/assets/i18n/*.tsv)
execute_process(COMMAND ${PYTHON} ${root}/tools/i18n/i18n_gen.py ${i18n_tables} --header ${CMAKE_BINARY_DIR}/generated/ink/i18n_table.h
    --sources ${i18n_services} ${root}/system/ui/shell/boot_flow.c ${root}/system/ui/shell/navigation.c ${root}/system/ui/shell/keyboard.c ${root}/system/ui/shell/settings_pages.c
    ${root}/system/ui/shell/status_bar.c ${root}/system/ui/shell/shell_home.c ${root}/system/ui/shell/control_center.c
    ${root}/system/ui/shell/notification_list.c
    ${root}/system/ui/shell/settings_body.c ${root}/system/ui/shell/today_edit.c
    ${root}/system/ui/shell/time_settings.c ${root}/system/ui/shell/settings_family.c ${root}/system/services/time/time_service.c
    ${root}/system/apps/clock/clock.c
    ${root}/system/apps/calculator/calculator.c ${root}/system/apps/camera/camera.c
    ${root}/system/apps/contacts/contacts.c ${root}/system/apps/gallery/gallery.c
    ${root}/system/apps/memo/memo.c ${root}/system/apps/phone/phone.c ${root}/system/apps/phone/vendor/lilyphone/call_card.c
    ${root}/system/apps/memo/memo_voice.c ${root}/system/apps/memo/vendor/zswatch/voice_memo_ui.c
    ${root}/system/apps/remote/remote.c ${root}/system/apps/sms/sms.c
    ${root}/system/apps/weather/weather.c ${root}/system/apps/camera/scan.c
  ${root}/system/ui/shell/activity_card.c
  ${root}/system/ui/shell/ai_chat.c
  ${root}/system/ui/shell/ai_pages.c
  ${root}/system/ui/shell/ai_panel.c
  ${root}/system/ui/shell/backup_pages.c
  ${root}/system/ui/shell/bluetooth_page.c
  ${root}/system/ui/shell/confirm_card.c
  ${root}/system/ui/shell/member_pages.c
  ${root}/system/ui/shell/transfer_pages.c
  ${root}/system/ui/shell/update_pages.c
  ${root}/system/ui/shell/wallpaper.c
  ${root}/system/ui/search.c
    ${root}/system/ui/shell/airplane_card.c
    ${root}/system/ui/shell/interrupt_card.c
    ${root}/system/ui/shell/kid_pages.c
    ${root}/system/ui/shell/member_gate.c
    ${root}/system/ui/shell/off_picture.c
    ${root}/system/ui/shell/pinpad.c
    ${root}/system/ui/file_picker.c
    ${root}/system/ui/lock_mark.c
    ${root}/system/ui/textfield.c
    ${root}/system/ui/ui_list.c
    ${root}/system/ui/ui_semantics.c
    ${root}/system/ui/shell/factory_pages.c
    ${root}/system/ui/shell/power_menu.c
    ${root}/system/ui/shell/power_pages.c
    ${root}/system/ui/card_files_notice.c
    ${root}/system/apps/gallery/vendor/agentui/camera_gallery_repository_part.h
    WORKING_DIRECTORY ${root} COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${i18n_tables})
execute_process(COMMAND ${PYTHON} ${root}/tools/licenses.py ${CMAKE_BINARY_DIR}/generated/ink/licenses.h
    WORKING_DIRECTORY ${root} COMMAND_ERROR_IS_FATAL ANY)
# The card files list (D-138) comes from the host build's bundle, so the firmware and build-s31/card-files agree.
if(NOT EXISTS ${root}/build/card-files/manifest.json)
    message(FATAL_ERROR "No ${root}/build/card-files/manifest.json: do the host build first (target ink_card_files)")
endif()
execute_process(COMMAND ${PYTHON} ${root}/tools/card_files.py header --manifest ${root}/build/card-files/manifest.json
    --out ${CMAKE_BINARY_DIR}/generated/ink/card_files_manifest.h WORKING_DIRECTORY ${root} COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${root}/build/card-files/manifest.json)

include(${root}/system/services/text/google_build.cmake)
ink_google_pinyin(${COMPONENT_LIB})
target_sources(${COMPONENT_LIB} PRIVATE ${root}/system/services/text/google_pinyin.cpp ${root}/system/services/text/google_port.cpp)
