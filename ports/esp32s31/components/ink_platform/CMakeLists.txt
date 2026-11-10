set(root ${CMAKE_CURRENT_LIST_DIR}/../../../..)
idf_component_register(SRCS ../../osal.c ../../hal_stubs.c ../../hal_i2c_s31.c ../../hal_uart_s31.c ../../s31_irq.c ../../hal_log_s31.c
    INCLUDE_DIRS ${root}/platform/include PRIV_INCLUDE_DIRS ../..
    REQUIRES freertos esp_timer heap esp_system esp_hw_support esp_psram
    PRIV_REQUIRES esp_driver_i2c esp_driver_uart esp_driver_gpio esp_partition app_update espcoredump)
execute_process(COMMAND git rev-parse --short=12 HEAD WORKING_DIRECTORY ${root} OUTPUT_VARIABLE ink_commit OUTPUT_STRIP_TRAILING_WHITESPACE)
target_compile_definitions(${COMPONENT_LIB} PRIVATE INK_BUILD_COMMIT="${ink_commit}")
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage -fcallgraph-info=su -Wextra -Wno-unused-parameter)
