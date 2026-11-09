# ime-lexicon 执行报告

基线：`a6f4140df5790c22d3f8ff5cfb00557289f3455c`。分支：`claude/codex-ime-lexicon`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 1,048,560 字节；同输入逐字节一致；四份 MIT 许可证随词库分发 | `python tools/imedict/build_base.py --cache build-deps/imedict --out work/base.ikd` |
| 2 | 【实测】通过 | 90 项质量：首选 75/90，前五 83/90；兜底均 16/90；普通全量 287/287、最终代码 ASan/UBSan 全量 287/287，均无跳过；最终专项 17/17 | `ctest --test-dir build -j2 --output-on-failure`；内存检查换 `build-asan`；专项命令见下文 |
| 3 | 【实现】通过 | 来源、锁定哈希、频率裁剪、人工短语、多音字与简拼局限见 [工具说明](../../../tools/imedict/README.md) | 见说明中的复现命令 |

## 没做到的

- 没有做实机加载或 Linux 实测；本次证据限 macOS 模拟器，没有运行远端 CI。
- 生僻词、新词、未覆盖的多音字和简拼冲突仍有缺口，详见下文；没有任意短句自动组句。超过五字的长候选展开排版未逐项验证。

## 和设计不一致的

- 【实现】新增简拼键是任务书明确要求；仍用原 IKD1 和查询规则。引擎增删 0 行。
- 【实现】正式数据使折叠候选栏均分六格时裁掉“今天”。仅改一行样式，让候选按钮沿用原来的内容宽度；“更多”和展开逻辑保持原实现，不增加状态或交互入口。

## 改过的已有测试

- 未改任何已有断言或期望。`integration-text-*` 改用单独生成的 `mini-content`，保留原有小词库顺序和损坏文件测试；默认 `test-content` 使用正式词库。原键盘画板断言保持不变。

## 要用户定的

- 暂无。

## 顺手发现

- 首轮完整测试因默认 Python 缺少中枢依赖跳过 3 项；复跑指定 `$INK_DEPS/hub/.venv/bin/python`，并用 `PYTHONPATH="$PWD/hub"` 确保验证本检出代码。

## 数据与质量

- 【实现】jieba 0.42.1 的 `dict.txt` 提供词与频率，pypinyin 0.55.0 wheel 提供单字与词组读音；两者及内含 pinyin-data、phrase-pinyin-data 均为 MIT。固定下载地址、SHA-256、子模块提交见 [来源锁](../../../tools/imedict/sources.lock.json)，完整许可证在 [licenses](../../../tools/imedict/licenses/)。另有 21 条项目自编日常短语与设备词，频率下限 1000 是人工保留优先级。
- 【实测】24,138 个文字项（4,054 个单字、20,084 个词语/短语），21,134 个键，45,762 个键词条，其中 20,084 条简拼；最低原始保留频率 199；文件 1,048,560 字节。
- 【实测】输出 SHA-256：`163bab7cad0b13816627252b14494bf78f2b671d56fccdd6b3620ded9df70bb4`。重复生成逐字节相同；篡改缓存输入会被 SHA-256 校验拒绝。
- 【实现】按原始频率降序裁剪（同频按文字），整体计入全拼、简拼、表和文字；词频用整数平方根压入 u16，单字次读音权重再除以 4；不在引擎里增加分词或简拼逻辑。详细规则、部署目录和复现命令见[工具说明](../../../tools/imedict/README.md)。

| 同一 90 条输入【实测，模拟器】 | 首选 | 前五 | 加载请求到完成 | 词库申请的 PSRAM |
| --- | --- | --- | --- | --- |
| 正式词库 | 75/90（83.33%） | 83/90（92.22%） | 879.95 ms | 1,048,560 字节 |
| 19 字兜底 | 16/90（17.78%） | 16/90（17.78%） | 29.95 ms（未找到文件） | 0 字节 |

