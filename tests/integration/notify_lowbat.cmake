# A nearly empty battery writes what waits and powers off (tests/integration/notify.c, phases lowbat1 and lowbat2):
# the second run boots from the same flash image. SIM, PROFILE, ASSETS, CONTENT, OUT, SRC.
file(REMOVE_RECURSE ${OUT})
file(MAKE_DIRECTORY ${OUT})
set(img ${OUT}/user.img)
foreach(phase lowbat1 lowbat2)
  execute_process(COMMAND ${CMAKE_COMMAND} -E env INK_NOTIFY_PHASE=${phase}
                          ${SIM} --profile ${PROFILE} --script ${SRC}/notify_${phase}.script --budget ${SRC}/notify_${phase}.budget.json
                          --assets ${ASSETS} --content ${CONTENT} --flash-image ${img} --out ${OUT}/${phase} --quiet --no-random-outages
                  RESULT_VARIABLE rc ERROR_VARIABLE e)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${phase} failed (${rc})\n${e}")
  endif()
endforeach()
