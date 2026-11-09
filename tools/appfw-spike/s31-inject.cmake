# Explicit injection into the unmodified port project, never a default option.
cmake_language(DEFER CALL include "${CMAKE_SOURCE_DIR}/../../tools/appfw-spike/s31.cmake")
