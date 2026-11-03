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
  drivers/epd/pqbn_v4.c
  drivers/tca9555/tca9555.c
  drivers/tps65185/tps65185.c
  drivers/board/metalio_epd_power.c
  ports/sim/hal_i2c_sim.c
  ports/sim/dev_epd_power.c
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
  ports/sim/hal_audio_sim.c
  ports/sim/hal_codec_sim.c
  ports/sim/sim_hub.c)
target_include_directories(ink_port_sim PUBLIC ports/sim platform/include drivers/epd drivers)
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
  system/core/actions.c
  system/core/members.c
  system/core/system_actions.c
  system/power/power_service.c
  system/ui/lock_mark.c
  system/net/hub_link.c
  system/ai/ai_service.c
  system/audio/audio_service.c
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
# Death test: a task with a PSRAM stack touching /user must crash the simulated device, as ESP-IDF does.
add_test(NAME selftest-psram-stack-flash-access-crashes
  COMMAND sim-selftest --profile ${CMAKE_SOURCE_DIR}/profiles/metalio-plus.json
          --out ${CMAKE_BINARY_DIR}/test-out/selftest-death --quiet --end 2s)
set_tests_properties(selftest-psram-stack-flash-access-crashes PROPERTIES
  ENVIRONMENT "INK_SELFTEST_DEATH=psram_flash"
  PASS_REGULAR_EXPRESSION "stack in PSRAM and accessed /user")
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
# Panel power safety (D-41): the board driver against the PMIC model must flag a VCOM that differs
# from the panel's (here a panel labelled 1.50 V while the board drives 1.20 V).
add_test(NAME selftest-panel-vcom-mismatch
  COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/profiles/metalio-plus.json
          --script ${CMAKE_SOURCE_DIR}/tests/scenarios/home_basic.script
          --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
          --out ${CMAKE_BINARY_DIR}/test-out/selftest-vcom --no-random-outages --panel-vcom-mv 1500 --end 8s)
set_tests_properties(selftest-panel-vcom-mismatch PROPERTIES
  PASS_REGULAR_EXPRESSION "VCOM driven at -1200 mV but the panel needs -1500 mV")
