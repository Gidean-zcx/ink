# Power cut during a save, then reboot from the same flash image (cmake -P).
# SIM, PROFILE, OUT, ASSETS, CONTENT, SRC (tests/scenarios)
file(REMOVE_RECURSE ${OUT})
file(MAKE_DIRECTORY ${OUT})
set(img ${OUT}/user.img)
execute_process(COMMAND ${SIM} --profile ${PROFILE} --script ${SRC}/powercut_save.script
                        --budget ${SRC}/powercut_save.budget.json --assets ${ASSETS} --content ${CONTENT}
                        --flash-image ${img} --out ${OUT}/cut --quiet --no-random-outages
                RESULT_VARIABLE rc1)
if(NOT EXISTS ${img})
  message(FATAL_ERROR "run 1 left no flash image (rc ${rc1})")
endif()
file(READ ${OUT}/cut/metrics.json m1)
string(JSON v1 ERROR_VARIABLE e1 GET ${m1} budget_violations)
if(NOT v1 EQUAL 0)
  message(FATAL_ERROR "run 1: the power cut did not hit a flash write")
endif()
execute_process(COMMAND ${SIM} --profile ${PROFILE} --script ${SRC}/powercut_resume.script
                        --budget ${SRC}/powercut_resume.budget.json --assets ${ASSETS} --content ${CONTENT}
                        --flash-image ${img} --out ${OUT}/resume --quiet --no-random-outages
                RESULT_VARIABLE rc2)
if(NOT rc2 EQUAL 0)
  message(FATAL_ERROR "run 2 (resume after the power cut) failed: ${rc2}")
endif()
