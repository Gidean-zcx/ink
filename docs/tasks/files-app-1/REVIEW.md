# files-app-1 第 1 轮复查

复查对象：`58510efed669e6ee8b3c331221acc01fc9318bd0`。基线：`3fce2beea07d9e25a814cd24c64e8d5305ea9222`。本轮独立运行命令、检查基线差异及页面图，未改产品和已有测试源码，未提交。

**结论：发现 3 个中级问题，严重 0、高 0。不能判定全部验收通过。** 常规文件操作、重启持久性、缩略图和儿童隔离可复现；目录返回和长目录名的 AI 快照存在缺陷，操作提示被遮挡。普通与 ASan/UBSan 全量均为 232/233，唯一失败是已披露的能力表文档过期。

## 问题

### R1-01｜中｜返回上级目录误用固定字节位置

- 文件和行号：`system/apps/files/files.c:220`。
- 复现命令：下文 P1。创建 `/me/2026/notes`，依次调用 `files.browse` 浏览 `/me`、`/me/2026`、`/me/2026/notes`，再调用 `files.up`。
- 【实测】预期回到 `FileList` 的 `/me/2026`；实际进入 `Files`，路径清空。P1 退出 1，`REVIEW_BACK` 断言失败；证据：`build/files-app-1-review/review/metrics.json`。
- 原因：`end != path + 8` 无条件把第 8 字节处的斜杠当作成员根目录边界，普通四字符一级目录也命中。这不需要并发或故障注入。
- 最小修法：仅在路径确实以 `/members/` 开头时应用该根目录边界；其他子目录照常截掉最后一级。补四字符目录及成员根目录的对照断言。

### R1-02｜中｜四个合法长目录名使生产 AI 快照整体失败

- 文件和行号：`system/apps/files/files.c:122`、`system/apps/files/files.c:124`、`system/apps/files/files.c:263`；影响路径：`system/services/ai/ai_service.c:524`。
- 复现命令：P1。在 `/me/SYNTH_资料目录` 下放四个名字为 `SYNTH_`＋17 个“测”＋数字 0～3 的目录，正常浏览该目录。名字和路径均未超出文件服务限制。
- 【实测】页面有四项，完整快照 **3180 字节**；用生产 **3072 字节**缓冲调用 `ink_ui_context_snapshot` 返回 **-1**。P1 的 `REVIEW_SNAPSHOT` 断言失败，证据同上。
- 【实现】AI 请求因此将 `page_gen` 设为 0，并发送空页面文本、空能力清单。现有 `tests/files_app/files_app.c:52` 只在短英文夹具上检查生产缓冲，漏掉了这条正常目录路径。
- 最小修法：目录的语义字段 ID 和对象引用使用短的行标识，完整路径保留在动作参数中，去掉重复路径；补上述夹具。无需扩大生产缓冲或改预算。

### R1-03｜中｜操作结果提示被“打开”按钮遮挡

- 文件和行号：`system/apps/files/files.c:235`，与 `system/apps/files/files.c:188` 的按钮布局相交。
- 复现命令：P2；也可正常选择 TXT 后点“打开”，查看视觉证据 `build/files-app-1-review/ui-compare/FileActionsPreview.png`。
- 【实测】提示文字为“仅预览文件开头”；LVGL 实际纵坐标是 **961～1003**，后创建的“打开”按钮为 **909～1009**。截图仅残留左侧极小的文字碎片，完整提示不可读。P2 退出 1，唯一失败为 `REVIEW_NOTICE`；证据：`build/files-app-1-review/notice/metrics.json`。
- 结果：`status()` 共用这个提示对象，文件操作的结果／失败信息也会落进按钮区域。现有预览测试核对了 label 文本和内容，未核对提示在最终画面中是否可见。
- 最小修法：在 `create()` 中把操作页提示放到按钮之外的空闲区域，沿用主题尺寸；补一次提示与操作控件不相交的检查，不改公共框架。

## 验收逐条核对

| 编号 | 本轮结论 | 独立证据与命令 |
| --- | --- | --- |
| 1 | 部分通过 | T1：触摸改名、移动，删除确认／取消，AI 不能代人确认，以及第二进程重启均通过；浏览返回有 R1-01。 |
| 2 | 通过本次图片夹具 | T1：真实 PNG 经 `file.thumb` 显示为 L8 图片；去掉像素绑定后 3 项断言失败。 |
| 3 | 通过 | T1：儿童无他人目录，伪造成人目录与文件编号被拒，私有数据不串；撤掉入口检查后 1 项失败，撤掉浏览检查后 1 项失败。 |
| 4 | 部分通过 | 四页都有语义快照；按钮和 AI 共用能力，真实 AI 面板下的翻页通过；长目录名导致 R1-02，结果提示存在 R1-03。 |
| 5 | 未全过；规模通过 | T2、T3 均 232/233，零跳过，唯一失败 `capability-table-doc`；无 sanitizer 诊断。产品与构建接线共 300/350 行。 |
| G4 | 像素目标未达，差异已核查 | T5 四页均超过 12/255；另目视补充状态，总计 13 张对照图，见下表。 |

