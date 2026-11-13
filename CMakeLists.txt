# ink — portable system + simulator build (host).
#
# Device firmware is built with ESP-IDF (ports/esp32s31, later); this file
# builds everything that runs on a PC: the simulator, the POSIX port, tests.
#
#   cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(ink C CXX)
set(CMAKE_CXX_LINKER_PREFERENCE_PROPAGATES OFF) # C fixtures reuse the executable's compiler driver.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Tests that recompile one source with its real flags for a reverse control (emergency-call, among others)
# read the compile database; CI configures without asking for it (2026-11-11), so it is always written.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
execute_process(COMMAND git rev-parse --short=12 HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE INK_BUILD_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE)
add_compile_definitions(INK_BUILD_COMMIT="${INK_BUILD_COMMIT}")
add_executable(test-s31-osal-ticks tests/esp32s31/osal_ticks.c)
target_compile_options(test-s31-osal-ticks PRIVATE -UNDEBUG)
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
# The S31 4G stack is the vendor's C++ modem (port-cell-3): built here as in ports/esp32s31/tests/CMakeLists.txt,
# so the reverse controls (cell_negative.py, uhci_negative.py) find its real flags in this build's compile database.
enable_language(CXX)
add_executable(test-s31-cell ports/esp32s31/tests/cell.c ports/esp32s31/tests/cell_host.cc ports/esp32s31/s31_modem.c
  ports/esp32s31/s31_uhci.cc ports/esp32s31/s31_cell_rx.c ports/esp32s31/hal_cellular_s31.c drivers/chips/nt26/nt26.c
  vendor/uart-eth-modem/uart_eth_modem.cc vendor/uart-eth-modem/nt26_lifecycle.cc
  vendor/iot_eth/iot_eth.c vendor/iot_eth/iot_eth_netif_glue.c)
target_include_directories(test-s31-cell PRIVATE ports/esp32s31/tests/cell_stub ports/esp32s31 platform/include drivers/chips boards
  vendor/uart-eth-modem vendor/iot_eth/include vendor/iot_eth/interface)
target_compile_definitions(test-s31-cell PRIVATE IOT_ETH_VER_MAJOR=0 IOT_ETH_VER_MINOR=1 IOT_ETH_VER_PATCH=0)
set_source_files_properties(vendor/iot_eth/iot_eth.c vendor/iot_eth/iot_eth_netif_glue.c
  PROPERTIES COMPILE_OPTIONS "-std=gnu2x;-Wno-old-style-declaration;-Wno-unknown-warning-option")
