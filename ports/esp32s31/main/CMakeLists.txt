set(root ${CMAKE_CURRENT_LIST_DIR}/../../..)
idf_component_register(SRCS main.c ../osal_selftest.c ../s31_boot.c ../s31_panic.c ../s31_coproc.c ${root}/system/boot/main.c
    PRIV_INCLUDE_DIRS ..
    REQUIRES ink_apps ink_system ink_drivers app_update esp_app_format esp_hal_lcd esp_hal_gpio)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_panic_handler")
# the interrupt entry's hardware-loop save, from IRAM (s31_coproc.c)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=pxPortGetCoprocArea" "-Wl,-u,__wrap_pxPortGetCoprocArea") # -u: nothing else pulls s31_coproc.o from libmain
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage)

# metadata.c includes the unchanged upstream tag parser.
set_source_files_properties(${root}/system/services/audio/vendor/kiikoread/music_tags.c TARGET_DIRECTORY __idf_ink_system PROPERTIES HEADER_FILE_ONLY ON)
