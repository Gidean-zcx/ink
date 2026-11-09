set(root ${CMAKE_CURRENT_LIST_DIR}/../../../..)
file(GLOB_RECURSE sources ${root}/drivers/*.c ${root}/boards/metalio_plus/*.c)
idf_component_register(SRCS ${sources} INCLUDE_DIRS ${root}/drivers/chips ${root}/drivers/display/pqbn ${root}/boards
    REQUIRES ink_platform)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage -Wextra -Wno-unused-parameter)
