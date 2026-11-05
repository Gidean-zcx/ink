# Alarms across a power-off (tests/integration/timesvc.c, phases reboot1 and reboot2): the second run boots five
# minutes later from the same flash image. SIM, PROFILE, OUT, SRC.
file(REMOVE_RECURSE ${OUT})
file(MAKE_DIRECTORY ${OUT})
set(img ${OUT}/user.img)
execute_process(COMMAND ${CMAKE_COMMAND} -E env INK_TIME_PHASE=reboot1
                        ${SIM} --profile ${PROFILE} --script ${SRC}/timesvc_reboot1.script --budget ${SRC}/timesvc_reboot.budget.json
                        --flash-image ${img} --out ${OUT}/first --quiet --no-random-outages
                RESULT_VARIABLE rc1 ERROR_VARIABLE e1)
if(NOT rc1 EQUAL 0)
  message(FATAL_ERROR "run 1 failed (${rc1})\n${e1}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} -E env INK_TIME_PHASE=reboot2
                        ${SIM} --profile ${PROFILE} --script ${SRC}/timesvc_reboot2.script --budget ${SRC}/timesvc_reboot.budget.json
                        --flash-image ${img} --out ${OUT}/second --quiet --no-random-outages --wall-start 1793577900
                RESULT_VARIABLE rc2 ERROR_VARIABLE e2)
if(NOT rc2 EQUAL 0)
  message(FATAL_ERROR "run 2 failed (${rc2})\n${e2}")
endif()

# A change of zone across a restart: an image of its own.
set(img ${OUT}/zone.img)
execute_process(COMMAND ${CMAKE_COMMAND} -E env INK_TIME_PHASE=zone1
                        ${SIM} --profile ${PROFILE} --script ${SRC}/timesvc_reboot1.script --budget ${SRC}/timesvc_reboot.budget.json
                        --flash-image ${img} --out ${OUT}/zone1 --quiet --no-random-outages
                RESULT_VARIABLE rc3 ERROR_VARIABLE e3)
if(NOT rc3 EQUAL 0)
  message(FATAL_ERROR "zone run 1 failed (${rc3})\n${e3}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} -E env INK_TIME_PHASE=zone2
                        ${SIM} --profile ${PROFILE} --script ${SRC}/timesvc_reboot1.script --budget ${SRC}/timesvc_reboot.budget.json
                        --flash-image ${img} --out ${OUT}/zone2 --quiet --no-random-outages --wall-start 1793577900
                RESULT_VARIABLE rc4 ERROR_VARIABLE e4)
if(NOT rc4 EQUAL 0)
  message(FATAL_ERROR "zone run 2 failed (${rc4})\n${e4}")
endif()
