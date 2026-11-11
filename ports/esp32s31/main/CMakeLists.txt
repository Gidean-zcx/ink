set(root ${CMAKE_CURRENT_LIST_DIR}/../../..)
idf_component_register(SRCS main.c ../osal_selftest.c ../s31_boot.c ../s31_panic.c ../s31_coproc.c ${root}/system/boot/main.c
    PRIV_INCLUDE_DIRS ..
    REQUIRES ink_apps ink_system ink_drivers app_update esp_app_format esp_hal_lcd esp_hal_gpio)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_panic_handler")
# the interrupt entry's hardware-loop save, from IRAM (s31_coproc.c)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=pxPortGetCoprocArea" "-Wl,-u,__wrap_pxPortGetCoprocArea") # -u: nothing else pulls s31_coproc.o from libmain
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage)
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
include(${root}/apps/reader/reader_build.cmake)
ink_reader_build(__idf_ink_apps __idf_ink_system __idf_ink_lvgl)
file(GLOB reader_tables ${root}/assets/i18n/*.tsv)
ink_reader_translations(reader_tables)
execute_process(COMMAND ${PYTHON} ${root}/tools/i18n/i18n_gen.py ${reader_tables}
    --header ${CMAKE_BINARY_DIR}/generated/ink/i18n_table.h WORKING_DIRECTORY ${root} COMMAND_ERROR_IS_FATAL ANY)
endif()
