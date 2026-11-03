# The calibration kernels are compiled unchanged from tools/iss/kernels; this directory's iss_rt.h
# replaces the simulator's. tinf and miniz come from fetch_deps.sh (pinned as in calibrate.sh).
set(iss ${CMAKE_CURRENT_LIST_DIR}/../../iss)
set(repo ${CMAKE_CURRENT_LIST_DIR}/../../..)
set(deps ${iss}/build/src)

idf_component_register(
    SRCS hw_probe.c iss_rt_idf.c
         ${iss}/kernels/kmain.c ${iss}/kernels/k_feed.c ${iss}/kernels/k_mem.c ${iss}/kernels/k_gfx.c
         ${iss}/kernels/k_text.c ${iss}/kernels/k_misc.c ${iss}/kernels/data.S
         ${repo}/third_party/cjson/cJSON.c ${repo}/third_party/tlsf/tlsf.c
         ${deps}/tinf/src/tinflate.c ${deps}/tinf/src/tinfzlib.c ${deps}/tinf/src/adler32.c
         ${deps}/miniz/miniz_tinfl.c
    INCLUDE_DIRS . ${iss}/kernels ${repo}/third_party/cjson ${repo}/third_party/tlsf ${deps}/tinf/src
                 ${deps}/miniz ${iss}/build/gen
    PRIV_REQUIRES app_update esp_mm esp_psram spi_flash)

# two kernels are standalone programs on the ISS: call their main() under another name
set_source_files_properties(${iss}/kernels/k_feed.c PROPERTIES COMPILE_DEFINITIONS "main=k_feed_main")
set_source_files_properties(${iss}/kernels/kmain.c PROPERTIES COMPILE_DEFINITIONS "main=kernels_main")
set_source_files_properties(${iss}/kernels/data.S PROPERTIES COMPILE_OPTIONS "-Wa,-I${iss}/kernels/data")
target_compile_definitions(${COMPONENT_LIB} PRIVATE MINIZ_NO_STDIO MINIZ_NO_TIME MINIZ_NO_ARCHIVE_APIS)
# third-party and measurement code: warnings stay warnings
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error)
