idf_component_register(SRCS "uart_eth_modem.cc" "nt26_lifecycle.cc"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../ports/esp32s31" "../../platform/include"
    REQUIRES uart-uhci iot_eth esp_driver_uart esp_driver_gpio esp_netif esp_event esp_timer lwip)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-parameter)
