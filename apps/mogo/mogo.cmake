set(mogo_page_root ${CMAKE_CURRENT_LIST_DIR}/../../third_party/mogo/main)
add_library(ink_mogo_native STATIC
  ${mogo_page_root}/apps/ink_book_page.c
  ${mogo_page_root}/book/reader_open.c ${mogo_page_root}/book/reader_chapter.c
  ${mogo_page_root}/book/reader_turn.c ${mogo_page_root}/book/reader_session.c
  ${mogo_page_root}/book/book_source.c ${mogo_page_root}/book/book_txt.c ${mogo_page_root}/book/gbk.c
  ${mogo_page_root}/book/book_epub.c ${mogo_page_root}/book/zip_reader.c ${mogo_page_root}/book/html_text.c
  ${mogo_page_root}/book/book_layout.c ${mogo_page_root}/image/book_image_header.c
  ${mogo_page_root}/font/ttf_font.c ${mogo_page_root}/font/ttf_font_source.c
  ${mogo_page_root}/font/ttf_block_cache.c ${mogo_page_root}/font/ttf_glyph_cache.c)
target_include_directories(ink_mogo_native BEFORE PRIVATE ${CMAKE_CURRENT_LIST_DIR}/port ${CMAKE_CURRENT_LIST_DIR}
  ${mogo_page_root}/book ${mogo_page_root}/font ${mogo_page_root}/diagnostics
  ${mogo_page_root}/image ${CMAKE_SOURCE_DIR}/third_party/reader_miniz)
target_link_libraries(ink_mogo_native PRIVATE ink_system ink_reader_vendor)
target_compile_options(ink_mogo_native PRIVATE -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable
  -include ${CMAKE_CURRENT_LIST_DIR}/port/runtime.h)
target_sources(ink_apps PRIVATE ${CMAKE_CURRENT_LIST_DIR}/mogo.c ${CMAKE_CURRENT_LIST_DIR}/port/runtime.c)
target_include_directories(ink_apps PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/port ${mogo_page_root}/book)
target_link_libraries(ink_apps PRIVATE ink_mogo_native)
target_compile_definitions(ink_boot PRIVATE INK_MOGO_SIM=1)
add_executable(sim-mogo-page ${CMAKE_SOURCE_DIR}/simulator/runtime/sim_main.c ${CMAKE_SOURCE_DIR}/tests/graft_mogo_page/device.c)
target_include_directories(sim-mogo-page PRIVATE ${CMAKE_SOURCE_DIR}/tests)
target_compile_definitions(sim-mogo-page PRIVATE INK_MOGO_SIM=1)
target_link_libraries(sim-mogo-page PRIVATE ink_apps ink_system ink_port_sim)
target_compile_options(sim-mogo-page PRIVATE ${INK_WARN})
add_test(NAME mogo-page COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/graft_mogo_page/run.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(mogo-page PROPERTIES TIMEOUT 600)
