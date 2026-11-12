set(root ${CMAKE_CURRENT_LIST_DIR}/../../../..)
file(GLOB_RECURSE sources ${root}/apps/*.c)
# Mogo sources are selected by the shared native-reader build function.
list(FILTER sources EXCLUDE REGEX "/apps/mogo/")
idf_component_register(SRCS ${sources} REQUIRES ink_system)
target_compile_options(${COMPONENT_LIB} PRIVATE -Os -fstack-usage -Wextra -Wno-unused-parameter)
