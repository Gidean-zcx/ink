set(root ${CMAKE_CURRENT_LIST_DIR}/../../..)
idf_component_register(SRCS main.c ../osal_selftest.c ${root}/system/boot/main.c
    REQUIRES ink_apps ink_system ink_drivers)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage)
