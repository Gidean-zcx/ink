# ink — portable system + simulator build (host).
#
# Device firmware is built with ESP-IDF (ports/esp32s31, later); this file
# builds everything that runs on a PC: the simulator, the POSIX port, tests.
#
#   cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(ink C)
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
add_executable(test-s31-hal-map tests/esp32s31/hal_map.c)
target_include_directories(test-s31-hal-map PRIVATE tests/esp32s31/stub platform/include)
target_compile_options(test-s31-hal-map PRIVATE ${INK_WARN} -UNDEBUG)
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
add_library(ink_port_sim STATIC
  ports/sim/osal/osal_sim.c
  ports/sim/hal/hal_i2c_sim.c
  ports/sim/hal/hal_sensors_sim.c
  ports/sim/hal/hal_display_sim.c
  ports/sim/hal/hal_input_sim.c
  ports/sim/hal/hal_storage_sim.c
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
  simulator/runtime/metrics.c
  simulator/runtime/trace.c
  simulator/runtime/script.c
  simulator/desktop/sim_gui.c)
target_include_directories(ink_port_sim PUBLIC simulator simulator/desktop)
target_link_libraries(ink_port_sim PUBLIC ink_board_metalio_plus ink_drivers ink_sim_core ink_littlefs ink_stb ink_lz4 ink_sha256)
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
  system/services/power/power_service.c
  system/services/hub/hub_link.c
  system/services/hub/pairing.c
  system/services/hub/rt.c
  system/services/ai/ai_service.c
  system/services/audio/audio_service.c
  system/services/res/exec.c
  system/services/res/res.c
  system/services/res/svc.c
  system/services/res/task.c
  system/services/data/datastore.c
  system/services/data/data_caps.c
  system/services/backup/backup_service.c
  system/services/file/file_service.c
  system/services/file/file_caps.c
  system/services/file/transfer.c
  system/services/time/lunar.c
  system/services/time/time_service.c
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
  system/services/notify/interrupt.c
  system/services/net/wifi.c
  system/ui/shell/pinpad.c
  system/ui/shell/member_gate.c
  system/services/audio/ogg.c
  system/services/audio/media_service.c
  system/services/bt/bt_service.c
  system/services/text/text_service.c
  system/services/text/pinyin.c
  system/services/text/clipboard.c
  system/services/text/text_caps.c
  system/services/text/voice_input.c
  system/ui/textfield.c
  system/ui/shell/keyboard.c
  system/ui/app_registry.c
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
  system/ui/ui_task.c
  system/ui/ui_semantics.c
  system/ui/ui_list.c
  system/ui/search.c
  system/ui/theme.c
  system/ui/icons.c
  system/ui/shell/navigation.c
  system/ui/shell/settings_pages.c
  system/ui/shell/bluetooth_page.c
  system/ui/shell/ai_panel.c
  system/ui/app.c
  system/apps/remote/remote.c)
