set(root ${CMAKE_CURRENT_LIST_DIR}/../../..)
idf_component_register(SRCS main.c ../osal_selftest.c ../s31_boot.c ../s31_panic.c ../s31_coproc.c ${root}/system/boot/main.c
    PRIV_INCLUDE_DIRS ..
    REQUIRES ink_apps ink_system ink_drivers app_update esp_app_format esp_hal_lcd esp_hal_gpio esp_http_server)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_panic_handler")
# the interrupt entry's hardware-loop save, from IRAM (s31_coproc.c)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=pxPortGetCoprocArea" "-Wl,-u,__wrap_pxPortGetCoprocArea") # -u: nothing else pulls s31_coproc.o from libmain
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage)

# Vendor file manager: protocol and workers are unchanged; storage is policy-backed.
set(ap ${CMAKE_CURRENT_LIST_DIR}/../vendor/ap_transfer)
set(cjson ${root}/third_party/cjson)
idf_component_get_property(transfer_platform ink_platform COMPONENT_LIB)
target_sources(${transfer_platform} PRIVATE ../hal_transfer_s31.c ${ap}/ap_transfer.cc ${ap}/ap_request_worker.cc
    ${ap}/qrcodegen.c ${cjson}/cJSON.c ${ap}/zlib/adler32.c ${ap}/zlib/crc32.c ${ap}/zlib/inffast.c
    ${ap}/zlib/inflate.c ${ap}/zlib/inftrees.c ${ap}/zlib/zutil.c)
target_include_directories(${transfer_platform} PRIVATE ${ap} ${cjson} ${ap}/zlib)
target_link_libraries(${transfer_platform} PRIVATE idf::esp_http_server)
idf_component_get_property(transfer_http esp_http_server COMPONENT_LIB)
target_compile_definitions(${transfer_http} PUBLIC CONFIG_HTTPD_WS_SUPPORT=1 CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT=1)
foreach(lang zh en)
    set(page ${CMAKE_CURRENT_BINARY_DIR}/ap_fileman_${lang}.html.gz)
    set(translation)
    if(lang STREQUAL en)
        set(translation ${ap}/fileman.en.json)
    endif()
    execute_process(COMMAND ${PYTHON} ${ap}/pack_ap_fileman.py ${ap}/fileman.html ${page} ${translation}
        COMMAND_ERROR_IS_FATAL ANY)
    target_add_binary_data(${COMPONENT_LIB} ${page} BINARY)
endforeach()
target_add_binary_data(${COMPONENT_LIB} ${ap}/cloudzao-logo.png BINARY)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,-u,_binary_ap_fileman_zh_html_gz_start"
    "-Wl,-u,_binary_ap_fileman_en_html_gz_start" "-Wl,-u,_binary_cloudzao_logo_png_start")
