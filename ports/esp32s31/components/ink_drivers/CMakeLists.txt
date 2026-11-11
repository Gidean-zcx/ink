set(root ${CMAKE_CURRENT_LIST_DIR}/../../../..)
file(GLOB_RECURSE sources ${root}/drivers/*.c ${root}/boards/metalio_plus/*.c)
include(${CMAKE_CURRENT_LIST_DIR}/../../display_upstream.cmake)
idf_component_register(SRCS ${sources} ../../s31_board.c ../../hal_input_s31.c ../../hal_power_s31.c ../../s31_sleep_bridge.cc
    ../../hal_audio_s31.c ../../hal_opus_s31.c ../../s31_modem.c ../../s31_cell_rx.c ../../s31_uhci.cc ../../hal_cellular_s31.c ../../hal_camera_s31.c
    ${ink_display_sources}
    INCLUDE_DIRS ${root}/drivers/chips ${root}/drivers/display/pqbn ${root}/boards PRIV_INCLUDE_DIRS ../..
    ${ink_display_includes}
    REQUIRES ink_platform PRIV_REQUIRES esp_driver_ledc esp_driver_gpio esp_pm esp_hal_usb esp_driver_usb_serial_jtag
    esp_lcd esp_hal_lcd esp_driver_rmt esp_hal_rmt esp_hal_i2s esp_mm esp_driver_i2s esp-opus
    esp_driver_uart esp_event esp_netif esp_hw_support uart-uhci esp_driver_cam)
target_compile_definitions(${COMPONENT_LIB} PRIVATE INK_S31_LIGHT_SLEEP=$<BOOL:${INK_S31_LIGHT_SLEEP}>)
if(ink_display_sources)
    idf_component_get_property(platform ink_platform COMPONENT_LIB)
    target_compile_definitions(${platform} PRIVATE INK_S31_DISPLAY=1)
    set_source_files_properties(${root}/drivers/display/pqbn/pqbn_v4.c PROPERTIES
        COMPILE_DEFINITIONS "PQBN_HOT=__attribute__((section(\".iram1.pqbn\")))")
endif()
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage -Wextra -Wno-unused-parameter)
include(${CMAKE_CURRENT_LIST_DIR}/../../s31_sleep_sdk_check.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../vendor/s31_sleep/s31_sleep_sdk.cmake)
