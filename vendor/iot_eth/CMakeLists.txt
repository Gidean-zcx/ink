idf_component_register(SRCS "iot_eth.c" "iot_eth_netif_glue.c"
                    INCLUDE_DIRS "include" "interface"
                    PRIV_INCLUDE_DIRS "../uart-eth-modem" "../../platform/include"
                    REQUIRES esp_netif)

target_compile_definitions(${COMPONENT_LIB} PRIVATE IOT_ETH_VER_MAJOR=0 IOT_ETH_VER_MINOR=1 IOT_ETH_VER_PATCH=0)
