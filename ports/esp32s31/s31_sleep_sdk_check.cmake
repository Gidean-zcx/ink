# Re-review the vendor wrappers and anchors before accepting another SDK.
include("$ENV{IDF_PATH}/tools/cmake/version.cmake")
if(NOT "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH}" STREQUAL "6.1.0")
    message(FATAL_ERROR "S31 sleep: unsupported IDF version; review the vendor backport")
endif()
file(SHA256 "$ENV{IDF_PATH}/components/esp_hw_support/port/esp32s31/pmu_sleep.c" _ink_pmu_hash)
if(NOT _ink_pmu_hash STREQUAL "b8215650d93607fc2e0a8e14818e411c5a59ab52c5e4dd7471ed74a229309840")
    message(FATAL_ERROR "S31 sleep: PMU source/anchors changed; review the vendor backport")
endif()
