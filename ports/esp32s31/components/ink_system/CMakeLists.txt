set(root ${CMAKE_CURRENT_LIST_DIR}/../../../..)
file(GLOB_RECURSE sources ${root}/system/*.c)
list(REMOVE_ITEM sources ${root}/system/boot/main.c ${root}/system/ui/lv_mem_osal.c)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)
idf_component_register(SRCS ${sources} INCLUDE_DIRS ${root}/system/include
    PRIV_INCLUDE_DIRS ${root}/system/services/display ${root}/third_party/jsmn ${root}/third_party/stb ${CMAKE_BINARY_DIR}/generated
    REQUIRES ink_platform ink_lvgl)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage -fcallgraph-info=su -Wextra -Wno-unused-parameter -include esp_attr.h)
target_compile_definitions(${COMPONENT_LIB} PRIVATE INK_PSRAM_BSS=EXT_RAM_BSS_ATTR)
# The two codec translation units include upstream stb implementations.
set_source_files_properties(${root}/system/services/media/image_codec.c ${root}/system/apps/camera/png.c
    PROPERTIES COMPILE_OPTIONS -w)
execute_process(COMMAND ${PYTHON} ${root}/tools/icongen/check.py --header ${CMAKE_BINARY_DIR}/generated/ink/ui_icon_names.h
    WORKING_DIRECTORY ${root} COMMAND_ERROR_IS_FATAL ANY)
