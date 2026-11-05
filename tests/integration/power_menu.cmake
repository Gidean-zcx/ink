# The power menu, boot after boot (tests/integration/power_menu.c): what was waiting when it went is there after. SIM, PROFILE, ASSETS, CONTENT, OUT, SRC.
file(REMOVE_RECURSE ${OUT})
file(MAKE_DIRECTORY ${OUT})
set(img ${OUT}/user.img)
foreach(phase main off off_after restart restart_after child closed battery battery_after)
  execute_process(COMMAND ${CMAKE_COMMAND} -E env INK_MENU_PHASE=${phase}
                          ${SIM} --profile ${PROFILE} --script ${SRC}/power_menu.script --budget ${SRC}/power_menu.budget.json
                          --assets ${ASSETS} --content ${CONTENT} --flash-image ${img} --sd-dir ${OUT}/sd --out ${OUT}/${phase} --quiet --no-random-outages
                  RESULT_VARIABLE rc ERROR_VARIABLE e)
  if(NOT rc EQUAL 0)
    file(READ ${OUT}/${phase}/metrics.json m)
    string(REGEX MATCH "\"warnings\":[^]]*]" w "${m}")
    message(FATAL_ERROR "${phase} failed (${rc})\n${w}\n${e}")
  endif()
  if(phase MATCHES "^(off|child|closed|battery)$")
    file(READ ${OUT}/${phase}/metrics.json m)
    foreach(what test.saw_off_picture test.off_picture_drawn ui.off_picture_on_panel)
      string(REGEX MATCH "\"${what}\":[ \t]*1" there "${m}")
      if(NOT there)
        message(FATAL_ERROR "${phase}: ${what} should have happened")
      endif()
    endforeach()
  endif()
endforeach()
