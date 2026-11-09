# settings-a 执行报告

基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`。下面保留初次执行记录（当时验收 1～3 通过，验收 4 与 G4 未满足全部条件）；最新修复和验收见末节“复查后的修复”。所有【实测】均指本机无窗口模拟器，未碰设备。改动仅本地提交，没有 push 或 PR。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】定向测试通过 | 四页打开；读取真实空间、版本、备份状态；三项已有设置在第二个进程从同一闪存恢复 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^settings-a$' --output-on-failure -j2` |
| 2 | 【实测】定向测试通过 | 四页使用 F1/F4；完整快照由 Python 解析；真实触摸与 AI 调同一 `settings.set`，隐私权限保留人工确认 | 同上；`tests/settings_a.c`、`tests/settings_a.py` |
| 3 | 【实测】正反对照通过 | 孩子不能从能力或直接页面入口打开四页，不能写三项成人设置；去掉页面检查或后端设置检查，分别退出 1 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/settings_a_reverse.py --build build` |
| 4 | 【实测】全量未全绿；预算通过 | 普通与 ASan/UBSan 构建均 224/225 通过、0 跳过，唯一失败均为 `capability-table-doc`（任务禁止修改的共享文档未更新）；未发现 ASan/UBSan 错误；产品改动 234/350 行 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；`INK_KEEP_EVIDENCE=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2`，均退出 8；日志分别为 `build/ctest-full-final.log`、`build-asan/ctest-full.log` |
| G4 | 【实测】未达到像素对照目标 | SetVoice 63.461、SetStorage 58.984、SetAbout 57.825、SetPrivacy 31.949（均为 /255，目标 ≤12）；尺寸检查 30 文件、0 违规 | `INK_DESIGN="$INK_DESIGN" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --out build/settings-a-compare --scenes SetVoice SetStorage SetAbout SetPrivacy`；`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_scale_check.py` |

## 没做到的

后端缺少的唤醒名字设置、回答打断设置、打字接话设置、分类空间统计、临时文件清理、设备详情与开源许可页面没有实现；界面明确写暂不能使用。AI 操作记录、记忆与记录删除页面属于后续任务，没有伪造入口。没有新增假设置或服务。

系统更新按钮调用现有 `update.check`；检查失败只返回异步回执、不改变现有 `update.state`，本页尚未呈现这类回执。没有扩大为更新服务改造。Linux/gcc 未实测，本机只验证 macOS/clang 的 `-Werror` 构建。

## 和设计不一致的

四页复用 F4 等高列表分页，未还原画板的全部分组、图标、关于页居中标志和底部按钮。采用规范第 1 节及代码现有的放大字号、深灰底色；画板仍是旧尺寸、浅灰。隐私额外呈现已有的剪贴板读取权限，关于额外呈现已有的自动下载设置。保留期限无法从设备现有服务读取，隐私页明确说明暂不能查看，没有把画板里的保留承诺写成实测。

## 改过的已有测试

`tests/integration/ai_surface.c` 的 `FOR_AI` 增加六条明确预期：`shell.voice`、`shell.storage`、`shell.about`、`shell.privacy`、`shell.settings_prev`、`shell.settings_next`，均不需要参数。原因是原测试要求每个注册能力必须且只能归入一个清单；首轮全量测试准确报出这六个遗漏。保留全部断言和预算，新增能力继续验证可发现性与参数合同。

## 要用户定的

暂无。

## 顺手发现

暂无。

## 新增的机制

没有新任务、状态机、持久文件格式或配置项。任务书要求的四个页面和六个导航能力复用现有设置能力、控件语义、通用列表和 UI 执行器。订阅现有设置、存储、版本状态和备份事件，不新增轮询或重试。服务层零修改。

## 要改的文档

按总规则第 7 节，本任务不修改共享文档：

- `docs/system/capabilities.md`：运行 `tools/capabilities.py`，加入 `shell.voice`、`shell.storage`、`shell.about`、`shell.privacy`、`shell.settings_prev`、`shell.settings_next` 及页面读取的已有设置、状态和事件。当前 `capability-table-doc` 会因旧文档不一致失败，不修改或跳过检查。
- `docs/system/ui-dependencies.md`：四页改为已接现有后端、F1/F4 与成员限制，保留上述缺口；画板像素对照尚未达标。

## 规模与架构

【实现】产品改动 234 行（新增 231、删除 3；包含 CMake 中 1 行源文件注册），低于 350 行预算。超过 100 行是因为四页的值、动作、生命周期与现有服务订阅均需接入。看过的更小方案是把页面塞进 `navigation.c`，会违反新页面单独成文件的边界；另开设置写入路径会违反复用要求，因此采用一个页面文件和少量入口注册。`navigation.c` 只改三个菜单绑定、一行页面转交、一行初始化；移除了菜单里没有查询后端就写“还没有备份”的旧文字。

【实现】设置写入只有既有 `settings.set`；只读值来自状态表或 `backup.status`；UI 不改服务内部数据。新导航能力由能力表统一执行儿童限制，页面入口再挡住直接调用。AI 读取同一列表模型，确认仍由已有设置策略决定。

## 构建与证据

构建沿用总规则的字体、Python、LVGL 参数；额外只把现成本地依赖目录传给 `FETCHCONTENT_SOURCE_DIR_IMGUI=$INK_DEPS/build/_deps/imgui-src`、`FETCHCONTENT_SOURCE_DIR_SDL2=$INK_DEPS/build/_deps/sdl2-src`，并设置 `INK_HUB_PYTHON=$INK_DEPS/hub/.venv/bin/python`。后者已核对可导入锁定测试依赖，避免把默认 Python 缺依赖的三项跳过计作通过。普通与内存检查配置均启用 `INK_WERROR=ON`，后者只额外启用 `INK_SANITIZE=ON`；构建均为 `taskpolicy -c utility cmake --build <目录> -j2`。

【实测】最终版本的两项反向对照均退出 1：`build/settings-a-reverse/summary.json`，逐条命令在同目录 `commands.json`。替换对象方式编译，不改产品源文件和正式二进制，并核对源文件哈希不变。

【实测】画面对照在 `build/settings-a-compare/scores.json`；四张拼图同目录。不提交设计稿、截图、构建产物或本机路径。

【实测】普通与内存检查构建的设置场景均 134 条检查通过，第二进程重启场景均 10 条通过；内部 RAM 最低余量均为 47840 字节（门槛 32768），无分配失败。状态变化检查使用测试内的合成值（4321 KiB、`0.0.7-test`）；产品代码没有示例空间、版本或备份数据。

【实测】最终普通构建的定向复查 3/3 通过：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^(settings-a|integration-ai-surface|ui-scale)$' --output-on-failure -j2`，退出 0，日志 `build/settings-final.log`。最后一次改动只删去了隐私说明中无法验证的保留策略归属；最终 ASan/UBSan 全量运行和此复查均包含该改动。