- 计时为默认 annotated CPU 档案的虚拟时间，20 ms 轮询，单次样本，排除键盘绘制。后台排队、读卡和校验确实执行，CRC 沿用平台的 `crc32.byte` 成本标注；键表/词表校验循环未单独标注 CPU 开销，不能外推为实机加载耗时。PSRAM 数字不含分配器元数据和其他文本缓冲。未上机。
- 测试集手写，部分与人工词表重合，不代表真实输入分布。全拼 73 条：71 条首选、73 条前五；简拼 16 条：4 条首选、10 条前五；另 1 条未收录短句两者都未命中。
- 简拼的前五缺口：`nh`→你好、`xx`→谢谢、`dx`→短信、`nz`→闹钟、`yy`→音乐、`sz`→设置；“你好”在 `nh` 第 9，测试确实通过公共文本服务选中并提交它。其余生僻词、新词、低频专名受裁剪影响，未收录多音词可能读错，不提供任意短句组句。
- 【实测】反向对照：不改二进制，只移走词库后，正式验收退出 1、`test.fail=3`；恢复词库通过。另实际撤掉一行候选样式修复并重编译，普通/大字/儿童 `ImePinyin` 三项全部报“今天”裁切（CTest 退出 8）；原样恢复源文件、重建后专项 17/17、退出 0。证据为 `work/ime-ui-negative.json` 和 `work/ime-ui-negative.log`。

## 为什么不能更小

- 生成器 113 行，超过默认 100 行说明线：固定来源校验、词组读音、全拼/简拼、精确计量裁剪、许可证随资源分发是本任务要求。复用原 `imedict.py` 写格式，没有新增格式。只沿用小词表无法交付正式词库；直接安装未锁定的拼音包无法保证相同输入；把简拼移进引擎会扩大运行时改动。硬性引擎预算仍为 0/100 行。

## 架构审视

- 【实现】电脑端处理数据、模拟器和设备沿用同一引擎与读卡接口；无系统层引用模拟器头文件。候选按钮由 UI 任务按原有内容宽度排版，按钮和 AI 继续共用文本能力，已有快照只列实际可见候选；没有新增权限路径或成员数据。

## 新增的机制

- 构建时下载校验和词频裁剪：任务书明确要求。沿用 IKD1，新增版本/哈希清单仅供电脑端构建。
- 无新增运行时任务、状态机、持久格式或配置项；仅测试程序创建有界驱动任务。

## 要改的文档

- 主会话合并时更新 `docs/tasks/backlog.md`、`docs/system/ui-dependencies.md` 的正式词库状态，以及 `docs/system/system-services-design.md` §9.2、§15 的“来源待定/正式词库未做”描述。这里不修改这些文档。

## 验证记录

- 普通构建为 macOS Apple clang、`INK_WERROR=ON`、`taskpolicy -c utility`、`-j2`；全量 287/287，446.74 秒，无跳过。最终一行样式缩简后补跑输入法/键盘专项 17/17。
- 中枢依赖另指定 `-DINK_HUB_PYTHON=$INK_DEPS/hub/.venv/bin/python`；测试时 `PYTHONPATH="$PWD/hub"`，已核对导入的是当前检出，不修改依赖目录。
- ASan/UBSan 同样以 `INK_WERROR=ON`、`-j2` 构建，加 `INK_SANITIZE=ON`；最终代码全量 287/287，464.10 秒，无跳过。日志为 `work/full-tests.log`、`work/asan-tests.log`；撤回/恢复后最后一次普通专项 17/17，4.99 秒。
- 正常与 ASan 构建生成的词库逐字节一致。完整质量证据保留在 `build/ime-lexicon-*/summary.json` 和 `dictionary-candidates.json`（含 `load_ms` 的最终测量）；重跑时加 `INK_KEEP_EVIDENCE=1`。
- 【实测】按总规则 §6 对基线到 HEAD 的差异执行隐私扫描：0 行匹配，grep 退出 1（无匹配）；`git diff --check` 与两份新增 Markdown 的相对链接检查通过。差异共 15 个相关文件，无词库二进制、原始语料、设计稿、构建产物或共享文档夹带。只提交到本地分支。

复现时先按[总规则](../codex-rules.md) §5 配置两个构建目录，构建并行度改为 `-j2`；两者都加上上述 `INK_HUB_PYTHON` 参数。然后执行：

```sh
PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R 'ime-lexicon|integration-text|ui-scene.*Ime'
```

候选栏反向对照：将唯一一行候选按钮的 `flex_grow` 临时恢复为基线的 `K.expanded ? 0 : 1`，重建 `sim-ui-scenes`，执行 `ctest --test-dir build --output-on-failure -j2 -R '^ui-scene(-big|-child)?-ImePinyin$'`；验证三项失败后恢复为 `0` 并重建，补跑上面的专项。未改变已有测试期望。
