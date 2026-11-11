cmake_minimum_required(VERSION 3.22)
execute_process(COMMAND git rev-parse --short=8 HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE ink_revision OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
string(TIMESTAMP ink_build_time "%y%m%d%H%M%S" UTC)
set(PROJECT_VER "g${ink_revision}-${ink_build_time}")
option(INK_S31_LIGHT_SLEEP "Enable unverified S31 light sleep" OFF)
include(${CMAKE_CURRENT_LIST_DIR}/opus.cmake)
list(APPEND EXTRA_COMPONENT_DIRS ${ink_opus_source})
list(APPEND EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/../../vendor/uart-uhci)
list(APPEND EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/../../vendor/iot_eth
    ${CMAKE_CURRENT_LIST_DIR}/../../vendor/uart-eth-modem)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(COMPONENTS main uart-eth-modem)
project(ink-s31)

idf_component_get_property(ink_cell_drivers ink_drivers COMPONENT_LIB)
target_link_libraries(${ink_cell_drivers} PRIVATE __idf_uart-eth-modem)
