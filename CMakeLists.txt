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
  ports/sim/hal_power_sim.c
  ports/sim/hal_assets_sim.c
  ports/sim/hal_net_sim.c
  ports/sim/sim_hub.c)
target_include_directories(ink_port_sim PUBLIC ports/sim platform/include)
target_link_libraries(ink_port_sim PUBLIC ink_sim_core ink_littlefs ink_stb)
target_compile_options(ink_port_sim PRIVATE ${INK_WARN})

# An executable = the simulator runner + the code under test (which provides ink_main()).
function(ink_sim_executable name)
  add_executable(${name} ports/sim/sim_main.c ${ARGN})
  target_link_libraries(${name} PRIVATE ink_port_sim)
  target_compile_options(${name} PRIVATE ${INK_WARN})
endfunction()

# ---------------------------------------------------------------- LVGL (pinned)
# Local mirror: -DFETCHCONTENT_SOURCE_DIR_LVGL=/path/to/lvgl (no network needed)
include(FetchContent)
FetchContent_Declare(lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG v9.5.0
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR do-not-add-subdirectory)
FetchContent_MakeAvailable(lvgl)
file(GLOB_RECURSE INK_LVGL_SOURCES CONFIGURE_DEPENDS ${lvgl_SOURCE_DIR}/src/*.c)
add_library(ink_lvgl STATIC ${INK_LVGL_SOURCES} system/ui/lv_mem_osal.c)
target_include_directories(ink_lvgl PUBLIC ${lvgl_SOURCE_DIR} system/ui platform/include)
target_compile_definitions(ink_lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
target_compile_options(ink_lvgl PRIVATE -w)

# ---------------------------------------------------------------- portable system + apps
add_library(ink_system STATIC
  system/main.c
  system/display/display_service.c
  system/display/refresh_policy.c
  system/input/input_service.c
  system/storage/storage_service.c
  system/core/ink_json.c
  system/net/hub_link.c
  system/ai/ai_service.c
  system/text/ink_font.c
  system/text/linebreak.c
  system/text/layout.c
  system/ui/ui_task.c
  apps/home/home.c
  apps/reader/reader.c)
target_include_directories(ink_system PUBLIC system/include platform/include system/display third_party/jsmn)
target_link_libraries(ink_system PUBLIC ink_lvgl)
target_compile_options(ink_system PRIVATE ${INK_WARN})

# ---------------------------------------------------------------- assets (generated)
# CJK bitmap fonts rendered from Noto Sans CJK (SIL OFL 1.1) by tools/fontgen.
set(INK_CJK_FONT "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc" CACHE FILEPATH "CJK font for the bitmap fonts")
set(INK_CJK_FACE 2 CACHE STRING "Face index in the collection (Noto Sans CJK .ttc: 2 = Simplified Chinese)")
set(INK_ASSETS_DIR ${CMAKE_BINARY_DIR}/assets)
find_package(Python3 COMPONENTS Interpreter)
set(INK_HAVE_FONTS OFF)
if(EXISTS ${INK_CJK_FONT} AND Python3_FOUND)
  set(INK_FONT_FILES)
  foreach(px 24 32)
    set(out ${INK_ASSETS_DIR}/fonts/sans-${px}.inkf)
    add_custom_command(OUTPUT ${out}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${INK_ASSETS_DIR}/fonts
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/fontgen/fontgen.py --font ${INK_CJK_FONT}
              --face ${INK_CJK_FACE} --px ${px} --bpp 2 --charset ascii,punct,pinyin,gb2312 -o ${out}
      DEPENDS ${CMAKE_SOURCE_DIR}/tools/fontgen/fontgen.py
      COMMENT "fontgen: CJK bitmap font ${px}px")
    list(APPEND INK_FONT_FILES ${out})
  endforeach()
  add_custom_target(ink_fonts ALL DEPENDS ${INK_FONT_FILES})
  set(INK_HAVE_FONTS ON)
else()
  message(WARNING "No CJK font (${INK_CJK_FONT}) or Python 3: CJK fonts not generated, reader scenarios skipped. "
                  "Debian/Ubuntu: apt install fonts-noto-cjk python3-pil")
endif()

# Scenario content: the committed sample books + a generated long book (pagination stress).
set(INK_TEST_CONTENT ${CMAKE_BINARY_DIR}/test-content)
add_custom_command(OUTPUT ${INK_TEST_CONTENT}/books/测试长书.txt
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/tests/content ${INK_TEST_CONTENT}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/testdata/make_long_book.py
          ${CMAKE_SOURCE_DIR}/tests/content/books/星星邮差.txt ${INK_TEST_CONTENT}/books/测试长书.txt 40
  DEPENDS ${CMAKE_SOURCE_DIR}/tools/testdata/make_long_book.py ${CMAKE_SOURCE_DIR}/tests/content/books/星星邮差.txt
  COMMENT "test content: long book")
add_custom_target(ink_test_content ALL DEPENDS ${INK_TEST_CONTENT}/books/测试长书.txt)

# The full system on the simulated device.
add_executable(ink-sim ports/sim/sim_main.c)
target_link_libraries(ink-sim PRIVATE ink_system ink_lvgl ink_port_sim)
target_compile_options(ink-sim PRIVATE ${INK_WARN})

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

# System scenarios: the real system on simulated devices, judged by budgets.
function(ink_scenario name profile)
  add_test(NAME scenario-${name}-${profile}
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/profiles/${profile}.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/${name}.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/${name}.budget.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/${name}-${profile} --quiet --trace --frames --no-random-outages)
endfunction()
ink_scenario(home_basic metalio-plus)
ink_scenario(home_basic metalio-plus-psram16)
if(INK_HAVE_FONTS)
  ink_scenario(reader_basic metalio-plus)
  ink_scenario(reader_basic metalio-plus-psram16)
  ink_scenario(ai_ask metalio-plus)
endif()
# Portability smoke runs: same system, other panels/classes (budget: no panic, input works)
foreach(p pinenote-rk3566 desktop-lcd)
  add_test(NAME portability-home-${p}
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/profiles/${p}.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/home_basic.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/portability.budget.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/portability-${p} --quiet)
endforeach()
