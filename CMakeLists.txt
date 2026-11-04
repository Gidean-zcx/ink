# ink — portable system + simulator build (host).
#
# Device firmware is built with ESP-IDF (ports/esp32s31, later); this file
# builds everything that runs on a PC: the simulator, the POSIX port, tests.
#
#   cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(ink C)
if(APPLE)
  enable_language(OBJC) # before SDL2's own configure (FetchContent) and the window's camera code
endif()

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
# Bind symbols at load (macOS does already): lazily, the dynamic linker would run on the stack of
# whichever simulated task first calls a libc function, with a CPU-dependent size (XSAVE area).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_link_options(-Wl,-z,now)
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

# ---------------------------------------------------------------- layers
# One target per layer (docs/system/repo-layout.md), from the bottom up; each exposes only its own
# headers. platform/include is the contract between the portable code and a port.
# LZ4 block format: the device decodes the remote screen, the simulated hub encodes it
add_library(ink_lz4 STATIC system/util/lz4.c)
target_include_directories(ink_lz4 PUBLIC system/include)

add_library(ink_platform INTERFACE)
target_include_directories(ink_platform INTERFACE platform/include)

# Chip drivers and the panel algorithm: portable, built the same for the device (ESP-IDF).
add_library(ink_drivers STATIC
  drivers/chips/tca9555/tca9555.c
  drivers/chips/tps65185/tps65185.c
  drivers/chips/ft6336u/ft6336u.c
  drivers/chips/bq27220/bq27220.c
  drivers/chips/cx25601n/cx25601n.c
  drivers/chips/pcf8563/pcf8563.c
  drivers/chips/sc7a20h/sc7a20h.c
  drivers/chips/gc2145/gc2145.c
  drivers/chips/bh1098g/bh1098g.c
  drivers/chips/nt26/nt26.c
  drivers/display/pqbn/pqbn_v4.c)
target_include_directories(ink_drivers PUBLIC drivers/chips drivers/display/pqbn)
target_link_libraries(ink_drivers PUBLIC ink_platform m) # m: the PQ-BN kernel's table build (expf, lrintf)
target_compile_options(ink_drivers PRIVATE ${INK_WARN})

# The Metalio E-Ink4 Plus: its wiring, the shared expander, panel power, touch coordinates.
add_library(ink_board_metalio_plus STATIC
  boards/metalio_plus/metalio_board.c
  boards/metalio_plus/metalio_epd_power.c
  boards/metalio_plus/metalio_touch.c)
target_include_directories(ink_board_metalio_plus PUBLIC boards)
target_link_libraries(ink_board_metalio_plus PUBLIC ink_drivers ink_platform)
target_compile_options(ink_board_metalio_plus PRIVATE ${INK_WARN})

# Simulator kernel: virtual time, device memory pools, device profiles.
add_library(ink_sim_core STATIC simulator/kernel/vtk/vtk.c simulator/memory/pools.c simulator/runtime/profile.c)
target_include_directories(ink_sim_core PUBLIC simulator/kernel/vtk simulator/memory simulator/runtime)
target_link_libraries(ink_sim_core PUBLIC ink_platform ink_tlsf ink_cjson Threads::Threads m)
target_compile_options(ink_sim_core PRIVATE ${INK_WARN})

add_library(ink_stb STATIC simulator/runtime/stb_impl.c)
target_include_directories(ink_stb PUBLIC third_party/stb)
target_compile_options(ink_stb PRIVATE -w)

