# Two modes (cmake -P):
#  MODE=make : build a fresh aged /user image with the self-test's aging mode
#              (SIM, PROFILE, IMAGE, OUT)
#  MODE=run  : copy IMAGE to a private file and run a scenario on it
#              (SIM, PROFILE, IMAGE, OUT, SCRIPT, BUDGET, ASSETS, CONTENT, EXTRA)
if(MODE STREQUAL "make")
  file(REMOVE ${IMAGE})
  set(ENV{INK_SELFTEST_AGE} 1)
  execute_process(COMMAND ${SIM} --profile ${PROFILE} --flash-image ${IMAGE} --out ${OUT} --end 200s --quiet
                  RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0 OR NOT EXISTS ${IMAGE})
    message(FATAL_ERROR "aging run failed (${rc})")
  endif()
elseif(MODE STREQUAL "run")
  if(NOT EXISTS ${IMAGE})
    message(FATAL_ERROR "aged image ${IMAGE} missing (fixture not run?)")
  endif()
  file(MAKE_DIRECTORY ${OUT})
  file(COPY_FILE ${IMAGE} ${OUT}/user.img)
  separate_arguments(extra UNIX_COMMAND "${EXTRA}")
  execute_process(COMMAND ${SIM} --profile ${PROFILE} --script ${SCRIPT} --budget ${BUDGET}
                          --assets ${ASSETS} --content ${CONTENT} --out ${OUT}
                          --flash-image ${OUT}/user.img --quiet ${extra}
                  RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "scenario failed on the aged image (${rc})")
  endif()
else()
  message(FATAL_ERROR "MODE must be make or run")
endif()
