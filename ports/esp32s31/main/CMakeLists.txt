set(root ${CMAKE_CURRENT_LIST_DIR}/../../..)
idf_component_register(SRCS main.c ../osal_selftest.c ../s31_boot.c ../s31_panic.c ${root}/system/boot/main.c
    PRIV_INCLUDE_DIRS ..
    REQUIRES ink_apps ink_system ink_drivers app_update esp_app_format esp_hal_lcd esp_hal_gpio)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_panic_handler")
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage)
