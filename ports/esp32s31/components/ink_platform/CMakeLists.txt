set(root ${CMAKE_CURRENT_LIST_DIR}/../../../..)
idf_component_register(SRCS ../../osal.c ../../hal_stubs.c
    INCLUDE_DIRS ${root}/platform/include
    REQUIRES freertos esp_timer heap esp_system esp_hw_support esp_psram)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage -fcallgraph-info=su -Wextra -Wno-unused-parameter)
