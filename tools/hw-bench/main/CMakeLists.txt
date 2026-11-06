# The panel driver is the factory firmware's own, compiled from the local clone and never vendored
# here (LGPL, and it is only a measuring tool). Without the clone the build leaves the panel out.
set(repo ${CMAKE_CURRENT_LIST_DIR}/../../..)
set(srcs bench.c ${repo}/system/util/sha256.c)
set(incs . ${repo}/system/include ${repo}/platform/include ${CMAKE_BINARY_DIR})
set(defs "")
set(up $ENV{INK_UPSTREAM}/main/boards/metalio-e-ink4-plus)
if(EXISTS ${up}/display/metalio_epd.c)
    file(GLOB epd ${up}/display/output_lcd/*.c ${up}/display/output_common/*.c
         ${up}/display/waveforms/*.c)
    list(APPEND srcs ${epd}
         ${up}/display/board_specific.c ${up}/display/builtin_waveforms.c ${up}/display/displays.c
         ${up}/display/epd_board.c ${up}/display/epd_board_common.c ${up}/display/epd_board_metalio_eink4_plus.c
         ${up}/display/epdiy.c ${up}/display/epdiy_riscv_ve_stub.c ${up}/display/highlevel.c
         ${up}/display/metalio_epd.c ${up}/display/render.c
         ${up}/bus/metalio_i2c1.c ${up}/bus/pca9555.c ${up}/bus/tps65185.c)
    list(APPEND incs ${up} ${up}/display ${up}/bus ${up}/periph ${up}/display/output_lcd ${up}/display/output_common
         ${up}/display/waveforms $ENV{INK_UPSTREAM}/main)
    list(APPEND defs INK_BENCH_EPD=1)
    if("$ENV{INK_BENCH_LAB}" STREQUAL "1")
        list(APPEND srcs ${repo}/drivers/display/pqbn/pqbn_v4.c)
        list(APPEND incs ${repo}/drivers/display/pqbn ${repo}/tools/pqbn-lab)
        list(APPEND defs INK_BENCH_LAB=1 "PQBN_HOT=__attribute__((section(\".iram1.pqbn\")))")
    endif()
endif()
idf_component_register(SRCS ${srcs} INCLUDE_DIRS ${incs}
    REQUIRES esp_driver_i2c esp_driver_uart esp_driver_sdmmc esp_driver_gpio esp_driver_i2s esp_driver_ledc esp_lcd
             esp_wifi esp_netif esp_event esp_timer esp_partition spi_flash sdmmc esp_hw_support esp_psram esp_mm esp_adc esp_driver_rmt esp_hal_rmt driver nvs_flash)
target_compile_definitions(${COMPONENT_LIB} PRIVATE ${defs})
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-function -Wno-unused-variable -Wno-format)