T1 原场景 **193 项通过**，重启 **34 项通过**，四个独立截图场景各 **194 项通过**，退出均为 0。证据目录：`build/files-app-31j1doez/`。写入／重启的内部 RAM 最低余量分别为 **47,728／47,840 字节**；零告警、零预算违规、零分配失败。

两套构建均退出 0，均启用 `INK_WERROR=ON`；内存构建启用 `INK_SANITIZE=ON`。普通全量 **135.51 秒、退出 8**；ASan/UBSan 全量 **262.88 秒、退出 8**。本轮完整日志保留为 `build/files-app-1-review/ctest.log`、`asan-ctest.log`。未排除失败项。能力表差异包含新增的八项 `files.*`；总规则第 7 节要求共享生成文档由合并主会话更新，故列为待合并时补齐的验收条件。

命令沿用用户给定的 `INK_DEPS`、`INK_DESIGN`。构建和全量测试逐个运行：

```sh
# T0：普通与 sanitizer 构建
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility cmake --build build-asan -j2

# T1：文件场景、重启及四页截图
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/files_app/run.py --build build --visual

# T2：普通全量
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2

# T3：ASan/UBSan 全量
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2

# T4：分别运行下表七个变体
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/files_app/run.py --build build --mutant child

# T5：使用 T1 新生成的四页截图重算对照
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/files-app-compare --scenes Files FileList FileActions FileMove --reuse
```

## 反向对照及测试可信度

七个变体本轮均独立编译、链接成功，运行退出 1，`test.finished=1`，因断言失败而失败。产品文件与 `HEAD` 一致，没有撤改工作树源码。

| T4 变体 | 本轮失败断言数 | 检测到的行为 |
| --- | ---: | --- |
| child | 1 | 儿童入口出现其他成员目录 |
| browse | 1 | 伪造成人目录不再被页面拒绝 |
| thumb | 3 | 列表及操作页不再绑定实际缩略图 |
| rename | 4 | 改名按钮没有调用文件能力，后续路径和编号断言失败 |
| paging | 14 | 翻页未执行，真实 AI 面板和末页断言失败 |
| preview | 2 | 真实文本未写入可见预览控件 |
| ai-panel | 3 | AI 面板打开后无法翻底下的文件页 |

除 child 外的汇总在 `build/files-app-1-review/mutants.json`；child 证据为 `build/files-app-dqxbqfje/write/metrics.json`。这些反例有效，但覆盖不到 R1-01 的父目录边界、R1-02 的长路径快照和 R1-03 的控件遮挡。

已有测试只修改应用数量 11→12、追加八项能力登记；未删测试、放宽预算或断言。新增测试中的延时用于虚拟输入、按条件等页面稳定及截图事件，未发现用固定 sleep 掩盖产品时序故障的改动。

## UI：实际打开并查看的 13 张对照图

开工时 `out/ui-compare/` 不存在。已重建并逐张查看该目录的四个页面及九个补充状态；查看后将本轮对照归入忽略的 `build/files-app-1-review/ui-compare/`，保持源码工作区只有本报告。补充状态由临时驱动在原流程中增加截图点，**233 项通过、零失败、13 次稳定帧捕获**；原始帧和快照在 `build/files-app-1-review/visual/`。

四页 T5 分数与执行报告一致。补充状态沿用同一灰度差算法，对应原四张画板，只用于核查状态和布局。