【实测】基线至提交的差分隐私扫描零命中（按总规则第 6 节原正则，`grep` 退出 1 表示无匹配）；`git diff --check` 通过。差分仅包含四页及其入口、构建注册、测试和本报告。

## 复查后的修复

复查基线：`418d66dd`。按用户要求修复全部四个中级问题；共享能力表文档仍交合并主会话生成。

- R1-01：【实现】绑定复用行时统一清除两个基础文字的隐藏标志；默认和特大字号均检查第二页正文可见。
- R1-02：【实现】备份服务成功保存启动意图并建立任务后，补发已有的 `backup.changed`；复用页面现有订阅，不加轮询。回归用真实触摸启动断网备份，核对后端阶段、可见文字和 AI 快照。
- R1-03：【实现】下一页后立即核对页码、偏移和记忆字段，再用实体上一页键核对回到第一页；增加禁用翻页的反向对照。
- 【实测】未修产品时新增回归准确报出四条失败：两个字号的记忆正文隐藏、备份快照陈旧、备份正文未刷新；`build/settings-fixes-before.log`。修复后的定向运行通过，见 `build/settings-fixes-after.log`。五项反向对照全部按预期退出 1，涵盖原有两个儿童限制、可见性、备份启动事件和翻页执行；`build/settings-fixes-reverse.log`。
- R1-04：【实现】仅调整四页绘制：恢复语音分段和说明、“AI”“打字”分组，存储备份图标、空间摘要和书／音频／照片分组，关于页居中标志、设备名和版本、两组设置与底部清空入口，隐私页无卡片内缩的标题／正文及底部记录入口。复用现有 F1/F4、图标、文字和控件；按实际文字高度分页，未改共享列表或主题。说明和分组也进入当前页 AI 快照的 `notes`。
- 【实测】四页默认／特大字号共 15 个分页画面已逐页查看；定向测试通过，检查每个字段文字可见且不被祖先裁切、图标与灰底、空间描边、居中标志、段落边距、底部入口。Python 对照固定字段清单核对顺序、偏移、总数和完整覆盖，保留全部原有 JSON、写入、重启、确认和儿童限制断言。四种撤回版面修复的变体均正常结束并退出 1；九项反向对照见 `build/settings-a-reverse/summary.json`，每项均有 `test.finished=1`。

### 画面对照与保留差异

【实测】命令：`INK_DESIGN="$INK_DESIGN" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --out build/settings-a-fixed-compare --scenes SetVoice SetStorage SetAbout SetPrivacy`。四张拼图和原图均已查看；全部分页图在 `build/test-out/settings-a-sim-settings-a/write/snap-font*.png`。

| 页面 | 原始画板 MAE /255 | 本轮核对 |
| --- | ---: | --- |
| SetVoice | 41.051 | 控件、图标、说明与两个分组恢复；语音控件一页，AI 和打字一页 |
| SetStorage | 52.353 | 备份、空间、三种分类、清理恢复为不同层次；显示真实 4.0 GB，分类与清理明确不可用 |
| SetAbout | 47.930 | 居中身份区和两组行恢复；自动下载仍为真实设置，清空入口在末页底部 |
| SetPrivacy | 35.177 | 段落按文字高度排，不再使用等高大卡片；记录入口在末页底部，明确未接入 |

