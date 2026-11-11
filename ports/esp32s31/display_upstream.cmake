# A private checkout: INK_UPSTREAM is a read-only object source, never a checkout target.
set(ink_upstream_commit 874538eb54f7f7eb593cd40cd1ba6dc8497226e7)
set(ink_upstream "${CMAKE_BINARY_DIR}/metalio-upstream")
set(ink_upstream_origin "$ENV{INK_UPSTREAM}")
if(NOT ink_upstream_origin)
    set(ink_upstream_origin https://github.com/CloudZao/Metalio-E-INK4-Plus.git)
endif()
if(NOT EXISTS "${ink_upstream}/.git")
    execute_process(COMMAND git clone --no-checkout ${ink_upstream_origin} "${ink_upstream}"
        TIMEOUT 90 RESULT_VARIABLE fetched OUTPUT_QUIET ERROR_QUIET)
    if(NOT fetched EQUAL 0)
        message(FATAL_ERROR "ink: cannot obtain factory screen output layer")
    endif()
endif()
execute_process(COMMAND git -C "${ink_upstream}" checkout --detach ${ink_upstream_commit}
    TIMEOUT 90 RESULT_VARIABLE fetched OUTPUT_QUIET ERROR_QUIET)
execute_process(COMMAND git -C "${ink_upstream}" rev-parse HEAD
    OUTPUT_VARIABLE ink_upstream_head OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT fetched EQUAL 0 OR NOT ink_upstream_head STREQUAL ink_upstream_commit)
    message(FATAL_ERROR "ink: missing or unverified factory screen output layer")
endif()
set(ink_display_vendor "${CMAKE_CURRENT_LIST_DIR}/vendor/metalio_display")
execute_process(COMMAND git -C "${ink_upstream}" apply --reverse --check "${ink_display_vendor}/iram.patch"
    RESULT_VARIABLE patched OUTPUT_QUIET ERROR_QUIET)
if(NOT patched EQUAL 0)
    execute_process(COMMAND git -C "${ink_upstream}" apply --check "${ink_display_vendor}/iram.patch"
        RESULT_VARIABLE applicable OUTPUT_QUIET ERROR_QUIET)
    if(NOT applicable EQUAL 0)
        message(FATAL_ERROR "ink: factory IRAM patch no longer matches")
    endif()
    execute_process(COMMAND git -C "${ink_upstream}" apply "${ink_display_vendor}/iram.patch"
        RESULT_VARIABLE patched OUTPUT_QUIET ERROR_QUIET)
    if(NOT patched EQUAL 0)
        message(FATAL_ERROR "ink: factory IRAM patch failed")
    endif()
endif()
set(ink_epd "${ink_upstream}/main/boards/metalio-e-ink4-plus/display")
# Replace only the S31 power implementation; simulator board power stays portable.
list(FILTER sources EXCLUDE REGEX "/(metalio_epd_power|tps65185)\\.c$")
set(ink_display_sources ../../hal_display_s31.c "${ink_display_vendor}/power.c"
    "${ink_epd}/output_lcd/lcd_driver.c" "${ink_epd}/output_common/line_queue.c")
set(ink_display_includes "${ink_epd}" "${ink_epd}/.."
    "${ink_epd}/output_lcd" "${ink_epd}/output_common")
message(STATUS "ink: factory screen output layer @${ink_upstream_commit}")
