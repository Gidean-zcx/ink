# font-pack-1 报告

（子代理不能写报告文件，由主会话照它交回的原文存进分支。）

# font-pack-1 报告：字形无损压缩，字体分区腾出约 2 MB，打包不再断崖

（写 .md 被拒，报告放在这里。）

## 结论
- 所有字体（正文 sans-44、中等 ui-1-36、粗体 ui-2-53、6 套 Outfit 数字）改成逐字形压缩（INKF v2）。合计 4,713,711 → 2,669,916 字节（少 43%）。【实测】
- font_data 装 2,670,864 / 4,718,592，剩 2,047,728（1.95 MiB；改前剩 1,483）。【实测，Mac】
- 加 almanac-2 的 8 个字和 50 个界面没用过的 GB2312 字，所有字体仍在 font_data，剩 2,012,872；不压缩要 4,776,411，放不下。【实测，font-pack-overflow】
- 屏幕上的字不变：9 套 9,665 个字形解出来和原位图逐字节相同；截图测试全过；S31 模型上同一页输出逐字节相同。
- 速度：S31 模型上解码每字多约 1,500 周期（4.7 µs），同时阅读页把逐像素除法换成查表，净省 27%；A8 省 68%；缩放省 2.4%。perf-latency 回归模式整页绘制 −4.1%。【ISS 估算 + 模拟器】
- 打包：font_data 只放字体，每个放得下的都进去，前面有放不下的也一样；放不下的进程序区并在构建打印名字和大小。图标、词库、示例文本照旧在程序区（和简报字面不同，见下）。

## 每套字体前后（字节）
| 字体 | 改前 v1 | 改后 v2 | 比例 |
|---|---:|---:|---:|
| sans-44（7,631 字形） | 3,519,177 | 2,112,083 | 60.0% |
| ui-1-36（963） | 299,745 | 187,158 | 62.4% |
| ui-2-53（963） | 655,678 | 313,308 | 47.8% |
| outfit-*-106（3 套） | 49,608 | 15,353 | 30.9% |
| outfit-*-211（3 套） | 189,503 | 42,014 | 22.2% |
| 合计 | 4,713,711 | 2,669,916 | 56.6% |
索引（16 字节/字形）没压缩，sans-44 的索引约 122 KB。

## 压缩方法
- 每个字形单独压缩、单独解码。格式写在 tools/fontgen/fontgen.py 文件头：字形的 2 bpp 像素按行连起来（不补齐），相同值的一段写成 4 位一组：vvnn = nn 个（1~3）值为 vv 的像素；nn=0 表示 4 个加后面几组之和，遇到第一个小于 15 的组为止；单出的半字节补 0。
- 文件头版本 2；索引条目原来的保留字节里放压缩后字节数（u16）。只有 2 bpp 写 v2，其它位深照旧 v1。
- 试过"和上一行异或再压"，能再小 10~25%，但要回看上一行，S31 上慢，没用。

## 运行时（system/text/ink_font.c）
- 字体照旧整份映射在闪存，不解压到内存。
- ink_font_pixels()：把一个字形解成每像素一个字节的覆盖度，放进一个 65 KB PSRAM 缓冲（INKF 最大字形 255×255 再加 16 字节余量），下一个字形覆盖；只在画字的任务（LVGL 和阅读页都在 UI 任务）里用。短段写固定 16 字节、下一段盖掉，少分支。
- ink_font_alpha()：整字形转 LVGL A8（同一个 ink_font_darken，先算 4 格表）；lv_get_bitmap 和 theme.c 原尺寸路径用它。
- theme.c 缩放路径：改读 ink_font_pixels() 的字节，算法不变。
- system/text/layout.c（阅读页贴字）：简报"只改"列表没有它，但它直接读字形位图，不改就会把压缩数据当位图画，所以改了，并换成查表。
- v1 照样能读。删了 ink_font_touch_bitmap()，模拟器闪存缓存计费改在 ink_font_pixels() 里按压缩后字节数记。

## 打包（tools/asset_pack.py）
- 按文件名顺序，每个字体放得下就进 font_data，前面有放不下的也不影响后面。
- 放不下的字体进程序区（不压缩），构建打印：`fonts that did not fit in font_data, now in the app image: fonts/xxx.inkf N bytes`。
- 图标、词库、示例文本：照旧在程序区，压缩。

