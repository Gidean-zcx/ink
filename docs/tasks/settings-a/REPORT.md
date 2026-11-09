# settings-a 执行报告

基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`。状态：页面实现和定向验证完成，全量验证进行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】定向测试通过 | 四页打开；读取真实空间、版本、备份状态；三项已有设置在第二个进程从同一闪存恢复 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^settings-a$' --output-on-failure -j2` |
| 2 | 【实测】定向测试通过 | 四页使用 F1/F4；完整快照由 Python 解析；真实触摸与 AI 调同一 `settings.set`，隐私权限保留人工确认 | 同上；`tests/settings_a.c`、`tests/settings_a.py` |
| 3 | 【实测】正反对照通过 | 孩子不能从能力或直接页面入口打开四页，不能写三项成人设置；去掉页面检查或后端设置检查，分别退出 1 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/settings_a_reverse.py --build build` |
| 4 | 待验证 | 全量测试、内存检查、350 行产品代码预算 | 待补 |
| G4 | 【实测】未达到像素对照目标 | SetVoice 63.461、SetStorage 58.984、SetAbout 57.825、SetPrivacy 31.949（均为 /255，目标 ≤12）；尺寸检查 30 文件、0 违规 | `INK_DESIGN="$INK_DESIGN" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --out build/settings-a-compare --scenes SetVoice SetStorage SetAbout SetPrivacy`；`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_scale_check.py` |

## 没做到的

后端缺少的唤醒名字设置、回答打断设置、打字接话设置、分类空间统计、临时文件清理、设备详情与开源许可页面没有实现；界面明确写暂不能使用。AI 操作记录、记忆与记录删除页面属于后续任务，没有伪造入口。没有新增假设置或服务。

## 和设计不一致的

四页复用 F4 等高列表分页，未还原画板的全部分组、图标、关于页居中标志和底部按钮。采用规范第 1 节及代码现有的放大字号、深灰底色；画板仍是旧尺寸、浅灰。保留期限无法从设备现有服务读取，隐私页明确说明暂不能查看，没有把画板里的保留承诺写成实测。

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

构建沿用总规则的字体、Python、LVGL 参数；额外只把现成本地依赖目录传给 `FETCHCONTENT_SOURCE_DIR_IMGUI=$INK_DEPS/build/_deps/imgui-src`、`FETCHCONTENT_SOURCE_DIR_SDL2=$INK_DEPS/build/_deps/sdl2-src`，并设置 `INK_HUB_PYTHON=$INK_DEPS/hub/.venv/bin/python`。后者已核对可导入锁定测试依赖，避免把默认 Python 缺依赖的三项跳过计作通过。构建均为 `taskpolicy -c utility cmake --build <目录> -j2`。

【实测】最终版本的两项反向对照均退出 1：`build/settings-a-reverse/summary.json`，逐条命令在同目录 `commands.json`。替换对象方式编译，不改产品源文件和正式二进制，并核对源文件哈希不变。

【实测】画面对照在 `build/settings-a-compare/scores.json`；四张拼图同目录。不提交设计稿、截图、构建产物或本机路径。