set_target_properties(test-s31-cell PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_compile_options(test-s31-cell PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -UNDEBUG
  $<$<BOOL:${INK_WERROR}>:-Werror>)
target_link_libraries(test-s31-cell PRIVATE Threads::Threads)
add_executable(test-s31-camera ports/esp32s31/tests/camera.c boards/metalio_plus/metalio_board.c drivers/chips/tca9555/tca9555.c)
target_include_directories(test-s31-camera PRIVATE ports/esp32s31 ports/esp32s31/tests/camera_stub platform/include drivers/chips boards)
target_compile_options(test-s31-camera PRIVATE ${INK_WARN} -UNDEBUG)
add_executable(test-s31-bt ports/esp32s31/tests/audio.c ports/esp32s31/hal_audio_s31.c
  ports/esp32s31/hal_bt_s31.c drivers/chips/bh1098g/bh1098g.c)
target_include_directories(test-s31-bt PRIVATE ports/esp32s31 ports/esp32s31/tests/audio_stub platform/include drivers/chips boards)
target_compile_options(test-s31-bt PRIVATE ${INK_WARN} -UNDEBUG)
target_link_libraries(test-s31-bt PRIVATE Threads::Threads)
add_executable(test-s31-hal-map tests/esp32s31/hal_map.c)
target_include_directories(test-s31-hal-map PRIVATE tests/esp32s31/stub platform/include)
target_compile_options(test-s31-hal-map PRIVATE ${INK_WARN} -UNDEBUG)
# Bind symbols at load (macOS does already): lazily, the dynamic linker would run on the stack of
# whichever simulated task first calls a libc function, with a CPU-dependent size (XSAVE area).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_link_options(-Wl,-z,now)
endif()

# ---------------------------------------------------------------- third party
add_subdirectory(third_party/speexdsp)
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
add_library(ink_sha256 STATIC system/util/sha256.c)
target_include_directories(ink_sha256 PUBLIC system/include platform/include)
target_compile_options(ink_sha256 PRIVATE ${INK_WARN})

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
add_library(ink_script STATIC system/autotest/script.c)
target_include_directories(ink_script PUBLIC system/autotest system/include platform/include)
target_compile_options(ink_script PRIVATE ${INK_WARN})
add_library(ink_port_sim STATIC
  ports/sim/osal/osal_sim.c
  ports/sim/hal/hal_i2c_sim.c
  ports/sim/hal/hal_sensors_sim.c
  ports/sim/hal/hal_display_sim.c
  ports/sim/hal/hal_input_sim.c
  ports/sim/hal/hal_storage_sim.c
  ports/sim/hal/hal_log_sim.c
  ports/sim/hal/hal_usb_sim.c
  ports/sim/hal/hal_bt_sim.c
  ports/sim/hal/hal_ota_sim.c
  ports/sim/hal/hal_power_sim.c
  ports/sim/hal/hal_assets_sim.c
  ports/sim/hal/hal_net_sim.c
  ports/sim/hal/hal_transfer_sim.c
  ports/sim/hal/hal_provision_sim.c
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
  simulator/environment/sim_backup.c
  simulator/environment/hub_bridge.c
  simulator/environment/sim_remote.c
  simulator/environment/sim_installer.c
  simulator/runtime/metrics.c
  simulator/runtime/trace.c
  simulator/runtime/script.c
  simulator/desktop/sim_gui.c)
target_include_directories(ink_port_sim PUBLIC simulator simulator/desktop)
target_link_libraries(ink_port_sim PUBLIC ink_board_metalio_plus ink_drivers ink_sim_core ink_littlefs ink_stb ink_lz4 ink_sha256 ink_script)
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
add_library(ink_image_codec STATIC system/services/media/image_codec.c)
target_include_directories(ink_image_codec PRIVATE system/include third_party/stb)
target_link_libraries(ink_image_codec PRIVATE ink_platform)
target_compile_options(ink_image_codec PRIVATE -w) # upstream stb implementation
add_library(ink_system STATIC
  system/services/media/image.c
  system/services/display/display_service.c
  system/services/display/light.c
  system/services/camera/camera_service.c
  system/services/comm/comm_service.c
  system/services/net/net_service.c
  system/services/display/refresh_policy.c
  system/services/input/input_service.c
  system/services/storage/storage_service.c
  system/services/log/log.c
  system/services/power/power_service.c
  system/services/hub/hub_link.c
  system/services/hub/pairing.c
  system/services/hub/rt.c
  system/services/ai/ai_service.c
  system/services/ai/ai_history.c
  system/services/audio/audio_service.c
  system/services/res/exec.c
  system/services/res/res.c
  system/services/res/svc.c
  system/services/res/task.c
  system/services/data/datastore.c
  system/services/data/data_caps.c
  system/services/backup/backup_service.c
  system/services/file/file_service.c
  system/services/card_files/card_files.c
  system/ui/card_files_notice.c
  system/services/file/file_caps.c
  system/services/file/transfer.c
  system/services/time/lunar.c
  system/services/time/almanac.c
  system/services/time/vendor/mogo-calendar/main/time/calendar.c
  system/services/time/time_service.c
  system/ui/shell/time_settings.c
  system/ui/shell/mobile_network.c
  system/services/update/update_service.c
  system/ui/shell/update_card.c
  system/ui/shell/power_menu.c
  system/ui/shell/airplane_card.c
  system/ui/shell/boot_flow.c
  system/ui/shell/factory_pages.c
  system/ui/shell/off_picture.c
  system/services/time/time_caps.c
  system/services/notify/notify_service.c
  system/services/notify/notify_caps.c
  system/services/notify/notify_model.c
  system/services/notify/interrupt.c
  system/services/net/wifi.c
  system/ui/shell/pinpad.c
  system/ui/shell/member_gate.c
  system/services/audio/ogg.c
  system/services/audio/media_service.c
  system/services/audio/codec_formats.c
  system/services/audio/vendor/kiikoread/metadata.c
  system/services/audio/vendor/kiikoread/music_lyrics.c
  system/apps/music/music.c
  system/services/bt/bt_service.c
  system/services/text/text_service.c
  system/services/text/google_port.cpp
  system/services/text/google_pinyin.cpp
  system/services/text/pinyin.c
  system/services/text/clipboard.c
  system/services/text/text_caps.c
  system/services/text/voice_input.c
  system/ui/textfield.c
  system/ui/shell/keyboard.c
  system/ui/app_registry.c
  system/ui/app_packages.c
  system/ui/shell/card.c
  system/ui/shell/interrupt_card.c
  system/ui/shell/confirm_card.c
  system/policy/cap.c
  system/policy/members.c
  system/policy/settings.c
  system/policy/confirm.c
  system/policy/state.c
  system/policy/system_actions.c
  system/policy/watch.c
  system/util/ink_args.c
  system/util/ink_crc.c
  system/util/ink_json.c
  system/util/layout.c
  system/text/ink_font.c
  system/text/i18n.c
  system/text/linebreak.c
  system/text/layout.c
  system/ui/lock_mark.c
  system/ui/lock_faces/faces.c
  system/ui/ui_task.c
  system/ui/ui_semantics.c
  system/ui/ui_list.c
  system/ui/search.c
  system/ui/theme.c
  system/ui/icons.c
  system/ui/shell/navigation.c
  system/ui/shell/status_bar.c system/ui/shell/shell_home.c system/ui/shell/control_center.c system/ui/shell/settings_body.c system/ui/shell/today_edit.c
  system/ui/shell/notification_list.c
  system/ui/shell/settings_pages.c system/ui/shell/settings_registry.c
  system/ui/shell/bluetooth_page.c
  system/ui/shell/ai_panel.c
  system/ui/app.c
  system/apps/remote/remote.c)
target_include_directories(ink_system PUBLIC system/include PRIVATE system/services/display third_party/jsmn third_party/stb)
target_include_directories(ink_system PRIVATE system/services/time/vendor/mogo-calendar/main)
target_link_libraries(ink_system PUBLIC ink_platform ink_lvgl ink_lz4 ink_sha256 PRIVATE ink_image_codec ink_resampler)
# The system calls the HAL; in this build the simulator port implements it. Link-only (no headers
# leak into the system or the apps), and it puts the port after the system on every link line, which
# GNU ld needs for HAL functions only the system references (e.g. hal_cell_*).
target_link_libraries(ink_system INTERFACE $<LINK_ONLY:ink_port_sim>)
target_sources(ink_system PRIVATE system/apps/clock/clock.c)
target_compile_options(ink_system PRIVATE ${INK_WARN})
include(system/services/text/google_build.cmake)
ink_google_pinyin(ink_system)
target_sources(ink_system PRIVATE system/apps/gomoku/gomoku.c system/apps/gomoku/engine.cpp
  system/apps/gomoku/vendor/crossmux/GomokuBoard.cpp system/apps/gomoku/vendor/crossmux/GomokuAI.cpp)
target_sources(ink_system PRIVATE system/apps/memory/memory.c system/apps/memory/engine.c
  system/apps/snake/snake.c system/apps/snake/engine.c)
target_include_directories(ink_system PRIVATE system/apps/gomoku/compat)
target_compile_features(ink_system PRIVATE cxx_std_20)
# Keep C-only host test drivers on the C compiler when they consume the gomoku engine.
set(CMAKE_CXX_LINKER_PREFERENCE_PROPAGATES FALSE)
set_source_files_properties(system/apps/gomoku/vendor/crossmux/GomokuAI.cpp PROPERTIES COMPILE_OPTIONS -Wno-shadow)

add_library(ink_apps STATIC apps/home/home.c apps/reader/reader.c)
target_link_libraries(ink_apps PUBLIC ink_system)
target_compile_options(ink_apps PRIVATE ${INK_WARN})
include(apps/reader/reader_build.cmake)
ink_reader_build(ink_apps ink_system ink_lvgl)
# The existing inflater; the HAL supplies its PSRAM output and workspace.
add_library(ink_miniz STATIC ${reader_third_party}/reader_miniz/miniz.c)
target_include_directories(ink_miniz PUBLIC third_party PRIVATE system/include platform/include)
target_compile_definitions(ink_miniz PUBLIC MINIZ_NO_MALLOC MINIZ_NO_ZLIB_APIS MINIZ_NO_ARCHIVE_APIS
  MINIZ_NO_STDIO MINIZ_NO_TIME MINIZ_NO_DEFLATE_APIS mz_version=ink_asset_mz_version
  miniz_def_alloc_func=ink_asset_alloc miniz_def_realloc_func=ink_asset_realloc miniz_def_free_func=ink_asset_free)
target_compile_options(ink_miniz PRIVATE -w)
target_link_libraries(ink_port_sim PRIVATE ink_miniz)
add_executable(sim-reader-epub simulator/runtime/sim_main.c tests/reader_epub/device.c)
target_include_directories(sim-reader-epub PRIVATE tests apps/reader)
target_link_libraries(sim-reader-epub PRIVATE ink_apps ink_system ink_port_sim)
target_compile_options(sim-reader-epub PRIVATE ${INK_WARN})

# The product: ink_main() starts the services and the apps (the composition root).
add_library(ink_boot STATIC system/boot/main.c)
target_link_libraries(ink_boot PUBLIC ink_apps ink_system)
target_compile_options(ink_boot PRIVATE ${INK_WARN})

# ---------------------------------------------------------------- assets (generated)
# CJK bitmap fonts rendered from Noto Sans CJK (SIL OFL 1.1) by tools/fontgen.
set(INK_CJK_FONT "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc" CACHE FILEPATH "CJK font for the bitmap fonts")
set(INK_CJK_FACE 2 CACHE STRING "Face index in the collection (Noto Sans CJK .ttc: 2 = Simplified Chinese)")
set(INK_ASSETS_DIR ${CMAKE_BINARY_DIR}/assets)
add_custom_target(ink_sample ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${INK_ASSETS_DIR}/sample
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/assets/sample/welcome.txt ${INK_ASSETS_DIR}/sample/welcome.txt)
find_package(Python3 COMPONENTS Interpreter)
add_test(NAME reader-epub COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/reader_epub/run.py --build ${CMAKE_BINARY_DIR})
add_test(NAME reader-epub-reverse COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/reader_epub/reverse.py --build ${CMAKE_BINARY_DIR})
add_test(NAME reader-epub-segments COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/reader_epub/reverse.py --build ${CMAKE_BINARY_DIR} --segments)
add_test(NAME reader-epub-toc-save-reverse COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/reader_epub/reverse.py --build ${CMAKE_BINARY_DIR} --unsaved-toc)
file(GLOB INK_I18N_TABLES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/assets/i18n/*.tsv) # interface languages
ink_reader_translations(INK_I18N_TABLES)
include(apps/mogo/translations.cmake)
set(INK_HAVE_FONTS OFF)
set(INK_RAW_FONTS_DIR ${CMAKE_BINARY_DIR}/assets-raw/fonts) # the same faces uncompressed (INKF v1), for tests/font_pack
if(EXISTS ${INK_CJK_FONT} AND Python3_FOUND)
  set(INK_FONT_FILES)
  foreach(px 44) # the one reading font; the UI scales it for text outside its own subset
    set(out ${INK_ASSETS_DIR}/fonts/sans-${px}.inkf)
    add_custom_command(OUTPUT ${out}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${INK_ASSETS_DIR}/fonts ${INK_RAW_FONTS_DIR}
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/fontgen/fontgen.py --font ${INK_CJK_FONT}
              --face ${INK_CJK_FACE} --px ${px} --bpp 2 --charset ascii,punct,pinyin,gb2312
              --digits-font ${CMAKE_SOURCE_DIR}/third_party/fonts/Outfit.ttf --digits-weight 400 -o ${out}
              --raw-out ${INK_RAW_FONTS_DIR}/sans-${px}.inkf
      DEPENDS ${CMAKE_SOURCE_DIR}/tools/fontgen/fontgen.py
      COMMENT "fontgen: CJK bitmap font ${px}px")
    list(APPEND INK_FONT_FILES ${out})
  endforeach()
  add_custom_target(ink_fonts ALL DEPENDS ${INK_FONT_FILES})
  # Additional faces are optional system packages (Ubuntu fonts-noto-cjk-extra),
  # otherwise fetch the pinned OFL release into this checkout's ignored build-deps.
  foreach(weight Medium Bold)
    string(TOUPPER ${weight} upper)
    find_file(INK_CJK_${upper}_FONT NAMES NotoSansCJK-${weight}.ttc NotoSansCJKsc-${weight}.otf
      PATHS /usr/share/fonts/opentype/noto ${CMAKE_SOURCE_DIR}/build-deps/fonts
      NO_DEFAULT_PATH)
    if(NOT INK_CJK_${upper}_FONT)
      set(INK_CJK_${upper}_FONT ${CMAKE_SOURCE_DIR}/build-deps/fonts/NotoSansCJKsc-${weight}.otf CACHE FILEPATH "CJK ${weight} face" FORCE)
      if(weight STREQUAL "Medium")
        set(hash ca094f6b0001fb048ca39ddd797a0cdb0179e1e55c6561e111c49c3e6a61d7b7)
      else()
        set(hash b5f0d1a190a7f9b43c310a8850630af12553df32c4c050543f9059732d9b4c0a)
      endif()
      file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/build-deps/fonts)
      file(DOWNLOAD https://raw.githubusercontent.com/notofonts/noto-cjk/Sans2.004/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-${weight}.otf
        ${INK_CJK_${upper}_FONT} EXPECTED_HASH SHA256=${hash} TLS_VERIFY ON STATUS status)
      list(GET status 0 code)
      if(NOT code EQUAL 0)
        message(FATAL_ERROR "Missing ${weight} CJK font. Install fonts-noto-cjk-extra or set INK_CJK_${upper}_FONT (${status})")
      endif()
    endif()
    if(NOT EXISTS ${INK_CJK_${upper}_FONT})
      message(FATAL_ERROR "Missing required CJK ${weight} face: ${INK_CJK_${upper}_FONT}")
    endif()
  endforeach()
  file(GLOB_RECURSE INK_UI_TEXT CONFIGURE_DEPENDS system/ui/*.c system/apps/*.c apps/*.c system/services/*.c system/policy/*.c)
  add_custom_command(OUTPUT ${INK_ASSETS_DIR}/fonts/ui-fonts.stamp
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/fontgen/ui_fonts.py
      --regular ${INK_CJK_FONT} --face ${INK_CJK_FACE} --medium ${INK_CJK_MEDIUM_FONT} --bold ${INK_CJK_BOLD_FONT}
      --out ${INK_ASSETS_DIR}/fonts --raw-out ${INK_RAW_FONTS_DIR}
    DEPENDS ${INK_FONT_FILES} ${INK_UI_TEXT} ${INK_I18N_TABLES} tools/fontgen/fontgen.py tools/fontgen/ui_fonts.py system/ui/ink/ui_theme.h third_party/fonts/Outfit.ttf
    COMMENT "fontgen: contract UI sizes, CJK weights and Outfit digits")
  add_custom_target(ink_ui_fonts ALL DEPENDS ${INK_ASSETS_DIR}/fonts/ui-fonts.stamp)
  set(INK_HAVE_FONTS ON)
else()
  message(FATAL_ERROR "The shell requires CJK fonts and Python 3 (${INK_CJK_FONT}). "
                      "Debian/Ubuntu: apt install fonts-noto-cjk fonts-noto-cjk-extra python3-pil")
endif()

# Icons are checked in as extracted geometry, not screenshots or design boards.
add_executable(ink-icon-raster tools/icongen/raster.c)
target_compile_options(ink-icon-raster PRIVATE -w)
target_link_libraries(ink-icon-raster PRIVATE m)
file(READ ${CMAKE_SOURCE_DIR}/assets/icons/manifest.json INK_ICON_MANIFEST)
string(JSON INK_ICON_COUNT LENGTH "${INK_ICON_MANIFEST}" names)
math(EXPR INK_ICON_LAST "${INK_ICON_COUNT} - 1")
string(JSON INK_ICON_SIZE_COUNT LENGTH "${INK_ICON_MANIFEST}" sizes)
math(EXPR INK_ICON_SIZE_LAST "${INK_ICON_SIZE_COUNT} - 1")
set(INK_ICON_SIZES)
foreach(index RANGE ${INK_ICON_SIZE_LAST})
  string(JSON size GET "${INK_ICON_MANIFEST}" sizes ${index})
  list(APPEND INK_ICON_SIZES ${size})
endforeach()
set(INK_ICON_FILES)
foreach(index RANGE ${INK_ICON_LAST})
  string(JSON icon GET "${INK_ICON_MANIFEST}" names ${index})
  foreach(look line solid)
    set(svg ${CMAKE_SOURCE_DIR}/assets/icons/${icon}-${look}.svg)
    if(NOT EXISTS ${svg})
      message(FATAL_ERROR "Missing required design icon ${icon}-${look}")
    endif()
    foreach(px ${INK_ICON_SIZES})
      set(out ${INK_ASSETS_DIR}/icons/${icon}-${look}-${px}.inki)
      add_custom_command(OUTPUT ${out}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${INK_ASSETS_DIR}/icons
        COMMAND ink-icon-raster ${svg} ${px} ${out}
        DEPENDS ink-icon-raster ${svg})
      list(APPEND INK_ICON_FILES ${out})
    endforeach()
  endforeach()
endforeach()
add_custom_target(ink_icons ALL DEPENDS ${INK_ICON_FILES})
add_custom_target(ink_ui_resource_check ALL
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/icongen/check.py
          --header ${CMAKE_BINARY_DIR}/generated/ink/ui_icon_names.h
  BYPRODUCTS ${CMAKE_BINARY_DIR}/generated/ink/ui_icon_names.h
  COMMENT "Check required design icon geometry")
add_dependencies(ink_system ink_ui_resource_check)
# Interface languages: assets/i18n/<code>.tsv, checked and turned into a const lookup table.
set(INK_I18N_SOURCES ${CMAKE_SOURCE_DIR}/system/ui/shell/boot_flow.c ${CMAKE_SOURCE_DIR}/system/ui/shell/navigation.c ${CMAKE_SOURCE_DIR}/system/ui/shell/keyboard.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/status_bar.c ${CMAKE_SOURCE_DIR}/system/ui/shell/shell_home.c ${CMAKE_SOURCE_DIR}/system/ui/shell/control_center.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/notification_list.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/settings_body.c ${CMAKE_SOURCE_DIR}/system/ui/shell/today_edit.c
  ${CMAKE_SOURCE_DIR}/system/apps/memory/memory.c ${CMAKE_SOURCE_DIR}/system/apps/snake/snake.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/mobile_network.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/settings_pages.c ${CMAKE_SOURCE_DIR}/system/ui/shell/settings_registry.c ${CMAKE_SOURCE_DIR}/system/ui/shell/time_settings.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/settings_family.c ${CMAKE_SOURCE_DIR}/system/services/time/time_service.c
  ${CMAKE_SOURCE_DIR}/system/apps/calendar/calendar.c ${CMAKE_SOURCE_DIR}/system/apps/files/files.c
  ${CMAKE_SOURCE_DIR}/system/services/file/file_caps.c ${CMAKE_SOURCE_DIR}/system/apps/clock/clock.c
  ${CMAKE_SOURCE_DIR}/system/apps/camera/camera.c ${CMAKE_SOURCE_DIR}/system/apps/camera/scan.c
  ${CMAKE_SOURCE_DIR}/system/policy/cap.c ${CMAKE_SOURCE_DIR}/system/policy/members.c
  ${CMAKE_SOURCE_DIR}/system/ui/app_registry.c ${CMAKE_SOURCE_DIR}/system/ui/lock_mark.c
  ${CMAKE_SOURCE_DIR}/system/apps/calculator/calculator.c ${CMAKE_SOURCE_DIR}/system/apps/contacts/contacts.c
  ${CMAKE_SOURCE_DIR}/system/apps/gallery/gallery.c
  ${CMAKE_SOURCE_DIR}/system/apps/memo/memo.c ${CMAKE_SOURCE_DIR}/system/apps/phone/phone.c
  ${CMAKE_SOURCE_DIR}/system/apps/remote/remote.c ${CMAKE_SOURCE_DIR}/system/apps/sms/sms.c
  ${CMAKE_SOURCE_DIR}/system/apps/weather/weather.c)
file(GLOB_RECURSE INK_I18N_SERVICES CONFIGURE_DEPENDS system/services/*.c system/policy/*.c)
list(APPEND INK_I18N_SOURCES ${INK_I18N_SERVICES})
list(APPEND INK_I18N_SOURCES ${CMAKE_SOURCE_DIR}/apps/home/home.c ${CMAKE_SOURCE_DIR}/apps/reader/reader.c
  ${CMAKE_SOURCE_DIR}/system/include/ink/sys/ai.h ${CMAKE_SOURCE_DIR}/system/ui/shell/member_pages.h ${CMAKE_SOURCE_DIR}/system/text/i18n.c)
list(APPEND INK_I18N_SOURCES
  ${CMAKE_SOURCE_DIR}/system/ui/shell/activity_card.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/ai_chat.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/ai_pages.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/ai_panel.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/backup_pages.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/bluetooth_page.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/confirm_card.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/member_pages.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/transfer_pages.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/update_pages.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/wallpaper.c
  ${CMAKE_SOURCE_DIR}/system/ui/search.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/airplane_card.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/interrupt_card.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/kid_pages.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/member_gate.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/off_picture.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/pinpad.c
  ${CMAKE_SOURCE_DIR}/system/ui/file_picker.c
  ${CMAKE_SOURCE_DIR}/system/ui/textfield.c
  ${CMAKE_SOURCE_DIR}/system/ui/ui_list.c
  ${CMAKE_SOURCE_DIR}/system/ui/ui_semantics.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/factory_pages.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/power_menu.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/power_pages.c
  ${CMAKE_SOURCE_DIR}/system/ui/card_files_notice.c
)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/generated/ink/i18n_table.h
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/i18n/i18n_gen.py ${INK_I18N_TABLES}
          --header ${CMAKE_BINARY_DIR}/generated/ink/i18n_table.h --sources ${INK_I18N_SOURCES}
  DEPENDS tools/i18n/i18n_gen.py ${INK_I18N_TABLES} ${INK_I18N_SOURCES}
  COMMENT "i18n: check the translation tables")
add_custom_target(ink_i18n_table DEPENDS ${CMAKE_BINARY_DIR}/generated/ink/i18n_table.h)
add_dependencies(ink_system ink_i18n_table)
file(GLOB INK_LICENSE_FILES CONFIGURE_DEPENDS third_party/licenses/* third_party/*/LICENSE* third_party/stb/*.h tools/imedict/licenses/* tools/appfw-spike/licenses/*.txt)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/generated/ink/licenses.h
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/licenses.py ${CMAKE_BINARY_DIR}/generated/ink/licenses.h
  DEPENDS tools/licenses.py ${INK_LICENSE_FILES} third_party/tlsf/tlsf.h third_party/fonts/Outfit-OFL.txt)
add_custom_target(ink_licenses DEPENDS ${CMAKE_BINARY_DIR}/generated/ink/licenses.h)
add_dependencies(ink_system ink_licenses)
# Card files (D-138): what apps need on the card, made from tools/card_files.json; the firmware carries the list.
# fontTools may live in another interpreter than the build's (build-deps/pyenv has none): the first that has it.
if(NOT INK_FONT_PYTHON)
  execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/card_files.py python ${Python3_EXECUTABLE} python3
    OUTPUT_VARIABLE found OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE missing)
  if(missing)
    message(FATAL_ERROR "Card files need Python with fontTools (${Python3_EXECUTABLE} or python3). "
                        "Debian/Ubuntu: apt install python3-fonttools; or pip install fonttools; or set INK_FONT_PYTHON")
  endif()
  set(INK_FONT_PYTHON ${found} CACHE FILEPATH "Python with fontTools for the card files")
endif()
set(INK_CARD_FILES_DIR ${CMAKE_BINARY_DIR}/card-files)
add_custom_command(OUTPUT ${INK_CARD_FILES_DIR}/manifest.json ${CMAKE_BINARY_DIR}/generated/ink/card_files_manifest.h
  COMMAND ${INK_FONT_PYTHON} tools/card_files.py bundle --spec tools/card_files.json --noto ${INK_CJK_FONT} --face ${INK_CJK_FACE} --out ${INK_CARD_FILES_DIR}
  COMMAND ${INK_FONT_PYTHON} tools/card_files.py header --manifest ${INK_CARD_FILES_DIR}/manifest.json --out ${CMAKE_BINARY_DIR}/generated/ink/card_files_manifest.h
  DEPENDS tools/card_files.json tools/card_files.py tools/mogo_font.py ${INK_CJK_FONT} tools/appfw-spike/licenses/NotoSansCJK-OFL.txt
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} COMMENT "card files: the reader's font and its licence (D-138)")
add_custom_target(ink_card_files DEPENDS ${INK_CARD_FILES_DIR}/manifest.json)
add_dependencies(ink_system ink_card_files)
target_include_directories(ink_system PUBLIC ${CMAKE_BINARY_DIR}/generated)

# Scenario content: the committed sample books + a generated long book (pagination stress).
set(INK_TEST_CONTENT ${CMAKE_BINARY_DIR}/test-content)
add_custom_command(OUTPUT ${INK_TEST_CONTENT}/books/测试长书.txt
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/tests/fixtures/content ${INK_TEST_CONTENT}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/testdata/make_long_book.py
          ${CMAKE_SOURCE_DIR}/tests/fixtures/content/books/星星邮差.txt ${INK_TEST_CONTENT}/books/测试长书.txt 40
  DEPENDS ${CMAKE_SOURCE_DIR}/tools/testdata/make_long_book.py ${CMAKE_SOURCE_DIR}/tests/fixtures/content/books/星星邮差.txt
  COMMENT "test content: long book")
add_custom_command(OUTPUT ${INK_TEST_CONTENT}/.system/ime/base.ikd
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/imedict/build_base.py
          --cache ${CMAKE_SOURCE_DIR}/build-deps/imedict --out ${INK_TEST_CONTENT}/.system/ime/base.ikd
  DEPENDS tools/imedict/build_base.py tools/imedict/imedict.py tools/imedict/sources.lock.json tools/imedict/common.tsv tools/fontgen/fontgen.py
          tools/imedict/licenses/jieba-MIT.txt tools/imedict/licenses/pypinyin-MIT.txt
          tools/imedict/licenses/pinyin-data-MIT.txt tools/imedict/licenses/phrase-pinyin-data-MIT.txt
  COMMENT "imedict: hash-locked full pinyin and initials within 1 MiB")
# The same word list ships in the firmware's asset pack (ime/, with its licences), so the device has Chinese
# without the card file (trial report 2026-11-11). The simulator maps the same assets as the device; a card copy
# still wins, and tests/ime_lexicon checks the no-dictionary fallback with assets that leave ime/ out.
set(INK_IME_LICENSES jieba-MIT.txt pypinyin-MIT.txt pinyin-data-MIT.txt phrase-pinyin-data-MIT.txt)
list(TRANSFORM INK_IME_LICENSES PREPEND ${CMAKE_SOURCE_DIR}/tools/imedict/licenses/ OUTPUT_VARIABLE INK_IME_LICENSE_FILES)
add_custom_command(OUTPUT ${INK_ASSETS_DIR}/ime/base.ikd
  COMMAND ${CMAKE_COMMAND} -E make_directory ${INK_ASSETS_DIR}/ime
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${INK_TEST_CONTENT}/.system/ime/base.ikd ${INK_IME_LICENSE_FILES} ${INK_ASSETS_DIR}/ime/
  DEPENDS ${INK_TEST_CONTENT}/.system/ime/base.ikd ${INK_IME_LICENSE_FILES}
  COMMENT "imedict: the shipping word list in the asset pack")
add_custom_target(ink_test_content ALL DEPENDS ${INK_TEST_CONTENT}/books/测试长书.txt ${INK_TEST_CONTENT}/.system/ime/base.ikd
  ${INK_ASSETS_DIR}/ime/base.ikd)
add_custom_target(ink_assets ALL
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/asset_pack.py ${INK_ASSETS_DIR} --out ${INK_ASSETS_DIR}
  BYPRODUCTS ${INK_ASSETS_DIR}/ink-fonts.bin ${INK_ASSETS_DIR}/ink-embedded.bin
  DEPENDS ink_icons ink_test_content ink_sample)
if(INK_HAVE_FONTS)
  add_dependencies(ink_assets ink_fonts ink_ui_fonts)
endif()
add_dependencies(ink_port_sim ink_assets)

# Keep the engine's original ordering fixture separate from the shipping data.
set(INK_MINI_CONTENT ${CMAKE_BINARY_DIR}/mini-content)
add_custom_command(OUTPUT ${INK_MINI_CONTENT}/.system/ime/base.ikd
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/tests/fixtures/content ${INK_MINI_CONTENT}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${INK_MINI_CONTENT}/.system/ime
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/imedict/imedict.py
          ${CMAKE_SOURCE_DIR}/tests/fixtures/ime/mini.tsv ${INK_MINI_CONTENT}/.system/ime/base.ikd
  DEPENDS tools/imedict/imedict.py tests/fixtures/ime/mini.tsv)
add_custom_target(ink_mini_content ALL DEPENDS ${INK_MINI_CONTENT}/.system/ime/base.ikd)

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
add_subdirectory(third_party/mogo)
include(system/graft/graft_build.cmake)
ink_graft_build(ink_system)
include(apps/mogo/mogo.cmake)
add_executable(sim-graft-carrier simulator/runtime/sim_main.c tests/graft_mogo/device.c)
target_link_libraries(sim-graft-carrier PRIVATE ink_apps ink_system ink_port_sim)
target_include_directories(sim-graft-carrier PRIVATE tests)
target_compile_options(sim-graft-carrier PRIVATE ${INK_WARN})
add_library(test-graft-cxx-abi OBJECT tests/graft_mogo/abi.cc)
target_link_libraries(test-graft-cxx-abi PRIVATE ink_system)
target_compile_options(test-graft-cxx-abi PRIVATE ${INK_WARN})
add_test(NAME graft-carrier COMMAND sim-graft-carrier
  --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
  --script ${CMAKE_SOURCE_DIR}/tests/graft_mogo/scenario.script
  --budget ${CMAKE_SOURCE_DIR}/tests/ui_scene.budget.json
  --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
  --out ${CMAKE_BINARY_DIR}/test-out/graft-carrier --quiet --no-random-outages)
add_executable(test-log tests/log/log.c ports/sim/hal/hal_log_sim.c)
target_include_directories(test-log PRIVATE platform/include system/include simulator simulator/kernel/vtk simulator/memory simulator/runtime)
target_compile_options(test-log PRIVATE ${INK_WARN} -UNDEBUG)
add_test(NAME unit-log COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/log/run.py $<TARGET_FILE:test-log>)
add_test(NAME unit-s31-osal-ticks COMMAND test-s31-osal-ticks)
add_test(NAME unit-s31-cell COMMAND test-s31-cell)
foreach(case nat baud started silent dhcp throttle detect privacy)
  add_test(NAME unit-s31-cell-${case} COMMAND test-s31-cell ${case})
endforeach()
set_tests_properties(unit-s31-cell unit-s31-cell-nat unit-s31-cell-baud unit-s31-cell-started
  unit-s31-cell-silent unit-s31-cell-dhcp unit-s31-cell-throttle unit-s31-cell-detect unit-s31-cell-privacy PROPERTIES TIMEOUT 120)
add_test(NAME unit-s31-bt COMMAND test-s31-bt)
set_tests_properties(unit-s31-bt PROPERTIES TIMEOUT 10)
add_test(NAME unit-s31-bt-negative COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/ports/esp32s31/tests/bt_negative.py ${CMAKE_BINARY_DIR})
add_test(NAME unit-s31-hal-map COMMAND test-s31-hal-map)
add_executable(test-board-vendor-charger tests/board_vendor/charger.c)
target_link_libraries(test-board-vendor-charger PRIVATE ink_drivers)
target_compile_options(test-board-vendor-charger PRIVATE ${INK_WARN} -UNDEBUG)
add_test(NAME board-vendor-charger COMMAND test-board-vendor-charger)
add_executable(test-board-vendor-light tests/board_vendor/light.c system/services/display/light.c system/services/res/svc.c)
target_include_directories(test-board-vendor-light PRIVATE platform/include system/include)
target_compile_options(test-board-vendor-light PRIVATE ${INK_WARN} -UNDEBUG)
add_test(NAME board-vendor-light COMMAND test-board-vendor-light)
ink_sim_executable(sim-board-vendor-power tests/board_vendor/power.c)
target_link_libraries(sim-board-vendor-power PRIVATE ink_system)
add_test(NAME board-vendor-power COMMAND sim-board-vendor-power
  --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
  --budget ${CMAKE_SOURCE_DIR}/tests/ui_scene.budget.json
  --out ${CMAKE_BINARY_DIR}/test-out/board-vendor-power --end 75s --quiet --no-random-outages)
set(INK_IME_CASES ${CMAKE_BINARY_DIR}/generated/ime_cases.h)
add_custom_command(OUTPUT ${INK_IME_CASES}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ime_lexicon/fixtures.py ${INK_IME_CASES}
  DEPENDS tests/ime_lexicon/fixtures.py tests/ime_lexicon/cases.tsv)
ink_sim_executable(sim-ime-lexicon tests/ime_lexicon/device.c ${INK_IME_CASES})
target_include_directories(sim-ime-lexicon PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(sim-ime-lexicon PRIVATE ink_apps ink_system)
add_dependencies(sim-ime-lexicon ink_fonts ink_test_content)
add_test(NAME ime-lexicon COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ime_lexicon/run.py --build ${CMAKE_BINARY_DIR})
ink_sim_executable(sim-ime-learning tests/ime_lexicon/learning.c)
target_link_libraries(sim-ime-learning PRIVATE ink_apps ink_system)
add_dependencies(sim-ime-learning ink_fonts ink_ui_fonts ink_icons)
add_test(NAME ime-learning COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ime_lexicon/learning.py --build ${CMAKE_BINARY_DIR})
set(INK_IMAGE_FIXTURES ${CMAKE_BINARY_DIR}/generated/image_fixtures.h)
add_custom_command(OUTPUT ${INK_IMAGE_FIXTURES}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/media/fixtures.py ${INK_IMAGE_FIXTURES}
  DEPENDS tests/media/fixtures.py)
ink_sim_executable(sim-image tests/media/image.c ${INK_IMAGE_FIXTURES})
target_include_directories(sim-image PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(sim-image PRIVATE ink_system)
foreach(case decode dither cache version)
  add_test(NAME image-${case} COMMAND ${CMAKE_COMMAND} -E env INK_IMAGE_TEST=${case} $<TARGET_FILE:sim-image>
    --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
    --budget ${CMAKE_SOURCE_DIR}/tests/media/image.budget.json
    --out ${CMAKE_BINARY_DIR}/test-out/image-${case} --end 10s --quiet --no-random-outages)
endforeach()
ink_sim_executable(sim-backup-restore tests/backup_restore/device.c)
target_link_libraries(sim-backup-restore PRIVATE ink_apps ink_system)
target_compile_options(sim-backup-restore PRIVATE -Wno-format-truncation) # a test path cut short fails the test anyway
if(INK_HAVE_FONTS)
  add_dependencies(sim-backup-restore ink_fonts ink_test_content)
endif()
add_test(NAME backup-restore-recovery COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/backup_restore/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(backup-restore-recovery PROPERTIES TIMEOUT 240 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
target_compile_options(sim-backup-restore PRIVATE -Wno-format-truncation) # a test path cut short fails the test anyway
add_test(NAME ui-scale COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/ui_scale_check.py)
ink_sim_executable(sim-lock-faces tests/lock_faces.c)
target_link_libraries(sim-lock-faces PRIVATE ink_apps ink_system)
add_test(NAME lock-faces COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/lock_faces.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(lock-faces PROPERTIES TIMEOUT 240)
ink_sim_executable(sim-lock-cover tests/lock_cover.c)
target_link_libraries(sim-lock-cover PRIVATE ink_apps ink_system)
add_test(NAME lock-cover COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/lock_cover.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(lock-cover PROPERTIES TIMEOUT 600)
add_test(NAME ui-scale-regressions COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/ui_scale_check_test.py)
add_test(NAME ui-compare-regressions COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/ui_compare_test.py)
add_test(NAME ui-icons COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/icongen/check.py)
add_test(NAME ui-icons-regressions COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/ui_icon_check_test.py)
add_test(NAME ui-assets COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/ui_assets_check.py ${INK_ASSETS_DIR})
add_test(NAME unit-asset-pack COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/ports/esp32s31/tests/asset_pack_test.py)
add_executable(sim-asset-pack simulator/runtime/sim_main.c tests/asset_pack/device.c)
target_link_libraries(sim-asset-pack PRIVATE ink_apps ink_system ink_port_sim)
target_compile_options(sim-asset-pack PRIVATE ${INK_WARN})
add_test(NAME integration-asset-pack COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/asset_pack/run.py --build ${CMAKE_BINARY_DIR})
# Compressed fonts (font-pack-1): every glyph of every face against fontgen's uncompressed copy, the negative control
# (ink_font.c reading the compressed bytes as bitmaps), and 58 more UI characters still leaving font_data 1 MiB.
add_executable(test-font-pack tests/font_pack/compare.c system/text/ink_font.c)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/font_pack/ink_font_undecoded.c
  COMMAND ${CMAKE_COMMAND} -DIN=${CMAKE_SOURCE_DIR}/system/text/ink_font.c -DOUT=${CMAKE_BINARY_DIR}/font_pack/ink_font_undecoded.c
          -P ${CMAKE_SOURCE_DIR}/tests/font_pack/undecoded.cmake
  DEPENDS system/text/ink_font.c tests/font_pack/undecoded.cmake)
add_executable(test-font-pack-undecoded tests/font_pack/compare.c ${CMAKE_BINARY_DIR}/font_pack/ink_font_undecoded.c)
foreach(t test-font-pack test-font-pack-undecoded)
  target_include_directories(${t} PRIVATE system/include)
  target_link_libraries(${t} PRIVATE ink_lvgl)
  target_compile_options(${t} PRIVATE ${INK_WARN} -UNDEBUG)
  add_dependencies(${t} ink_fonts ink_ui_fonts)
endforeach()
add_test(NAME font-pack-glyphs COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/font_pack/run.py glyphs --build ${CMAKE_BINARY_DIR})
add_test(NAME font-pack-glyphs-reverse COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/font_pack/run.py glyphs
  --build ${CMAKE_BINARY_DIR} --binary $<TARGET_FILE:test-font-pack-undecoded> --expect-fail)
add_test(NAME font-pack-overflow COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/font_pack/run.py overflow
  --build ${CMAKE_BINARY_DIR} --regular ${INK_CJK_FONT} --face ${INK_CJK_FACE} --medium ${INK_CJK_MEDIUM_FONT} --bold ${INK_CJK_BOLD_FONT})
set_tests_properties(font-pack-glyphs font-pack-glyphs-reverse font-pack-overflow PROPERTIES TIMEOUT 300)
add_test(NAME unit-flash-plan COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/ports/esp32s31/tests/flash_plan_test.py)
add_executable(sim-ui-scenes simulator/runtime/sim_main.c tests/ui_shell_scenes.c)
target_link_libraries(sim-ui-scenes PRIVATE ink_apps ink_system ink_port_sim)
target_compile_options(sim-ui-scenes PRIVATE ${INK_WARN} -Wno-format-truncation)
ink_sim_executable(sim-ui-borders tests/ui_borders.c)
target_link_libraries(sim-ui-borders PRIVATE ink_apps ink_system)
add_test(NAME ui-borders COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ui_borders.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(ui-borders PROPERTIES TIMEOUT 180)
set(INK_SHELL_SCENES Main Today Apps AppsSearch Lock LockPage Control Notifications Members MembersBack Pin PinWrong
  ImePinyin ImeCandidates ImeEdit ImePassword ImeVoice Confirm ConfirmDelete ConfirmSecret AiListen AiThink Ai
  Settings SetDisplay SetSound SetNetwork SetVoice SetAbout SetPrivacy ImeSymbols
  PowerMenu PowerOff PhoneIncoming ClockRing Components ComponentsPanel)
foreach(scene ${INK_SHELL_SCENES})
  if(scene STREQUAL "PowerOff")
    set(budget ui_off_scene)
  else()
    set(budget ui_scene)
  endif()
  set(script ui_shell)
  if(scene MATCHES "^(MembersBack|Pin|PinWrong)$")
    set(script ui_shell_gate) # waits past the D-95 two minutes
  endif()
  add_test(NAME ui-scene-${scene}
    COMMAND ${CMAKE_COMMAND} -E env INK_UI_SCENE=${scene} $<TARGET_FILE:sim-ui-scenes>
      --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json --script ${CMAKE_SOURCE_DIR}/tests/${script}.script
      --hub ${CMAKE_SOURCE_DIR}/tests/ui_shell.hub.json
      --budget ${CMAKE_SOURCE_DIR}/tests/${budget}.budget.json --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
      --out ${CMAKE_BINARY_DIR}/test-out/ui-scene-${scene} --quiet --no-random-outages)
  # The largest text, which is also what a child sees.
  add_test(NAME ui-scene-big-${scene}
    COMMAND ${CMAKE_COMMAND} -E env INK_UI_SCENE=${scene} INK_UI_SCENE_TEXT=3 $<TARGET_FILE:sim-ui-scenes>
      --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json --script ${CMAKE_SOURCE_DIR}/tests/${script}.script
      --hub ${CMAKE_SOURCE_DIR}/tests/ui_shell.hub.json
      --budget ${CMAKE_SOURCE_DIR}/tests/${budget}.budget.json --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
      --out ${CMAKE_BINARY_DIR}/test-out/ui-scene-big-${scene} --quiet --no-random-outages)
endforeach()
foreach(scene Main Today Apps AppsSearch Control Notifications Ai ImePinyin)
  add_test(NAME ui-scene-child-${scene}
    COMMAND ${CMAKE_COMMAND} -E env INK_UI_SCENE=${scene} INK_UI_SCENE_MEMBER=kid $<TARGET_FILE:sim-ui-scenes>
      --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json --script ${CMAKE_SOURCE_DIR}/tests/ui_shell.script
      --hub ${CMAKE_SOURCE_DIR}/tests/ui_shell.hub.json
      --budget ${CMAKE_SOURCE_DIR}/tests/ui_scene.budget.json --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
      --out ${CMAKE_BINARY_DIR}/test-out/ui-scene-child-${scene} --quiet --no-random-outages)
endforeach()
add_executable(sim-ui-shell-test simulator/runtime/sim_main.c tests/ui_shell_test.c)
target_link_libraries(sim-ui-shell-test PRIVATE ink_apps ink_system ink_port_sim)
target_compile_options(sim-ui-shell-test PRIVATE ${INK_WARN} -Wno-format-truncation)
add_test(NAME ui-shell-priorities
  COMMAND sim-ui-shell-test --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
    --script ${CMAKE_SOURCE_DIR}/tests/ui_shell_test.script --budget ${CMAKE_SOURCE_DIR}/tests/ui_shell.budget.json
    --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT} --out ${CMAKE_BINARY_DIR}/test-out/ui-shell --quiet --no-random-outages)
add_test(NAME ui-panel-restore
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/ui_panel_check.py ${CMAKE_BINARY_DIR}/test-out/ui-shell)
set_tests_properties(ui-panel-restore PROPERTIES DEPENDS ui-shell-priorities)
add_executable(sim-ui-regressions simulator/runtime/sim_main.c tests/ui_shell_regressions.c)
target_link_libraries(sim-ui-regressions PRIVATE ink_apps ink_system ink_port_sim)
target_compile_options(sim-ui-regressions PRIVATE ${INK_WARN} -Wno-format-truncation)
ink_sim_executable(sim-ui-semantics tests/ui_semantics.c)
target_link_libraries(sim-ui-semantics PRIVATE ink_apps ink_system)
ink_sim_executable(sim-semantics-shell tests/semantics_shell/main.c tests/semantics_shell/confirm.c
  tests/semantics_shell/interrupt.c tests/semantics_shell/power.c tests/semantics_shell/member.c
  tests/semantics_shell/lock.c tests/semantics_shell/off.c)
target_link_libraries(sim-semantics-shell PRIVATE ink_apps ink_system)
foreach(scene confirm interrupt power member lock off)
  add_test(NAME semantics-shell-${scene} COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/semantics_shell/run.py
    --build ${CMAKE_BINARY_DIR} --scene ${scene})
  set_tests_properties(semantics-shell-${scene} PROPERTIES TIMEOUT 120 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
endforeach()
ink_sim_executable(sim-semantics-shell2 tests/semantics_shell2/main.c tests/semantics_shell2/keyboard.c
  tests/semantics_shell2/ai.c tests/semantics_shell2/member.c tests/semantics_shell2/kid.c
  tests/semantics_shell2/battery.c tests/semantics_shell2/home.c)
target_link_libraries(sim-semantics-shell2 PRIVATE ink_apps ink_system)
ink_sim_executable(sim-ime-keyboard1 tests/ime_keyboard1.c)
target_link_libraries(sim-ime-keyboard1 PRIVATE ink_apps ink_system)
add_test(NAME ime-keyboard1 COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/ime_keyboard1.py
  --build ${CMAKE_BINARY_DIR})
set_tests_properties(ime-keyboard1 PROPERTIES TIMEOUT 180 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
foreach(scene keyboard ai member kid battery home)
  add_test(NAME semantics-shell2-${scene} COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/semantics_shell2/run.py
    --build ${CMAKE_BINARY_DIR} --scene ${scene})
  set_tests_properties(semantics-shell2-${scene} PROPERTIES TIMEOUT 120 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
endforeach()
ink_sim_executable(sim-jot tests/jot/app.c examples/jot/jot.c)
target_link_libraries(sim-jot PRIVATE ink_apps ink_system)
add_test(NAME app-framework-example COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/jot/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(app-framework-example PROPERTIES TIMEOUT 320)
add_test(NAME app-framework-snippets COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/jot/check_doc.py)
ink_sim_executable(sim-fw-gaps tests/fw_gaps/app.c tests/fw_gaps/db_probe.c)
target_link_libraries(sim-fw-gaps PRIVATE ink_apps ink_system)
add_test(NAME app-framework-gaps COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/fw_gaps/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(app-framework-gaps PROPERTIES TIMEOUT 180 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
add_test(NAME ui-semantics
  COMMAND sim-ui-semantics --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
    --script ${CMAKE_SOURCE_DIR}/tests/ui_semantics.script --budget ${CMAKE_SOURCE_DIR}/tests/ui_scene.budget.json
    --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT} --out ${CMAKE_BINARY_DIR}/test-out/ui-semantics --quiet --no-random-outages)
ink_sim_executable(sim-ui-list-paging tests/ui_list_paging.c)
ink_sim_executable(sim-settings-a tests/settings_a.c)
target_link_libraries(sim-settings-a PRIVATE ink_apps ink_system)
add_test(NAME settings-a COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/settings_a.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(settings-a PROPERTIES TIMEOUT 180)
foreach(case pages phone voice)
  ink_sim_executable(sim-small-gaps-${case} tests/small_gaps/${case}.c)
  target_link_libraries(sim-small-gaps-${case} PRIVATE ink_apps ink_system)
  add_dependencies(sim-small-gaps-${case} ink_licenses)
endforeach()
add_test(NAME small-gaps-1 COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/small_gaps/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(small-gaps-1 PROPERTIES TIMEOUT 600)
ink_sim_executable(sim-small-gaps-2 tests/small_gaps_2/ui.c)
target_link_libraries(sim-small-gaps-2 PRIVATE ink_apps ink_system)
foreach(case icons phone picker calculator airplane)
  add_test(NAME small-gaps-2-${case} COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/small_gaps_2/run.py --build ${CMAKE_BINARY_DIR} --case ${case})
  set_tests_properties(small-gaps-2-${case} PROPERTIES TIMEOUT 200)
endforeach()
target_link_libraries(sim-ui-list-paging PRIVATE ink_apps ink_system)
ink_sim_executable(sim-search tests/search/search.c)
target_link_libraries(sim-search PRIVATE ink_apps ink_system)
add_test(NAME search COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/search/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(search PROPERTIES TIMEOUT 240)
add_test(NAME ui-list-paging
  COMMAND sim-ui-list-paging --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
    --script ${CMAKE_SOURCE_DIR}/tests/ui_list_paging.script --budget ${CMAKE_SOURCE_DIR}/tests/ui_scene.budget.json
    --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT} --out ${CMAKE_BINARY_DIR}/test-out/ui-list-paging --quiet --no-random-outages)
add_test(NAME ui-list-snapshots COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ui_list_snapshots.py
  ${CMAKE_BINARY_DIR}/test-out/ui-list-paging/list-snapshots.ndjson)
set_tests_properties(ui-list-snapshots PROPERTIES DEPENDS ui-list-paging)
ink_sim_executable(sim-ui-input-host tests/ui_input_host.c)
target_link_libraries(sim-ui-input-host PRIVATE ink_apps ink_system)
add_test(NAME ui-input-host
  COMMAND sim-ui-input-host --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
    --script ${CMAKE_SOURCE_DIR}/tests/ui_input_host.script --budget ${CMAKE_SOURCE_DIR}/tests/ui_scene.budget.json
    --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT} --out ${CMAKE_BINARY_DIR}/test-out/ui-input-host --quiet --no-random-outages)
ink_sim_executable(sim-ui-cards tests/ui_cards.c)
ink_sim_executable(sim-boot-flow tests/boot_flow.c)
ink_sim_executable(sim-configured tests/configured_boot.c)
target_link_libraries(sim-configured PRIVATE ink_apps ink_system)
target_link_libraries(sim-boot-flow PRIVATE ink_apps ink_system)
add_test(NAME boot-flow COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/boot_flow_check.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(boot-flow PROPERTIES TIMEOUT 200)
ink_sim_executable(sim-i18n-lang tests/i18n_lang.c)
ink_sim_executable(sim-i18n-services tests/i18n_services.c)
target_link_libraries(sim-i18n-services PRIVATE ink_apps ink_system)
add_test(NAME i18n-services COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/i18n_services.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(i18n-services PROPERTIES TIMEOUT 180)
add_executable(sim-i18n-services-negative EXCLUDE_FROM_ALL simulator/runtime/sim_main.c tests/i18n_services.c)
target_link_libraries(sim-i18n-services-negative PRIVATE ink_apps ink_system ink_port_sim)
target_compile_definitions(sim-i18n-services-negative PRIVATE INK_I18N_REVERSE=1)
ink_sim_executable(sim-time-settings tests/time_settings.c)
target_link_libraries(sim-time-settings PRIVATE ink_apps ink_system)
add_test(NAME time-settings COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/time_settings.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(time-settings PROPERTIES TIMEOUT 200)
target_link_libraries(sim-i18n-lang PRIVATE ink_apps ink_system)
add_test(NAME i18n-lang COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/i18n_check.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(i18n-lang PROPERTIES TIMEOUT 300)
ink_sim_executable(sim-i18n-apps tests/i18n_apps.c)
ink_sim_executable(sim-i18n-memo tests/i18n_apps.c)
target_compile_definitions(sim-i18n-memo PRIVATE INK_I18N_MEMO=1)
target_link_libraries(sim-i18n-apps PRIVATE ink_apps ink_system)
target_link_libraries(sim-i18n-memo PRIVATE ink_apps ink_system)
add_test(NAME i18n-apps COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/i18n_apps.py --build ${CMAKE_BINARY_DIR} --case apps)
add_test(NAME i18n-memo COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/i18n_apps.py --build ${CMAKE_BINARY_DIR} --case memo)
set_tests_properties(i18n-apps i18n-memo PROPERTIES TIMEOUT 660)
ink_sim_executable(sim-i18n-shell tests/i18n_shell.c)
target_link_libraries(sim-i18n-shell PRIVATE ink_apps ink_system)
add_test(NAME i18n-shell COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/i18n_shell.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(i18n-shell PROPERTIES TIMEOUT 240)
ink_sim_executable(sim-i18n-shell-remaining tests/i18n_shell_remaining.c)
target_link_libraries(sim-i18n-shell-remaining PRIVATE ink_apps ink_system)
add_test(NAME i18n-shell-remaining COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/i18n_shell.py
  --build ${CMAKE_BINARY_DIR} --binary $<TARGET_FILE:sim-i18n-shell-remaining> --out ${CMAKE_BINARY_DIR}/test-out/i18n-shell-remaining)
set_tests_properties(i18n-shell-remaining PROPERTIES TIMEOUT 240)
add_test(NAME i18n-bookshelf COMMAND ${CMAKE_COMMAND} -E env INK_I18N_BOOKSHELF=1 $<TARGET_FILE:sim-i18n-shell-remaining>
  --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json --script ${CMAKE_SOURCE_DIR}/tests/i18n_lang.script
  --budget ${CMAKE_SOURCE_DIR}/tests/ui_scene.budget.json --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
  --out ${CMAKE_BINARY_DIR}/test-out/i18n-bookshelf --quiet --no-random-outages)
add_test(NAME i18n-extras COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/i18n_shell.py
  --build ${CMAKE_BINARY_DIR} --binary $<TARGET_FILE:sim-i18n-shell-remaining>
  --out ${CMAKE_BINARY_DIR}/test-out/i18n-extras --min-frames 10)
set_tests_properties(i18n-extras PROPERTIES ENVIRONMENT "INK_I18N_EXTRAS=1" TIMEOUT 240)
add_test(NAME i18n-tables COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/i18n_tables.py)
add_test(NAME i18n-literals COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/i18n_literals.py --root ${CMAKE_BINARY_DIR}/reader-i18n-check)
add_test(NAME boot-flow-reboot COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/boot_flow_reboot.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(boot-flow-reboot PROPERTIES TIMEOUT 200)
target_link_libraries(sim-ui-cards PRIVATE ink_apps ink_system)
add_test(NAME ui-cards
  COMMAND sim-ui-cards --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
    --script ${CMAKE_SOURCE_DIR}/tests/ui_cards.script --budget ${CMAKE_SOURCE_DIR}/tests/ui_scene.budget.json
    --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT} --out ${CMAKE_BINARY_DIR}/test-out/ui-cards --quiet --no-random-outages)
# D-135, the one-page home: touch and the AI reach the same entries; each member keeps their own six favourites.
ink_sim_executable(sim-home-one-page tests/home_one_page.c)
target_link_libraries(sim-home-one-page PRIVATE ink_apps ink_system)
add_test(NAME home-one-page
  COMMAND sim-home-one-page --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
    --script ${CMAKE_SOURCE_DIR}/tests/ui_cards.script --budget ${CMAKE_SOURCE_DIR}/tests/ui_scene.budget.json
    --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT} --out ${CMAKE_BINARY_DIR}/test-out/home-one-page --quiet --no-random-outages)
# control-1: the control centre after AOSP quick settings and the status bar after MuditaOS, by touch and by the AI;
# a toggled tile and a new minute refresh only what changed (checked from the display trace).
ink_sim_executable(sim-control-center tests/control_center.c)
target_link_libraries(sim-control-center PRIVATE ink_apps ink_system)
add_test(NAME control-center COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/control_center.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(control-center PROPERTIES TIMEOUT 900)
ink_sim_executable(sim-text-long tests/text_long/editor.c)
target_link_libraries(sim-text-long PRIVATE ink_apps ink_system)
add_test(NAME text-long COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/text_long/run.py --build ${CMAKE_BINARY_DIR})
foreach(profile metalio-plus desktop-lcd)
  foreach(regression confirm slider context privacy answer snapshot alarm theme)
    add_test(NAME ui-${regression}-regressions-${profile}
      COMMAND ${CMAKE_COMMAND} -E env INK_UI_REGRESSION=${regression} $<TARGET_FILE:sim-ui-regressions>
        --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/${profile}.json --script ${CMAKE_SOURCE_DIR}/tests/ui_shell_regressions.script
        --budget ${CMAKE_SOURCE_DIR}/tests/ui_scene.budget.json --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
        --out ${CMAKE_BINARY_DIR}/test-out/ui-${regression}-${profile} --quiet --no-random-outages)
  endforeach()
endforeach()
ink_sim_executable(sim-pairing tests/pairing/device.c)
target_link_libraries(sim-pairing PRIVATE ink_apps ink_system)
if(INK_HAVE_FONTS)
  add_dependencies(sim-pairing ink_fonts ink_test_content)
endif()
add_test(NAME pairing-recovery COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/pairing/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(pairing-recovery PROPERTIES TIMEOUT 180 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
ink_sim_executable(sim-pairing-storage tests/pairing/storage_faults.c)
target_link_libraries(sim-pairing-storage PRIVATE ink_apps ink_system)
if(INK_HAVE_FONTS)
  add_dependencies(sim-pairing-storage ink_fonts ink_test_content)
endif()
add_test(NAME pairing-storage-failures COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/pairing/storage.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(pairing-storage-failures PROPERTIES TIMEOUT 120 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
set(INK_HUB_PYTHON "${Python3_EXECUTABLE}" CACHE FILEPATH "Python with the locked hub test dependencies")
add_executable(hub-json-check tests/hub/json_check.c system/util/ink_json.c)
target_include_directories(hub-json-check PRIVATE platform/include system/include third_party/jsmn)
target_compile_options(hub-json-check PRIVATE ${INK_WARN})
ink_sim_executable(sim-hub-cap-export tests/hub/export_caps.c)
target_link_libraries(sim-hub-cap-export PRIVATE ink_apps ink_system)
ink_sim_executable(sim-hub-e2e tests/hub/e2e_device.c)
target_link_libraries(sim-hub-e2e PRIVATE ink_apps ink_system)
if(INK_HAVE_FONTS)
  add_dependencies(sim-hub-cap-export ink_fonts ink_test_content)
  add_dependencies(sim-hub-e2e ink_fonts ink_test_content)
endif()
add_test(NAME hub-export-caps COMMAND sim-hub-cap-export
  --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
  --assets ${INK_ASSETS_DIR} --content ${CMAKE_BINARY_DIR}/test-content
  --out ${CMAKE_BINARY_DIR}/test-out/hub --end 3s --quiet --no-random-outages)
set_tests_properties(hub-export-caps PROPERTIES FIXTURES_SETUP hub-caps)
ink_sim_executable(sim-capability-inventory tests/capability_table/export.c)
target_compile_definitions(sim-capability-inventory PRIVATE INK_MOGO_SIM=1)
target_link_libraries(sim-capability-inventory PRIVATE ink_apps ink_system)
if(INK_HAVE_FONTS)
  add_dependencies(sim-capability-inventory ink_fonts ink_test_content)
endif()
add_test(NAME hw-bench-report COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/hw_bench/report_test.py)
add_test(NAME hw-selftest-run COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/hw_selftest/run_test.py)
add_test(NAME trial-run-fonts COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/hw_selftest/trial_run_test.py)
add_test(NAME capability-table-doc COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tools/capabilities.py --check)
add_test(NAME capability-table-regressions COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/capability_table/test_catalogue.py)
add_test(NAME capability-table-export COMMAND sim-capability-inventory
  --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
  --assets ${INK_ASSETS_DIR} --content ${CMAKE_BINARY_DIR}/test-content
  --out ${CMAKE_BINARY_DIR}/test-out/capability-table --end 3s --quiet --no-random-outages)
set_tests_properties(capability-table-export PROPERTIES FIXTURES_SETUP capability-inventory)
add_test(NAME capability-table-runtime COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/capability_table/check_runtime.py
  ${CMAKE_BINARY_DIR}/test-out/capability-table/capability-inventory.json)
set_tests_properties(capability-table-runtime PROPERTIES FIXTURES_REQUIRED capability-inventory)
ink_sim_executable(sim-capability-policy tests/capability_table/policy.c)
target_link_libraries(sim-capability-policy PRIVATE ink_apps ink_system)
if(INK_HAVE_FONTS)
  add_dependencies(sim-capability-policy ink_fonts ink_test_content)
endif()
add_test(NAME capability-table-policy-export COMMAND sim-capability-policy
  --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
  --budget ${CMAKE_SOURCE_DIR}/tests/capability_table/policy.budget.json
  --assets ${INK_ASSETS_DIR} --content ${CMAKE_BINARY_DIR}/test-content
  --out ${CMAKE_BINARY_DIR}/test-out/capability-policy --end 15s --quiet --no-random-outages)
set_tests_properties(capability-table-policy-export PROPERTIES FIXTURES_SETUP capability-policy)
add_test(NAME capability-table-policy COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/capability_table/check_policy.py
  ${CMAKE_BINARY_DIR}/test-out/capability-policy/capability-policy.json)
set_tests_properties(capability-table-policy PROPERTIES FIXTURES_REQUIRED capability-policy)
add_executable(hub-bridge-lifecycle tests/hub/bridge_lifecycle.c)
target_include_directories(hub-bridge-lifecycle PRIVATE simulator)
target_link_libraries(hub-bridge-lifecycle PRIVATE ink_sim_core)
target_compile_options(hub-bridge-lifecycle PRIVATE ${INK_WARN})
add_test(NAME hub-bridge-lifecycle COMMAND hub-bridge-lifecycle)
set_tests_properties(hub-bridge-lifecycle PROPERTIES TIMEOUT 30)
add_test(NAME services-mid-capability-doc COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/services_mid/check_capability_doc.py)
add_test(NAME services-mid-regressions COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/services_finish/run.py --build ${CMAKE_BINARY_DIR}
  update_history offpanel text_window keyboard_snapshot undo empty_removal)
set_tests_properties(services-mid-regressions PROPERTIES TIMEOUT 180 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
add_test(NAME file-index-regressions COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/services_finish/run.py --build ${CMAKE_BINARY_DIR} file_index)
set_tests_properties(file-index-regressions PROPERTIES TIMEOUT 180 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
add_test(NAME confirm-durable COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/confirm_durable/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(confirm-durable PROPERTIES TIMEOUT 180 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
ink_sim_executable(sim-hub-bridge-state tests/hub/bridge_state.c)
ink_sim_executable(sim-device-events tests/device_events/protocol.c tests/device_events/cap_boundary.c
  tests/device_events/hub_boundary.c tests/device_events/ai_boundary.c)
target_link_libraries(sim-device-events PRIVATE ink_apps ink_system)
ink_sim_executable(sim-device-events-review tests/device_events/review.c tests/device_events/hub_boundary.c
  tests/device_events/ai_boundary.c tests/device_events/hub_observer.c)
target_link_libraries(sim-device-events-review PRIVATE ink_apps ink_system)
ink_sim_executable(sim-device-events-isolation tests/device_events/isolation.c tests/device_events/member_boundary.c
  tests/device_events/ai_boundary.c tests/device_events/hub_boundary.c tests/device_events/hub_observer.c)
target_link_libraries(sim-device-events-isolation PRIVATE ink_apps ink_system)
ink_sim_executable(sim-device-events-private tests/device_events/private.c tests/device_events/member_boundary.c
  tests/device_events/ai_boundary.c tests/device_events/hub_boundary.c tests/device_events/hub_observer.c)
target_link_libraries(sim-device-events-private PRIVATE ink_apps ink_system)
add_test(NAME device-events COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/device_events/run.py
  --build ${CMAKE_BINARY_DIR})
set_tests_properties(device-events PROPERTIES TIMEOUT 120 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
add_test(NAME hub-bridge-generations COMMAND sim-hub-bridge-state
  --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
  --out ${CMAKE_BINARY_DIR}/test-out/hub-bridge-state --end 2s --quiet --no-random-outages)
if(INK_HUB_PYTHON)
  add_test(NAME hub-contract COMMAND ${INK_HUB_PYTHON} ${CMAKE_SOURCE_DIR}/tests/hub/run_contract.py
    --caps ${CMAKE_BINARY_DIR}/test-out/hub/caps.json --samples ${CMAKE_BINARY_DIR}/test-out/hub/messages.jsonl)
  # The hub suite grew with hub-real-1 (about 70 s alone); 120 s timed out on a loaded machine.
  set_tests_properties(hub-contract PROPERTIES FIXTURES_REQUIRED hub-caps FIXTURES_SETUP hub-messages SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME hub-json-crosscheck COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/hub/check_samples.py
    $<TARGET_FILE:hub-json-check> ${CMAKE_BINARY_DIR}/test-out/hub/messages.jsonl)
  set_tests_properties(hub-json-crosscheck PROPERTIES FIXTURES_REQUIRED hub-messages SKIP_RETURN_CODE 77)
  add_test(NAME hub-simulator-e2e COMMAND ${INK_HUB_PYTHON} ${CMAKE_SOURCE_DIR}/tests/hub/e2e.py
    --sim $<TARGET_FILE:sim-hub-e2e> --parser $<TARGET_FILE:hub-json-check>
    --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
    --out ${CMAKE_BINARY_DIR}/test-out/hub-e2e)
  set_tests_properties(hub-simulator-e2e PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
endif()
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
ink_sim_executable(sim-binary-preview tests/simulator/binary_preview.c)
add_test(NAME selftest-binary-preview COMMAND sim-binary-preview
  --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
  --budget ${CMAKE_SOURCE_DIR}/tests/simulator/binary_preview.budget.json
  --out ${CMAKE_BINARY_DIR}/test-out/binary-preview --end 8s --quiet --no-random-outages)
add_executable(pmic-retry-test tests/unit/pmic_retry_test.c drivers/chips/tps65185/tps65185.c)
target_include_directories(pmic-retry-test PRIVATE platform/include drivers/chips)
target_compile_options(pmic-retry-test PRIVATE ${INK_WARN})
add_test(NAME unit-pmic-retry COMMAND pmic-retry-test)
add_executable(pools-test tests/unit/pools_test.c)
target_include_directories(pools-test PRIVATE simulator/memory simulator/kernel/vtk)
target_link_libraries(pools-test PRIVATE ink_platform ink_tlsf)
target_compile_options(pools-test PRIVATE ${INK_WARN})
add_test(NAME unit-pools COMMAND pools-test)
add_test(NAME sdk-boundary COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/tests/sdk_boundary.cmake)
add_test(NAME layering COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/tests/layering.cmake)
add_executable(pqbn-clock-test tests/unit/pqbn_clock_test.c)
target_link_libraries(pqbn-clock-test PRIVATE ink_drivers)
target_compile_options(pqbn-clock-test PRIVATE ${INK_WARN})
add_test(NAME unit-pqbn-clock COMMAND pqbn-clock-test)
add_executable(pqbn-path-test tests/unit/pqbn_path_test.c)
target_link_libraries(pqbn-path-test PRIVATE ink_drivers)
target_compile_options(pqbn-path-test PRIVATE ${INK_WARN})
add_test(NAME unit-pqbn-path COMMAND pqbn-path-test)
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
# The structured data store: records, pages, members, limits, the capabilities (tests/integration/data.c).
add_executable(sim-data simulator/runtime/sim_main.c tests/integration/data.c)
target_link_libraries(sim-data PRIVATE ink_system ink_port_sim)
target_compile_options(sim-data PRIVATE ${INK_WARN} -Wno-format-truncation) # a message cut short does no harm
add_test(NAME integration-data
  COMMAND sim-data --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          --budget ${CMAKE_SOURCE_DIR}/tests/integration/data.budget.json
          --script ${CMAKE_SOURCE_DIR}/tests/integration/data.script
          --out ${CMAKE_BINARY_DIR}/test-out/data --quiet --no-random-outages)
# The file service: paths, permissions, copy and delete jobs, the USB disk (tests/integration/files.c).
configure_file(tests/integration/files.script.in ${CMAKE_BINARY_DIR}/test-out/files.script @ONLY)
add_executable(sim-files simulator/runtime/sim_main.c tests/integration/files.c)
target_link_libraries(sim-files PRIVATE ink_system ink_port_sim)
target_compile_options(sim-files PRIVATE ${INK_WARN} -Wno-format-truncation) # a message cut short does no harm
add_test(NAME integration-files
  COMMAND sim-files --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          --budget ${CMAKE_SOURCE_DIR}/tests/integration/files.budget.json
          --script ${CMAKE_BINARY_DIR}/test-out/files.script
          --out ${CMAKE_BINARY_DIR}/test-out/files --quiet --no-random-outages)
# The time service: the clock, zones, alarms and reminders, waking from sleep (tests/integration/timesvc.c).
ink_sim_executable(sim-wireless-transfer tests/wireless_transfer/device.c)
target_link_libraries(sim-wireless-transfer PRIVATE ink_apps ink_system)
add_test(NAME integration-wireless-transfer COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/wireless_transfer/client.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(integration-wireless-transfer PROPERTIES TIMEOUT 65)
add_executable(sim-time simulator/runtime/sim_main.c tests/integration/timesvc.c)
ink_sim_executable(sim-s31-net-time tests/esp32s31/net_time.c)
target_link_libraries(sim-s31-net-time PRIVATE ink_system)
add_test(NAME unit-s31-net-time COMMAND sim-s31-net-time --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
  --budget ${CMAKE_SOURCE_DIR}/tests/ui_scene.budget.json --out ${CMAKE_BINARY_DIR}/test-out/s31-net-time --end 8s --quiet --no-random-outages)
target_link_libraries(sim-time PRIVATE ink_system ink_port_sim)
target_compile_options(sim-time PRIVATE ${INK_WARN} -Wno-format-truncation) # a message cut short does no harm
foreach(v main rtc_lost line_low)
  add_test(NAME integration-time-${v}
    COMMAND ${CMAKE_COMMAND} -E env INK_TIME_PHASE=${v}
            $<TARGET_FILE:sim-time> --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --budget ${CMAKE_SOURCE_DIR}/tests/integration/timesvc_${v}.budget.json
            --script ${CMAKE_SOURCE_DIR}/tests/integration/timesvc_${v}.script
            --out ${CMAKE_BINARY_DIR}/test-out/time-${v} --quiet --no-random-outages)
endforeach()
# Alarms across a power-off: what came due is reported missed, what is ahead still rings.
add_test(NAME integration-time-reboot
  COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:sim-time> -DPROFILE=${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          -DOUT=${CMAKE_BINARY_DIR}/test-out/time_reboot -DSRC=${CMAKE_SOURCE_DIR}/tests/integration
          -P ${CMAKE_SOURCE_DIR}/tests/integration/timesvc_reboot.cmake)
add_executable(sha256-test tests/unit/sha256_test.c system/util/sha256.c)
target_include_directories(sha256-test PRIVATE system/include platform/include)
target_compile_options(sha256-test PRIVATE ${INK_WARN})
add_test(NAME unit-sha256 COMMAND sha256-test)
add_executable(lunar-test tests/unit/lunar_test.c system/services/time/lunar.c)
target_include_directories(lunar-test PRIVATE system/include)
target_compile_options(lunar-test PRIVATE ${INK_WARN})
add_test(NAME unit-lunar COMMAND lunar-test ${CMAKE_SOURCE_DIR}/tests/fixtures/lunar/chinese_years_apple.txt)
# The refresh policy on its own: EPDiy-class as before; a panel that cleans pages itself, a clean by count.
add_executable(refresh-policy-test tests/unit/refresh_policy_test.c system/services/display/refresh_policy.c)
target_include_directories(refresh-policy-test PRIVATE system/include platform/include)
target_compile_options(refresh-policy-test PRIVATE ${INK_WARN})
add_test(NAME unit-refresh-policy COMMAND refresh-policy-test)
# Power cut at every flash write of a run that adds, replaces, deletes and folds: nothing durable is lost.
add_test(NAME integration-data-powercut
  COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:sim-data> -DPROFILE=${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          -DOUT=${CMAKE_BINARY_DIR}/test-out/data_powercut -DSRC=${CMAKE_SOURCE_DIR}/tests/integration
          -P ${CMAKE_SOURCE_DIR}/tests/integration/data_powercut.cmake)
set_tests_properties(integration-data-powercut PROPERTIES TIMEOUT 300)
add_test(NAME determinism
  COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:sim-selftest>
          -DPROFILE=${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          -DSCRIPT=${CMAKE_SOURCE_DIR}/tests/simulator/selftest.script
          -DOUT=${CMAKE_BINARY_DIR}/test-out/determinism
          -P ${CMAKE_SOURCE_DIR}/tests/simulator/determinism.cmake)

# System scenarios: the real system on simulated devices, judged by budgets.
function(ink_scenario name profile)
  add_test(NAME scenario-${name}-${profile}
    COMMAND sim-configured --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/${profile}.json
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
  COMMAND sim-configured --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
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
  # Wi-Fi, members and the PIN (tests/integration/people.c).
  add_executable(sim-people simulator/runtime/sim_main.c tests/integration/people.c tests/integration/people_table.c)
  target_link_libraries(sim-people PRIVATE ink_apps ink_system ink_port_sim)
  target_compile_options(sim-people PRIVATE ${INK_WARN} -Wno-format-truncation)
  add_test(NAME integration-people
    COMMAND sim-people --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --budget ${CMAKE_SOURCE_DIR}/tests/integration/people.budget.json
            --script ${CMAKE_SOURCE_DIR}/tests/integration/people.script
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/people --quiet --no-random-outages)
  add_test(NAME integration-people-reset
    COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:sim-people> -DPROFILE=${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            -DASSETS=${INK_ASSETS_DIR} -DCONTENT=${INK_TEST_CONTENT}
            -DOUT=${CMAKE_BINARY_DIR}/test-out/people-reset -DSRC=${CMAKE_SOURCE_DIR}/tests/integration
            -P ${CMAKE_SOURCE_DIR}/tests/integration/people_reset.cmake)
  add_executable(sim-member-wake simulator/runtime/sim_main.c tests/integration/member_wake.c)
  target_link_libraries(sim-member-wake PRIVATE ink_apps ink_system ink_port_sim)
  target_compile_options(sim-member-wake PRIVATE ${INK_WARN} -Wno-format-truncation)
  add_test(NAME integration-member-wake
    COMMAND sim-member-wake --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --budget ${CMAKE_SOURCE_DIR}/tests/integration/member_wake.budget.json
            --script ${CMAKE_SOURCE_DIR}/tests/integration/member_wake.script
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/member-wake --quiet --no-random-outages)
  add_executable(sim-member-wake-stream simulator/runtime/sim_main.c tests/integration/member_wake_stream.c
                 tests/integration/member_wake_ai.c tests/integration/member_wake_dictation.c)
  target_link_libraries(sim-member-wake-stream PRIVATE ink_apps ink_system ink_port_sim)
  target_compile_options(sim-member-wake-stream PRIVATE ${INK_WARN} -Wno-format-truncation)
  add_test(NAME integration-member-wake-stream
    COMMAND sim-member-wake-stream --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus-handsfree.json
            --budget ${CMAKE_SOURCE_DIR}/tests/integration/member_wake.budget.json
            --script ${CMAKE_SOURCE_DIR}/tests/integration/member_wake_stream.script
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/member-wake-stream --quiet --no-random-outages)
  # Audio focus, media, recording, the headset (tests/integration/media.c).
  add_executable(sim-media simulator/runtime/sim_main.c tests/integration/media.c)
  target_link_libraries(sim-media PRIVATE ink_apps ink_system ink_port_sim)
  target_compile_options(sim-media PRIVATE ${INK_WARN} -Wno-format-truncation) # a message cut short does no harm
  add_test(NAME integration-media
    COMMAND sim-media --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --budget ${CMAKE_SOURCE_DIR}/tests/integration/media.budget.json
            --script ${CMAKE_SOURCE_DIR}/tests/integration/media.script
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/media --quiet --no-random-outages)
  # Text input and the clipboard (tests/integration/textinput.c).
  add_executable(sim-textinput simulator/runtime/sim_main.c tests/integration/textinput.c)
  target_link_libraries(sim-textinput PRIVATE ink_apps ink_system ink_port_sim)
  target_compile_options(sim-textinput PRIVATE ${INK_WARN} -Wno-format-truncation) # a message cut short does no harm
  add_dependencies(sim-textinput ink_mini_content)
  # The no-dictionary scenarios: the asset pack's word list is shadowed by an empty one (searched first), so only the
  # program's small table is left, as before the word list shipped in the pack.
  file(WRITE ${CMAKE_BINARY_DIR}/assets-noime/ime/base.ikd "")
  execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/asset_pack.py
    ${CMAKE_BINARY_DIR}/assets-noime --out ${CMAKE_BINARY_DIR}/assets-noime COMMAND_ERROR_IS_FATAL ANY)
  foreach(v main nocard baddict)
    set(text_assets --assets ${INK_ASSETS_DIR})
    if(NOT v STREQUAL main)
      set(text_assets --assets ${CMAKE_BINARY_DIR}/assets-noime --assets ${INK_ASSETS_DIR})
    endif()
    add_test(NAME integration-text-${v}
      COMMAND ${CMAKE_COMMAND} -E env INK_TEXT_PHASE=${v}
              $<TARGET_FILE:sim-textinput> --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
              --budget ${CMAKE_SOURCE_DIR}/tests/integration/textinput.budget.json
              --script ${CMAKE_SOURCE_DIR}/tests/integration/textinput_${v}.script
              ${text_assets} --content ${INK_MINI_CONTENT}
              --out ${CMAKE_BINARY_DIR}/test-out/text-${v} --quiet --no-random-outages)
  endforeach()
  # What the AI can reach, against the table in the design (tests/integration/ai_surface.c).
  # System upgrades, boot after boot: over the air, from the card, what goes wrong with each.
  add_executable(sim-update simulator/runtime/sim_main.c tests/integration/update.c)
  target_link_libraries(sim-update PRIVATE ink_apps ink_system ink_port_sim)
  target_compile_options(sim-update PRIVATE ${INK_WARN} -Wno-format-truncation) # a message cut short does no harm
  add_test(NAME integration-update
    COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:sim-update> -DPROFILE=${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            -DASSETS=${INK_ASSETS_DIR} -DCONTENT=${INK_TEST_CONTENT} -DOUT=${CMAKE_BINARY_DIR}/test-out/update
            -DSRC=${CMAKE_SOURCE_DIR}/tests/integration -DPY=${Python3_EXECUTABLE} -DTOOL=${CMAKE_SOURCE_DIR}/tools/inkfw/make_inkfw.py
            -P ${CMAKE_SOURCE_DIR}/tests/integration/update.cmake)

  # The power key held down: the power menu, and what it leaves alone.
  add_executable(sim-power-menu simulator/runtime/sim_main.c tests/integration/power_menu.c)
  target_link_libraries(sim-power-menu PRIVATE ink_apps ink_system ink_port_sim)
  target_compile_options(sim-power-menu PRIVATE ${INK_WARN} -Wno-format-truncation) # a message cut short does no harm
  add_test(NAME integration-power-menu
    COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:sim-power-menu> -DPROFILE=${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            -DASSETS=${INK_ASSETS_DIR} -DCONTENT=${INK_TEST_CONTENT} -DOUT=${CMAKE_BINARY_DIR}/test-out/power_menu
            -DSRC=${CMAKE_SOURCE_DIR}/tests/integration -P ${CMAKE_SOURCE_DIR}/tests/integration/power_menu.cmake)

  # Dictation: the input method's microphone, with the hub's stand-in for asr.stream.
  add_executable(sim-dictation simulator/runtime/sim_main.c tests/integration/dictation.c)
  target_link_libraries(sim-dictation PRIVATE ink_apps ink_system ink_port_sim)
  target_compile_options(sim-dictation PRIVATE ${INK_WARN} -Wno-format-truncation) # a message cut short does no harm
  add_test(NAME integration-dictation
    COMMAND sim-dictation --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --budget ${CMAKE_SOURCE_DIR}/tests/integration/dictation.budget.json
            --script ${CMAKE_SOURCE_DIR}/tests/integration/dictation.script
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/dictation --quiet --no-random-outages)
  add_test(NAME integration-dictation-handsfree
    COMMAND ${CMAKE_COMMAND} -E env INK_DICTATION_PHASE=handsfree
            $<TARGET_FILE:sim-dictation> --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus-handsfree.json
            --budget ${CMAKE_SOURCE_DIR}/tests/integration/dictation.budget.json
            --script ${CMAKE_SOURCE_DIR}/tests/integration/dictation.script
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/dictation-handsfree --quiet --no-random-outages)

  # Hands-free listening, all three settings, on the profile with the voice front end.
  add_executable(sim-listening simulator/runtime/sim_main.c tests/integration/listening.c)
  target_link_libraries(sim-listening PRIVATE ink_apps ink_system ink_port_sim)
  target_compile_options(sim-listening PRIVATE ${INK_WARN} -Wno-format-truncation) # a message cut short does no harm
  add_test(NAME integration-listening
    COMMAND sim-listening --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus-handsfree.json
            --budget ${CMAKE_SOURCE_DIR}/tests/integration/listening.budget.json
            --script ${CMAKE_SOURCE_DIR}/tests/integration/listening.script
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/listening --quiet --no-random-outages)

  # Two tasks at the worst moment: the services built with that moment in them, instead of the library's.
  add_executable(sim-races simulator/runtime/sim_main.c tests/integration/races.c tests/integration/races_notify.c tests/integration/races_interrupt.c)
  target_link_libraries(sim-races PRIVATE ink_apps ink_system ink_port_sim)
  target_compile_options(sim-races PRIVATE ${INK_WARN} -Wno-format-truncation) # a message cut short does no harm
  add_test(NAME integration-races
    COMMAND sim-races --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --budget ${CMAKE_SOURCE_DIR}/tests/integration/races.budget.json
            --script ${CMAKE_SOURCE_DIR}/tests/integration/races.script
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/races --quiet --no-random-outages)

  add_executable(sim-ai-surface simulator/runtime/sim_main.c tests/integration/ai_surface.c)
  target_link_libraries(sim-ai-surface PRIVATE ink_apps ink_system ink_port_sim)
  target_compile_options(sim-ai-surface PRIVATE ${INK_WARN} -Wno-format-truncation) # a message cut short does no harm
  add_test(NAME integration-ai-surface
    COMMAND sim-ai-surface --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --budget ${CMAKE_SOURCE_DIR}/tests/integration/ai_surface.budget.json
            --script ${CMAKE_SOURCE_DIR}/tests/integration/ai_surface.script
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/ai_surface --quiet --no-random-outages)
  # The app directory and lifecycle (tests/integration/apps.c).
  add_executable(sim-apps simulator/runtime/sim_main.c tests/integration/apps.c)
  target_link_libraries(sim-apps PRIVATE ink_apps ink_system ink_port_sim)
  target_compile_options(sim-apps PRIVATE ${INK_WARN} -Wno-format-truncation) # a message cut short does no harm
  add_test(NAME integration-apps
    COMMAND sim-apps --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --budget ${CMAKE_SOURCE_DIR}/tests/integration/apps.budget.json
            --script ${CMAKE_SOURCE_DIR}/tests/integration/apps.script
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/apps --quiet --no-random-outages)
  # The confirmation card: what the AI asks for waits for a person (tests/integration/confirm.c).
  add_executable(sim-confirm simulator/runtime/sim_main.c tests/integration/confirm.c)
  target_link_libraries(sim-confirm PRIVATE ink_apps ink_system ink_port_sim)
  target_compile_options(sim-confirm PRIVATE ${INK_WARN} -Wno-format-truncation) # a message cut short does no harm
  add_test(NAME integration-confirm
    COMMAND sim-confirm --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --budget ${CMAKE_SOURCE_DIR}/tests/integration/confirm.budget.json
            --script ${CMAKE_SOURCE_DIR}/tests/integration/confirm.script
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/confirm --quiet --no-random-outages)
  # Notifications and interruptions, on the whole product (tests/integration/notify.c).
  add_executable(sim-notify simulator/runtime/sim_main.c tests/integration/notify.c)
  target_link_libraries(sim-notify PRIVATE ink_apps ink_system ink_port_sim)
  target_compile_options(sim-notify PRIVATE ${INK_WARN} -Wno-format-truncation) # a message cut short does no harm
  add_test(NAME integration-notify
    COMMAND sim-notify --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --budget ${CMAKE_SOURCE_DIR}/tests/integration/notify.budget.json
            --script ${CMAKE_SOURCE_DIR}/tests/integration/notify.script
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/notify --quiet --no-random-outages)
  # A nearly empty battery writes what waits and powers off; it is there at the next boot.
  add_test(NAME integration-notify-lowbat
    COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:sim-notify> -DPROFILE=${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            -DASSETS=${INK_ASSETS_DIR} -DCONTENT=${INK_TEST_CONTENT}
            -DOUT=${CMAKE_BINARY_DIR}/test-out/notify-lowbat -DSRC=${CMAKE_SOURCE_DIR}/tests/integration
            -P ${CMAKE_SOURCE_DIR}/tests/integration/notify_lowbat.cmake)
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
  # The spoken answer over a pessimistic LTE link does not depend on one lucky run: seeds where a shorter prebuffer cut into it.
  foreach(seed 26 29)
    add_test(NAME scenario-ai_ask-metalio-plus-seed-${seed}
      COMMAND sim-configured --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
              --script ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_ask.script
              --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_ask.budget.json
              --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT} --seed ${seed}
              --out ${CMAKE_BINARY_DIR}/test-out/ai_ask-seed-${seed} --quiet --no-random-outages)
  endforeach()
  add_test(NAME scenario-ai_mic_unavailable-metalio-plus
    COMMAND sim-configured --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_mic_unavailable.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_mic_unavailable.budget.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/ai_mic_unavailable-metalio-plus --quiet --trace --frames --no-random-outages
            --i2c-faults 1.0:io-expander)
  add_test(NAME scenario-reader_empty-metalio-plus
    COMMAND sim-configured --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_empty.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_empty.budget.json
            --assets ${INK_ASSETS_DIR} --content ${CMAKE_SOURCE_DIR}/tests/fixtures/content-bad
            --out ${CMAKE_BINARY_DIR}/test-out/reader_empty-metalio-plus --quiet --trace --frames --no-random-outages)
  # PQ-BN direct drive (research PR #7) on the same scenarios and budgets, plus its flash pause
  add_test(NAME scenario-reader_basic-metalio-plus-pqbn
    COMMAND sim-configured --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus-pqbn.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_basic.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_basic.pqbn.budget.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/reader_basic-metalio-plus-pqbn --quiet --trace --frames --no-random-outages)
  ink_scenario(ai_ask metalio-plus-pqbn)
  add_test(NAME scenario-pqbn_flash_pause-metalio-plus-pqbn
    COMMAND sim-configured --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus-pqbn.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/pqbn_flash_pause.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/pqbn_flash_pause.budget.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/pqbn_flash_pause --quiet --trace --no-random-outages)
  # requests about the screen: content version and retries (audit F-6)
  add_test(NAME scenario-ai_page_version-metalio-plus
    COMMAND sim-configured --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_page_version.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_page_version.budget.json
            --hub ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_page_version.hub.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/ai_page_version-metalio-plus --quiet --trace --frames --no-random-outages)
  add_test(NAME scenario-remote_browse-metalio-plus
    COMMAND sim-configured --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/remote_browse.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/remote_browse.budget.json
            --hub ${CMAKE_SOURCE_DIR}/tests/scenarios/remote_browse.hub.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/remote_browse-metalio-plus --quiet --trace --frames --no-random-outages)
  add_test(NAME scenario-remote_reconnect-metalio-plus
    COMMAND sim-configured --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/remote_reconnect.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/remote_reconnect.budget.json
            --hub ${CMAKE_SOURCE_DIR}/tests/scenarios/remote_browse.hub.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/remote_reconnect-metalio-plus --quiet --trace --frames --no-random-outages)
  add_test(NAME scenario-ai_tools-metalio-plus
    COMMAND sim-configured --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_tools.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_tools.budget.json
            --hub ${CMAKE_SOURCE_DIR}/tests/scenarios/ai_tools.hub.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/ai_tools-metalio-plus --quiet --trace --frames --no-random-outages)
  # Power cut in the middle of a position save, reboot from the torn flash image.
  add_test(NAME scenario-powercut-during-save
    COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:sim-configured>
            -DPROFILE=${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            -DOUT=${CMAKE_BINARY_DIR}/test-out/powercut -DSRC=${CMAKE_SOURCE_DIR}/tests/scenarios
            -DASSETS=${INK_ASSETS_DIR} -DCONTENT=${INK_TEST_CONTENT}
            -P ${CMAKE_SOURCE_DIR}/tests/scenarios/powercut.cmake)
  # The real system must be bit-for-bit deterministic too (budgets compare single runs).
  add_test(NAME determinism-reader
    COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:sim-configured>
            -DPROFILE=${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            -DSCRIPT=${CMAKE_SOURCE_DIR}/tests/scenarios/reader_basic.script
            -DOUT=${CMAKE_BINARY_DIR}/test-out/determinism-reader
            "-DEXTRA=--assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT} --no-random-outages"
            -P ${CMAKE_SOURCE_DIR}/tests/simulator/determinism.cmake)
  # Same reader on the upstream display pipeline (timing comparison; lenient budget).
  add_test(NAME scenario-reader_basic-upstream-pipeline
    COMMAND sim-configured --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus-upstream-display.json
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
    COMMAND ${CMAKE_COMMAND} -DMODE=run -DSIM=$<TARGET_FILE:sim-configured>
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
    COMMAND sim-configured --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
            --script ${CMAKE_SOURCE_DIR}/tests/scenarios/reader_basic.script
            --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/panel_power_faults.budget.json
            --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
            --out ${CMAKE_BINARY_DIR}/test-out/reader_basic-panel-power-faults --quiet --no-random-outages
            --i2c-faults 0.08:pmic)
endif()
# The panel supply never comes up: every refresh fails (ok = false) instead of pretending.
add_test(NAME scenario-home-panel-power-dead
  COMMAND sim-configured --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          --script ${CMAKE_SOURCE_DIR}/tests/scenarios/home_basic.script
          --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/panel_power_dead.budget.json
          --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
          --out ${CMAKE_BINARY_DIR}/test-out/home-panel-power-dead --quiet --no-random-outages --i2c-faults 1.0:pmic --end 8s)
# The fuel gauge never answers (--i2c-faults 1.0:fuel-gauge): the battery reads unknown, which must not count as
# empty; on battery the low-battery policy must neither warn nor power off.
add_test(NAME scenario-home-gauge-dead
  COMMAND sim-configured --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
          --script ${CMAKE_SOURCE_DIR}/tests/scenarios/home_basic.script
          --budget ${CMAKE_SOURCE_DIR}/tests/scenarios/gauge_dead.budget.json
          --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT}
          --out ${CMAKE_BINARY_DIR}/test-out/home-gauge-dead --quiet --no-random-outages --i2c-faults 1.0:fuel-gauge --end 20s)
# Portability smoke runs: same system, other panels/classes (budget: no panic, input works)
foreach(p pinenote-rk3566 desktop-lcd metalio-plus-pqbn)
  add_test(NAME portability-home-${p}
    COMMAND sim-configured --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/${p}.json
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

add_test(NAME thumb-wiring COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/services_finish/run.py --build ${CMAKE_BINARY_DIR} thumb_wiring)
set_tests_properties(thumb-wiring PROPERTIES TIMEOUT 180 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
target_sources(ink_system PRIVATE system/ui/file_picker.c)
ink_sim_executable(sim-ui-file-picker tests/ui_file_picker.c)
target_link_libraries(sim-ui-file-picker PRIVATE ink_apps ink_system)
add_test(NAME ui-file-picker COMMAND sim-ui-file-picker --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
  --script ${CMAKE_SOURCE_DIR}/tests/ui_file_picker.script --budget ${CMAKE_SOURCE_DIR}/tests/ui_scene.budget.json
  --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT} --out ${CMAKE_BINARY_DIR}/test-out/ui-file-picker --quiet --no-random-outages)
add_test(NAME ui-file-picker-frames COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ui_file_picker_frames.py ${CMAKE_BINARY_DIR}/test-out/ui-file-picker)
set_tests_properties(ui-file-picker-frames PROPERTIES DEPENDS ui-file-picker)
target_sources(ink_system PRIVATE system/apps/contacts/contacts.c)
ink_sim_executable(sim-contacts tests/contacts/contacts.c)
target_link_libraries(sim-contacts PRIVATE ink_apps ink_system)
add_test(NAME contacts COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/contacts/run.py
  $<TARGET_FILE:sim-contacts> ${CMAKE_SOURCE_DIR} ${INK_ASSETS_DIR} ${INK_TEST_CONTENT} ${CMAKE_BINARY_DIR}/test-out/contacts)
ink_sim_executable(sim-clock-alarm tests/clock_alarm.c)
ink_sim_executable(sim-timer-seconds tests/timer_seconds.c)
target_link_libraries(sim-timer-seconds PRIVATE ink_apps ink_system)
add_test(NAME timer-seconds COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/timer_seconds.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(timer-seconds PROPERTIES TIMEOUT 180)
ink_sim_executable(sim-clock-extras tests/clock_extras.c)
target_link_libraries(sim-clock-extras PRIVATE ink_apps ink_system)
add_test(NAME clock-extras COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/clock_extras.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(clock-extras PROPERTIES TIMEOUT 240)
target_link_libraries(sim-clock-alarm PRIVATE ink_apps ink_system)
add_test(NAME clock-alarm COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/clock_alarm.py --build ${CMAKE_BINARY_DIR})
ink_sim_executable(sim-sound-clock tests/sound_clock.c)
target_link_libraries(sim-sound-clock PRIVATE ink_apps ink_system)
add_test(NAME sound-clock COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/sound_clock.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(sound-clock PROPERTIES TIMEOUT 300)
add_test(NAME sound-clock-input COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/sound_clock_input.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(sound-clock-input PROPERTIES TIMEOUT 90)
ink_sim_executable(sim-vendor-haptic tests/vendor_haptic.c tests/vendor_haptic_hal.c)
target_link_libraries(sim-vendor-haptic PRIVATE ink_apps ink_system)
add_test(NAME vendor-haptic COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/vendor_haptic.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(vendor-haptic PROPERTIES TIMEOUT 360)
# AI chat pages use the existing service and input hosts.
target_sources(ink_system PRIVATE system/ui/shell/ai_chat.c)
ink_sim_executable(sim-ai-chat tests/ai_chat.c)
target_link_libraries(sim-ai-chat PRIVATE ink_apps ink_system)
add_test(NAME ai-chat COMMAND sim-ai-chat --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
  --script ${CMAKE_SOURCE_DIR}/tests/ai_chat.script --hub ${CMAKE_SOURCE_DIR}/tests/ai_chat.hub.json
  --budget ${CMAKE_SOURCE_DIR}/tests/ai_chat.budget.json
  --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT} --out ${CMAKE_BINARY_DIR}/test-out/ai-chat --quiet --no-random-outages)
add_test(NAME ai-chat-snapshots COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ai_chat_snapshots.py
  ${CMAKE_BINARY_DIR}/test-out/ai-chat ${CMAKE_BINARY_DIR}/ai-chat-compare)
set_tests_properties(ai-chat-snapshots PROPERTIES DEPENDS ai-chat)
# Long AI answers: paged word for word, cut with a note over the limit, spoken to the end.
ink_sim_executable(sim-ai-answer-long tests/ai_answer_long/ai_answer_long.c)
target_link_libraries(sim-ai-answer-long PRIVATE ink_apps ink_system)
add_test(NAME ai-answer-long COMMAND sim-ai-answer-long --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
  --script ${CMAKE_SOURCE_DIR}/tests/ai_answer_long/ai_answer_long.script --hub ${CMAKE_SOURCE_DIR}/tests/ai_answer_long/hub.json
  --budget ${CMAKE_SOURCE_DIR}/tests/ai_answer_long/budget.json
  --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT} --out ${CMAKE_BINARY_DIR}/test-out/ai-answer-long --quiet --no-random-outages)
add_test(NAME ai-answer-long-en COMMAND ${CMAKE_COMMAND} -E env INK_I18N_ANSWER_EN=1 $<TARGET_FILE:sim-ai-answer-long>
  --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
  --script ${CMAKE_SOURCE_DIR}/tests/ai_answer_long/ai_answer_long.script --hub ${CMAKE_SOURCE_DIR}/tests/ai_answer_long/hub.json
  --budget ${CMAKE_SOURCE_DIR}/tests/ai_answer_long/budget.json
  --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT} --out ${CMAKE_BINARY_DIR}/test-out/ai-answer-long-en --quiet --no-random-outages)
ink_sim_executable(sim-ai-speech-long tests/ai_speech_long/ai_speech_long.c)
target_link_libraries(sim-ai-speech-long PRIVATE ink_apps ink_system)
add_test(NAME ai-speech-long COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ai_speech_long/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(ai-speech-long PROPERTIES TIMEOUT 180)
target_sources(ink_system PRIVATE system/apps/phone/phone.c)
ink_sim_executable(sim-emergency-call tests/emergency_call/emergency_call.c)
target_link_libraries(sim-emergency-call PRIVATE ink_apps ink_system)
add_test(NAME emergency-call COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/emergency_call/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(emergency-call PROPERTIES TIMEOUT 300)
ink_sim_executable(sim-phone-calls tests/phone_calls/phone_calls.c)
target_link_libraries(sim-phone-calls PRIVATE ink_apps ink_system)
add_test(NAME phone-calls COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/phone_calls/run.py
  $<TARGET_FILE:sim-phone-calls> ${CMAKE_SOURCE_DIR} ${INK_ASSETS_DIR} ${INK_TEST_CONTENT} ${CMAKE_BINARY_DIR}/test-out/phone-calls)

target_sources(ink_system PRIVATE system/ui/shell/power_pages.c)
ink_sim_executable(sim-power-pages tests/power_pages.c)
target_link_libraries(sim-power-pages PRIVATE ink_apps ink_system)
add_test(NAME power-pages COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/power_pages.py
  $<TARGET_FILE:sim-power-pages> ${CMAKE_SOURCE_DIR} ${INK_ASSETS_DIR} ${INK_TEST_CONTENT} ${CMAKE_BINARY_DIR}/test-out/power-pages)
target_sources(ink_system PRIVATE system/apps/sms/sms.c)
ink_sim_executable(sim-sms tests/sms/sms.c)
target_link_libraries(sim-sms PRIVATE ink_apps ink_system)
add_test(NAME sms COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/sms/run.py
  $<TARGET_FILE:sim-sms> ${CMAKE_SOURCE_DIR} ${INK_ASSETS_DIR} ${INK_TEST_CONTENT} ${CMAKE_BINARY_DIR}/test-out/sms)
add_executable(nt26-pdu-test tests/sms_multipart/pdu_test.c drivers/chips/nt26/nt26.c)
target_include_directories(nt26-pdu-test PRIVATE platform/include drivers/chips)
target_compile_options(nt26-pdu-test PRIVATE ${INK_WARN})
add_test(NAME unit-nt26-pdu COMMAND nt26-pdu-test)
ink_sim_executable(sim-sms-multipart tests/sms_multipart/sms_multipart.c)
target_link_libraries(sim-sms-multipart PRIVATE ink_apps ink_system)
add_test(NAME sms-multipart COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/sms_multipart/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(sms-multipart PROPERTIES TIMEOUT 600)
ink_sim_executable(sim-mobile-network tests/mobile_network/mobile_network.c)
target_link_libraries(sim-mobile-network PRIVATE ink_apps ink_system)
add_test(NAME mobile-network COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/mobile_network/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(mobile-network PROPERTIES TIMEOUT 600)
target_sources(ink_system PRIVATE system/apps/memo/memo.c)
ink_sim_executable(sim-memo tests/memo/memo.c)
target_link_libraries(sim-memo PRIVATE ink_apps ink_system)
ink_sim_executable(sim-memo-save-busy tests/memo_save_busy.c)
target_link_libraries(sim-memo-save-busy PRIVATE ink_apps ink_system)
add_test(NAME memo-save-busy COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/memo_save_busy.py --build ${CMAKE_BINARY_DIR})
# Repro of an open bug (saving while the screen keeps refreshing times out); the fix was reverted in
# fixes-1112 after it broke three other checks. Disabled until a new fix lands, then re-enable.
set_tests_properties(memo-save-busy PROPERTIES TIMEOUT 120 DISABLED TRUE)
add_test(NAME memo COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/memo/run.py --build ${CMAKE_BINARY_DIR})
# Voice memos: recording, cancelling, the AI key, a call, the hub's words, members, a power cut (tests/memo_record).
ink_sim_executable(sim-memo-record tests/memo_record/memo_record.c)
target_link_libraries(sim-memo-record PRIVATE ink_apps ink_system)
add_test(NAME memo-record COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/memo_record/run.py --build ${CMAKE_BINARY_DIR})

target_sources(ink_system PRIVATE system/ui/shell/settings_family.c system/ui/shell/wallpaper.c)
ink_sim_executable(sim-settings-b tests/settings_b.c)
target_link_libraries(sim-settings-b PRIVATE ink_apps ink_system)
add_test(NAME settings-b COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/settings_b.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(settings-b PROPERTIES TIMEOUT 360)
ink_sim_executable(sim-settings-gaps tests/settings_gaps.c)
target_link_libraries(sim-settings-gaps PRIVATE ink_apps ink_system)
add_test(NAME settings-gaps COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/settings_gaps.py --build ${CMAKE_BINARY_DIR})
# settings-1: the home and its search after AOSP, one registry behind rows, search and settings.open; one row per change.
ink_sim_executable(sim-settings-1 tests/settings_1.c)
target_link_libraries(sim-settings-1 PRIVATE ink_apps ink_system)
add_test(NAME settings-1 COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/settings_1.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(settings-1 PROPERTIES TIMEOUT 900)
ink_sim_executable(sim-notify-manage tests/notify_manage.c)
target_link_libraries(sim-notify-manage PRIVATE ink_apps ink_system)
add_test(NAME notify-manage COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/notify_manage.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(notify-manage PROPERTIES TIMEOUT 400)

ink_sim_executable(sim-notify-model tests/notify_model.c)
target_link_libraries(sim-notify-model PRIVATE ink_apps ink_system)
add_test(NAME notify-model COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/notify_model.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(notify-model PROPERTIES TIMEOUT 600)
ink_sim_executable(sim-settings-small tests/settings_small.c)
target_link_libraries(sim-settings-small PRIVATE ink_apps ink_system)
add_test(NAME settings-small COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/settings_small.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(settings-small PROPERTIES TIMEOUT 300)
ink_sim_executable(sim-voice-name-assist tests/voice_name_assist.c tests/voice_name_assist_observer.c)
target_link_libraries(sim-voice-name-assist PRIVATE ink_apps ink_system)
add_test(NAME voice-name-assist COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/voice_name_assist.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(voice-name-assist PROPERTIES TIMEOUT 180)
foreach(case b10-01 b10-02)
  if(case STREQUAL "b10-01")
    set(audit10_source sms_arrival)
  else()
    set(audit10_source wifi_pages)
  endif()
  ink_sim_executable(sim-audit10-${case} tests/audit10_fix/${audit10_source}.c)
  target_link_libraries(sim-audit10-${case} PRIVATE ink_apps ink_system)
  add_dependencies(sim-audit10-${case} ink_fonts ink_test_content)
  add_test(NAME audit10-${case} COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/audit10_fix/run.py
    --build ${CMAKE_BINARY_DIR} --case ${case})
endforeach()
ink_sim_executable(sim-bluetooth tests/bluetooth.c)
target_link_libraries(sim-bluetooth PRIVATE ink_apps ink_system)
add_test(NAME bluetooth-settings COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/bluetooth.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(bluetooth-settings PROPERTIES TIMEOUT 360)
ink_sim_executable(sim-airplane-mode tests/airplane_mode.c)
target_link_libraries(sim-airplane-mode PRIVATE ink_apps ink_system)
add_test(NAME airplane-mode COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/airplane_mode.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(airplane-mode PROPERTIES TIMEOUT 600)
ink_sim_executable(sim-audit12-fix tests/audit12_fix/device.c)
target_link_libraries(sim-audit12-fix PRIVATE ink_apps ink_system)
add_dependencies(sim-audit12-fix ink_fonts ink_test_content)
foreach(case m1 m2 m3 l7)
  add_test(NAME audit12-${case} COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/audit12_fix/run.py --build ${CMAKE_BINARY_DIR} --case ${case})
  set_tests_properties(audit12-${case} PROPERTIES TIMEOUT 300)
endforeach()
ink_sim_executable(sim-accel-use tests/accel_use/accel_use.c)
target_link_libraries(sim-accel-use PRIVATE ink_apps ink_system)
add_test(NAME accel-use COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/accel_use/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(accel-use PROPERTIES TIMEOUT 900)
foreach(case bt-ai ime-layout)
  string(REPLACE "-" "_" source ${case})
  ink_sim_executable(sim-audit9-${case} tests/audit9/${source}.c)
  target_link_libraries(sim-audit9-${case} PRIVATE ink_apps ink_system)
  add_dependencies(sim-audit9-${case} ink_fonts ink_test_content)
  add_test(NAME audit9-${case} COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/audit9/run.py
    --build ${CMAKE_BINARY_DIR} --case ${case})
endforeach()
add_test(NAME audit9-font-coverage COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/audit9/font_coverage.py
  --build ${CMAKE_BINARY_DIR})
target_sources(ink_system PRIVATE system/ui/shell/member_pages.c)
ink_sim_executable(sim-kid-1 tests/kid_1.c)
target_link_libraries(sim-kid-1 PRIVATE ink_apps ink_system)
add_test(NAME kid-1 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/kid_1.py --build ${CMAKE_BINARY_DIR})

target_sources(ink_system PRIVATE system/ui/shell/update_pages.c)
ink_sim_executable(sim-update-pages tests/update_pages.c)
target_link_libraries(sim-update-pages PRIVATE ink_apps ink_system)
add_test(NAME update-pages COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/update_pages.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(update-pages PROPERTIES TIMEOUT 240)

target_sources(ink_system PRIVATE system/apps/calendar/calendar.c)
ink_sim_executable(sim-calendar tests/calendar/calendar.c)
target_link_libraries(sim-calendar PRIVATE ink_apps ink_system)
add_test(NAME calendar COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/calendar/run.py --build ${CMAKE_BINARY_DIR})
ink_sim_executable(sim-calendar-events tests/calendar_events/events.c)
target_link_libraries(sim-calendar-events PRIVATE ink_apps ink_system)
add_test(NAME calendar-events COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/calendar_events/run.py --build ${CMAKE_BINARY_DIR})

ink_sim_executable(sim-reminder-edit tests/reminder_edit/reminder_edit.c)
target_link_libraries(sim-reminder-edit PRIVATE ink_apps ink_system)
add_test(NAME reminder-edit COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/reminder_edit/run.py --build ${CMAKE_BINARY_DIR})

target_sources(ink_system PRIVATE system/apps/calculator/calculator.c)
ink_sim_executable(sim-calculator tests/calculator/calculator.c)
target_link_libraries(sim-calculator PRIVATE ink_apps ink_system)
add_test(NAME calculator COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/calculator/run.py --build ${CMAKE_BINARY_DIR})

target_sources(ink_system PRIVATE system/apps/weather/weather.c)
ink_sim_executable(sim-weather tests/weather/weather.c)
target_link_libraries(sim-weather PRIVATE ink_apps ink_system)
add_test(NAME weather COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/weather/run.py --build ${CMAKE_BINARY_DIR})

ink_sim_executable(sim-confirm-pages tests/confirm_pages/pages.c tests/confirm_pages/boundary.c)
target_link_libraries(sim-confirm-pages PRIVATE ink_apps ink_system)
add_test(NAME confirm-pages COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/confirm_pages/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(confirm-pages PROPERTIES TIMEOUT 180 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")

target_sources(ink_system PRIVATE system/apps/files/files.c)
ink_sim_executable(sim-files-app tests/files_app/files_app.c)
target_link_libraries(sim-files-app PRIVATE ink_apps ink_system)
add_dependencies(sim-files-app sim-image)
target_include_directories(sim-files-app PRIVATE ${CMAKE_BINARY_DIR}/generated)
add_test(NAME files-app COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/files_app/run.py --build ${CMAKE_BINARY_DIR})
ink_sim_executable(sim-files-ops tests/files_ops/files_ops.c)
target_link_libraries(sim-files-ops PRIVATE ink_apps ink_system)
add_test(NAME files-ops COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/files_ops/run.py --build ${CMAKE_BINARY_DIR})

target_sources(ink_system PRIVATE system/apps/camera/camera.c system/apps/gallery/gallery.c)
add_library(ink_camera_png STATIC system/apps/camera/png.c)
target_include_directories(ink_camera_png PRIVATE third_party/stb platform/include)
target_compile_options(ink_camera_png PRIVATE -w) # upstream stb implementation
target_link_libraries(ink_camera_png PRIVATE ink_platform)
target_link_libraries(ink_system PRIVATE ink_camera_png)
add_library(ink_quirc STATIC third_party/quirc/quirc.c third_party/quirc/identify.c third_party/quirc/decode.c third_party/quirc/version_db.c)
target_include_directories(ink_quirc PUBLIC third_party/quirc)
target_link_libraries(ink_quirc PRIVATE ink_platform)
target_compile_options(ink_quirc PRIVATE -include ${CMAKE_SOURCE_DIR}/system/apps/camera/quirc_alloc.h -w)
target_sources(ink_system PRIVATE system/apps/camera/scan.c)
target_link_libraries(ink_system PRIVATE ink_quirc)
ink_sim_executable(sim-camera-gallery tests/camera_gallery/camera_gallery.c ${INK_IMAGE_FIXTURES})
target_include_directories(sim-camera-gallery PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(sim-camera-gallery PRIVATE ink_apps ink_system)
add_test(NAME camera-gallery COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/camera_gallery/run.py --build ${CMAKE_BINARY_DIR})
ink_sim_executable(sim-ai-history-photo tests/ai_history_photo/test.c ${INK_IMAGE_FIXTURES})
target_include_directories(sim-ai-history-photo PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(sim-ai-history-photo PRIVATE ink_apps ink_system)
add_test(NAME ai-history-photo COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/ai_history_photo/run.py --build ${CMAKE_BINARY_DIR})

target_sources(ink_system PRIVATE system/ui/shell/backup_pages.c)
ink_sim_executable(sim-backup-pages tests/backup_pages.c)
target_link_libraries(sim-backup-pages PRIVATE ink_apps ink_system)
add_test(NAME backup-pages COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/backup_pages.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(backup-pages PROPERTIES TIMEOUT 180)
ink_sim_executable(sim-factory-pages tests/factory_pages.c)
target_link_libraries(sim-factory-pages PRIVATE ink_apps ink_system)
add_test(NAME factory-pages COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/factory_pages.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(factory-pages PROPERTIES TIMEOUT 420)
foreach(ui_fix_a_case backup confirm transfer factory)
  ink_sim_executable(sim-ui-fix-a-${ui_fix_a_case} tests/ui_fix_a/${ui_fix_a_case}.c)
  target_link_libraries(sim-ui-fix-a-${ui_fix_a_case} PRIVATE ink_apps ink_system)
  add_test(NAME ui-fix-a-${ui_fix_a_case} COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ui_fix_a/run.py
    --build ${CMAKE_BINARY_DIR} --case ${ui_fix_a_case})
  set_tests_properties(ui-fix-a-${ui_fix_a_case} PROPERTIES TIMEOUT 180)
endforeach()

target_sources(ink_system PRIVATE system/ui/shell/ai_pages.c)
ink_sim_executable(sim-ai-pages tests/ai_pages.c)
target_link_libraries(sim-ai-pages PRIVATE ink_apps ink_system)
add_test(NAME ai-pages COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ai_pages.py --build ${CMAKE_BINARY_DIR})

target_sources(ink_system PRIVATE system/ui/shell/activity_card.c)
ink_sim_executable(sim-activity-card tests/activity_card.c)
target_link_libraries(sim-activity-card PRIVATE ink_apps ink_system)
add_test(NAME activity-card COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/activity_card.py --build ${CMAKE_BINARY_DIR})
ink_sim_executable(sim-remote-app tests/remote_app/remote_app.c)
target_link_libraries(sim-remote-app PRIVATE ink_apps ink_system)
add_test(NAME remote-app COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/remote_app/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(remote-app PROPERTIES TIMEOUT 180)
target_sources(sim-remote-app PRIVATE tests/remote_app/hub_observer.c)
ink_sim_executable(sim-fw-f8 tests/fw_f8/editor.c)
target_link_libraries(sim-fw-f8 PRIVATE ink_apps ink_system)
add_test(NAME fw-f8 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/fw_f8/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(fw-f8 PROPERTIES TIMEOUT 250)
add_executable(test-audit3-png tests/audit3_png.c)
target_link_libraries(test-audit3-png PRIVATE ink_camera_png ink_platform)
target_compile_options(test-audit3-png PRIVATE ${INK_WARN})
ink_sim_executable(sim-audit3-fix tests/audit3_fix.c)
target_link_libraries(sim-audit3-fix PRIVATE ink_apps ink_system)
foreach(n RANGE 1 9)
  add_test(NAME audit3-b3-0${n} COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/audit3_fix.py
    --build ${CMAKE_BINARY_DIR} --case ${n})
  set_tests_properties(audit3-b3-0${n} PROPERTIES TIMEOUT 240)
endforeach()
target_sources(ink_system PRIVATE system/ui/shell/kid_pages.c)
ink_sim_executable(sim-kid-2 tests/kid_2.c)
target_link_libraries(sim-kid-2 PRIVATE ink_apps ink_system)
add_test(NAME kid-2 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/kid_2.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(kid-2 PROPERTIES TIMEOUT 180)
ink_sim_executable(sim-storage-usage tests/storage_usage/storage_usage.c tests/storage_usage/boundary.c)
target_link_libraries(sim-storage-usage PRIVATE ink_apps ink_system)
add_test(NAME storage-usage COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/storage_usage/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(storage-usage PROPERTIES TIMEOUT 180 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
add_dependencies(sim-storage-usage sim-image)
target_include_directories(sim-storage-usage PRIVATE ${CMAKE_BINARY_DIR}/generated)
ink_sim_executable(sim-file-index-3 tests/file_index_3/device.c tests/file_index_3/boundary.c tests/file_index_3/data_boundary.c)
target_link_libraries(sim-file-index-3 PRIVATE ink_apps ink_system)
add_test(NAME file-index-3 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/file_index_3/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(file-index-3 PROPERTIES TIMEOUT 180 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
ink_sim_executable(sim-storage-order tests/storage_order/device.c tests/storage_order/data_boundary.c
  tests/storage_order/storage_boundary.c tests/file_index_3/boundary.c)
target_link_libraries(sim-storage-order PRIVATE ink_apps ink_system)
add_test(NAME storage-order COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/storage_order/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(storage-order PROPERTIES TIMEOUT 180 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")

ink_sim_executable(sim-audit5-fix tests/audit5_fix/device.c tests/file_index_3/boundary.c)
target_link_libraries(sim-audit5-fix PRIVATE ink_apps ink_system)
foreach(pair "b5-01;confirmation" "b5-02;index" "b5-03;receipts")
    list(GET pair 0 number)
    list(GET pair 1 scenario)
    add_test(NAME audit5-${number} COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/audit5_fix/run.py --build ${CMAKE_BINARY_DIR} --case ${scenario})
    set_tests_properties(audit5-${number} PROPERTIES TIMEOUT 240)
endforeach()
ink_sim_executable(sim-index-durable tests/index_durable/device.c tests/file_index_3/boundary.c)
target_link_libraries(sim-index-durable PRIVATE ink_apps ink_system)
add_test(NAME index-durable COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/index_durable/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(index-durable PROPERTIES TIMEOUT 420 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
ink_sim_executable(sim-audit7-file tests/audit7_fix/file.c tests/audit7_fix/file_boundary.c)
target_link_libraries(sim-audit7-file PRIVATE ink_apps ink_system)
foreach(number b7-01 b7-02 b7-03 b7-04)
    add_test(NAME audit7-${number} COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/audit7_fix/run.py --build ${CMAKE_BINARY_DIR} --case ${number})
    set_tests_properties(audit7-${number} PROPERTIES TIMEOUT 240 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
endforeach()
ink_sim_executable(sim-index-compact tests/index_compact/device.c tests/index_compact/boundary.c tests/file_index_3/data_boundary.c)
target_link_libraries(sim-index-compact PRIVATE ink_apps ink_system)
add_test(NAME index-compact COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/index_compact/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(index-compact PROPERTIES TIMEOUT 180 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
ink_sim_executable(sim-index-prune tests/index_prune/device.c tests/index_prune/boundary.c tests/file_index_3/data_boundary.c ${INK_IMAGE_FIXTURES})
target_link_libraries(sim-index-prune PRIVATE ink_apps ink_system)
target_include_directories(sim-index-prune PRIVATE ${CMAKE_BINARY_DIR}/generated)
add_test(NAME index-prune COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/index_prune/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(index-prune PROPERTIES TIMEOUT 240 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
ink_sim_executable(sim-audit11-fix tests/audit11_fix/device.c tests/audit11_fix/boundary.c)
target_link_libraries(sim-audit11-fix PRIVATE ink_apps ink_system)
target_include_directories(sim-audit11-fix PRIVATE ${CMAKE_BINARY_DIR}/generated)
foreach(case b11-01 b11-02)
  add_test(NAME audit11-${case} COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/audit11_fix/run.py --build ${CMAKE_BINARY_DIR} --case ${case})
  set_tests_properties(audit11-${case} PROPERTIES TIMEOUT 300 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
endforeach()
target_sources(ink_system PRIVATE system/ui/shell/transfer_pages.c)
ink_sim_executable(sim-transfer-pages tests/transfer_pages/device.c)
target_link_libraries(sim-transfer-pages PRIVATE ink_apps ink_system)
add_test(NAME transfer-pages COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/transfer_pages/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(transfer-pages PROPERTIES TIMEOUT 120)
target_sources(sim-transfer-pages PRIVATE tests/transfer_pages/hal.c)

ink_sim_executable(sim-audit4-fix tests/audit4_fix.c)
target_link_libraries(sim-audit4-fix PRIVATE ink_apps ink_system)
foreach(n 1 5 6)
  add_test(NAME audit4-b4-0${n} COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/audit4_fix.py
    --build ${CMAKE_BINARY_DIR} --case ${n})
  set_tests_properties(audit4-b4-0${n} PROPERTIES TIMEOUT 240)
endforeach()
ink_sim_executable(sim-privacy-records tests/privacy_records/device.c)
target_link_libraries(sim-privacy-records PRIVATE ink_apps ink_system)
add_test(NAME privacy-records COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/privacy_records/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(privacy-records PROPERTIES TIMEOUT 200)
ink_sim_executable(sim-kid-3 tests/kid_3.c)
target_link_libraries(sim-kid-3 PRIVATE ink_apps ink_system)
add_test(NAME kid-3 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/kid_3.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(kid-3 PROPERTIES TIMEOUT 450)
ink_sim_executable(sim-reader-continue tests/reader_continue.c)
target_link_libraries(sim-reader-continue PRIVATE ink_apps ink_system)
add_test(NAME reader-continue COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/reader_continue.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(reader-continue PROPERTIES TIMEOUT 450)

ink_sim_executable(sim-perf-latency tests/perf_latency/device.c tests/perf_latency/activity.c tests/perf_latency/count.c tests/perf_latency/image.c ${INK_IMAGE_FIXTURES})
target_include_directories(sim-perf-latency PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(sim-perf-latency PRIVATE ink_apps ink_system)
add_test(NAME perf-latency COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/perf_latency/run.py --build ${CMAKE_BINARY_DIR} --regression)
ink_sim_executable(sim-lock-camera tests/lock_camera/lock_camera.c)
target_link_libraries(sim-lock-camera PRIVATE ink_apps ink_system)
add_test(NAME lock-camera COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/lock_camera/run.py --build ${CMAKE_BINARY_DIR})
ink_sim_executable(sim-camera-touch-bus tests/camera_touch_bus/camera_touch_bus.c)
target_link_libraries(sim-camera-touch-bus PRIVATE ink_apps ink_system)
add_test(NAME camera-touch-bus COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/camera_touch_bus/run.py --build ${CMAKE_BINARY_DIR})
add_test(NAME camera-touch-bus-rewrite COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/camera_touch_bus/run.py --build ${CMAKE_BINARY_DIR} --reverse rewrite)
add_test(NAME camera-touch-bus-yield COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/camera_touch_bus/run.py --build ${CMAKE_BINARY_DIR} --reverse yield)
add_test(NAME camera-touch-bus-both COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/camera_touch_bus/run.py --build ${CMAKE_BINARY_DIR} --reverse both)
set_tests_properties(perf-latency PROPERTIES TIMEOUT 180)
add_test(NAME perf-latency-2 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/perf_latency_2/run.py
  --build ${CMAKE_BINARY_DIR} --runs 1 --regression)
set_tests_properties(perf-latency-2 PROPERTIES TIMEOUT 180)

ink_sim_executable(sim-kid-apps tests/kid_apps.c)
target_link_libraries(sim-kid-apps PRIVATE ink_apps ink_system)
add_test(NAME kid-apps COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/kid_apps.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(kid-apps PROPERTIES TIMEOUT 300)
add_test(NAME audit14-ownership COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/kid_apps.py --build ${CMAKE_BINARY_DIR} --phase ownership)
set_tests_properties(audit14-ownership PROPERTIES TIMEOUT 200)
ink_sim_executable(sim-audit14-locked tests/audit14_locked.c)
target_link_libraries(sim-audit14-locked PRIVATE ink_apps ink_system)
add_test(NAME audit14-locked COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/time_settings.py --build ${CMAKE_BINARY_DIR} --binary $<TARGET_FILE:sim-audit14-locked>)
add_test(NAME audit14-hub-locks COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/audit14_locks.py)
ink_sim_executable(sim-qr-scan tests/qr_scan/device.c)
target_include_directories(sim-qr-scan PRIVATE third_party/quirc)
target_link_libraries(sim-qr-scan PRIVATE ink_apps ink_system)
add_test(NAME qr-scan COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/qr_scan/run.py --build ${CMAKE_BINARY_DIR})

ink_sim_executable(sim-cell-status tests/esp32s31/cell_status.c)
target_include_directories(sim-cell-status PRIVATE drivers/chips)
target_link_libraries(sim-cell-status PRIVATE ink_apps ink_system)
add_test(NAME cell-status COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/esp32s31/cell_status.py ${CMAKE_BINARY_DIR})
add_test(NAME unit-s31-cell-negative COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/ports/esp32s31/tests/cell_negative.py ${CMAKE_BINARY_DIR} --cc ${CMAKE_C_COMPILER})
# Retain the existing serialization of the recompilation-heavy negative controls.
set_tests_properties(unit-s31-cell-negative PROPERTIES RUN_SERIAL TRUE)
# Focused small-gaps scenarios use the real product and a synthetic peer.
ink_sim_executable(sim-small-gaps-3 tests/small_gaps_3/main.c)
target_link_libraries(sim-small-gaps-3 PRIVATE ink_apps ink_system)
foreach(scene backup sample notes empty-notes)
  add_test(NAME small-gaps-3-${scene} COMMAND ${Python3_EXECUTABLE} -B
    ${CMAKE_SOURCE_DIR}/tests/small_gaps_3/run.py --build ${CMAKE_BINARY_DIR} --case ${scene})
  set_tests_properties(small-gaps-3-${scene} PROPERTIES TIMEOUT 200)
endforeach()
ink_sim_executable(sim-app-runtime tests/app_runtime/app.c)
target_link_libraries(sim-app-runtime PRIVATE ink_apps ink_system)
add_test(NAME app-runtime COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/app_runtime/run.py --build ${CMAKE_BINARY_DIR})
set(ink_uhci_test_args)
if(INK_SANITIZE)
  list(APPEND ink_uhci_test_args --sanitize)
endif()
add_test(NAME unit-s31-uhci-negative COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/ports/esp32s31/tests/uhci_negative.py
  ${CMAKE_BINARY_DIR} --cc ${CMAKE_C_COMPILER} ${ink_uhci_test_args})
set_tests_properties(unit-s31-uhci-negative PROPERTIES RUN_SERIAL TRUE)
add_test(NAME unit-s31-camera COMMAND test-s31-camera)
add_test(NAME unit-s31-camera-negative COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/ports/esp32s31/tests/camera_negative.py ${CMAKE_BINARY_DIR})
ink_sim_executable(sim-camera-absent tests/port_camera/absent.c)
target_link_libraries(sim-camera-absent PRIVATE ink_apps ink_system)
add_test(NAME camera-absent COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/port_camera/run.py ${CMAKE_BINARY_DIR})

ink_sim_executable(sim-comm-ux2 tests/comm_ux2/comm_ux2.c)
target_link_libraries(sim-comm-ux2 PRIVATE ink_apps ink_system)
add_test(NAME comm-ux2 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/comm_ux2/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(comm-ux2 PROPERTIES TIMEOUT 120)
add_executable(test-autotest-parser tests/device_autotest/parser.c)
target_link_libraries(test-autotest-parser PRIVATE ink_script)
target_compile_options(test-autotest-parser PRIVATE ${INK_WARN} -UNDEBUG)
add_test(NAME autotest-parser COMMAND test-autotest-parser)
file(READ ${CMAKE_SOURCE_DIR}/tests/device_autotest/device.script device_script HEX)
string(REGEX REPLACE "(..)" "0x\\1," device_script "${device_script}")
file(WRITE ${CMAKE_BINARY_DIR}/autotest-test/autotest_script.h "static const char ink_autotest_script[] = {${device_script}0};\n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/device_autotest/device.script)
add_executable(test-autotest-device tests/device_autotest/device.c)
target_include_directories(test-autotest-device PRIVATE ${CMAKE_BINARY_DIR}/autotest-test)
target_link_libraries(test-autotest-device PRIVATE ink_script ink_lvgl)
target_compile_options(test-autotest-device PRIVATE ${INK_WARN} -UNDEBUG)
add_test(NAME autotest-device COMMAND test-autotest-device)
ink_sim_executable(sim-autotest tests/device_autotest/sim.c)
target_link_libraries(sim-autotest PRIVATE ink_apps ink_system)
add_test(NAME autotest-cap COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/device_autotest/run.py ${CMAKE_BINARY_DIR})
add_test(NAME autotest-build-switch COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/device_autotest/build_switch.py ${CMAKE_BINARY_DIR})

ink_sim_executable(sim-gomoku tests/gomoku/device.c)
target_link_libraries(sim-gomoku PRIVATE ink_apps ink_system)
add_dependencies(sim-gomoku ink_fonts ink_test_content)
add_test(NAME gomoku COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/gomoku/run.py --build ${CMAKE_BINARY_DIR})

# music-1: reference PCM checks are ordinary host programs.
add_executable(test-music-formats tests/music/formats.c system/services/audio/codec_formats.c
  system/services/audio/vendor/kiikoread/metadata.c system/services/audio/vendor/kiikoread/music_lyrics.c)
target_include_directories(test-music-formats PRIVATE platform/include system/include)
target_compile_options(test-music-formats PRIVATE ${INK_WARN} -UNDEBUG)
target_link_libraries(test-music-formats PRIVATE m)
add_test(NAME music-formats COMMAND test-music-formats ${CMAKE_SOURCE_DIR}/tests/music/fixtures)

ink_sim_executable(sim-music tests/music/product.c)
target_link_libraries(sim-music PRIVATE ink_apps ink_system)
add_test(NAME music-product COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/music/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(music-product PROPERTIES TIMEOUT 180)

add_dependencies(sim-music ink_fonts ink_ui_fonts ink_icons ink_test_content)

ink_sim_executable(sim-audio-out tests/audio_out/device.c)
target_link_libraries(sim-audio-out PRIVATE ink_apps ink_system)
add_dependencies(sim-audio-out ink_fonts ink_ui_fonts ink_icons ink_test_content)
add_test(NAME audio-out COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/audio_out/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(audio-out PROPERTIES TIMEOUT 240)
add_test(NAME audio-out-alias-reverse COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/audio_out/control.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(audio-out-alias-reverse PROPERTIES TIMEOUT 120)
add_executable(test-audio-out-bench tests/audio_out/bench.c system/services/audio/codec_formats.c
  system/services/audio/vendor/kiikoread/metadata.c system/services/audio/vendor/kiikoread/music_lyrics.c)
target_include_directories(test-audio-out-bench PRIVATE platform/include system/include)
target_compile_options(test-audio-out-bench PRIVATE ${INK_WARN} -UNDEBUG)
target_link_libraries(test-audio-out-bench PRIVATE ink_resampler)
add_dependencies(sim-audio-out test-audio-out-bench)

ink_sim_executable(sim-games2 tests/games2/device.c)
target_link_libraries(sim-games2 PRIVATE ink_apps ink_system)
add_dependencies(sim-games2 ink_fonts ink_test_content)
add_test(NAME games2 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/games2/run.py --build ${CMAKE_BINARY_DIR})

# Google Pinyin uses the same simulator and public text session as the shipping IME.
set(INK_GOOGLE_SENTENCES ${CMAKE_BINARY_DIR}/generated/google_sentences.h)
add_custom_command(OUTPUT ${INK_GOOGLE_SENTENCES}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ime_lexicon/google_fixtures.py ${INK_GOOGLE_SENTENCES}
  DEPENDS tests/ime_lexicon/google_fixtures.py tests/ime_lexicon/google_sentences.tsv)
ink_sim_executable(sim-google-ime tests/ime_lexicon/google_device.c ${INK_IME_CASES} ${INK_GOOGLE_SENTENCES})
target_include_directories(sim-google-ime PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(sim-google-ime PRIVATE ink_apps ink_system)
add_dependencies(sim-google-ime ink_fonts ink_ui_fonts ink_icons ink_test_content)
add_test(NAME ime-google COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ime_lexicon/google_run.py --build ${CMAKE_BINARY_DIR})

# Card files (auto-provision-1, D-138): the list's tool, the device's protocol, the install tool, the whole product over boots.
add_test(NAME card-files-tool COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/card_files/tool_test.py --build ${CMAKE_BINARY_DIR})
add_executable(test-card-files-unit tests/card_files/unit.c system/util/sha256.c system/util/ink_crc.c)
target_include_directories(test-card-files-unit BEFORE PRIVATE tests/card_files/manifest system/include platform/include)
target_link_libraries(test-card-files-unit PRIVATE ink_lvgl)
target_compile_options(test-card-files-unit PRIVATE ${INK_WARN} -UNDEBUG)
add_test(NAME card-files-unit COMMAND test-card-files-unit)
add_test(NAME card-files-serve COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/card_files/serve_test.py)
ink_sim_executable(sim-card-files tests/card_files/device.c)
target_include_directories(sim-card-files PRIVATE tests)
target_compile_definitions(sim-card-files PRIVATE INK_MOGO_SIM=1)
target_link_libraries(sim-card-files PRIVATE ink_apps ink_system)
add_dependencies(sim-card-files ink_card_files ink_fonts ink_ui_fonts ink_icons)
add_test(NAME card-files-sim COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/card_files/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(card-files-sim PROPERTIES TIMEOUT 600)
# Offline almanac: fixed fixtures need neither node nor an upstream checkout.
add_executable(almanac-model tests/almanac/model.c system/services/time/almanac.c
  system/services/time/vendor/mogo-calendar/main/time/calendar.c system/services/time/lunar.c
  system/util/ink_args.c system/util/ink_json.c)
target_include_directories(almanac-model PRIVATE system/include platform/include third_party/jsmn
  system/services/time/vendor/mogo-calendar/main)
target_compile_options(almanac-model PRIVATE ${INK_WARN})
add_test(NAME almanac-fixtures COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/almanac/run.py
  --binary $<TARGET_FILE:almanac-model>)
add_test(NAME almanac-oracle COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/almanac/run.py
  --binary $<TARGET_FILE:almanac-model> --mode oracle)
set_tests_properties(almanac-oracle PROPERTIES SKIP_RETURN_CODE 77)
# Mogo's own host test, unchanged but for its Date fields; tests/almanac/run_reader_baseline.py is its shim.
add_test(NAME almanac-mogo-host COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_SOURCE_DIR}/tests/almanac
  ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/system/services/time/vendor/mogo-calendar/tools/calendar_host_test.py)
set_tests_properties(almanac-mogo-host PROPERTIES SKIP_RETURN_CODE 77)
add_test(NAME almanac-reverse COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/almanac/reverse.py
  --build ${CMAKE_BINARY_DIR})
ink_sim_executable(sim-almanac tests/almanac/device.c)
target_link_libraries(sim-almanac PRIVATE ink_apps ink_system)
add_test(NAME almanac-capability COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/almanac/device.py
  --build ${CMAKE_BINARY_DIR})