## 和简报不一致的
简报第 3 条字面是"后面放得下的照样进 font_data"。我做成"后面放得下的字体照样进，图标、词库、示例文本留在程序区"。原因：
1. 系统升级（update_service.c 958 行起）先换程序、自检过了才写字库：新程序第一次启动时 font_data 还是旧的。图标若搬进 font_data，新程序找不到图标，icons.c 会 osal_panic，自检失败，永远回退。
2. trial_run.py 的 --fonts 是可选的，只装程序时同样缺图标。
3. 腾出的空间是留给加字的；按字面会被图标（约 0.7 MB）和词库（约 0.97 MB）占掉，只剩约 0.35 MB，下次加字时词库又被挤回程序区（还要解压进 PSRAM 多占约 1 MB）；Mac 和 Linux 字体大小不同，两边放进去的东西会不一样。
4. integration-asset-pack、ime-lexicon 按"图标、词库在程序区、解压进 PSRAM"写的，按字面要改。
要用户定：要不要也把图标和词库搬进 font_data（程序镜像约少 780 KB；代价见上）。我建议先不搬。

## 正确性（主机测试）
| 测试 | 查什么 | 结果 |
|---|---|---|
| font-pack-glyphs | 每套发货字体每个字形：条目、像素、A8、v1 读法都和 fontgen 同一次渲染写出的不压缩版（build/assets-raw/fonts）逐字节相同；闪存计费等于压缩字节数；每个字形的数据刚好用完 | 9 套 9,665 字形全同 |
| font-pack-glyphs-reverse | 构建时从 ink_font.c 生成去掉解码的版本（压缩数据当位图），同一比较必须失败 | 失败，12,977,627 处不同 ✔ |
| font-pack-overflow | 中等和粗体加 58 个新字重生成：字形数确实多 58；全部字体在 font_data、剩 ≥1 MiB；不压缩放不下；再把 font_data 缩到 sans-44 放不下，后面的 ui-1-36、ui-2-53 仍进去，构建只点名 sans-44 | 通过，剩 2,012,872 |
| unit-asset-pack 新增 test_fonts_after_one_that_does_not_fit_still_go_in | 合成数据：中间一个放不下，后面照样进 | 通过 |
反向对照（手工）：split() 改回"一个放不下后面全挤出"，font-pack-overflow 失败（三套 CJK 全被挤出），新单元测试失败；改回后通过。

## 性能
S31 模型（tools/iss，docs/tasks/font-pack-1/iss/run.sh；600 字，字体在闪存、冷缓存，代码热；改前用 fe5938e3 的循环原文和 v1 字体）：
| 每字形周期 | 改前 | 改后 | 变化 |
|---|---:|---:|---:|
| 阅读页贴字 | 59,542 | 43,499 | −26.9% |
| 其中只算解码 | | | +1,513（约 4.7 µs） |
| LVGL A8 原尺寸 | 99,675 | 31,814 | −68.1% |
| 缩放到 38 px | 436,164 | 425,692 | −2.4% |
perf-latency 回归模式：reader.page_render_ms 26.828→25.741（−4.1%），page_readable 710.16→710.03，action_to_visible 669.13→668.77，tap_first_change 224.56→224.20，打开各页不变。
主机原生：阅读页每字慢 16~26%（主机流水线深，分支猜错贵），A8 快约 40%，缩放快约 5%。模拟器"实测 CPU"模式会显得阅读页变慢；这台机器负载高，实测模式三次在 24~48 ms 跳，这次不能下结论。S31 以 ISS 为准，上机时可顺手看一下翻页的 reader.page_render_ms。

## S31 固件
build.sh 通过；ink-s31.bin 4,486,032 字节，应用分区 5,242,880，剩 756,848（14%）；font_data 2,670,864 / 4,718,592；日志有 "factory screen output layer @690a023"，没有"没有屏幕输出层"；改动文件无新警告。gcccheck：ink_font.c、theme.c、layout.c、tests/font_pack/compare.c 全过。

## 装机和退回
- 装新版：新固件读得了旧格式；图标、词库仍在程序区；只装程序不换 font_data 也能跑（字体是旧的未压缩版）。flash_plan.py trial 和 trial_run.py --image … --fonts … 本来就成对写，不用改。
- 退回旧试用版：旧程序只认 v1，font_data 是 v2 时所有字体打不开，界面没有字。退回时必须连同那一版的 ink-fonts.bin 一起写：trial_run.py --image <旧 ink-s31.bin> --fonts <旧 ink-fonts.bin>。--restore-only 从整片备份写回 font_data，不受影响。
- 工具要不要改：现在能用，但 --fonts 可选，退回时漏写就没字。建议（没做，等决定）：给了 --image 就必须给 --fonts；ink-hw/trial-*/delivery/ 每一版连同 ink-fonts.bin 一起保存。

