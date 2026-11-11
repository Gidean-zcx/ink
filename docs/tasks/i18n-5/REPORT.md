# i18n-5 执行报告

基线：`2bbf0302ac9d7bc3c97c06d1e4393fdc59793ae7`。状态：实现及定向验证完成，全量和端口验证进行中。开工时工作树干净；只读依赖主检出的 HEAD 与指定基线一致，无需合并。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过 | 【实测】`i18n-literals` 为 0；临时直接写中文标签时退出 1，恢复后通过；56 帧的新覆盖及英文长回答均 `i18n.missing=0` | 下列定向及反向命令 |
| 2 | 待验证 | 全量、ASan、S31、gcccheck、ui-scale、隐私扫描 | 待补 |
| 3 | 通过 | 【实现】逐文件计数见下；【实测】英文截图经标签尺寸、重叠、越界检查 | `i18n-shell-remaining`、`i18n-apps`、`i18n-memo` |

## 实现和逐文件清单

检查递归扫描 `system/ui/`、`system/apps/`、`apps/` 的 C/H，以及长回答提示头文件；不依赖 Git 历史。连本地变量、数组和组件包装函数中的中文一起检查，并核对已标记键在各语言表中存在。白名单在 [检查源码](../../../tests/i18n_literals.py)：能力／数据／状态定义的 AI 元数据、逐函数列出的 AI 页面描述、源语言比较和解析、逐项说明的持久目录／文件名。没有整文件豁免。新源码自动纳入。

下表为新增 `TR/TR_NOOP` 标记的中文字面量处数，同一行的多个文案分别计数；包括原先显示时已翻译、这次补标记的常量。

| 文件 | 处数 |
| --- | ---: |
| `apps/home/home.c` | 2 |
| `apps/reader/reader.c` | 9 |
| `system/apps/calculator/calculator.c` | 1 |
| `system/apps/calendar/calendar.c` | 36 |
| `system/apps/camera/camera.c` | 1 |
| `system/apps/camera/scan.c` | 3 |
| `system/apps/clock/clock.c` | 8 |
| `system/apps/contacts/contacts.c` | 1 |
| `system/apps/files/files.c` | 44 |
| `system/apps/gallery/gallery.c` | 1 |
| `system/apps/memo/memo.c` | 3 |
| `system/apps/phone/phone.c` | 7 |
| `system/apps/remote/remote.c` | 10 |
| `system/apps/sms/sms.c` | 1 |
| `system/apps/weather/weather.c` | 5 |
| `system/include/ink/sys/ai.h` | 1 |
| `system/ui/file_picker.c` | 1 |
| `system/ui/shell/member_pages.h` | 1 |
| `system/ui/shell/mobile_network.c` | 1 |
| `system/ui/shell/navigation.c` | 149 |
| `system/ui/shell/settings_family.c` | 79 |
| `system/ui/theme.c` | 8 |

另改 `system/services/ai/ai_service.c` 一处截断逻辑：提示先翻译，再按实际字节数留位、复制和终止；保留 UTF-8 整字截断。CMake 补键检查来源、注册静态检查和英文长回答测试。英文表新增 171 键，缩短 5 个既有键和 4 个本次新增键以适应实际宽度。

## 英文截图

均为当前检出的相对路径，不提交图片；同名 JSON 为 AI 快照。

- `build/test-out/i18n-shell-remaining/snap-{SetMembers,FamilyAdd,FamilyAdult,SetKid,Wallpaper,Wallpaper-next,Records,AiMemory,RuntimeRecords,Control,Control-more}-en.png`。
- 同目录 `snap-{SetDisplay,SetSound,SetNetwork}-en-{1,2,3,4}.png`；后续页到末页后保持末页，未声称四张都是不同页。
- 同目录已有 AI、备份、传输、搜索和更新页；本次合计 56 帧，899 条检查，0 排版问题。另有 `build/test-out/i18n-shell/` 的 35 帧。
- 应用英文：`build/test-out/i18n-apps/{apps,memo}/en/snap-*.png`，覆盖日历、文件、时钟、电话等原有截图用例。
- 长回答最后一页：`build/test-out/ai-answer-long-en/snap-AiLongCut.png`，快照 `AiLongCut/snapshot.json`。

## 定向及反向验证

【实测】静态检查临时在 `navigation.c` 增加一个 `lv_label_set_text` 中文字面量，退出 1 并指到该行；恢复后源码哈希一致，检查为 0。日志 `build/verification/literals{,-reverse}.log`。CTest 内另有直接标签、多行按钮、间接数组、相邻字面量、Unicode 转义及白名单不能吞掉同一行第二个标签的反向自检。

【实测】定向复测 `100% tests passed out of 3`（32.98 秒）。长回答的隔离反向对象只撤掉提示翻译，编译和链接退出 0，英文测试退出 1，指定断言 `the last page ends with the note` 失败；未修改产品源码。命令和日志在 `build/i18n-5-reverse/`。

```sh
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build \
  -R '^(i18n-shell-remaining|i18n-shell|ai-answer-long-en)$' --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/i18n_5_reverse.py --build build
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/i18n_literals.py
```

## 没做到的

Linux 原生 CI 未运行；本机为 macOS，另做任务要求的 GCC 检查。全量和内存检查结果待补。

## 和设计不一致的

无设计偏离。固定文案经现有翻译表；用户姓名、正文、文件名、能力 ID、状态比较键和持久路径保持其语义。按钮与 AI 仍经原能力路径，未新增权限或跨成员访问。

## 改过的已有测试

没有删除断言、放宽预算或增加 sleep。

- `i18n_shell_remaining.c` 新增家人、控制中心和三组设置截图；扫描时不重复统计滑块用 `LV_OBJ_FLAG_USER_1` 标注的白色裁剪副本，底层完整标签仍检查尺寸和重叠。
- `i18n_shell.c` 的存储截图先等待实际统计／备份记录加载完成，再沿用原有截图断言和时限；首轮在异步加载重画时出现 `frame presented` 失败，复测通过。
- `ai_answer_long.c` 默认中文期望保留语义：页码改为按固定字段 ID 及 `=` 后数字读取，原来只识别中文前缀；截断提示、未截断检查和面板溢出说明改为当前语言完整译文。新增英文运行和缺译为 0 的断言。

## 要用户定的

暂无。

## 顺手发现

暂无。

## 新增的机制

无新增产品任务、线程、状态机、文件格式、配置项、缓存、队列或重试。静态检查和英文长回答用例由任务书验收 1、2 要求；截图扩展复用现有测试任务。

## 要改的文档

主会话合并后更新任务总表的 G7 英文覆盖状态；共享需求、决策、能力表和 UI 依赖文档未改。

## 规模与收尾

产品代码当前 `+281/-262`，净增 19 行（译文、测试和文档另算），低于净增 300 行预算。超过 100 行的原因是逐处标记 22 个源码文件中的固定文案；更小的做法已采用常量 `TR_NOOP` 加原显示点查表。没有选择在通用标签组件里翻译任意输入，以免把用户内容当键、重复翻译译文或改变状态判断。

全量、ASan、S31、gcccheck 和最终隐私扫描结果待补。