# The simulated device: the port (OSAL and HAL on the models), the chip models, the environment
# (hub, signals), the runner's metrics, trace and scripts, the desktop bridge. One library while
# models and HAL implementations still call each other (see the layout doc).
add_library(ink_port_sim STATIC
  ports/sim/osal/osal_sim.c
  ports/sim/hal/hal_i2c_sim.c
  ports/sim/hal/hal_sensors_sim.c
  ports/sim/hal/hal_display_sim.c
  ports/sim/hal/hal_input_sim.c
  ports/sim/hal/hal_storage_sim.c
  ports/sim/hal/hal_power_sim.c
  ports/sim/hal/hal_assets_sim.c
  ports/sim/hal/hal_net_sim.c
  ports/sim/hal/hal_audio_sim.c
  ports/sim/hal/hal_cellular_sim.c
  ports/sim/hal/hal_afe_sim.c
  ports/sim/hal/hal_jpeg_sim.c
  ports/sim/hal/hal_codec_sim.c
  simulator/models/dev_epd_power.c
  simulator/models/dev_tca9555.c
  simulator/models/dev_power_chips.c
  simulator/models/dev_rtc_accel.c
  simulator/models/dev_camera.c
  simulator/models/dev_bt_audio.c
  simulator/environment/sim_hub.c
  simulator/environment/sim_remote.c
  simulator/runtime/metrics.c
  simulator/runtime/trace.c
  simulator/runtime/script.c
  simulator/desktop/sim_gui.c)
target_include_directories(ink_port_sim PUBLIC simulator simulator/desktop)
target_link_libraries(ink_port_sim PUBLIC ink_board_metalio_plus ink_drivers ink_sim_core ink_littlefs ink_stb ink_lz4)
target_compile_options(ink_port_sim PRIVATE ${INK_WARN})

# An executable = the simulator runner + the code under test (which provides ink_main()).
function(ink_sim_executable name)
  add_executable(${name} simulator/runtime/sim_main.c ${ARGN})
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
  system/services/display/display_service.c
  system/services/display/light.c
  system/services/camera/camera_service.c
  system/services/comm/comm_service.c
  system/services/net/net_service.c
  system/services/display/refresh_policy.c
  system/services/input/input_service.c
  system/services/storage/storage_service.c
  system/services/power/power_service.c
  system/services/hub/hub_link.c
  system/services/hub/rt.c
  system/services/ai/ai_service.c
  system/services/audio/audio_service.c
  system/services/res/exec.c
  system/services/res/res.c
  system/policy/cap.c
  system/policy/members.c
  system/policy/settings.c
  system/policy/state.c
  system/policy/system_actions.c
  system/policy/watch.c
  system/util/ink_args.c
  system/util/ink_json.c
  system/text/ink_font.c
  system/text/linebreak.c
  system/text/layout.c
  system/ui/lock_mark.c
  system/ui/ui_task.c
  system/ui/app.c
  system/apps/remote/remote.c)
target_include_directories(ink_system PUBLIC system/include PRIVATE system/services/display third_party/jsmn)
target_link_libraries(ink_system PUBLIC ink_platform ink_lvgl ink_lz4)
# The system calls the HAL; in this build the simulator port implements it. Link-only (no headers
# leak into the system or the apps), and it puts the port after the system on every link line, which
# GNU ld needs for HAL functions only the system references (e.g. hal_cell_*).
target_link_libraries(ink_system INTERFACE $<LINK_ONLY:ink_port_sim>)
target_compile_options(ink_system PRIVATE ${INK_WARN})

add_library(ink_apps STATIC apps/home/home.c apps/reader/reader.c)
target_link_libraries(ink_apps PUBLIC ink_system)
target_compile_options(ink_apps PRIVATE ${INK_WARN})

# The product: ink_main() starts the services and the apps (the composition root).
add_library(ink_boot STATIC system/boot/main.c)
target_link_libraries(ink_boot PUBLIC ink_apps ink_system)
target_compile_options(ink_boot PRIVATE ${INK_WARN})

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
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/tests/fixtures/content ${INK_TEST_CONTENT}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/testdata/make_long_book.py
          ${CMAKE_SOURCE_DIR}/tests/fixtures/content/books/星星邮差.txt ${INK_TEST_CONTENT}/books/测试长书.txt 40
  DEPENDS ${CMAKE_SOURCE_DIR}/tools/testdata/make_long_book.py ${CMAKE_SOURCE_DIR}/tests/fixtures/content/books/星星邮差.txt
  COMMENT "test content: long book")
add_custom_target(ink_test_content ALL DEPENDS ${INK_TEST_CONTENT}/books/测试长书.txt)

# The full system on the simulated device.
add_executable(ink-sim simulator/runtime/sim_main.c)
target_link_libraries(ink-sim PRIVATE ink_boot ink_port_sim)
target_compile_options(ink-sim PRIVATE ${INK_WARN})

