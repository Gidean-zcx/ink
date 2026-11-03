# Runs the same scenario twice and requires bit-identical metrics and screenshots
# (the annotated CPU model must be deterministic for CI budgets to be meaningful).
separate_arguments(extra UNIX_COMMAND "${EXTRA}")
foreach(run a b)
  execute_process(COMMAND ${SIM} --profile ${PROFILE} --script ${SCRIPT} --out ${OUT}/${run} --quiet ${extra}
                  RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "run ${run} failed with ${rc}")
  endif()
endforeach()
foreach(f metrics.json final.png)
  execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files ${OUT}/a/${f} ${OUT}/b/${f} RESULT_VARIABLE d)
  if(NOT d EQUAL 0)
    message(FATAL_ERROR "non-deterministic output: ${f} differs between identical runs")
  endif()
endforeach()
