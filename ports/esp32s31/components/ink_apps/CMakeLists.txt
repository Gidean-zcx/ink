set(root ${CMAKE_CURRENT_LIST_DIR}/../../../..)
file(GLOB_RECURSE sources ${root}/apps/*.c)
idf_component_register(SRCS ${sources} REQUIRES ink_system)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage -Wextra -Wno-unused-parameter)
