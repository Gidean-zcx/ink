# Desktop simulator, headless (SDL dummy video driver): boot the device in the window, open the
# shelf and a book with mouse clicks on the panel, turn the page with the NEXT cover key, and check
# that the system saw it all. The clicks go through the window's own input path (--gui-click at
# window points; the layout is fixed for the 1400x940 headless window).
#   cmake -DGUI=ink-sim-gui -DOUT=dir -P gui_smoke.cmake
file(REMOVE_RECURSE ${OUT})
set(ENV{SDL_VIDEODRIVER} dummy)
execute_process(
  COMMAND ${GUI} --data ${OUT} --speed 4 --no-random-outages
          --gui-click 441,207@3 --gui-click 498,207@6 --gui-click 481,657@12
          --gui-exit-after 16 --gui-screenshot ${OUT}/window.png
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "ink-sim-gui exited with ${rc}\n${err}")
endif()
if(NOT EXISTS ${OUT}/window.png)
  message(FATAL_ERROR "no window screenshot")
endif()
file(READ ${OUT}/out/metrics.json m)
foreach(check "input.touches\":[ \t]*([0-9]+)" "reader.page_turns\":[ \t]*([0-9]+)")
  if(NOT m MATCHES "${check}" OR CMAKE_MATCH_1 LESS 1)
    message(FATAL_ERROR "metrics.json: expected ${check} >= 1")
  endif()
endforeach()
message(STATUS "gui smoke: screenshot ${OUT}/window.png, touches and a page turn reached the system")