**原始像素分数仍超过 12，不宣称像素目标通过。** 按本次指示，只用规范 §1 和当前主题：字号为旧图的 1.2 倍、灰底 `#888888`、页边距 38；必要分页保留。画板的示例设备名、版本、备份时间、容量与分类占用没有写入产品；后端没有卡总容量和分类数据，未画伪造比例条。隐私保留期限仍写无法查看。卡与 U 盘的实际不可用状态集中呈现在空间摘要，不再各铺一张卡；初版额外的内部空间行退出这四页绘制，后端接口保留。

### 测试调整、规模与架构

- 已有测试期望仅随真实版面调整：存储从原始 KiB 改为固定夹具的 `4.0 GB`／`4.2 MB`；隐私首页检查可见说明，剪贴板权限由完整分页遍历及原有触摸确认测试覆盖；关于页的两个身份字段以可见值核对，其说明性字段名不再作为屏幕标题。没有删除设置、确认、重启或权限检查。
- `settings_a.script` 的场景结束由 80 秒延至 120 秒，是因为新增两种字号的所有分页遍历后，撤掉儿童页面检查的变体在第 79.72 秒才触发第一条预期失败。保留所有等待上限和资源预算，没有加 sleep；反向测试现在还要求正常跑到 `test.finished`，不能靠截断或崩溃判失败。
- 【实现】累计产品改动 336/350 行（包括一行源文件注册；把另外四行 CMake 测试接线也计入为 340）。版面修复局限于原文件，未拆出布局框架；更小的仅去外框方案仍会丢失分组和身份区，不能满足 R1-04。服务只增加一行既有事件通知。
- 新增的机制：无。没有新任务、线程、持久格式、配置项、轮询、重试或缓存。局部测量只用于这四页绘制，分页和动作仍走现有 F4/F1；设置仍走 `settings.set`，备份状态仍由服务事件触发查询，触摸和 AI 不直接写服务状态。
- 要改的共享文档：合并时生成能力表，并同步界面依赖中四页的状态及缺失后端说明。本轮不改这些共享文件。

### 最终验收

修复提交：`19567727`、`6f70a411`。

| 编号 | 结论 | 证据与命令 |
| --- | --- | --- |
| 1 | 【实测】通过 | 四页真实值、备份启动后的可见状态和 AI 快照一致；三项设置重启恢复；`settings-a` 正常进程 580 项、重启进程 10 项通过 |
| 2 | 【实测】通过 | 15 个分页画面逐字段可见性与布局断言；说明进入 AI 快照，原有触摸／AI 共用能力和确认测试保留；`build/settings-fixes-final-target.log` 定向 3/3 通过 |
| 3 | 【实测】通过 | 两项儿童限制和七项功能／版面反向对照，九项均正常结束并准确退出 1；`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/settings_a_reverse.py --build build` |
| 4（普通全量） | 【实测】除用户豁免项外全过 | 224/225、0 跳过、130.58 秒；唯一失败 `capability-table-doc`。命令：`INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；日志 `build/settings-full-final.log` |
| 4（内存检查） | 【实测】除用户豁免项外全过 | 224/225、0 跳过、247.29 秒；唯一失败 `capability-table-doc`，无 ASan/UBSan 错误。命令同普通全量，目录换为 `build-asan` 并加 `UBSAN_OPTIONS=halt_on_error=1`；日志 `build-asan/settings-fixes-full.log` |
| G4 | 【实测】分组／版面回归通过；原始像素目标仍未达 | 对照分数、必要分页及规范／真实数据差异见上表；未更改主题迎合旧图 |

【实测】依赖原因已核清：构建用 Python 缺 `pytest`、`pytest_asyncio`、`aiohttp`、`yaml`，中枢适配器因此会返回 77。保留 `INK_HUB_PYTHON=$INK_DEPS/hub/.venv/bin/python`，实测可加载 pytest 8.4.2、pytest-asyncio 1.2.0、aiohttp 3.12.15、PyYAML 6.0.3；三项中枢测试实际运行，不以跳过计通过。没有安装或升级依赖。

【实测】普通构建和内存构建都以 `taskpolicy -c utility cmake --build <目录> -j2` 完成；后者保留 `INK_WERROR=ON`、启用 `INK_SANITIZE=ON`，实际编译参数含 address 和 undefined 两种 sanitizer。两轮全量串行且均退出 8，仅因能力表文档不同步；用户已明确由合并主会话重新生成，没有跳过或放宽检查。两种构建的设置场景均为 580＋10 项通过；内部 RAM 最低余量均为写入进程 47840 字节、重启进程 43736 字节，高于 32768；内外部分配失败均为 0。

【实测】`git diff --check f0b0c539` 通过；差分只涉及本任务。全任务差分隐私扫描唯一命中是原有 `REVIEW.md:88` 扫描命令中的正则字面量，不含真实机器路径或设备标识；本轮 `418d66dd..HEAD` 差分零匹配。未改 `REVIEW.md`、共享文档、其它并行任务文件或资源预算；未提交证据图片和构建产物。Linux/gcc 与实机仍未验证。