## 全量 ctest
"99% tests passed, 5 tests failed out of 453"。boot-flow（restore 段）和 ai-history-photo（photo 段）是时序竞争：前者同一二进制换回旧字库包通过、另一份干净目录同代码同新包也通过；后者在未改的 fe5938e3 上只给 ink_font 结构体加一个 bool 就同样失败（缩略图回复时文件修订号为 0，gallery 丢弃回复）。autotest-cap 在 fe5938e3 原样也失败。unit-s31-cell-negative / uhci-negative 是满载下模组 ACK 超时，单独重跑通过。都没改（改测试等待属于改时间让它过）。

## 改过的已有测试
ports/esp32s31/tests/asset_pack_test.py（unit-asset-pack）：
- test_compressed_entries_roundtrip_and_first_pack_stays_unchanged 改名 …_and_icons_stay_in_the_image：后半段原来要求"空间够时图标进 font_data"，现在要求"空间够也进程序区"，两个包都逐字节比较。
- test_state_icon_filter_is_the_shipping_filter：原来在 font_data 找图标名，现在要求 font_data 为空、程序区包条目正好是那 3 个图标（整表相等）。
- test_original_sample_is_packed：示例文本现在在程序区、压缩，逐字节比较并解压后找 CC0 声明。
- 新增 test_fonts_after_one_that_does_not_fit_still_go_in。
没有放宽、跳过或改时间的测试。

## 新增
- tests/font_pack/（比较程序、驱动、反向对照生成脚本），CMake 3 个测试。
- 构建多写一份不压缩字体到 build/assets-raw/fonts/（fontgen/ui_fonts 的 --raw-out，测试用，不打包）；ui_fonts.py --extra（测试加字用）。
- docs/tasks/font-pack-1/iss/：S31 模型和主机上的测量程序。

## 没做的 / 要用户定
- 图标、词库要不要也搬进 font_data（建议不搬）。
- trial_run.py 是否强制成对写（建议要）。
- boot-flow、ai-history-photo 两个时序竞争要不要另开任务修产品里的竞争。
- 没上机：像素不变，不用看屏；解码耗时是 S31 模型估算。

## 复核修复（两路审查后的修复轮，修复代理交回的原文）

## Summary of the review fixes (font-pack-1)

Worktree: .claude/worktrees/wf_6cea0dde-754-1 (branch worktree-wf_6cea0dde-754-1). Nothing was pushed, and the device was not touched.
Last commit: `0ecfc22f trial_run: --image needs --fonts, the pack built with that image`. The commit before it is `ed23acf2 Fonts: a v1 face opens only at 1, 2, 4 or 8 bpp (the coverage tables have 256 entries)`.

Full ctest line, verbatim: `99% tests passed, 3 tests failed out of 454` (boot-flow, ai-history-photo and autotest-cap fail; this run was after both fixes, with build/test-out cleared and cmake re-run before it).

- **Fixed, problem 1 (a bad bpp in a v1 header overflows a stack array): real.** ink_font_open now rejects a v1 bpp other than 1, 2, 4 or 8; v2 must still be 2. A new check in tests/font_pack/compare.c sets the bpp byte of a v1 face and a v2 face to every value from 0 to 255. Only the valid values may open, and a face that opens draws every glyph. Reverse check: the same test against the old ink_font.c (ASan + UBSan build in the scratchpad) reports "INKF v1 at 0 bpp: opened", a division by zero, "shift exponent -1", and `AddressSanitizer: stack-buffer-overflow ... WRITE of size 1 #0 ink_font_alpha ink_font_old.c:133`, then exit 134. With the fix the run is clean. Under ASan, all 9 shipped faces still match byte for byte.
- **Fixed, problem 2 (old firmware and a new font pack paired by mistake): real.** In tools/device/trial_run.py, `--image` now requires `--fonts`. Otherwise the tool stops with exit code 2 before reading or writing anything; if an ink-fonts.bin sits beside the image, the message names it. `--restore-only` is unchanged. New test trial-run-fonts (tests/hw_selftest/trial_run_test.py) passes, and fails against the old tool.
- **Problem 3 (full ctest fails): real, but only partly fixable here. Not all fixed.** Of the 5 failures in the earlier report:
  - unit-s31-cell-negative and unit-s31-uhci-negative pass in this run.
  - autotest-cap and ai-history-photo existed before this task. They fail the same way with the pre-task font pack layout, and they also fail in other worktrees without this task's commits; the main build already failed autotest-cap at 14:27.
  - boot-flow is set off by this task, but it is a timing race that was already in the test. It needs the orchestrator or user to decide; I did not change the test or the product.

