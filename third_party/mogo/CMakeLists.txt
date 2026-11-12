# Original Mogo host tests; these stand-ins are never linked into ink_apps.
find_package(ZLIB REQUIRED)
set(mogo_main ${CMAKE_CURRENT_SOURCE_DIR}/main)
set(mogo_tools ${CMAKE_CURRENT_SOURCE_DIR}/tools)
function(mogo_host name)
  add_executable(mogo-${name} ${ARGN})
  target_include_directories(mogo-${name} PRIVATE ${mogo_main}/book ${mogo_main}
    ${mogo_main}/diagnostics ${mogo_main}/display ${mogo_main}/settings)
  target_compile_definitions(mogo-${name} PRIVATE _DEFAULT_SOURCE)
  target_compile_options(mogo-${name} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  if(INK_SANITIZE)
    target_compile_options(mogo-${name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(mogo-${name} PRIVATE -fsanitize=address,undefined)
  endif()
endfunction()
set(source_files main/book/reader_session.c main/book/book_source.c main/book/book_index_cache.c
  main/book/book_txt.c main/book/gbk.c main/book/book_epub.c main/image/book_image_header.c
  main/book/zip_reader.c main/book/html_text.c)
mogo_host(source tools/book_source_host_test.c tools/reader_session_no_layout_stubs.c ${source_files})
target_include_directories(mogo-source BEFORE PRIVATE ${mogo_tools}/book_epub_stubs
  ${mogo_tools}/zip_host_stubs ${mogo_tools}/book_layout_stubs)
target_link_libraries(mogo-source PRIVATE ZLIB::ZLIB)
mogo_host(utf16 ${CMAKE_SOURCE_DIR}/tests/graft_mogo/utf16.c tools/reader_session_no_layout_stubs.c ${source_files})
target_include_directories(mogo-utf16 BEFORE PRIVATE ${mogo_tools}/book_epub_stubs
  ${mogo_tools}/zip_host_stubs ${mogo_tools}/book_layout_stubs)
target_link_libraries(mogo-utf16 PRIVATE ZLIB::ZLIB)
mogo_host(layout tools/book_layout_host_test.c main/book/reader_session.c
  main/book/book_layout.c main/book/html_text.c)
target_include_directories(mogo-layout BEFORE PRIVATE ${mogo_tools}/book_layout_stubs ${mogo_tools}/book_source_host_stubs)
mogo_host(progress tools/book_storage_host_test.c main/book/book_progress.c)
target_include_directories(mogo-progress BEFORE PRIVATE ${mogo_tools}/book_storage_stubs)
mogo_host(store tools/book_store_host_test.c)
target_include_directories(mogo-store BEFORE PRIVATE ${mogo_tools}/book_storage_stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/components/board_support/include)
mogo_host(policy tools/book_policy_host_test.c)
mogo_host(html tools/html_text_host_test.c main/book/html_text.c)
target_include_directories(mogo-html BEFORE PRIVATE ${mogo_tools}/html_text_stubs)
add_test(NAME mogo-fixtures COMMAND ${Python3_EXECUTABLE} ${mogo_tools}/gen_book_fixtures.py
  --output ${CMAKE_CURRENT_BINARY_DIR}/fixtures)
set_tests_properties(mogo-fixtures PROPERTIES FIXTURES_SETUP mogo_books)
add_test(NAME mogo-source COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/graft_mogo/run_source.py
  $<TARGET_FILE:mogo-source> ${CMAKE_CURRENT_BINARY_DIR}/fixtures)
set_tests_properties(mogo-source PROPERTIES FIXTURES_REQUIRED mogo_books RUN_SERIAL TRUE)
foreach(name layout progress store policy html)
  add_test(NAME mogo-${name} COMMAND mogo-${name})
endforeach()
add_test(NAME mogo-shelf-bookmarks COMMAND ${Python3_EXECUTABLE} ${mogo_tools}/book_ui_host_test.py)
add_test(NAME mogo-session COMMAND ${Python3_EXECUTABLE} ${mogo_tools}/reader_session_host_test.py)
add_test(NAME mogo-utf16 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/graft_mogo/run_utf16.py $<TARGET_FILE:mogo-utf16>)
add_test(NAME mogo-settings-drawing COMMAND ${Python3_EXECUTABLE} ${mogo_tools}/reader_settings_view_host_test.py)
add_test(NAME mogo-provenance COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/graft_mogo/verify_import.py)
