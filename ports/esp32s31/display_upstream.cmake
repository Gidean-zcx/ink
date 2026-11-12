# Only the factory's output transport; board/I2C/PMIC stay owned by ink.
set(ink_upstream_commit 690a023c66862af103a3187464373ee7c5efcd14)
set(ink_upstream "$ENV{INK_UPSTREAM}")
if(NOT ink_upstream)
    set(ink_upstream "${CMAKE_BINARY_DIR}/metalio-upstream")
    if(NOT EXISTS "${ink_upstream}/.git")
        execute_process(COMMAND git clone --no-checkout --filter=blob:none
            https://github.com/CloudZao/Metalio-E-INK4-Plus.git "${ink_upstream}"
            TIMEOUT 90 RESULT_VARIABLE fetched OUTPUT_QUIET ERROR_QUIET)
    endif()
    # This clone belongs to the build: a pin change must not be blocked by an older pin's patches.
    if(EXISTS "${ink_upstream}/.git")
        execute_process(COMMAND git -C "${ink_upstream}" checkout --force --detach ${ink_upstream_commit}
            TIMEOUT 90 RESULT_VARIABLE fetched OUTPUT_QUIET ERROR_QUIET)
    endif()
endif()
execute_process(COMMAND git -C "${ink_upstream}" rev-parse HEAD
    OUTPUT_VARIABLE ink_upstream_head OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
set(ink_epd "${ink_upstream}/main/boards/metalio-e-ink4-plus/display")
if(ink_upstream_head STREQUAL ink_upstream_commit AND EXISTS "${ink_epd}/output_lcd/lcd_driver.c")
    set(ink_display_sources ../../hal_display_s31.c
        "${ink_epd}/output_lcd/lcd_driver.c" "${ink_epd}/output_common/line_queue.c")
    set(ink_display_includes "${ink_epd}" "${ink_epd}/.."
        "${ink_epd}/output_lcd" "${ink_epd}/output_common")
    message(STATUS "ink: factory screen output layer @${ink_upstream_commit}")
else()
    message(WARNING "ink: 没有屏幕输出层 (missing or unverified upstream); using display stubs")
endif()
