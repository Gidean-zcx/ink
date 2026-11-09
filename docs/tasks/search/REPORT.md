# search 任务报告

基线：`558c7ba8604fc321073b943716f49512a7dc3690`。状态：完成交付；验收 5 的共享文档检查未通过，画板对照未达标，交主会话复核。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过，文件仅信息页 | 五类真实来源、三页结果；触摸打开联系人再返回原页和选中项；AI 打开实际文件索引信息 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/search/run.py --build build` |
| 2 | 【实测】通过 | 孩子看不到大人的联系人、备忘、文件；旧快照、旧打开参数拒绝；去掉数据库成员过滤后，模拟器退出 1、出现 4 条预期失败 | 同上加 `--reverse`（反例驱动退出 0） |
| 3 | 【实测】通过 | `SearchEmpty` 活动、可见空文案及 AI 同文案；旧查询回执被拒绝 | 同 1 |
| 4 | 【实测】通过 | 人用真实键盘、AI 用 `text.insert` 提交，都进入 `search.query`；六个能力和必填参数进入真实 AI 目录 | 同 1；`ctest --test-dir build -R '^integration-ai-surface$' --output-on-failure` |
| 5 | 【实测】未全部通过 | 普通和 ASan/UBSan 全量均 232/233 通过、0 跳过；唯一失败均为按规则未更新的共享能力文档，无内存错误。产品新增 282、删除 1 行，合计改动 283 行，含全部 CMake 接线，未超 300 | 下列全量命令；`git diff 558c7ba8..HEAD --numstat -- system CMakeLists.txt` |

## 没做到的

- 文件应用未在基线中提供打开能力，文件结果只打开真实索引信息页 `SearchFile`，没有内容预览；设置结果打开对应分组页，没有定位到单个开关。
- 两屏对照均未达 12/255，见下一节。未运行 Linux/gcc 或真机验证；本次为 macOS clang 模拟器证据。
- 没有跨数据变更保留搜索结果快照；插入、删除或移动内容后应重新搜索。这类同时变更未列为本次验收。

## 和设计不一致的

任务书明确要求全局搜索应用、设置、联系人、备忘、文件；框架设计 F7 仍写仅应用和设置。本次以任务书的五类范围为准，不修改设计文档。

| 页面 | 1/4 灰度平均差 | 差异 |
| --- | ---: | --- |
| Search | 【实测】40.653/255 | 复用 F4 的双行描边卡片；画板为单行灰底分组；真实设置条目与画稿不同，现有固定键盘、放大字号和灰度也与旧画稿不同 |
| SearchEmpty | 【实测】22.172/255 | 当前主题的字号、灰度、输入框搜索图标与旧画稿不同；空文案适配五类内容为“没有找到相关内容” |

对照来自最终普通构建的真实已稳定帧，未改字体、灰度或规格追分。

## 改过的已有测试

`tests/ui_shell_test.c`：主页搜索提交后应进入 `Search`，原先预期留在 `Apps`；新增返回原 `Apps` 的断言。原有 `shell.search` 应用过滤能力保持兼容；没有删测试或放宽断言。

`tests/integration/ai_surface.c`：在已有 AI 能力合同中新增六个搜索能力及必填参数；保留完整目录和参数断言。

## 要用户定的

暂无。

## 顺手发现

依赖目录中已有完整中枢 Python 环境；使用 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"` 后，首轮跳过的三项也通过。未安装或升级依赖。

## 新增的机制

- 查询游标、查询版本、成员归属、三条可见结果和返回位置：由任务书 F7 要求，复用 F2/F4；各应用数据、存储格式不变。
- 产品新任务/线程、持久文件格式、配置项、后台重试：均无；复用现有 BG/UI 执行器。
- 服务小接口：`ink_setting_at` 只读取静态设置元数据；没有另建搜索索引或缓存框架。

## 为什么不能更小

五类来源要接入真实目录、设置表、成员数据库和文件索引，还要完成 F2/F4 页面、同一能力路径、异步回执和返回位置。看过更小的静态示例列表和 UI 线程扫描方案，分别不满足真实数据、不可阻塞要求。按全部新增行计数（包括空行、注释、CMake 测试接线）仍只有 283 行改动。

## 过程证据

- 【实测】普通和 ASan 均以 `INK_WERROR=ON` 构建成功，最多 `-j2`；界面尺寸检查通过。
- 【实测】普通全量耗时 520.55 秒，ASan/UBSan 全量 656.56 秒；各 233 项、232 通过、1 失败、0 跳过，失败项均为 `capability-table-doc`。
- 【实测】再以 `UBSAN_OPTIONS=halt_on_error=1` 单独复核搜索，1/1 通过，耗时 4.18 秒，证据 `build-asan/search-ubsan-strict.log`。
- 【实测】最终普通搜索两场景各 108 条断言；内部 RAM 最低余量 47,840 字节（预算 32,768），PSRAM 最低余量 13,027,272 字节；BG 栈估计 4,784 字节。仅为模拟器证据。
- 【实测】首轮测试发现文件信息页字段缺少 `role`，ASan 定位到快照空指针；仅补该声明后同一场景通过，没有新增防御框架。
- 【实测】最终反例证据 `build/search-yapxa42q/`，普通全量日志 `build/search-ctest-final.log`，ASan 全量日志 `build-asan/search-ctest-final.log`，对照图和分数 `build/search-compare/`；构建产物不提交。
- 【实测】构建目录中的能力表生成副本与运行目录一致：268 个能力、16 个设置、共 330 项定义。共享文档检查失败没有跳过或放宽。

复核命令（先按总规则配置两套构建，均指定上述中枢 Python；依赖只读）：

```sh
PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2 -R '^search$'
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/search/run.py --build build --reverse
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --out build/search-compare --scenes Search SearchEmpty --reuse
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output build/search-capabilities.md --runtime build/test-out/capability-table/capability-inventory.json
```

结束检查：`git diff --check` 通过；`git diff --stat` 核对仅 12 个相关文件；基线至 HEAD 按总规则的隐私扫描无匹配（`rg` 退出 1），没有提交绝对路径、设备号、真实凭据、设计稿或构建产物。

## 要改的文档

- `docs/system/ui-dependencies.md`：更新搜索的实际状态及文件仅信息页的限制。
- `docs/system/capabilities.md`：主会话合并后重新运行 `python tools/capabilities.py`；本分支按总规则第 7 节未改共享生成文档，这是唯一剩余普通全量失败。
- `docs/system/app-framework-design.md` F7：主会话统一核对任务书要求的五类范围与旧文档差异。本分支不修改合同迁就实现。
