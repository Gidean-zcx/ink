# System upgrades, boot after boot (tests/integration/update.c): the same flash image and card through all the phases.
# SIM, PROFILE, ASSETS, CONTENT, OUT, SRC, PY, TOOL.
file(REMOVE_RECURSE ${OUT})
file(MAKE_DIRECTORY ${OUT}/sd/content/update)
set(img ${OUT}/user.img)

function(pkg name)
  execute_process(COMMAND ${PY} ${TOOL} ${ARGN} RESULT_VARIABLE rc ERROR_VARIABLE e)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "package ${name}: ${e}")
  endif()
endfunction()

# what the hub offers
pkg(hub ${OUT}/hub.inkfw --version 2.0.0 --code 200 --app-size 1048576 --font 4096 --resources 8192)

# How a run has to end: restart (the device restarted), off (the power went), done (the test got to the end),
# gives_up (the new system did not hold, and restarted by itself soon after the panel first drew, before the test could check).
function(run phase expect)
  set(envs INK_UPDATE_PHASE=${phase} INK_SIM_UPDATE_PKG=${OUT}/hub.inkfw ${ARGN})
  execute_process(COMMAND ${CMAKE_COMMAND} -E env ${envs}
                          ${SIM} --profile ${PROFILE} --script ${SRC}/update_${phase}.script --budget ${SRC}/update.budget.json
                          --assets ${ASSETS} --content ${CONTENT} --flash-image ${img} --sd-dir ${OUT}/sd --out ${OUT}/${phase} --quiet --no-random-outages
                  RESULT_VARIABLE rc ERROR_VARIABLE e)
  file(READ ${OUT}/${phase}/metrics.json m)
  if(NOT rc EQUAL 0)
    string(REGEX MATCH "\"warnings\":[^]]*]" w "${m}")
    message(FATAL_ERROR "${phase} failed (${rc})\n${w}\n${e}")
  endif()
  string(REGEX MATCH "\"power.reboot\":[ \t]*1" restarted "${m}")
  string(REGEX MATCH "\"test.finished\":[ \t]*1" finished "${m}")
  if((expect STREQUAL "restart" OR expect STREQUAL "gives_up") AND NOT restarted)
    message(FATAL_ERROR "${phase} should have ended in a restart")
  endif()
  if(expect STREQUAL "gives_up")
    string(REGEX MATCH "\"update.self_check_failed\":[ \t]*1" gave_up "${m}")
    if(NOT gave_up)
      message(FATAL_ERROR "${phase}: the self-check should have failed")
    endif()
    return()
  endif()
  if(expect STREQUAL "off" AND restarted)
    message(FATAL_ERROR "${phase} should have ended in a power cut, not a restart")
  endif()
  if(NOT finished)
    message(FATAL_ERROR "${phase} did not get to the end of its test")
  endif()
endfunction()

run(ota1 restart)
run(ota2 done)

# bad packages next to a good one; the bad section is only found when its hash is read: not the newest, or it would be the one offered
set(u ${OUT}/sd/content/update)
pkg(a ${u}/other-product.inkfw --version 9.0.0 --code 900 --product some-other-device)
pkg(b ${u}/bad-signature.inkfw --version 9.0.0 --code 900 --corrupt-signature)
pkg(c ${u}/bad-header.inkfw --version 9.0.0 --code 900 --corrupt-header)
pkg(d ${u}/older.inkfw --version 0.9.0 --code 90)
pkg(e ${u}/needs-newer.inkfw --version 9.0.0 --code 900 --min-from 500)
pkg(f ${u}/too-big.inkfw --version 9.0.0 --code 900 --app-size 2200000)
pkg(g ${u}/good-3.inkfw --version 3.0.0 --code 300)
pkg(i ${u}/bad-section.inkfw --version 2.5.0 --code 250 --corrupt-section)
file(WRITE ${u}/not-a-package.inkfw "this is not an upgrade package, it only has the name of one")
run(card1 restart)
run(card2 gives_up INK_TEST_SELF_CHECK=fails)
run(card3 done)

file(REMOVE ${u}/good-3.inkfw ${u}/other-product.inkfw ${u}/bad-signature.inkfw ${u}/bad-header.inkfw ${u}/older.inkfw ${u}/needs-newer.inkfw ${u}/too-big.inkfw ${u}/bad-section.inkfw ${u}/not-a-package.inkfw)
pkg(h ${u}/good-4.inkfw --version 4.0.0 --code 400 --app-size 524288)
run(cut1 off)
run(cut2 restart)
run(cut3 off INK_TEST_SELF_CHECK=hangs)
run(cut4 done)

file(REMOVE ${u}/good-4.inkfw)
pkg(j ${u}/good-5.inkfw --version 5.0.0 --code 500)
run(hang1 restart)
run(hang2 restart INK_TEST_SELF_CHECK=hangs)
run(hang3 done)

file(REMOVE ${u}/good-5.inkfw)
pkg(l ${u}/good-7.inkfw --version 7.0.0 --code 700)
run(swap1 done)
run(swap2 done)
file(REMOVE ${u}/good-7.inkfw)
pkg(k ${u}/good-6.inkfw --version 6.0.0 --code 600 --font 4096 --resources 409600)
run(res1 restart)
run(res2 off)
run(res3 done)

# AUD16-004, 005: writing the resources fails, or the package has changed since the restart: they stay to be done, nothing bad goes in.
pkg(good7 ${u}/good-7.inkfw --version 7.0.0 --code 700 --font 4096 --resources 409600)
run(rfail1 restart)
run(rfail2 done INK_SIM_OTA_RESOURCE_FAILS=8192)
pkg(bad7 ${u}/good-7.inkfw --version 7.0.0 --code 700 --font 4096 --resources 409600 --corrupt-last-section)
run(rbad done)
pkg(other7 ${u}/good-7.inkfw --version 8.0.0 --code 800 --font 4096 --resources 409600)
run(rother done)
pkg(good7again ${u}/good-7.inkfw --version 7.0.0 --code 700 --font 4096 --resources 409600)
run(rgood done)
run(rdone done)

# On a new device, as it were: a download the power cut short.
file(REMOVE_RECURSE ${OUT}/sd)
file(MAKE_DIRECTORY ${OUT}/sd/content)
file(REMOVE ${img} ${img}.ota)
run(resume1 off)
run(resume2 done)

# Again: by itself when that was asked for, and only on Wi-Fi.
file(REMOVE_RECURSE ${OUT}/sd)
file(MAKE_DIRECTORY ${OUT}/sd/content)
file(REMOVE ${img} ${img}.ota)
run(auto_wifi done)
file(REMOVE_RECURSE ${OUT}/sd)
file(MAKE_DIRECTORY ${OUT}/sd/content)
file(REMOVE ${img} ${img}.ota)
run(auto_mobile done)
