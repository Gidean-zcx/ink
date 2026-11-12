set(root ${CMAKE_CURRENT_LIST_DIR}/../../..)
option(INK_AUTOTEST "Embed and run a device test script" OFF)
set(INK_AUTOTEST_SCRIPT "" CACHE FILEPATH "Script embedded in a test image")
idf_component_register(SRCS main.c ../osal_selftest.c ../s31_boot.c ../s31_panic.c ../s31_coproc.c ${root}/system/boot/main.c
    PRIV_INCLUDE_DIRS ..
    REQUIRES ink_apps ink_system ink_drivers app_update esp_app_format esp_hal_lcd esp_hal_gpio esp_http_server
        tcp_transport esp-tls mbedtls http_parser)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_panic_handler")
# the interrupt entry's hardware-loop save, from IRAM (s31_coproc.c)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=pxPortGetCoprocArea" "-Wl,-u,__wrap_pxPortGetCoprocArea") # -u: nothing else pulls s31_coproc.o from libmain
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage)
# ink_system globs system/*.c; keep test-only sources out of every normal image.
idf_component_get_property(ink_system_lib ink_system COMPONENT_LIB)
target_sources(${ink_system_lib} PRIVATE ${root}/system/apps/gomoku/engine.cpp
    ${root}/system/apps/gomoku/vendor/crossmux/GomokuBoard.cpp ${root}/system/apps/gomoku/vendor/crossmux/GomokuAI.cpp)
target_include_directories(${ink_system_lib} PRIVATE ${root}/system/apps/gomoku/compat)
target_compile_features(${ink_system_lib} PRIVATE cxx_std_20)
get_target_property(ink_system_sources ${ink_system_lib} SOURCES)
list(FILTER ink_system_sources EXCLUDE REGEX "/autotest/")
set_property(TARGET ${ink_system_lib} PROPERTY SOURCES ${ink_system_sources})
if(INK_AUTOTEST)
    if(NOT EXISTS "${INK_AUTOTEST_SCRIPT}")
        message(FATAL_ERROR "INK_AUTOTEST requires INK_AUTOTEST_SCRIPT")
    endif()
    file(READ "${INK_AUTOTEST_SCRIPT}" script HEX)
    string(REGEX REPLACE "(..)" "0x\\1," script "${script}")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/autotest_script.h "static const char ink_autotest_script[] = {${script}0};\n")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${INK_AUTOTEST_SCRIPT}")
    target_sources(${COMPONENT_LIB} PRIVATE ${root}/system/autotest/device.c ${root}/system/autotest/script.c)
    target_include_directories(${COMPONENT_LIB} PRIVATE ${root}/system/autotest ${CMAKE_CURRENT_BINARY_DIR})
    foreach(symbol ink_main hal_input_set_sink ink_cap_invoke ink_cap_invoke_json ink_cap_call ink_confirm_decide)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=${symbol}")
    endforeach()
endif()
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
include(${root}/apps/reader/reader_build.cmake)
ink_reader_build(__idf_ink_apps __idf_ink_system __idf_ink_lvgl)
include(${root}/system/graft/graft_build.cmake)
ink_graft_build(__idf_ink_system)
# Share the reader's pinned inflate implementation with uploads.
set(miniz ${root}/third_party/reader_miniz/miniz.c)
get_target_property(reader_sources ink_reader_vendor SOURCES)
list(FILTER reader_sources EXCLUDE REGEX "/reader_miniz/miniz\\.c$")
set_property(TARGET ink_reader_vendor PROPERTY SOURCES ${reader_sources})
add_library(ink_miniz STATIC ${miniz})
target_include_directories(ink_miniz PUBLIC ${root}/third_party PRIVATE ${root}/system/include)
get_target_property(miniz_definitions ink_reader_vendor INTERFACE_COMPILE_DEFINITIONS)
list(FILTER miniz_definitions INCLUDE REGEX "^(MINIZ_|tinfl_|mz_)")
target_compile_definitions(ink_miniz PUBLIC ${miniz_definitions})
target_compile_options(ink_miniz PRIVATE -w -fstack-usage)
target_link_libraries(ink_reader_vendor PRIVATE ink_miniz)
file(GLOB reader_tables ${root}/assets/i18n/*.tsv)
ink_reader_translations(reader_tables)
execute_process(COMMAND ${PYTHON} ${root}/tools/i18n/i18n_gen.py ${reader_tables}
    --header ${CMAKE_BINARY_DIR}/generated/ink/i18n_table.h
    --sources ${root}/system/apps/memory/memory.c ${root}/system/apps/snake/snake.c
    WORKING_DIRECTORY ${root} COMMAND_ERROR_IS_FATAL ANY)
endif()
# The vendor file manager needs IDF's own components: not in the early expansion pass.
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
# Keep the TLS transport in the platform target without changing its public HAL.
idf_component_get_property(ink_ws_platform ink_platform COMPONENT_LIB)
target_sources(${ink_ws_platform} PRIVATE ../hal_ws_s31.c)
target_link_libraries(${ink_ws_platform} PRIVATE idf::tcp_transport idf::esp-tls idf::mbedtls idf::http_parser)
endif()

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
# Kiikoread HTTP handlers with the existing authorization and file policy.
set(ap ${CMAKE_CURRENT_LIST_DIR}/../vendor/read_pico_transfer)
set(cjson ${root}/third_party/cjson)
idf_component_get_property(transfer_platform ink_platform COMPONENT_LIB)
target_sources(${transfer_platform} PRIVATE ../hal_transfer_s31.c ${ap}/read_pico_transfer.c ${ap}/ink_radio.cc
    ${ap}/qrcodegen.c ${cjson}/cJSON.c)
target_include_directories(${transfer_platform} PRIVATE ${ap} ${cjson})
set_source_files_properties(${ap}/read_pico_transfer.c PROPERTIES COMPILE_OPTIONS "-Wno-unused-function")
target_link_libraries(${transfer_platform} PRIVATE idf::esp_http_server)
set(page ${CMAKE_CURRENT_BINARY_DIR}/upload.html.gz)
execute_process(COMMAND ${PYTHON} ${ap}/pack_upload.py ${ap}/upload.html ${page} COMMAND_ERROR_IS_FATAL ANY)
target_add_binary_data(${COMPONENT_LIB} ${page} BINARY)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,-u,_binary_upload_html_gz_start")
endif()

# The music parser line names an IDF target: not in the early expansion pass.
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
# metadata.c includes the unchanged upstream tag parser.
set_source_files_properties(${root}/system/services/audio/vendor/kiikoread/music_tags.c TARGET_DIRECTORY __idf_ink_system PROPERTIES HEADER_FILE_ONLY ON)
endif()
