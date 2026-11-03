# Audit-only workaround: enable ObjC in the root directory before SDL2 is added.
# Usage: -DCMAKE_PROJECT_ink_INCLUDE=<absolute path to this file>
if(APPLE)
  enable_language(OBJC)
endif()
