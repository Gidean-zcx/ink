# The port's I2C HAL and the chip drivers, compiled unchanged; selftest.c only adds the checks.
set(repo ${CMAKE_CURRENT_LIST_DIR}/../../..)
set(chips ${repo}/drivers/chips)
idf_component_register(
    SRCS selftest.c ${repo}/ports/esp32s31/hal_i2c_s31.c
         ${chips}/tca9555/tca9555.c ${chips}/bq27220/bq27220.c ${chips}/cx25601n/cx25601n.c ${chips}/ft6336u/ft6336u.c
         ${chips}/pcf8563/pcf8563.c ${chips}/sc7a20h/sc7a20h.c ${chips}/tps65185/tps65185.c
    INCLUDE_DIRS . ${repo}/platform/include ${repo}/ports/esp32s31 ${repo}/boards/metalio_plus ${chips}
                 ${chips}/tca9555 ${chips}/tps65185
    PRIV_REQUIRES esp_driver_i2c esp_driver_uart esp_driver_gpio esp_driver_sdmmc sdmmc esp_wifi esp_netif esp_event
                  esp_timer esp_psram spi_flash esp_partition app_update esp_hw_support esp_hal_usb esp_hal_wdt esp_driver_usb_serial_jtag)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)

# The light-sleep build (HWTEST_SLEEP=1, its own build directory): the vendor's S31 sleep patch, built the way the port
# builds it (ports/esp32s31/components/ink_drivers/CMakeLists.txt).
if("$ENV{HWTEST_SLEEP}" STREQUAL "1")
    target_sources(${COMPONENT_LIB} PRIVATE ${repo}/ports/esp32s31/s31_sleep_bridge.cc)
    target_include_directories(${COMPONENT_LIB} PRIVATE ${repo}/ports/esp32s31)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE HWTEST_SLEEP=1)
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-function) # the other tests are not in this build
    include(${repo}/ports/esp32s31/s31_sleep_sdk_check.cmake)
    include(${repo}/ports/esp32s31/vendor/s31_sleep/s31_sleep_sdk.cmake)
endif()
