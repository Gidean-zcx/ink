# Mogo's lock styles (lock-styles-1): the original drawing compiled as is, our glue beside it (apps/mogo_lock).
function(ink_mogo_lock_build app_target system_target)
get_filename_component(lock_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
set(lock_dir ${lock_root}/apps/mogo_lock)
set(lock_main ${lock_root}/third_party/mogo-lock/main)
set(lock_book ${lock_root}/third_party/mogo/main)
add_library(ink_mogo_lock_native STATIC
  ${lock_main}/book/book_ticket.c ${lock_main}/book/book_lock_collage.c ${lock_main}/book/book_mini_library.c
  ${lock_main}/services/home_library.c ${lock_main}/ui/ui_lock_clock.c ${lock_main}/ui/ui_calendar.c
  ${lock_main}/ui/ui_mini_library.c ${lock_main}/ui/ui_image_dither.c
  ${lock_book}/book/book_cover.c ${lock_book}/book/book_cover_auto.c ${lock_book}/book/book_excerpt.c
  ${lock_book}/book/book_title.c ${lock_book}/book/book_metadata.c ${lock_book}/book/book_shelf_scan.c
  ${lock_dir}/mogo_lock.c ${lock_dir}/port/lock_port.c)
target_include_directories(ink_mogo_lock_native BEFORE PRIVATE ${lock_dir}/port ${lock_root}/apps/mogo/port
  ${lock_main} ${lock_main}/book ${lock_main}/ui ${lock_main}/services
  ${lock_book}/book ${lock_book}/font ${lock_book}/image ${lock_book}/diagnostics
  ${lock_root}/system/services/time/vendor/mogo-calendar/main ${lock_dir} ${lock_root}/system/ui)
target_link_libraries(ink_mogo_lock_native PRIVATE ${system_target})
target_compile_definitions(ink_mogo_lock_native PRIVATE BOOK_LOCK_CACHE_PARENT="/me/mogo")
target_compile_options(ink_mogo_lock_native PRIVATE -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable
  -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers
  "SHELL:-include ${lock_root}/apps/mogo/port/runtime.h" "SHELL:-include ${lock_dir}/port/lock_port.h")
if(ESP_PLATFORM)
  target_compile_options(ink_mogo_lock_native PRIVATE -Os)
endif()
target_link_libraries(${app_target} PRIVATE ink_mogo_lock_native ink_mogo_native)
endfunction()
if(NOT ESP_PLATFORM)
ink_mogo_lock_build(ink_apps ink_system)
endif()