target_include_directories(ink_system PUBLIC system/include PRIVATE system/services/display third_party/jsmn third_party/stb)
target_link_libraries(ink_system PUBLIC ink_platform ink_lvgl ink_lz4 ink_sha256 PRIVATE ink_image_codec)
# The system calls the HAL; in this build the simulator port implements it. Link-only (no headers
# leak into the system or the apps), and it puts the port after the system on every link line, which
# GNU ld needs for HAL functions only the system references (e.g. hal_cell_*).
target_link_libraries(ink_system INTERFACE $<LINK_ONLY:ink_port_sim>)
target_sources(ink_system PRIVATE system/apps/clock/clock.c)
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
file(GLOB INK_I18N_TABLES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/assets/i18n/*.tsv) # interface languages
set(INK_HAVE_FONTS OFF)
if(EXISTS ${INK_CJK_FONT} AND Python3_FOUND)
  set(INK_FONT_FILES)
  foreach(px 44) # the one reading font; the UI scales it for text outside its own subset
    set(out ${INK_ASSETS_DIR}/fonts/sans-${px}.inkf)
    add_custom_command(OUTPUT ${out}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${INK_ASSETS_DIR}/fonts
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/fontgen/fontgen.py --font ${INK_CJK_FONT}
              --face ${INK_CJK_FACE} --px ${px} --bpp 2 --charset ascii,punct,pinyin,gb2312
              --digits-font ${CMAKE_SOURCE_DIR}/third_party/fonts/Outfit.ttf --digits-weight 400 -o ${out}
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
  file(GLOB_RECURSE INK_UI_TEXT CONFIGURE_DEPENDS system/ui/*.c apps/*.c system/services/*.c system/policy/*.c)
  add_custom_command(OUTPUT ${INK_ASSETS_DIR}/fonts/ui-fonts.stamp
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/fontgen/ui_fonts.py
      --regular ${INK_CJK_FONT} --face ${INK_CJK_FACE} --medium ${INK_CJK_MEDIUM_FONT} --bold ${INK_CJK_BOLD_FONT}
      --out ${INK_ASSETS_DIR}/fonts
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
set(INK_I18N_SOURCES ${CMAKE_SOURCE_DIR}/system/ui/shell/boot_flow.c ${CMAKE_SOURCE_DIR}/system/ui/shell/navigation.c
  ${CMAKE_SOURCE_DIR}/system/ui/shell/settings_pages.c)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/generated/ink/i18n_table.h
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/i18n/i18n_gen.py ${INK_I18N_TABLES}
          --header ${CMAKE_BINARY_DIR}/generated/ink/i18n_table.h --sources ${INK_I18N_SOURCES}
  DEPENDS tools/i18n/i18n_gen.py ${INK_I18N_TABLES} ${INK_I18N_SOURCES}
  COMMENT "i18n: check the translation tables")
add_custom_target(ink_i18n_table DEPENDS ${CMAKE_BINARY_DIR}/generated/ink/i18n_table.h)
add_dependencies(ink_system ink_i18n_table)
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
add_custom_target(ink_test_content ALL DEPENDS ${INK_TEST_CONTENT}/books/测试长书.txt ${INK_TEST_CONTENT}/.system/ime/base.ikd)

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
add_test(NAME unit-s31-osal-ticks COMMAND test-s31-osal-ticks)
add_test(NAME unit-s31-hal-map COMMAND test-s31-hal-map)
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
add_test(NAME ui-scale-regressions COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/ui_scale_check_test.py)
add_test(NAME ui-compare-regressions COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/ui_compare_test.py)
add_test(NAME ui-icons COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/icongen/check.py)
add_test(NAME ui-icons-regressions COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/ui_icon_check_test.py)
add_test(NAME ui-assets COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/ui_assets_check.py ${INK_ASSETS_DIR})
add_executable(sim-ui-scenes simulator/runtime/sim_main.c tests/ui_shell_scenes.c)
target_link_libraries(sim-ui-scenes PRIVATE ink_apps ink_system ink_port_sim)
target_compile_options(sim-ui-scenes PRIVATE ${INK_WARN} -Wno-format-truncation)
set(INK_SHELL_SCENES Main Today Apps Lock LockPage Control Notifications Members MembersBack Pin PinWrong
  ImePinyin ImeCandidates ImeEdit ImePassword ImeVoice Confirm ConfirmDelete ConfirmSecret AiListen AiThink Ai
  Settings SetDisplay SetSound SetNetwork SetVoice SetAbout SetPrivacy ImeSymbols
  PowerMenu PowerOff PhoneIncoming ClockRing Components ComponentsDock ComponentsPanel)
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
foreach(scene Main Today Apps Control Notifications Ai ImePinyin)
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
target_link_libraries(sim-i18n-lang PRIVATE ink_apps ink_system)
add_test(NAME i18n-lang COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/i18n_check.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(i18n-lang PROPERTIES TIMEOUT 300)
add_test(NAME i18n-tables COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/i18n_tables.py)
add_test(NAME boot-flow-reboot COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/boot_flow_reboot.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(boot-flow-reboot PROPERTIES TIMEOUT 200)
target_link_libraries(sim-ui-cards PRIVATE ink_apps ink_system)
add_test(NAME ui-cards
  COMMAND sim-ui-cards --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
    --script ${CMAKE_SOURCE_DIR}/tests/ui_cards.script --budget ${CMAKE_SOURCE_DIR}/tests/ui_scene.budget.json
    --assets ${INK_ASSETS_DIR} --content ${INK_TEST_CONTENT} --out ${CMAKE_BINARY_DIR}/test-out/ui-cards --quiet --no-random-outages)
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
target_link_libraries(sim-capability-inventory PRIVATE ink_apps ink_system)
if(INK_HAVE_FONTS)
  add_dependencies(sim-capability-inventory ink_fonts ink_test_content)
endif()
add_test(NAME hw-bench-report COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/hw_bench/report_test.py)
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
  set_tests_properties(hub-contract PROPERTIES FIXTURES_REQUIRED hub-caps FIXTURES_SETUP hub-messages SKIP_RETURN_CODE 77 TIMEOUT 120)
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
  foreach(v main nocard baddict)
    add_test(NAME integration-text-${v}
      COMMAND ${CMAKE_COMMAND} -E env INK_TEXT_PHASE=${v}
              $<TARGET_FILE:sim-textinput> --profile ${CMAKE_SOURCE_DIR}/simulator/profiles/metalio-plus.json
              --budget ${CMAKE_SOURCE_DIR}/tests/integration/textinput.budget.json
              --script ${CMAKE_SOURCE_DIR}/tests/integration/textinput_${v}.script
              --assets ${INK_ASSETS_DIR} --content ${INK_MINI_CONTENT}
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
target_link_libraries(sim-clock-alarm PRIVATE ink_apps ink_system)
add_test(NAME clock-alarm COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/clock_alarm.py --build ${CMAKE_BINARY_DIR})
ink_sim_executable(sim-sound-clock tests/sound_clock.c)
target_link_libraries(sim-sound-clock PRIVATE ink_apps ink_system)
add_test(NAME sound-clock COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/sound_clock.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(sound-clock PROPERTIES TIMEOUT 300)
add_test(NAME sound-clock-input COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/sound_clock_input.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(sound-clock-input PROPERTIES TIMEOUT 90)
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
target_sources(ink_system PRIVATE system/apps/phone/phone.c)
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
target_sources(ink_system PRIVATE system/apps/memo/memo.c)
ink_sim_executable(sim-memo tests/memo/memo.c)
target_link_libraries(sim-memo PRIVATE ink_apps ink_system)
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

target_sources(ink_system PRIVATE system/apps/calculator/calculator.c)
ink_sim_executable(sim-calculator tests/calculator/calculator.c)
target_link_libraries(sim-calculator PRIVATE ink_apps ink_system)
add_test(NAME calculator COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/calculator/run.py --build ${CMAKE_BINARY_DIR})

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

target_sources(ink_system PRIVATE system/apps/camera/camera.c system/apps/gallery/gallery.c)
add_library(ink_camera_png STATIC system/apps/camera/png.c)
target_include_directories(ink_camera_png PRIVATE third_party/stb platform/include)
target_compile_options(ink_camera_png PRIVATE -w) # upstream stb implementation
target_link_libraries(ink_camera_png PRIVATE ink_platform)
target_link_libraries(ink_system PRIVATE ink_camera_png)
ink_sim_executable(sim-camera-gallery tests/camera_gallery/camera_gallery.c ${INK_IMAGE_FIXTURES})
target_include_directories(sim-camera-gallery PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(sim-camera-gallery PRIVATE ink_apps ink_system)
add_test(NAME camera-gallery COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/camera_gallery/run.py --build ${CMAKE_BINARY_DIR})

target_sources(ink_system PRIVATE system/ui/shell/backup_pages.c)
ink_sim_executable(sim-backup-pages tests/backup_pages.c)
target_link_libraries(sim-backup-pages PRIVATE ink_apps ink_system)
add_test(NAME backup-pages COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/backup_pages.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(backup-pages PROPERTIES TIMEOUT 180)
ink_sim_executable(sim-factory-pages tests/factory_pages.c)
target_link_libraries(sim-factory-pages PRIVATE ink_apps ink_system)
add_test(NAME factory-pages COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/factory_pages.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(factory-pages PROPERTIES TIMEOUT 420)

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
set_tests_properties(perf-latency PROPERTIES TIMEOUT 180)