## Changed tests (all new checks; no existing check was loosened or re-timed)
1. tests/font_pack/compare.c: added `headers()`, the 0..255 bpp check above, and an in-memory asset for it.
2. tests/hw_selftest/trial_run_test.py: new file, registered as trial-run-fonts in CMakeLists.txt.

For the boot-flow experiment I temporarily edited tests/boot_flow.c, then restored it with `git checkout` and rebuilt sim-boot-flow; it is not committed.

## Checks
- **gcc check:** `gcccheck: 4 compiled, 0 with findings` (ink_font.c, layout.c, theme.c, tests/font_pack/compare.c).
- **Affected tests:** font-pack-glyphs, font-pack-glyphs-reverse, font-pack-overflow and trial-run-fonts: 4/4 passed.
- **S31 build:**
  - The log contains "factory screen output layer @690a023" twice and "没有屏幕输出层" zero times; 0 compiler warnings.
  - ink-s31.bin is 4,486,048 B (0x4473a0). App partition headroom is 0xb8c60 = 756,832 B (14%).
  - font_data use is 2,670,864 / 4,718,592 B (2,047,728 B left); the embedded pack is 779,690 B.
- build-s31-own is deleted; build-asan was never created; build is kept.
- **Full ASan build:** not run. The memory checks for this fix used the ASan harness in the scratchpad.

---

# font-pack-1 report (updated)

The first report was handed in as structured output and was not in the worktree. The version below is rebuilt from the commit messages and this session's measurements; the numbers are the same.

## What was done
1. **Lossless glyph compression (INKF v2).**
   - fontgen writes every 2 bpp face as v2: each glyph is compressed on its own, as runs of equal 2-bit values packed in nibbles.
   - ink_font.c reads both v1 and v2. `ink_font_pixels()` unpacks one glyph into a PSRAM buffer (65 KB, the largest glyph INKF can hold).
   - The reader's page blit, LVGL's A8 path and the theme's scaled path all read that buffer. Fonts stay mapped in flash and are never unpacked as a whole.
2. **Every font is compressed** (sizes in bytes):

   | Face | Before | After |
   |---|---|---|
   | sans-44 (body text) | 3,519,177 | 2,112,083 |
   | ui-1-36 (medium) | 299,745 | 187,158 |
   | ui-2-53 (bold) | 655,678 | 313,308 |
   | Six Outfit digit faces | 239,111 | 57,367 |

   font_data now holds 2,670,864 of 4,718,592 B, leaving 2,047,728 (it was 1,483).
3. **No more cliff when packing.** In `split()`, each font that fits goes into font_data, even after one that did not fit. A font that does not fit goes into the app image, and the build names it with its size. Icons, the word list and the sample stay in the image, compressed.

