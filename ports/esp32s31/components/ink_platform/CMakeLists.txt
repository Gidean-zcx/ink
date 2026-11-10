set(root ${CMAKE_CURRENT_LIST_DIR}/../../../..)
idf_component_register(SRCS ../../osal.c ../../hal_stubs.c ../../hal_i2c_s31.c ../../hal_uart_s31.c ../../s31_irq.c
    INCLUDE_DIRS ${root}/platform/include PRIV_INCLUDE_DIRS ../..
    REQUIRES freertos esp_timer heap esp_system esp_hw_support esp_psram
    PRIV_REQUIRES esp_driver_i2c esp_driver_uart esp_driver_gpio)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage -fcallgraph-info=su -Wextra -Wno-unused-parameter)
