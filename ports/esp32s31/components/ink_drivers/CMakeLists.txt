set(root ${CMAKE_CURRENT_LIST_DIR}/../../../..)
file(GLOB_RECURSE sources ${root}/drivers/*.c ${root}/boards/metalio_plus/*.c)
idf_component_register(SRCS ${sources} ../../s31_board.c ../../hal_input_s31.c ../../hal_power_s31.c ../../s31_sleep_bridge.cc
    INCLUDE_DIRS ${root}/drivers/chips ${root}/drivers/display/pqbn ${root}/boards PRIV_INCLUDE_DIRS ../..
    REQUIRES ink_platform PRIV_REQUIRES esp_driver_ledc esp_driver_gpio esp_pm esp_hal_usb esp_driver_usb_serial_jtag)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage -Wextra -Wno-unused-parameter)
include(${CMAKE_CURRENT_LIST_DIR}/../../s31_sleep_sdk_check.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../vendor/s31_sleep/s31_sleep_sdk.cmake)
