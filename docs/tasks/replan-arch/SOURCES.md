# 源码证据索引

查阅：2026-11-12。O 为本项目基线，C 为 CrossPoint，K 为 kiikoread，F 为固定 SDK，M/X 为旁系。范围为本次引用的位置；哈希对应整个文件，不代表逐行审计了整个文件。完整提交、SHA-256 与接口枚举见 [source-evidence.json](source-evidence.json)。

| 编号 | 固定源码与行号 |
| --- | --- |
| O01 | [ink:system/include/ink/sys/ui.h，L36–52, 71–147](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/system/include/ink/sys/ui.h#L36) |
| O02 | [ink:system/include/ink/sys/appreg.h，L25–99](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/system/include/ink/sys/appreg.h#L25) |
| O03 | [ink:system/include/ink/sys/cap.h，L64–138, 155–195](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/system/include/ink/sys/cap.h#L64) |
| O04 | [ink:system/include/ink/sys/ui_semantics.h，L10–56](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/system/include/ink/sys/ui_semantics.h#L10) |
| O05 | [ink:system/include/ink/sys/file.h，L1–14, 74–102](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/system/include/ink/sys/file.h#L1) |
| O06 | [ink:system/include/ink/sys/members.h，L35–64](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/system/include/ink/sys/members.h#L35) |
| O07 | [ink:system/ui/ui_task.c，L190–225, 424–455, 900–922, 37–37](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/system/ui/ui_task.c#L190) |
| O08 | [ink:system/services/display/refresh_policy.c，L94–168](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/system/services/display/refresh_policy.c#L94) |
| O09 | [ink:system/services/display/refresh_policy.h，L12–16](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/system/services/display/refresh_policy.h#L12) |
| O10 | [ink:system/include/ink/sys/display.h，L23–52](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/system/include/ink/sys/display.h#L23) |
| O11 | [ink:system/ui/app_packages.c，L122–153](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/system/ui/app_packages.c#L122) |
| O12 | [ink:apps/reader/reader.c，L601–624, 994–1014, 1066–1073](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/apps/reader/reader.c#L601) |
| O13 | [ink:system/include/ink/sys/i18n.h，L2–23](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/system/include/ink/sys/i18n.h#L2) |
| O14 | [ink:ports/esp32s31/partitions.csv，L1–10](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/ports/esp32s31/partitions.csv#L1) |
| O15 | [ink:docs/hardware/flash-layout.md，L7–23](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/docs/hardware/flash-layout.md#L7) |
| O16 | [ink:system/apps/remote/remote.c，L17–20, 157–205, 540–555](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/system/apps/remote/remote.c#L17) |
| O17 | [ink:system/services/display/display_service.c，L210–225](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/system/services/display/display_service.c#L210) |
| O18 | [ink:simulator/profiles/metalio-plus.json，L30–42](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/simulator/profiles/metalio-plus.json#L30) |
| O19 | [ink:tests/sdk_boundary.cmake，L1–32](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/tests/sdk_boundary.cmake#L1) |
| O20 | [ink:CMakeLists.txt，L303–325](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/CMakeLists.txt#L303) |
| O21 | [ink:docs/tasks/fw-size-1/REPORT.md，L9–48](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/docs/tasks/fw-size-1/REPORT.md#L9) |
| O22 | [ink:docs/tasks/fw-size-1/RESOURCE-PLAN.md，L5–18](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/docs/tasks/fw-size-1/RESOURCE-PLAN.md#L5) |
| O23 | [ink:ports/esp32s31/vendor/metalio_display/SOURCE.md，L1–7](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/ports/esp32s31/vendor/metalio_display/SOURCE.md#L1) |
| O24 | [ink:system/policy/cap.c，L86–145](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/system/policy/cap.c#L86) |
| O25 | [ink:docs/tasks/picks-status.md，L1–8](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/docs/tasks/picks-status.md#L1) |
| O26 | [ink:docs/tasks/reuse-crosspoint-1/REPORT.md，L94–98](https://github.com/Gidean-zcx/ink/blob/d09b79195e2dc57908e33219216048e529e082c6/docs/tasks/reuse-crosspoint-1/REPORT.md#L94) |
| C01 | [crosspoint-reader:src/activities/Activity.h，L16–73](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/activities/Activity.h#L16) |
| C02 | [crosspoint-reader:src/activities/ActivityManager.h，L24–68, 76–135](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/activities/ActivityManager.h#L24) |
| C03 | [crosspoint-reader:src/activities/ActivityManager.cpp，L40–70](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/activities/ActivityManager.cpp#L40) |
| C04 | [crosspoint-reader:lib/GfxRenderer/GfxRenderer.h，L31–83, 235–275, 103–223](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/lib/GfxRenderer/GfxRenderer.h#L31) |
| C05 | [crosspoint-reader:lib/GfxRenderer/GfxRenderer.cpp，L170–178, 1772–1795, 2420–2480, 2483–2556](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/lib/GfxRenderer/GfxRenderer.cpp#L170) |
| C06 | [crosspoint-reader:lib/hal/HalDisplay.h，L27–129](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/lib/hal/HalDisplay.h#L27) |
| C07 | [crosspoint-reader:src/MappedInputManager.h，L18–131](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/MappedInputManager.h#L18) |
| C08 | [crosspoint-reader:lib/hal/HalStorage.h，L24–76, 97–129](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/lib/hal/HalStorage.h#L24) |
| C09 | [crosspoint-reader:lib/I18n/I18n.h，L18–40](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/lib/I18n/I18n.h#L18) |
| C10 | [crosspoint-reader:src/components/UITheme.h，L23–43](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/components/UITheme.h#L23) |
| C11 | [crosspoint-reader:src/components/UiAppHost.h，L22–75](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/components/UiAppHost.h#L22) |
| C12 | [crosspoint-reader:src/activities/network/NetworkModeSelectionActivity.cpp，L22–88](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/activities/network/NetworkModeSelectionActivity.cpp#L22) |
| C13 | [crosspoint-reader:src/activities/reader/EpubReaderActivity.cpp，L1–62, 257–300](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/activities/reader/EpubReaderActivity.cpp#L1) |
| C14 | [crosspoint-reader:docs/webserver-endpoints.md，L70–100](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/docs/webserver-endpoints.md#L70) |
| K01 | [kiikoread:main/app/app.h，L35–205](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/app/app.h#L35) |
| K02 | [kiikoread:main/app/app_registry.c，L10–47](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/app/app_registry.c#L10) |
| K03 | [kiikoread:main/app/app_loop.c，L397–517, 1650–1682](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/app/app_loop.c#L397) |
| K04 | [kiikoread:main/apps/app_book.c，L124–196, 258–274, 1890–1910, 3868–3889, 8449–8461](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/apps/app_book.c#L124) |
| K05 | [kiikoread:main/ui/ui_kit.h，L25–67, 93–117](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/ui/ui_kit.h#L25) |
| K06 | [kiikoread:main/ui/ui_kit.c，L17–29, 82–110](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/ui/ui_kit.c#L17) |
| K07 | [kiikoread:main/book/book_progress.c，L5–33](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/book/book_progress.c#L5) |
| K08 | [kiikoread:main/settings.c，L216–218, 760–764](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/settings.c#L216) |
| K09 | [kiikoread:boards/metalio47/partitions.csv，L1–8](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/boards/metalio47/partitions.csv#L1) |
| K10 | [kiikoread:boards/metalio47/sdkconfig.defaults，L89–95](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/boards/metalio47/sdkconfig.defaults#L89) |
| K11 | [kiikoread:README.zh-CN.md，L3–31](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/README.zh-CN.md#L3) |
| K12 | [kiikoread:main/ui/ui_nav.c，L109–122, 149–175, 190–258](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/ui/ui_nav.c#L109) |
| K13 | [kiikoread:main/book/book_layout.h，L18–102](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/book/book_layout.h#L18) |
| K14 | [kiikoread:main/font/ttf_font.c，L5–14](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/font/ttf_font.c#L5) |
| X01 | [kiikoread-xiugai:main/music/music_player.c，L5–17](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/main/music/music_player.c#L5) |
| X02 | [kiikoread-xiugai:main/music/music_tags.h，L5–35](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/main/music/music_tags.h#L5) |
| M01 | [crossmux:src/activities/apps/standby/StandbyFace.h，L18–59](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/standby/StandbyFace.h#L18) |
| M02 | [crossmux:src/activities/apps/gomoku/GomokuBoard.h，L9–50](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/gomoku/GomokuBoard.h#L9) |
| F01 | [freeink-sdk:libs/ui/FreeInkUI/include/FreeInkUICore.h，L121–145, 709–746, 1013–1038](https://github.com/Free-Ink/freeink-sdk/blob/6a208c0c46cce99bafa334bc4f171f5e558c5195/libs/ui/FreeInkUI/include/FreeInkUICore.h#L121) |
| F02 | [freeink-sdk:libs/ui/FreeInkUI/include/FreeInkApp.h，L875–890, 923–985](https://github.com/Free-Ink/freeink-sdk/blob/6a208c0c46cce99bafa334bc4f171f5e558c5195/libs/ui/FreeInkUI/include/FreeInkApp.h#L875) |
| F03 | [freeink-sdk:libs/ui/FreeInkUI/include/FreeInkUIGfxRenderer.h，L33–99](https://github.com/Free-Ink/freeink-sdk/blob/6a208c0c46cce99bafa334bc4f171f5e558c5195/libs/ui/FreeInkUI/include/FreeInkUIGfxRenderer.h#L33) |
| K15 | [kiikoread:main/book/book_layout.c，L1–21](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/book/book_layout.c#L1) |