# ---------------------------------------------------------------- desktop simulator (R-042, D-43)
# ink-sim-gui: the same simulation in a window (SDL2 + Dear ImGui). Uses the system's SDL2 when
# installed (Debian/Ubuntu: libsdl2-dev; macOS: brew install sdl2), otherwise builds a pinned SDL2.
option(INK_GUI "Build the desktop simulator ink-sim-gui" ON)
if(INK_GUI)
  enable_language(CXX)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(INK_FETCH_SYSTEM)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.25)
    set(INK_FETCH_SYSTEM SYSTEM)
  endif()
  find_package(SDL2 CONFIG QUIET)
  if(SDL2_FOUND AND TARGET SDL2::SDL2)
    set(INK_SDL2 SDL2::SDL2)
    message(STATUS "ink-sim-gui: SDL2 ${SDL2_VERSION} from the system")
  else()
    message(STATUS "ink-sim-gui: no SDL2 installed, building release-2.32.10 from source")
    foreach(opt SDL_SHARED SDL_TEST SDL_TESTS SDL_TEST_LIBRARY SDL_JOYSTICK SDL_HAPTIC SDL_HIDAPI SDL_SENSOR)
      set(${opt} OFF CACHE BOOL "" FORCE)
    endforeach()
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(sdl2
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-2.32.10
      GIT_SHALLOW TRUE
      ${INK_FETCH_SYSTEM})
    FetchContent_MakeAvailable(sdl2)
    set(INK_SDL2 SDL2::SDL2-static)
  endif()
  FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(imgui)
  add_library(ink_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp)
  target_include_directories(ink_imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(ink_imgui PUBLIC ${INK_SDL2})
  target_compile_options(ink_imgui PRIVATE -w)

  # Every character the window's labels use goes into its font atlas.
  set(INK_GUI_GLYPHS ${CMAKE_BINARY_DIR}/gui/gui_glyphs.h)
  if(Python3_FOUND)
    add_custom_command(OUTPUT ${INK_GUI_GLYPHS}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/gui
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/simulator/desktop/gui/glyphs.py -o ${INK_GUI_GLYPHS}
              ${CMAKE_SOURCE_DIR}/simulator/desktop/gui/gui_main.cpp
      DEPENDS ${CMAKE_SOURCE_DIR}/simulator/desktop/gui/glyphs.py ${CMAKE_SOURCE_DIR}/simulator/desktop/gui/gui_main.cpp
      COMMENT "ink-sim-gui: glyphs used by the window")
  else()
    file(WRITE ${INK_GUI_GLYPHS} "static const char kGuiGlyphs[] = \"\";\n")
  endif()
  if(APPLE)
    set(INK_HOST_CAMERA simulator/desktop/gui/host_camera_mac.m)
    set_source_files_properties(simulator/desktop/gui/host_camera_mac.m PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(INK_HOST_CAMERA simulator/desktop/gui/host_camera_linux.c)
  else()
    message(FATAL_ERROR "ink-sim-gui: no computer-camera backend for ${CMAKE_SYSTEM_NAME}")
  endif()
  add_executable(ink-sim-gui simulator/runtime/sim_main.c simulator/desktop/gui/gui_main.cpp ${INK_HOST_CAMERA} ${INK_GUI_GLYPHS})
  if(APPLE)
    target_link_libraries(ink-sim-gui PRIVATE "-framework AVFoundation" "-framework CoreMedia" "-framework CoreVideo" "-framework Foundation")
    # usage descriptions, so macOS can ask for the camera and the microphone
    target_link_options(ink-sim-gui PRIVATE "LINKER:-sectcreate,__TEXT,__info_plist,${CMAKE_SOURCE_DIR}/simulator/desktop/gui/Info.plist")
  endif()
  target_compile_definitions(ink-sim-gui PRIVATE INK_SIM_NO_MAIN
    INK_SOURCE_DIR="${CMAKE_SOURCE_DIR}" INK_BINARY_DIR="${CMAKE_BINARY_DIR}"
    INK_GUI_FONT="${INK_CJK_FONT}" INK_GUI_FONT_FACE=${INK_CJK_FACE})
  target_include_directories(ink-sim-gui PRIVATE ${CMAKE_BINARY_DIR}/gui)
  target_link_libraries(ink-sim-gui PRIVATE ink_boot ink_port_sim ink_imgui)
  target_compile_options(ink-sim-gui PRIVATE ${INK_WARN})
endif()

# ---------------------------------------------------------------- tests
enable_testing()
ink_sim_executable(sim-selftest tests/simulator/selftest.c)
foreach(p metalio-plus metalio-plus-psram16 pinenote-rk3566 desktop-lcd)
  add_test(NAME selftest-${p}
    COMMAND sim-selftest --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/${p}.json
            --script ${CMAKE_SOURCE_DIR}/tests/simulator/selftest.script
            --budget ${CMAKE_SOURCE_DIR}/tests/simulator/selftest.budget.json
            --out ${CMAKE_BINARY_DIR}/test-out/selftest-${p} --quiet)
endforeach()
# Death test: a task with a PSRAM stack touching /user must crash the simulated device, as ESP-IDF does.
add_test(NAME selftest-psram-stack-flash-access-crashes
  COMMAND sim-selftest --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          --out ${CMAKE_BINARY_DIR}/test-out/selftest-death --quiet --end 2s)
set_tests_properties(selftest-psram-stack-flash-access-crashes PROPERTIES
  ENVIRONMENT "INK_SELFTEST_DEATH=psram_flash"
  PASS_REGULAR_EXPRESSION "stack in PSRAM and accessed /user")
# The modem's two SIM slots: online switches, an empty slot, switching back (INK_SELFTEST_CELL).
add_test(NAME selftest-sim-slots
  COMMAND sim-selftest --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          --budget ${CMAKE_SOURCE_DIR}/tests/simulator/cell.budget.json
          --out ${CMAKE_BINARY_DIR}/test-out/selftest-sim-slots --quiet --end 60s --no-random-outages)
set_tests_properties(selftest-sim-slots PROPERTIES ENVIRONMENT "INK_SELFTEST_CELL=1")
# The Plus board's IO expander wiring: motor, shared reset and supply of the modem, power-off pulses.
add_test(NAME selftest-board-wiring
  COMMAND sim-selftest --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          --budget ${CMAKE_SOURCE_DIR}/tests/simulator/board.budget.json
          --out ${CMAKE_BINARY_DIR}/test-out/selftest-board --quiet --end 90s --no-random-outages)
set_tests_properties(selftest-board-wiring PROPERTIES ENVIRONMENT "INK_SELFTEST_BOARD=1")
add_test(NAME selftest-board-wiring-pqbn
  COMMAND sim-selftest --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus-pqbn.json
          --budget ${CMAKE_SOURCE_DIR}/tests/simulator/board.pqbn.budget.json
          --out ${CMAKE_BINARY_DIR}/test-out/selftest-board-pqbn --quiet --end 90s --no-random-outages)
set_tests_properties(selftest-board-wiring-pqbn PROPERTIES ENVIRONMENT "INK_SELFTEST_BOARD=1")
add_test(NAME selftest-expander-dead
  COMMAND sim-selftest --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          --budget ${CMAKE_SOURCE_DIR}/tests/simulator/expander_dead.budget.json
          --out ${CMAKE_BINARY_DIR}/test-out/selftest-expander-dead --quiet --end 8s --no-random-outages
          --i2c-faults 1.0:io-expander)
set_tests_properties(selftest-expander-dead PROPERTIES ENVIRONMENT "INK_SELFTEST_EXPANDER_DEAD=1")
# Unit tests: one part on its own, checked against its reference (here the pools against TLSF).
add_executable(pools-test tests/unit/pools_test.c)
target_include_directories(pools-test PRIVATE simulator/memory simulator/kernel/vtk)
target_link_libraries(pools-test PRIVATE ink_platform ink_tlsf)
target_compile_options(pools-test PRIVATE ${INK_WARN})
add_test(NAME unit-pools COMMAND pools-test)
add_test(NAME sdk-boundary COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/tests/sdk_boundary.cmake)
add_executable(pqbn-clock-test tests/unit/pqbn_clock_test.c)
target_link_libraries(pqbn-clock-test PRIVATE ink_drivers)
target_compile_options(pqbn-clock-test PRIVATE ${INK_WARN})
add_test(NAME unit-pqbn-clock COMMAND pqbn-clock-test)
# System service checks (storage cache versions; tests/integration/systest.c), on the services themselves.
add_executable(sim-systest simulator/runtime/sim_main.c tests/integration/systest.c)
target_link_libraries(sim-systest PRIVATE ink_system ink_port_sim)
target_compile_options(sim-systest PRIVATE ${INK_WARN})
add_test(NAME systest-services
  COMMAND sim-systest --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          --budget ${CMAKE_SOURCE_DIR}/tests/integration/systest.budget.json
          --script ${CMAKE_SOURCE_DIR}/tests/integration/systest.script
          --out ${CMAKE_BINARY_DIR}/test-out/systest --quiet --no-random-outages)
# Calls, jobs, captures and the remote picture at awkward moments (tests/integration/lifecycle.c).
add_executable(sim-lifecycle simulator/runtime/sim_main.c tests/integration/lifecycle.c)
target_link_libraries(sim-lifecycle PRIVATE ink_system ink_port_sim)
target_compile_options(sim-lifecycle PRIVATE ${INK_WARN})
# Starting an executor under a higher-priority caller; a session whose request cannot be made (tests/integration/starts.c).
add_executable(sim-starts simulator/runtime/sim_main.c tests/integration/starts.c)
target_link_libraries(sim-starts PRIVATE ink_system ink_port_sim)
target_compile_options(sim-starts PRIVATE ${INK_WARN})
add_test(NAME integration-starts
  COMMAND sim-starts --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          --budget ${CMAKE_SOURCE_DIR}/tests/integration/starts.budget.json
          --script ${CMAKE_SOURCE_DIR}/tests/integration/starts.script
          --out ${CMAKE_BINARY_DIR}/test-out/starts --quiet --no-random-outages)
# A connection does not survive a move to the other network (tests/integration/hal_switch.c).
add_executable(sim-hal-switch simulator/runtime/sim_main.c tests/integration/hal_switch.c)
target_link_libraries(sim-hal-switch PRIVATE ink_system ink_port_sim)
target_compile_options(sim-hal-switch PRIVATE ${INK_WARN})
add_test(NAME integration-hal-switch
  COMMAND sim-hal-switch --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          --budget ${CMAKE_SOURCE_DIR}/tests/integration/hal_switch.budget.json
          --script ${CMAKE_SOURCE_DIR}/tests/integration/hal_switch.script
          --out ${CMAKE_BINARY_DIR}/test-out/hal_switch --quiet --no-random-outages)
# Which network "auto" uses as networks come and go (tests/integration/net_auto.c).
add_executable(sim-net-auto simulator/runtime/sim_main.c tests/integration/net_auto.c)
target_link_libraries(sim-net-auto PRIVATE ink_system ink_port_sim)
target_compile_options(sim-net-auto PRIVATE ${INK_WARN})
add_test(NAME integration-net-auto
  COMMAND sim-net-auto --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          --budget ${CMAKE_SOURCE_DIR}/tests/integration/net_auto.budget.json
          --script ${CMAKE_SOURCE_DIR}/tests/integration/net_auto.script
          --out ${CMAKE_BINARY_DIR}/test-out/net_auto --quiet --no-random-outages)
add_test(NAME determinism
  COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:sim-selftest>
          -DPROFILE=${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          -DSCRIPT=${CMAKE_SOURCE_DIR}/tests/simulator/selftest.script
          -DOUT=${CMAKE_BINARY_DIR}/test-out/determinism
          -P ${CMAKE_SOURCE_DIR}/tests/simulator/determinism.cmake)

# System scenarios: the real system on simulated devices, judged by budgets.
function(ink_scenario name profile)
  add_test(NAME scenario-${name}-${profile}
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/${profile}.json
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
  COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
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
  ink_scenario(ai_turn_end metalio-plus)
  ink_scenario(ai_turn_lost metalio-plus)
  # A voice turn's answer arriving while the next turn starts (tests/integration/ai_race.c): the product, booted.
  add_executable(sim-ai-race simulator/runtime/sim_main.c tests/integration/ai_race.c)
  target_link_libraries(sim-ai-race PRIVATE ink_apps ink_system ink_port_sim)
  target_compile_options(sim-ai-race PRIVATE ${INK_WARN})
  add_test(NAME integration-ai-race
    COMMAND sim-ai-race --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --budget ${CMAKE_SOURCE_DIR}/tests/integration/ai_race.budget.json
            --script ${CMAKE_SOURCE_DIR}/tests/integration/ai_race.script
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/ai_race --quiet --no-random-outages)
  add_test(NAME integration-lifecycle
    COMMAND sim-lifecycle --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus-handsfree.json
            --budget ${CMAKE_SOURCE_DIR}/tests/integration/lifecycle.budget.json
            --script ${CMAKE_SOURCE_DIR}/tests/integration/lifecycle.script
            --assets ${INK_ASSETS_DIR}
            --out ${CMAKE_BINARY_DIR}/test-out/lifecycle --quiet --no-random-outages)
  ink_scenario(standby metalio-plus)
  # regressions from the 2026-11 audit review (docs/audits/README.md): position saved while reading
  # on, fast page flipping, late AI speech after the sheet is closed, a book that cannot be opened
  ink_scenario(reader_continuous metalio-plus)
  ink_scenario(reader_rapid metalio-plus)
  ink_scenario(ai_dismiss metalio-plus)
  ink_scenario(ai_dismiss_held metalio-plus)
  # hands-free voice and barge-in (R-047), and when the device listens (D-52): voice front end modelled
  ink_scenario(voice_handsfree metalio-plus-handsfree)
  ink_scenario(voice_listen_policy metalio-plus-handsfree)
  ink_scenario(ai_offline_intent metalio-plus-handsfree)
  # the microphone cannot start (the expander, and with it the audio chip, not set up): AUD5-003
  add_test(NAME scenario-ai_mic_unavailable-metalio-plus
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_mic_unavailable.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_mic_unavailable.budget.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/ai_mic_unavailable-metalio-plus --quiet --trace --frames --no-random-outages
            --i2c-faults 1.0:io-expander)
  add_test(NAME scenario-reader_empty-metalio-plus
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_empty.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_empty.budget.json
            --assets ${INK_ASSETS_DIR} --content ${CMAKE_SOURCE_DIR}/tests/fixtures/content-bad
            --out ${CMAKE_BINARY_DIR}/test-out/reader_empty-metalio-plus --quiet --trace --frames --no-random-outages)
  # PQ-BN direct drive (research PR #7) on the same scenarios and budgets, plus its flash pause
  add_test(NAME scenario-reader_basic-metalio-plus-pqbn
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus-pqbn.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_basic.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_basic.pqbn.budget.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/reader_basic-metalio-plus-pqbn --quiet --trace --frames --no-random-outages)
  ink_scenario(ai_ask metalio-plus-pqbn)
  add_test(NAME scenario-pqbn_flash_pause-metalio-plus-pqbn
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus-pqbn.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/pqbn_flash_pause.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/pqbn_flash_pause.budget.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/pqbn_flash_pause --quiet --trace --no-random-outages)
  # requests about the screen: content version and retries (audit F-6)
  add_test(NAME scenario-ai_page_version-metalio-plus
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_page_version.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_page_version.budget.json
            --hub ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_page_version.hub.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/ai_page_version-metalio-plus --quiet --trace --frames --no-random-outages)
  add_test(NAME scenario-remote_browse-metalio-plus
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/remote_browse.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/remote_browse.budget.json
            --hub ${CMAKE_SOURCE_DIR}/tests/scenarios/remote_browse.hub.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/remote_browse-metalio-plus --quiet --trace --frames --no-random-outages)
  add_test(NAME scenario-remote_reconnect-metalio-plus
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/remote_reconnect.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/remote_reconnect.budget.json
            --hub ${CMAKE_SOURCE_DIR}/tests/scenarios/remote_browse.hub.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/remote_reconnect-metalio-plus --quiet --trace --frames --no-random-outages)
  add_test(NAME scenario-ai_tools-metalio-plus
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_tools.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_tools.budget.json
            --hub ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_tools.hub.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/ai_tools-metalio-plus --quiet --trace --frames --no-random-outages)
  # Power cut in the middle of a position save, reboot from the torn flash image.
  add_test(NAME scenario-powercut-during-save
    COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:ink-sim>
            -DPROFILE=${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            -DOUT=${CMAKE_BINARY_DIR}/test-out/powercut -DSRC=${CMAKE_SOURCE_DIR}/tests/scenarios
            -DASSETS=${INK_ASSETS_DIR} -DCONTENT=${INK_TEST_CONTENT}
            -P ${CMAKE_SOURCE_DIR}/tests/scenarios/powercut.cmake)
  # The real system must be bit-for-bit deterministic too (budgets compare single runs).
  add_test(NAME determinism-reader
    COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:ink-sim>
            -DPROFILE=${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            -DSCRIPT=${CMAKE_SOURCE_DIR}/tests/scenarios/reader_basic.script
            -DOUT=${CMAKE_BINARY_DIR}/test-out/determinism-reader
            "-DEXTRA=--assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT} --no-random-outages"
            -P ${CMAKE_SOURCE_DIR}/tests/simulator/determinism.cmake)
  # Same reader on the upstream display pipeline (timing comparison; lenient budget).
  add_test(NAME scenario-reader_basic-upstream-pipeline
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus-upstream-display.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_basic.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/upstream_pipeline.budget.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/reader_basic-upstream-pipeline --quiet --trace --no-random-outages)
  # Aged /user (50-60 % full, fragmented) and worst-case flash timing: the reader must still
  # meet its budgets while position saves hit a used filesystem.
  add_test(NAME fixture-aged-user-image
    COMMAND ${CMAKE_COMMAND} -DMODE=make -DSIM=$<TARGET_FILE:sim-selftest>
            -DPROFILE=${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            -DIMAGE=${CMAKE_BINARY_DIR}/test-out/aged-user.img -DOUT=${CMAKE_BINARY_DIR}/test-out/aging
            -P ${CMAKE_SOURCE_DIR}/tests/scenarios/aged_image.cmake)
  set_tests_properties(fixture-aged-user-image PROPERTIES FIXTURES_SETUP aged_user)
  add_test(NAME scenario-reader_basic-aged-flash-max
    COMMAND ${CMAKE_COMMAND} -DMODE=run -DSIM=$<TARGET_FILE:ink-sim>
            -DPROFILE=${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
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
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_basic.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/panel_power_faults.budget.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/reader_basic-panel-power-faults --quiet --no-random-outages
            --i2c-faults 0.08:pmic)
endif()
# The panel supply never comes up: every refresh fails (ok = false) instead of pretending.
add_test(NAME scenario-home-panel-power-dead
  COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          --script ${CMAKE_SOURCE_DIR}/tests/scenarios/home_basic.script
          --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/panel_power_dead.budget.json
          --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
          --out ${CMAKE_BINARY_DIR}/test-out/home-panel-power-dead --quiet --no-random-outages --i2c-faults 1.0:pmic --end 8s)
# Portability smoke runs: same system, other panels/classes (budget: no panic, input works)
foreach(p pinenote-rk3566 desktop-lcd metalio-plus-pqbn)
  add_test(NAME portability-home-${p}
    COMMAND ink-sim --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/${p}.json
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

# The desktop simulator, headless: boot, clicks on the panel and a cover key reach the system.
if(INK_GUI AND INK_HAVE_FONTS)
  add_test(NAME gui-smoke
    COMMAND ${CMAKE_COMMAND} -DGUI=$<TARGET_FILE:ink-sim-gui> -DOUT=${CMAKE_BINARY_DIR}/test-out/gui-smoke
            -P ${CMAKE_SOURCE_DIR}/tests/simulator/gui_smoke.cmake)
  set_tests_properties(gui-smoke PROPERTIES TIMEOUT 180)
endif()
