set(root ${CMAKE_CURRENT_LIST_DIR}/../../../..)
file(GLOB_RECURSE sources ${root}/system/*.c)
set(quirc_sources ${root}/third_party/quirc/quirc.c ${root}/third_party/quirc/identify.c ${root}/third_party/quirc/decode.c ${root}/third_party/quirc/version_db.c)
list(APPEND sources ${quirc_sources})
list(REMOVE_ITEM sources ${root}/system/boot/main.c ${root}/system/ui/lv_mem_osal.c)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)
idf_component_register(SRCS ${sources} INCLUDE_DIRS ${root}/system/include
    PRIV_INCLUDE_DIRS ${root}/system/services/display ${root}/third_party/jsmn ${root}/third_party/stb ${root}/third_party/quirc ${CMAKE_BINARY_DIR}/generated
    REQUIRES ink_platform ink_lvgl)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage -fcallgraph-info=su -Wextra -Wno-unused-parameter -include esp_attr.h)
target_compile_definitions(${COMPONENT_LIB} PRIVATE INK_PSRAM_BSS=EXT_RAM_BSS_ATTR)
# The two codec translation units include upstream stb implementations.
set_source_files_properties(${root}/system/services/media/image_codec.c ${root}/system/apps/camera/png.c
    PROPERTIES COMPILE_OPTIONS -w)
set_source_files_properties(${quirc_sources} PROPERTIES COMPILE_OPTIONS "-include;${root}/system/apps/camera/quirc_alloc.h;-w")
execute_process(COMMAND ${PYTHON} ${root}/tools/icongen/check.py --header ${CMAKE_BINARY_DIR}/generated/ink/ui_icon_names.h
    WORKING_DIRECTORY ${root} COMMAND_ERROR_IS_FATAL ANY)
file(GLOB i18n_tables ${root}/assets/i18n/*.tsv)
execute_process(COMMAND ${PYTHON} ${root}/tools/i18n/i18n_gen.py ${i18n_tables} --header ${CMAKE_BINARY_DIR}/generated/ink/i18n_table.h
    --sources ${root}/system/ui/shell/boot_flow.c ${root}/system/ui/shell/navigation.c ${root}/system/ui/shell/keyboard.c ${root}/system/ui/shell/settings_pages.c
    ${root}/system/ui/shell/time_settings.c ${root}/system/ui/shell/settings_family.c ${root}/system/services/time/time_service.c
    ${root}/system/apps/clock/clock.c
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
    WORKING_DIRECTORY ${root} COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${i18n_tables})
execute_process(COMMAND ${PYTHON} ${root}/tools/licenses.py ${CMAKE_BINARY_DIR}/generated/ink/licenses.h
    WORKING_DIRECTORY ${root} COMMAND_ERROR_IS_FATAL ANY)
