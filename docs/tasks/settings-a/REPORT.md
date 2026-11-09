# settings-a 执行报告

基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`。状态：执行结束；验收 1～3 通过，验收 4 与 G4 未满足全部条件。所有【实测】均指本机无窗口模拟器，未碰设备。改动仅本地提交，没有 push 或 PR。

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
- R1-04：正在恢复四页分组和版面；本节后续补齐布局测试、逐页图像核对及最终全量验收结果。
