# Validation only: mirror the S31 component/source flags in the host test build.
if(PROJECT_NAME STREQUAL "ink")
  function(ink_size_host_options)
    target_compile_options(ink_lvgl PRIVATE -Os)
    target_compile_options(ink_apps PRIVATE -Os)
    file(GLOB_RECURSE ui_sources ${CMAKE_SOURCE_DIR}/system/ui/*.c
      ${CMAKE_SOURCE_DIR}/system/apps/*.c ${CMAKE_SOURCE_DIR}/system/apps/*.cpp)
    set_property(SOURCE ${ui_sources} APPEND PROPERTY COMPILE_OPTIONS -Os)
  endfunction()
  cmake_language(DEFER CALL ink_size_host_options)
endif()
