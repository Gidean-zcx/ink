set(mquickjs "${CMAKE_CURRENT_LIST_DIR}/../../work/appfw-spike/mquickjs-4119f6819ccb5bae2d21a18a106e89522d18ad45")
set(mquickjs_gen "${CMAKE_BINARY_DIR}/appfw-mquickjs-generated")
file(MAKE_DIRECTORY "${mquickjs_gen}")
find_program(spike_host_cc NAMES cc REQUIRED)
execute_process(COMMAND "${spike_host_cc}" -O2 -I "${mquickjs}"
  "${CMAKE_CURRENT_LIST_DIR}/mquickjs_stdlib.c" "${mquickjs}/mquickjs_build.c"
  -o "${mquickjs_gen}/stdlib-build" COMMAND_ERROR_IS_FATAL ANY)
set(mquickjs_word_flag)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(mquickjs_word_flag -m32)
endif()
execute_process(COMMAND "${mquickjs_gen}/stdlib-build" -a ${mquickjs_word_flag}
  OUTPUT_FILE "${mquickjs_gen}/mquickjs_atom.h" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${mquickjs_gen}/stdlib-build" ${mquickjs_word_flag}
  OUTPUT_FILE "${mquickjs_gen}/ink_mquickjs_stdlib.h" COMMAND_ERROR_IS_FATAL ANY)
add_library(appfw_mquickjs_vm STATIC EXCLUDE_FROM_ALL
  "${mquickjs}/mquickjs.c" "${mquickjs}/dtoa.c" "${mquickjs}/libm.c" "${mquickjs}/cutils.c")
target_include_directories(appfw_mquickjs_vm SYSTEM PUBLIC "${mquickjs}" "${mquickjs_gen}")
target_link_libraries(appfw_mquickjs_vm PUBLIC m)
