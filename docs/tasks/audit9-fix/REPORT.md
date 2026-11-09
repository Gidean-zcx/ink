# audit9-fix 修复报告

基线：`5f2648809457522b0901366d54e5fa52c18ec9ee`。日期：2026-11-10。
范围仅 B9-01～B9-03；全部验收完成，修改已本地提交，未推送。证据标记：【实测】本轮命令；【实现】代码核查；【估算】。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| B9-01 | 通过 | 【实测】`audit9-bt-ai` 修前 6 条失败，修后通过；AI 关、开、配对连接后的快照和可见标签一致 | `ctest --test-dir build -R '^audit9-' --output-on-failure -j2` |
| B9-02 | 通过 | 【实测】`audit9-ime-layout` 初次 22 条失败；加入固定位置检查后反向 25 条失败，修后通过；三种输入／字号下按钮完整可触摸，长词在展开页完整显示 | 同上 |
| B9-03 | 通过 | 【实测】`audit9-font-coverage` 修前 508 项含缺字，修后 23,783 项全部有字形 | 同上 |
| 全量与内存检查 | 通过 | 【实测】普通与 ASan/UBSan 均 295/295、退出 0；213.20／378.14 秒；无跳过或 sanitizer 诊断 | `ctest --test-dir build --output-on-failure -j2`；内存版改为 `--test-dir build-asan` |
| S31 | 通过 | 【实测】完整编译、链接、尺寸和栈报告生成，脚本退出 0 | `taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 资源与重复生成 | 通过 | 【实测】1,048,549 B；重复生成字节相同；90 条首选 75、前五 83 | `python tests/ime_lexicon/run.py --build build` |

## 没做到的

B9-01～03 的验收范围内无未完成项。未操作设备，未执行 S31 固件自检或原生 Linux CI；本轮证据为模拟器测试及 S31 离线编译。

## 和设计不一致的

三项均按现有合同修复，未修改设计、预算或能力门禁。模拟器截图不代表实机效果。

## 改过的已有测试

未改既有测试的期望、断言或预算；只新增三项 CTest 和手工反向驱动。

## 要用户定的

暂无；沿用任务书给定方案。

## 顺手发现

暂无。

## 新增的机制

无新增产品任务、线程、状态机、文件格式、配置项或抽象层。B9-03 要求的过滤只进入现有宿主生成器，设备继续读取 IKD1；计量仍写已有 JSON。

- `bt-ai-test`：B9-01 要求的有界测试线程，只进入测试二进制。
- `ime-layout-test`：B9-02 要求的有界测试线程，只进入测试二进制。

## 要改的文档

主会话可在蓝牙和词库原任务验收记录里补上本报告的三项回归、字体过滤及新词库计量。本轮仅更新工具 README，不改共享文档。

## 执行记录

- 修前对照：三个新测试全部失败；日志 `build/audit9/ctest-before.log`，截图及指标 `build/audit9/before-bt/`、`before-layout/`。
- 以上及后续构建／测试命令均由 `taskpolicy -c utility` 启动，构建和 CTest 最多 `-j2`；本机路径只留在忽略目录。

## 修复与词库计量

- B9-01【实现】：只在现有 `changed()` 中清除重新开启后的旧关闭提示，保留搜索、错误等其他回执；按钮和 AI 仍走原能力与设置订阅。
- B9-02【实现】：折叠栏先扣除两个固定按钮和间距，再按完整文本宽度保留候选前缀；首候选只扩展空余宽度并保留文字最小宽度，使按钮固定在右侧。其余候选进入现有展开页，展开按内容宽度换行；快照按实际可见对象输出候选、更多和话筒。没有恢复均分裁切。
- B9-03【实现】：生成器复用 `fontgen.charset()`，先按随包字符集过滤，再照原排序、权重和 1 MiB 裁剪规则生成；CMake 加入字符集工具依赖，测试逐字读取实际 INKF 索引核对。
- R-061／G-002【实现＋实测】：未增加服务、能力或跨层依赖；按钮和 AI 共用原路径，页面与键盘快照反映本次验证中的可见状态。

| 指标 | 过滤前 | 过滤后 |
| --- | ---: | ---: |
| 来源文字项（裁剪前） | 348,926 | 342,428（过滤 6,498） |
| 入包文字项 | 24,138 | 23,783 |
| 入包键下词条 | 45,762 | 45,422 |
| 含缺字文字项 | 508 | 0 |
| 文件大小（B） | 1,048,560 | 1,048,549 |
| 90 条首选命中 | 75/90（83.33%） | 75/90（83.33%） |
| 90 条前五命中 | 83/90（92.22%） | 83/90（92.22%） |

【实测】新文件 SHA-256：`c27739624e271c1a12afc97bddafa06dc3e4b915a5d3163e9858a2dc82823cf8`；模拟器加载 879.95 ms、词库 PSRAM 1,048,549 B；固定测试集不代表真实输入分布。前后质量日志分别为 `build/audit9/quality-before.log` 和 `build/audit9/ctest-fix.log` 对应保留的 `ime-lexicon-*/summary.json`。

【实测】已目视核对三种折叠栏和长词展开页截图，见 `build/test-out/audit9-sim-audit9-ime-layout/clean-*.png`；带面板残影的 `snap-*.png` 同目录保留，不将干净渲染当作真屏测量。

- 【实测】修后普通全量构建及 CTest 均退出 0，295/295；日志 `build/audit9/build-full.log`、`ctest-full.log`。
- 【实测】反向驱动退出 0，分别恢复指定基线的两个 UI 对象及未过滤生成器；三个验收均重新失败（子命令退出 1）：蓝牙 6 条、候选栏 25 条、字体缺字 508 项。证据 `build/audit9/reverse/{summary,commands}.json`。仅替换忽略目录中的构建对象，不改产品源码；逆向脚本不注册 CTest，不让浅克隆 CI 依赖历史。
- 【实现】产品源码净增 19 行（蓝牙、键盘、宿主生成器三文件共增加 26、删除 7），低于 120 行；不改排序权重，不加常驻分配。CMake 仅加测试及字符集依赖。

```sh
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" \
  tests/audit9/reverse.py --build build --baseline 5f2648809457522b0901366d54e5fa52c18ec9ee
```

- 【实测】S31 编译脚本退出 0；证据 `build-s31/audit9/build.log`、`build-s31/{size,size-components,stack}.txt`。仅离线构建，未运行固件自检或操作设备。

复现配置（内存版仅将构建目录改为 `build-asan` 并加 `-DINK_SANITIZE=ON`）：

```sh
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_DEPS/build/_deps/sdl2-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src"
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" \
  taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
```

- 【实测】ASan/UBSan 配置、构建及全量 CTest 均退出 0，295/295；证据 `build-asan/audit9/{configure,build,ctest}.log`。实际编译含 `-fsanitize=address,undefined`，仍启用 `INK_WERROR=ON`；两套 `LastTest.log` 的 sanitizer 诊断和跳过标记均为 0，汇总 `build/audit9/final-checks.json`。
- 【实测】两套构建生成的正式词库逐字节一致；既有 `ime-lexicon` 在两套全量中均通过重复生成和缺文件反向检查。
- 【实测】最终基线差异仅 11 个任务相关文件；`git diff --check` 通过。按总规则的隐私正则扫描，本机路径匹配 0，地址匹配 1，唯一匹配为蓝牙测试中允许的假地址；证据 `build/audit9/privacy.{log,json}`。未提交构建产物、设计稿或受限共享文档。
