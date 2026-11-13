# Desktop simulator, headless (SDL dummy video driver): boot the device in the window, open the
# shelf (the 阅读 favourite on the one-page home, D-135) and a book with mouse clicks on the panel, turn the page
# with the NEXT cover key, and check that the system saw it all. The clicks go through the window's own input path (--gui-click at
# window points; the layout is fixed for the 1400x940 headless window). Real-time speed: a click's
# press and release reach the system a window frame apart, so at --speed 4 a slow runner's frames
# (one over 0.2 s, seen once on the macOS runner) stretched the NEXT tap past the 0.8 s long press.
# Even at real time a starved runner can hold a frame that long (seen once more on the macOS runner
# under capacity limits), so a run that misses is tried once more; a broken input path fails both.
#   cmake -DGUI=ink-sim-gui -DOUT=dir -P gui_smoke.cmake
set(ENV{SDL_VIDEODRIVER} dummy)
set(ENV{UBSAN_OPTIONS} "$ENV{UBSAN_OPTIONS}:halt_on_error=1")
set(problem "")
foreach(attempt 1 2)
  set(dir ${OUT})
  if(attempt EQUAL 2)
    message(STATUS "gui smoke: first run missed (${problem}); trying once more")
    set(dir ${OUT}-retry)
  endif()
  file(REMOVE_RECURSE ${dir})
  execute_process(
    COMMAND ${GUI} --data ${dir} --speed 1 --no-random-outages
            --gui-click 389,657@1 --gui-click 498,372@3 --gui-click 498,207@6 --gui-click 481,657@12
            --gui-exit-after 16 --gui-screenshot ${dir}/window.png
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "ink-sim-gui exited with ${rc}\n${err}") # a crash is never retried
  endif()
  set(problem "")
  if(NOT EXISTS ${dir}/window.png)
    set(problem "no window screenshot")
  else()
    file(READ ${dir}/out/metrics.json m)
    foreach(check "input.touches\":[ \t]*([0-9]+)" "reader.page_turns\":[ \t]*([0-9]+)")
      if(NOT m MATCHES "${check}" OR CMAKE_MATCH_1 LESS 1)
        set(problem "metrics.json: expected ${check} >= 1")
      endif()
    endforeach()
  endif()
  if(problem STREQUAL "")
    message(STATUS "gui smoke: screenshot ${dir}/window.png, touches and a page turn reached the system (run ${attempt})")
    return()
  endif()
endforeach()
message(FATAL_ERROR "${problem} (two runs)")
