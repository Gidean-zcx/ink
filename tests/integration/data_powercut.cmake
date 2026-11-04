# Power cut at many flash writes of a run that adds, replaces, deletes and folds, then a boot from the
# torn image: the collection is exactly the state after some prefix of the series, at least as far as was
# reported durable (tests/integration/data.c). SIM, PROFILE, OUT, SRC.
file(REMOVE_RECURSE ${OUT})
file(MAKE_DIRECTORY ${OUT})
set(cuts 0)
foreach(n RANGE 1 720 3)
  set(run ${OUT}/cut${n})
  file(MAKE_DIRECTORY ${run})
  set(img ${run}/user.img)
  set(ack ${run}/ack.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -E env INK_DATA_PHASE=write INK_DATA_CUT=${n} INK_DATA_ACK=${ack}
                          ${SIM} --profile ${PROFILE} --flash-image ${img} --out ${run}/write --quiet --no-random-outages --end 60s
                  RESULT_VARIABLE rc1 ERROR_QUIET)
  if(NOT EXISTS ${img})
    message(FATAL_ERROR "cut ${n}: run 1 left no flash image (rc ${rc1})")
  endif()
  file(READ ${run}/write/metrics.json m)
  string(JSON torn ERROR_VARIABLE noval GET ${m} metrics flash.torn_ops)
  if(NOT noval AND torn GREATER 0)
    math(EXPR cuts "${cuts} + 1")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} -E env INK_DATA_PHASE=verify INK_DATA_ACK=${ack}
                          ${SIM} --profile ${PROFILE} --flash-image ${img} --out ${run}/verify --quiet --no-random-outages --end 60s
                          --budget ${SRC}/data_powercut.budget.json
                  RESULT_VARIABLE rc2 OUTPUT_VARIABLE out2 ERROR_VARIABLE err2)
  if(NOT rc2 EQUAL 0)
    message(FATAL_ERROR "cut at write ${n}: the collection is not consistent after the power cut (rc ${rc2})\n${out2}\n${err2}")
  endif()
endforeach()
if(cuts LESS 150)
  message(FATAL_ERROR "only ${cuts} runs tore a flash operation: the test proves little")
endif()
message(STATUS "data-powercut: ${cuts} runs tore a flash operation, all consistent")