| 图名／状态 | 参考画板 | 灰度差 /255 | 目视结论 |
| --- | --- | ---: | --- |
| Files | Files | 57.581 | 按任务书显示家庭、我的、家人入口；原稿的容量卡和分类列表未保留，三行独立灰卡之间留白较多。 |
| FileList | FileList | 48.865 | 四行独立卡替代五行分组列表；每行缺少原稿的右箭头，页码条离底部约两百像素。 |
| FileActions | FileActions | 27.000 | 单文件输入框和改名按钮替代多选列表；底部是打开／移动／删除。 |
| FileMove | FileMove | 26.450 | 当前空目录显示内部 `/me/...` 路径；“这里还没有文件”不准确表达仅列目标文件夹的用途。 |
| FileListThumb | FileList | 35.308 | 末页两行，真实全黑 PNG 显示为黑色方块，缩略图确实可见；页码仍不贴底。 |
| FileListLast | FileList | 35.308 | 按键／AI 翻到末页后内容相同；此图与上项相同，不重复算一个独立视觉状态。 |
| FileActionsImage | FileActions | 27.700 | 图片只有 53×53 的缩略图；“打开”仍是缩略图能力，不是完整图片浏览页。 |
| FileActionsPreview | FileActions | 27.443 | 真实文本开头可见；“仅预览文件开头”被打开按钮盖住，见 R1-03。 |
| FileMoveRoots | FileMove | 49.782 | 根入口三张灰卡；尚未选目标时黑色“放到这里”仍显示，外观没有表达动作尚不可用。 |
| FilesChild | Files | 51.396 | 只有家庭、我的两项，无其他成员目录；分页仍显示 1/1。 |
| FileListChild | FileList | 28.626 | 只有儿童自己的文件，未混入成人夹具；大片空白和悬空页码条与原稿不同。 |
| FileActionsShared | FileActions | 25.602 | 儿童查看家庭文件时只有打开，写操作隐藏；文件名可读。 |
| FileListLarge | FileList | 50.078 | 放大字体后仍为四行，文本和副标题未相撞，缩略图正常；页码位置未变。 |

共查看 13 张、12 个不同视觉状态，超过至少 10 屏的要求。字号和灰度采用代码中的当前主题，原稿尚未更新的大小／灰度差异按任务书接受。批量选择、拷贝、新建目录、完整文件打开等差异已由执行报告披露；本轮不把它们误记成伪造验收结果。当前可见中文未发现明显错字；移动页内部路径和空状态用词仍需后续对齐。

## 过度设计与架构审视

【实现】未发现需单列的问题：没有新增产品线程、任务、持久格式、配置、重试队列或通用框架。页面模型在 PSRAM，列表复用 F4；文件查询、权限、变更、作业和删除确认仍归既有服务，LVGL 操作在 UI 执行器。八项 `files.*` 是本任务需要的导航／输入／预览动作；改名、移动、删除直接复用文件能力。

可删内容是 R1-02 中目录语义 ID／对象里反复保存的完整路径。这三个局部问题采用现有布局和能力即可修复，无需新增机制。保留页面可见性检查与服务访问控制，两者各自承担隐藏不可用入口和拒绝越权访问的职责。

## 新问题的复现入口

P1、P2 是本轮在原测试驱动上追加操作和断言后，通过编译器标准输入生成的临时程序；仅链接已有构建库，没有改写产品或测试源码。P1 含 R1-01、R1-02 两个独立断言，结果 **35 通过、2 失败**；P2 只检查提示遮挡，结果 **34 通过、1 失败**，均正常跑到 `test.finished=1`。

可在当前构建目录直接重复以下命令，使用新的合成存储目录：

```sh
review_probe() {
    review_evidence=$(mktemp -d "$PWD/build/files-review-rerun.XXXXXX")
    INK_FILES_PHASE="$2" INK_FILES_SCENE="" taskpolicy -c utility "build/files-app-1-review/$1" \
        --profile simulator/profiles/metalio-plus.json \
        --script tests/files_app/files_app.script --budget tests/ui_scene.budget.json \
        --assets build/assets --content build/test-content \
        --flash-image "$review_evidence/flash.img" --sd-dir "$review_evidence/sd" \
        --out "$review_evidence/run" --quiet --no-random-outages
}
# P1：应退出 1，出现 REVIEW_BACK 和 REVIEW_SNAPSHOT
review_probe sim-review-files review
# P2：应退出 1，只出现 REVIEW_NOTICE
review_probe sim-review-notice write
```

这些二进制和图片随构建目录清理会消失；问题条目已保留完整构造步骤、源码位置和实测值，便于重新加入现有测试复现。

## 范围与证据边界

- 基线差异仅原任务的 9 个文件。新应用 291 行、启动接线 2 行、CMake 末尾 7 行，保守合计 300 行；没有改文件服务、共享规范、预算或并行任务产品代码。
- `git diff --check` 通过；按总规则的绝对路径／MAC 正则扫描基线至 HEAD 无命中。本轮源码与 HEAD 的差异为空，工作区只新增本报告。
- 执行报告已明确“打开”只提供 128 字节文本预览、图片缩略图及音频能力接线；未声称实现 EPUB/PDF 阅读器。音频打开本轮仅核查接线，未新增播放场景。
- 本轮证据为 macOS 模拟器；没有实机或 Linux/gcc 结论。未碰设备、读取硬件目录、调用付费接口或执行外部发布。

SEVERE=0 HIGH=0
