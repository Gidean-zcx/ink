function(ink_mogo_build app_target system_target)
get_filename_component(mogo_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
set(mogo_dir ${mogo_root}/apps/mogo)
set(mogo_page_root ${mogo_root}/third_party/mogo/main)
add_library(ink_mogo_native STATIC
  ${mogo_page_root}/apps/ink_book_page.c ${mogo_dir}/port/migrate.c
  ${mogo_page_root}/book/ink_reader_open.c ${mogo_page_root}/book/reader_chapter.c
  ${mogo_page_root}/book/reader_turn.c ${mogo_page_root}/book/reader_session.c
  ${mogo_page_root}/book/book_progress.c ${mogo_page_root}/book/book_shelf_model.c ${mogo_page_root}/apps/book_toc.c
  ${mogo_page_root}/book/book_source.c ${mogo_page_root}/book/book_txt.c ${mogo_page_root}/book/gbk.c
  ${mogo_page_root}/book/book_epub.c ${mogo_page_root}/book/zip_reader.c ${mogo_page_root}/book/html_text.c
  ${mogo_page_root}/book/book_layout.c ${mogo_page_root}/image/book_image_header.c
  ${mogo_page_root}/font/ttf_font.c ${mogo_page_root}/font/ttf_font_source.c
  ${mogo_page_root}/font/ttf_block_cache.c ${mogo_page_root}/font/ttf_glyph_cache.c)
target_include_directories(ink_mogo_native BEFORE PRIVATE ${mogo_dir}/port ${mogo_dir}
  ${mogo_page_root}/book ${mogo_page_root}/font ${mogo_page_root}/diagnostics
  ${mogo_page_root}/image ${mogo_page_root}/apps ${mogo_root}/third_party/reader_miniz)
# Mogo inflates EPUB entries with the core tinfl_decompress of the shared inflater.
target_link_libraries(ink_mogo_native PRIVATE ${system_target} ink_miniz)
target_compile_options(ink_mogo_native PRIVATE -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable
  -include ${mogo_dir}/port/runtime.h)
target_sources(${app_target} PRIVATE ${mogo_dir}/mogo.c ${mogo_dir}/port/runtime.c ${mogo_dir}/port/preferences.c ${mogo_dir}/port/drawing.c)
target_include_directories(${app_target} PRIVATE ${mogo_dir} ${mogo_dir}/port ${mogo_page_root}/book ${mogo_page_root}/font ${mogo_root}/system/ui)
target_link_libraries(${app_target} PRIVATE ink_mogo_native)
if(ESP_PLATFORM)
  target_compile_options(ink_mogo_native PRIVATE -Os -fstack-usage)
  # The page's text buffers stay out of internal RAM, like the system's own tables.
  set_source_files_properties(${mogo_dir}/mogo.c TARGET_DIRECTORY ${app_target} PROPERTIES
    COMPILE_DEFINITIONS INK_PSRAM_BSS=EXT_RAM_BSS_ATTR COMPILE_OPTIONS "-include;esp_attr.h")
else()
  target_include_directories(${app_target} PRIVATE ${mogo_root}/simulator/kernel/vtk)
endif()
endfunction()
if(NOT ESP_PLATFORM)
ink_mogo_build(ink_apps ink_system)
foreach(test graft_mogo_page:mogo-page graft_mogo_flow:mogo-flow reader_epub:reader-epub reader_settings:reader-settings
    reader_more:reader-more reader_one:reader-one)
  string(REPLACE ":" ";" pair ${test})
  list(GET pair 0 dir)
  list(GET pair 1 name)
  add_executable(sim-${name} ${CMAKE_SOURCE_DIR}/simulator/runtime/sim_main.c ${CMAKE_SOURCE_DIR}/tests/${dir}/device.c)
  target_include_directories(sim-${name} PRIVATE ${CMAKE_SOURCE_DIR}/tests ${CMAKE_SOURCE_DIR}/system/ui)
  target_link_libraries(sim-${name} PRIVATE ink_apps ink_system ink_port_sim)
  target_compile_options(sim-${name} PRIVATE ${INK_WARN})
endforeach()
add_test(NAME mogo-page COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/graft_mogo_page/run.py --build ${CMAKE_BINARY_DIR})
add_test(NAME mogo-flow COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/graft_mogo_flow/run.py --build ${CMAKE_BINARY_DIR})
add_test(NAME reader-epub COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/reader_epub/run.py --build ${CMAKE_BINARY_DIR})
add_test(NAME reader-epub-reverse COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/reader_epub/reverse.py --build ${CMAKE_BINARY_DIR})
add_test(NAME reader-epub-segments COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/reader_epub/run.py --build ${CMAKE_BINARY_DIR} --segments)
add_test(NAME reader-epub-toc-save-reverse COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/reader_epub/reverse.py --build ${CMAKE_BINARY_DIR} --unsaved-toc)
add_test(NAME reader-settings COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/reader_settings/run.py --build ${CMAKE_BINARY_DIR})
add_test(NAME reader-settings-reverse COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/reader_settings/run.py --build ${CMAKE_BINARY_DIR} --reverse)
add_test(NAME reader-more COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/reader_more/run.py --build ${CMAKE_BINARY_DIR})
add_test(NAME reader-more-reverse COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/reader_more/run.py --build ${CMAKE_BINARY_DIR} --reverse)
foreach(phase migrate migrate-reverse states bookmarks)
  add_test(NAME reader-one-${phase} COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/reader_one/run.py --build ${CMAKE_BINARY_DIR} --phase ${phase})
endforeach()
set_tests_properties(mogo-page mogo-flow reader-epub reader-epub-reverse reader-epub-segments reader-epub-toc-save-reverse reader-settings
  reader-settings-reverse reader-more reader-more-reverse reader-one-migrate reader-one-migrate-reverse reader-one-states reader-one-bookmarks PROPERTIES TIMEOUT 600)
endif()