if(INK_HAVE_FONTS)
  ink_scenario(reader_basic metalio-plus)
  ink_scenario(keys_overlap metalio-plus)
  ink_scenario(reader_basic metalio-plus-psram16)
  ink_scenario(ai_ask metalio-plus)
  ink_scenario(standby metalio-plus)
  # regressions from the 2026-11 audit review (docs/audits/README.md): position saved while reading
  # on, fast page flipping, late AI speech after the sheet is closed, a book that cannot be opened
  ink_scenario(reader_continuous metalio-plus)
  ink_scenario(reader_rapid metalio-plus)
  ink_scenario(ai_dismiss metalio-plus)
  add_test(NAME scenario-reader_empty-metalio-plus
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_empty.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_empty.budget.json
            --assets ${INK_ASSETS_DIR} --content ${CMAKE_SOURCE_DIR}/tests/content-bad
            --out ${CMAKE_BINARY_DIR}/test-out/reader_empty-metalio-plus --quiet --trace --frames --no-random-outages)
  # PQ-BN direct drive (research PR #7) on the same scenarios and budgets, plus its flash pause
  add_test(NAME scenario-reader_basic-metalio-plus-pqbn
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/profiles/metalio-plus-pqbn.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_basic.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_basic.pqbn.budget.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/reader_basic-metalio-plus-pqbn --quiet --trace --frames --no-random-outages)
  ink_scenario(ai_ask metalio-plus-pqbn)
  add_test(NAME scenario-pqbn_flash_pause-metalio-plus-pqbn
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/profiles/metalio-plus-pqbn.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/pqbn_flash_pause.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/pqbn_flash_pause.budget.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/pqbn_flash_pause --quiet --trace --no-random-outages)
  add_test(NAME scenario-ai_tools-metalio-plus
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_tools.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_tools.budget.json
            --hub ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_tools.hub.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/ai_tools-metalio-plus --quiet --trace --frames --no-random-outages)
  # Power cut in the middle of a position save, reboot from the torn flash image.
  add_test(NAME scenario-powercut-during-save
    COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:ink-sim>
            -DPROFILE=${CMAKE_SOURCE_DIR}/profiles/metalio-plus.json
            -DOUT=${CMAKE_BINARY_DIR}/test-out/powercut -DSRC=${CMAKE_SOURCE_DIR}/tests/scenarios
            -DASSETS=${INK_ASSETS_DIR} -DCONTENT=${INK_TEST_CONTENT}
            -P ${CMAKE_SOURCE_DIR}/tests/scenarios/powercut.cmake)
  # The real system must be bit-for-bit deterministic too (budgets compare single runs).
  add_test(NAME determinism-reader
    COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:ink-sim>
            -DPROFILE=${CMAKE_SOURCE_DIR}/profiles/metalio-plus.json
            -DSCRIPT=${CMAKE_SOURCE_DIR}/tests/scenarios/reader_basic.script
            -DOUT=${CMAKE_BINARY_DIR}/test-out/determinism-reader
            "-DEXTRA=--assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT} --no-random-outages"
            -P ${CMAKE_SOURCE_DIR}/tests/sim/determinism.cmake)
  # Same reader on the upstream display pipeline (timing comparison; lenient budget).
  add_test(NAME scenario-reader_basic-upstream-pipeline
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/profiles/metalio-plus-upstream-display.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_basic.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/upstream_pipeline.budget.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/reader_basic-upstream-pipeline --quiet --trace --no-random-outages)
  # Aged /user (50-60 % full, fragmented) and worst-case flash timing: the reader must still
  # meet its budgets while position saves hit a used filesystem.
  add_test(NAME fixture-aged-user-image
    COMMAND ${CMAKE_COMMAND} -DMODE=make -DSIM=$<TARGET_FILE:sim-selftest>
            -DPROFILE=${CMAKE_SOURCE_DIR}/profiles/metalio-plus.json
            -DIMAGE=${CMAKE_BINARY_DIR}/test-out/aged-user.img -DOUT=${CMAKE_BINARY_DIR}/test-out/aging
            -P ${CMAKE_SOURCE_DIR}/tests/scenarios/aged_image.cmake)
  set_tests_properties(fixture-aged-user-image PROPERTIES FIXTURES_SETUP aged_user)
  add_test(NAME scenario-reader_basic-aged-flash-max
    COMMAND ${CMAKE_COMMAND} -DMODE=run -DSIM=$<TARGET_FILE:ink-sim>
            -DPROFILE=${CMAKE_SOURCE_DIR}/profiles/metalio-plus.json
            -DIMAGE=${CMAKE_BINARY_DIR}/test-out/aged-user.img -DOUT=${CMAKE_BINARY_DIR}/test-out/reader_basic-aged
            -DSCRIPT=${CMAKE_SOURCE_DIR}/tests/scenarios/reader_basic.script
            -DBUDGET=${CMAKE_SOURCE_DIR}/tests/scenarios/reader_basic.budget.json
            -DASSETS=${INK_ASSETS_DIR} -DCONTENT=${INK_TEST_CONTENT}
            "-DEXTRA=--flash-timing max --no-random-outages --trace"
            -P ${CMAKE_SOURCE_DIR}/tests/scenarios/aged_image.cmake)
  set_tests_properties(scenario-reader_basic-aged-flash-max PROPERTIES FIXTURES_REQUIRED aged_user)
  # Shared-bus faults on the panel power path (D-41): 2 % of hal_i2c transfers time out, as one did on
  # the device in phase 0; the driver must recover by its retries and never scan an unpowered panel.
  add_test(NAME scenario-reader_basic-panel-power-faults
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_basic.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/panel_power_faults.budget.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/reader_basic-panel-power-faults --quiet --no-random-outages
            --i2c-faults 0.02)
endif()
# Portability smoke runs: same system, other panels/classes (budget: no panic, input works)
foreach(p pinenote-rk3566 desktop-lcd metalio-plus-pqbn)
  add_test(NAME portability-home-${p}
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/profiles/${p}.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/home_basic.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/portability.budget.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/portability-${p} --quiet)
endforeach()
# ---------------------------------------------------------------- ISS (Level 2 calibration)
# tools/iss: instruction-set simulator used by tools/iss/calibrate.sh to measure
# cycles per unit for the cost classes. The self-test needs no cross compiler.
set(ISS_SRC tools/iss/src/iss.c tools/iss/src/iss_cfg.c tools/iss/src/iss_elf.c)
add_executable(iss ${ISS_SRC} tools/iss/src/main.c)
add_executable(iss-selftest ${ISS_SRC} tools/iss/test/iss_selftest.c)
foreach(t iss iss-selftest)
  target_include_directories(${t} PRIVATE tools/iss/src)
  target_compile_options(${t} PRIVATE ${INK_WARN} -frounding-math)
  target_link_libraries(${t} PRIVATE m)
endforeach()
add_test(NAME iss-selftest COMMAND iss-selftest)