## Acceptance
1. **font_data has ≥ 1 MB left:** 2,047,728 B measured. The test adds almanac-2's 8 new characters plus 50 GB2312 characters the UI does not use yet. Every font stays in font_data with 2,012,872 B left; uncompressed, the same faces would need 4,776,411 B and would not fit. A face that does not fit no longer pushes the ones after it out (test font-pack-overflow).
2. **Glyph-by-glyph comparison:** every glyph of all 9 faces matches the uncompressed copy byte for byte (font-pack-glyphs). The UI screenshot tests pass and the pixels do not change (boot-flow's fresh case: all 11 frames verified).
   - **Full ctest: not met.** It ends `99% tests passed, 3 tests failed out of 454`; see the review section below.
   - **S31 build:** passes with no new warnings.
   - **gcc check:** passes.
3. **Reverse checks:**
   - With the unpacking removed, the glyph comparison fails (font-pack-glyphs-reverse).
   - With the old `split()`, the "50 more characters" test fails.
   - New in this round: with the old header check, the bpp test fails and ASan reports the stack overflow.
   - New in this round: with the old trial_run, trial-run-fonts fails.
4. **Install and rollback:** new firmware reads old v1 fonts. Old firmware cannot read v2: it falls back to Montserrat and shows no Chinese, and it looks for icons in font_data. trial_run therefore now requires the font pack with the image. Rolling back to an old trial build needs that build's own ink-fonts.bin. Older delivery folders without one (for example 2026-11-10) can no longer be installed with --image alone, and the tool says why. OTA cannot rewrite font_data on the S31 (hal_ota_write_resource is a stub), so OTA cannot repair a mismatch either.

## Performance
On the S31 model (tools/iss), unpacking a 44 px glyph costs about 1,500 cycles. Per glyph, the reader's page blit is 27% cheaper, the A8 path 68% cheaper and the scaled path 2.4% cheaper, because a 4-entry lookup table replaces a division per pixel. Outputs are byte for byte identical. The simulator still records the `glyph.blit` cost. Script: docs/tasks/font-pack-1/iss/run.sh.

## Product code size
Budget is 250 lines. Changed so far:

| File | Added | Removed |
|---|---|---|
| font.h | 8 | 3 |
| ink_font.c | 51 | 21 |
| layout.c | 7 | 8 |
| theme.c | 4 | 10 |
| asset_pack.py | 22 | 7 |
| trial_run.py (outside the brief's file list; changed because of review finding 2) | 19 | 9 |

## Review fixes (复核修复)
**1. A bad bpp in a v1 header (major): real, fixed (ed23acf2).**
- The old check tested the bpp byte only for v2. A v1 header with bpp ≥ 9 overflowed the 256-entry tables in ink_font_alpha and in the reader's blit (a stack write).
- bpp 3, or above 8, gave out-of-range shifts; bpp 0 divided by zero.
- Now only 1, 2, 4 or 8 bpp opens for v1, and only 2 for v2. The test and the reverse check are described above.

**2. Firmware and font pack paired wrongly (major): real, fixed (0ecfc22f).**
- `trial_run.py --image` without `--fonts` left whatever font_data the device held.
- Now the tool stops before reading or writing anything and names the ink-fonts.bin beside the image. `--restore-only` does not change.
- I did not add a format marker to the pack: that would be a new mechanism the brief does not ask for. Requiring the pack built with the image is enough, and passing a different pack on purpose remains possible.

**3. Full ctest does not pass (major): real. Partly explained, not fully fixed.** This run: `99% tests passed, 3 tests failed out of 454`.

- **unit-s31-cell-negative and unit-s31-uhci-negative:** pass this time. The earlier failures were with 5 builders running at once.
- **autotest-cap** (check "camera script returns home", tests/device_autotest/sim.c:32):
  - It also fails with the pre-task font pack layout (v1 fonts, old `split()`).
  - It fails in other worktrees without this task's commits: wf_7b1bb08e-589-33, wf_fa602b56-3c7-8, -13 and -14.
  - The main build already failed it at 14:27, before this task.
  - Not caused by this task.
- **ai-history-photo** (photo case: "simulated hub validates received gray attachment", test.c:158):
  - It also fails with the pre-task pack layout, and in worktree wf_fa602b56-3c7-6, which does not have this task.
  - Not caused by this task.
- **boot-flow** (restore case: "arrive at BootWifi (actual BootClaim)", boot_flow.c:267):
  - **Set off by this task.** It fails every time with the v2 fonts, whether packed with the old or the new `split()`. With v1 fonts it passes, whichever split is used.
  - **Cause, found by experiment:** after 重新加入, the product's first page jumps to the join-link page by itself when a claim arrives, about 1.4 s later. That is by design: phone setup goes the same way.
  - The test taps 在设备上设置 right after the first page settles, so it only passes if its tap beats the jump. Before this task it won by about 80 ms.
  - Fewer flash reads with v2 shift the simulated timing (show(FIRST) takes about 190 ms longer at that point), and the tap now lands on the join-link page.
  - **Proof that the race was already there:** with the pre-task pack and the test waiting 2 s before the tap (a temporary edit, removed afterwards), boot-flow fails in both the fresh and restore cases ("visible product control: 在设备上设置", then actual BootClaim).
  - Fixing it needs either a product change (on the first page, do not jump while the user is about to tap) or a change to what the test asserts. Both are outside font-pack-1's scope, and the rules forbid re-timing or loosening a test. **This needs a decision by the orchestrator or user.**

Pixels on screen are unchanged: boot-flow's fresh case verified all 11 frames, and every font comparison is identical.
