# ink — portable system + simulator build (host).
#
# Device firmware is built with ESP-IDF (ports/esp32s31, later); this file
# builds everything that runs on a PC: the simulator, the POSIX port, tests.
#
#   cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(ink C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
option(INK_WERROR "Treat warnings in ink code as errors" OFF)
option(INK_SANITIZE "Build with AddressSanitizer + UBSan" OFF)

find_package(Threads REQUIRED)

set(INK_WARN -Wall -Wextra -Wshadow -Wno-unused-parameter)
if(INK_WERROR)
  list(APPEND INK_WARN -Werror)
endif()
if(INK_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------- third party
add_library(ink_tlsf STATIC third_party/tlsf/tlsf.c)
target_include_directories(ink_tlsf PUBLIC third_party/tlsf)

add_library(ink_cjson STATIC third_party/cjson/cJSON.c)
target_include_directories(ink_cjson PUBLIC third_party/cjson)

add_library(ink_littlefs STATIC third_party/littlefs/lfs.c third_party/littlefs/lfs_util.c)
target_include_directories(ink_littlefs PUBLIC third_party/littlefs)
# All LittleFS buffers come from the device heaps (osal_malloc), never libc.
target_compile_definitions(ink_littlefs PUBLIC LFS_NO_MALLOC LFS_NO_DEBUG LFS_NO_TRACE)

# ---------------------------------------------------------------- simulator core
add_library(ink_sim_core STATIC sim/vtk/vtk.c sim/mem/pools.c sim/profile.c)
target_include_directories(ink_sim_core PUBLIC sim sim/vtk sim/mem platform/include)
target_link_libraries(ink_sim_core PUBLIC ink_tlsf ink_cjson Threads::Threads m)
target_compile_options(ink_sim_core PRIVATE ${INK_WARN})

add_library(ink_stb STATIC ports/sim/stb_impl.c)
target_include_directories(ink_stb PUBLIC third_party/stb)
target_compile_options(ink_stb PRIVATE -w)

add_library(ink_port_sim STATIC
  ports/sim/osal_sim.c
  ports/sim/metrics.c
  ports/sim/trace.c
  ports/sim/script.c
  ports/sim/hal_display_sim.c
  ports/sim/hal_input_sim.c
  ports/sim/hal_storage_sim.c
  ports/sim/hal_power_sim.c)
target_include_directories(ink_port_sim PUBLIC ports/sim platform/include)
target_link_libraries(ink_port_sim PUBLIC ink_sim_core ink_littlefs ink_stb)
target_compile_options(ink_port_sim PRIVATE ${INK_WARN})

# An executable = the simulator runner + the code under test (which provides ink_main()).
function(ink_sim_executable name)
  add_executable(${name} ports/sim/sim_main.c ${ARGN})
  target_link_libraries(${name} PRIVATE ink_port_sim)
  target_compile_options(${name} PRIVATE ${INK_WARN})
endfunction()

# ---------------------------------------------------------------- tests
enable_testing()
ink_sim_executable(sim-selftest tests/sim/selftest.c)
foreach(p metalio-plus metalio-plus-psram16 pinenote-rk3566 desktop-lcd)
  add_test(NAME selftest-${p}
    COMMAND sim-selftest --profile ${CMAKE_SOURCE_DIR}/profiles/${p}.json
            --script ${CMAKE_SOURCE_DIR}/tests/sim/selftest.script
            --budget ${CMAKE_SOURCE_DIR}/tests/sim/selftest.budget.json
            --out ${CMAKE_BINARY_DIR}/test-out/selftest-${p} --quiet)
endforeach()
add_test(NAME determinism
  COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:sim-selftest>
          -DPROFILE=${CMAKE_SOURCE_DIR}/profiles/metalio-plus.json
          -DSCRIPT=${CMAKE_SOURCE_DIR}/tests/sim/selftest.script
          -DOUT=${CMAKE_BINARY_DIR}/test-out/determinism
          -P ${CMAKE_SOURCE_DIR}/tests/sim/determinism.cmake)
