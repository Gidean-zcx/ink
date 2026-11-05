# A factory reset, and the family's table not readable (tests/integration/people.c): boot after boot on the same flash and card.
# SIM, PROFILE, ASSETS, CONTENT, OUT, SRC.
file(REMOVE_RECURSE ${OUT})
file(MAKE_DIRECTORY ${OUT})

# chain: the flash image and the card; phase: which run; ENV: what else
function(run chain phase script)
  set(dir ${OUT}/${chain})
  file(MAKE_DIRECTORY ${dir})
  set(path ${SRC}/people_${script}.script)
  if(EXISTS ${script})
    set(path ${script})
    get_filename_component(script ${script} NAME_WE)
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} -E env INK_PEOPLE_PHASE=${phase} ${ARGN}
                          ${SIM} --profile ${PROFILE} --script ${path} --budget ${SRC}/people_reset.budget.json
                          --assets ${ASSETS} --content ${CONTENT} --flash-image ${dir}/user.img --sd-dir ${dir}/sd --out ${dir}/${script}-${phase} --quiet --no-random-outages
                  RESULT_VARIABLE rc ERROR_VARIABLE e)
  file(READ ${dir}/${script}-${phase}/metrics.json m)
  if(NOT rc EQUAL 0)
    string(REGEX MATCH "\"warnings\":[^]]*]" w "${m}")
    message(FATAL_ERROR "${chain}/${phase} failed (${rc})\n${w}\n${e}")
  endif()
  set(last_metrics "${m}" PARENT_SCOPE)
endfunction()

function(must_have what)
  string(REGEX MATCH "\"${what}\":[ \t]*1" there "${last_metrics}")
  if(NOT there)
    message(FATAL_ERROR "${what} should have happened")
  endif()
endfunction()

function(must_not_have what)
  string(REGEX MATCH "\"${what}\":" there "${last_metrics}")
  if(there)
    message(FATAL_ERROR "${what} should not have happened")
  endif()
endfunction()

# the card is there
run(a reset1 reset1)
must_have(test.finished)
must_have(power.reboot)
must_have(system.factory_resets)
run(a reset2 reset2)
must_have(test.finished)
must_have(system.factory_reset_done)

# the power goes in the middle of the wiping: at each of the flash writes it takes. The mark goes last, so the boot after that does all of it
run(b reset1 reset1)
must_have(power.reboot)
set(cuts 0)
foreach(n RANGE 1 60)
  file(REMOVE_RECURSE ${OUT}/cut)
  file(MAKE_DIRECTORY ${OUT}/cut)
  file(COPY ${OUT}/b/user.img ${OUT}/b/user.img.ota ${OUT}/b/sd DESTINATION ${OUT}/cut)
  file(WRITE ${OUT}/cut.script "@0 powercut write ${n}\n@30s end\n")
  run(cut boot ${OUT}/cut.script)
  string(REGEX MATCH "\"system.factory_reset_done\":" whole "${last_metrics}")
  if(whole)
    break() # it took fewer than that
  endif()
  math(EXPR cuts "${cuts} + 1")
  run(cut reset2 reset2)
  must_have(test.finished)
endforeach()
if(cuts LESS 5)
  message(FATAL_ERROR "the wiping should take more writes than ${cuts}")
endif()
message(STATUS "power cut at each of ${cuts} writes")

# the card is not there; it is put in while the device runs
run(c nocard1 nocard1)
must_have(test.finished)
must_have(power.reboot)
run(c nocard2 nocard2)
must_have(test.finished)
must_have(system.factory_reset_done)
run(c nocard3 nocard3)
must_have(test.finished)

# the card is not there; the next boot has it
run(d nocard1 nocard1)
must_have(power.reboot)
run(d noboot2 noboot2)
must_have(test.finished)
run(d noboot3 noboot3)
must_have(test.finished)

# the card says no to being cleared: not called done, it stays to do
run(f stuck1 reset1)
must_have(power.reboot)
execute_process(COMMAND chmod u-w ${OUT}/f/sd/content/.m/owner/stuck)
run(f stuck2 reset2)
must_have(test.finished)
must_have(system.factory_reset_done)
execute_process(COMMAND chmod u+w ${OUT}/f/sd/content/.m/owner/stuck)
run(f stuck3 reset2)
must_have(test.finished)

# the family's table cannot be read at first; when it can again, everything starts over
run(e table1 table1)
must_have(test.finished)
run(e table2 table2 INK_TEST_TABLE_FAILS=2)
must_have(test.finished)
must_have(members.table_back)
must_have(power.reboot)
run(e table3 table3)
must_have(test.finished)
