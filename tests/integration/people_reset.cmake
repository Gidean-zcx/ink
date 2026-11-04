# A factory reset (tests/integration/people.c): the second run boots from the same flash and card. SIM, PROFILE, ASSETS, CONTENT, OUT, SRC.
file(REMOVE_RECURSE ${OUT})
file(MAKE_DIRECTORY ${OUT})
set(img ${OUT}/user.img)
foreach(phase reset1 reset2)
  execute_process(COMMAND ${CMAKE_COMMAND} -E env INK_PEOPLE_PHASE=${phase}
                          ${SIM} --profile ${PROFILE} --script ${SRC}/people_${phase}.script --budget ${SRC}/people_reset.budget.json
                          --assets ${ASSETS} --content ${CONTENT} --flash-image ${img} --sd-dir ${OUT}/sd --out ${OUT}/${phase} --quiet --no-random-outages
                  RESULT_VARIABLE rc ERROR_VARIABLE e)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${phase} failed (${rc})\n${e}")
  endif()
endforeach()
