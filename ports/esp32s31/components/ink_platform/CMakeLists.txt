set(root ${CMAKE_CURRENT_LIST_DIR}/../../../..)
file(GLOB assets ${root}/build/assets/fonts/*.inkf ${root}/build/assets/icons/*.inki)
execute_process(COMMAND ${PYTHON} ${root}/tools/asset_pack.py ${root}/build/assets --out ${CMAKE_BINARY_DIR}
    COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${assets} ${root}/tools/asset_pack.py)
idf_component_register(SRCS ../../hal_assets_s31.c ../../hal_storage_s31.c ${root}/third_party/littlefs/lfs.c ${root}/third_party/littlefs/lfs_util.c ../../osal.c ../../hal_stubs.c ../../hal_i2c_s31.c ../../hal_uart_s31.c ../../s31_irq.c
    INCLUDE_DIRS ${root}/platform/include PRIV_INCLUDE_DIRS ../.. ${root}/third_party/littlefs
    EMBED_FILES ${CMAKE_BINARY_DIR}/ink-embedded.bin
    REQUIRES freertos esp_timer heap esp_system esp_hw_support esp_psram
    PRIV_REQUIRES esp_driver_i2c esp_driver_uart esp_driver_gpio esp_partition esp_event fatfs sdmmc esp_driver_sdmmc)
target_compile_definitions(${COMPONENT_LIB} PRIVATE LFS_NO_MALLOC LFS_NO_DEBUG LFS_NO_TRACE)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage -fcallgraph-info=su -Wextra -Wno-unused-parameter)
