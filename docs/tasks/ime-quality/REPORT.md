# ime-quality 执行报告

基线：`5e1e3964b5aafae3587b805c2edcf13922cccd44`。仅本地分支提交，不推送。证据范围为主机模拟器，不代表实机验证。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 原 90 条用例不变；简拼首选 4/16 → 12/16，前五 10/16 → 16/16；全拼 71/73、73/73 不变 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^ime-lexicon$'` |
| 2 | 【实测】通过 | 单次选择提升；全拼/简拼及前缀隔离；成人与儿童各自记忆；新进程重启保留；密码不学；513 个合成键验证 512 条限额及最近使用淘汰；AI 读取被拒绝 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^ime-learning$'` |
| 3 | 【实测】普通全量通过；内存检查进行中 | 普通全量 289/289，无跳过；词库 969,866 字节 | 见下方全量命令 |

| 原测试集【实测，模拟器】 | 基线首选 | 完成版首选 | 基线前五 | 完成版前五 |
| --- | --- | --- | --- | --- |
| 全拼 73 条 | 71/73（97.26%） | 71/73（97.26%） | 73/73（100%） | 73/73（100%） |
| 简拼 16 条 | 4/16（25%） | 12/16（75%） | 10/16（62.5%） | 16/16（100%） |

## 没做到的

- 未做实机或 Linux gcc 运行，证据限本机 macOS 模拟器；没有联网 AI 接话。
- 没有迁移旧 schema 1 偏好：旧记录只有按词的哈希和次数，无法还原原拼音。旧记录不当作新格式偏好使用，随容量淘汰。
- 简拼保留前 32 项，未入选的候选需要全拼；该手写测试集不代表真实使用分布。四个未首选是 `xx` 的学校、学习，以及 `sh` 的上海、`yh` 的银行，均在前五。

## 和设计不一致的

- 无页面、快照、查询规则变更。沿用设计 §9.2 的 `INK_DB_LAZY`，最多 60 秒合并落盘，突然断电仍可能丢尚未落盘的选词；重启验收先等到持久化。

## 改过的已有测试

- 保留原有用例、断言和阈值；`ime-lexicon` 新增分组统计和全拼 71/73、前五 73/73，简拼首选 ≥10/16、前五 ≥14/16 的硬断言。新加独立 `ime-learning` 测试，不读取 Git 历史。

## 要用户定的

- 暂无。学习记录默认不允许 AI 读取，保留现有选词能力入口。

## 顺手发现

- 【实测】只编译原 `sim-ime-lexicon` 目标不会生成全部 UI 字体和图标，首次专项缺资源；按任务规则先全量构建后通过，未修改原目标依赖。

## 新增的机制

- 任务书第 1 项：仅构建期简拼先验和每键 32 候选裁剪，沿用 IKD1。
- 任务书第 2 项：复用成员集合 `text.user_freq`，512 条、2048 字节载荷上限，按选词顺序更新已有 `sort` 做淘汰；无新任务、状态机、文件格式或配置项。schema 2 的计数载荷仍为 4 字节，旧 schema 1 不能还原原拼音，不混用。

## 要改的文档

- 主会话合并时同步 `docs/tasks/backlog.md`、`docs/system/ui-dependencies.md` 的输入法状态；`docs/system/system-services-design.md` §9.2 更新简拼先验、32 候选、按拼音学习、512 条/2 KiB 载荷及 AI 默认不可读的说明。这里不改共享文档。

## 里程碑与复现

- 开工：已核对 HEAD 与指定基线一致，工作区干净，分支为 `claude/codex-ime-quality`；已阅读任务规则（第 7 节优先）和任务书。
- 简拼里程碑：【实测】基线专项通过；新词库专项通过，90 条合计首选 75 → 83、前五 83 → 89，未收录短句仍未命中。词库从 1,048,560 缩到 969,866 字节，同输入重复生成逐字节一致。基线与新候选证据分别为 `work/baseline-candidates.json`、`build/ime-lexicon-w_0xkvu6/dictionary-candidates.json`。
- 【实测】基线与完成版的 25,678 个全拼键/词/频率项逐项相同，原测试集 73 条全拼的前五列表全部相同；结果 `work/full-pinyin-unchanged.txt`。词库 SHA-256 为 `f2e8f00f192c85a43ab1fd9810bdd9642a0e359e96dd2f22a74673f782cbf34b`。
- 学习里程碑：【实现】引擎不变，文本服务增 18 / 删 14，合计 32/200 行。计数按拼音前缀和文字的哈希存储，同长度候选按次数优先，单次选择已可超越基础词频；计数封顶避免排序溢出。淘汰使用持久化顺序，不依赖墙钟是否设置或是否倒退。【实测】两个专项 2/2，学习证据在 `build/ime-learning-08f3nbeb/summary.json`。
- 反向对照：【实测】只将生成器、人工词表和文本服务临时恢复为指定基线版本，保留全部新断言；两个专项均失败，CTest 退出 8。简拼回到 4/16、10/16，学习写入/重启/满额淘汰均有实际断言失败，四阶段都运行完成；证据 `work/reverse-tests.log`、`build/ime-learning-if50nvid/summary.json`。随后逐字节恢复实现，工作区核对无遗留，并完成 `-Werror` 全量构建。
- 普通全量：【实测】289/289，431.85 秒，无跳过，退出 0；`INK_WERROR=ON`，构建和测试均为 `taskpolicy -c utility`、`-j2`。日志 `work/full-build.log`、`work/full-tests.log`。中枢测试用 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`，并指定当前检出的 `PYTHONPATH`。

先按[总规则](../codex-rules.md) §5 配置（增加上述中枢 Python 参数），再执行：

```sh
taskpolicy -c utility cmake --build build -j2
PYTHONPATH="$PWD/hub" INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
PYTHONPATH="$PWD/hub" INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

## 架构审视

- 【实现】电脑端生成先验，运行时查询仍是最长前缀优先；学习属于文本服务，存储、限额和淘汰复用数据服务，没有新抽象或线程。2 KiB 是记录载荷上限，索引、元数据和待写日志由现有数据服务在 PSRAM 管理。
- 【实测/实现】按钮与 AI 共用 `keyboard.pick` → `ink_text_pick`；记录默认不开放 `data.list/get`，因此 AI 能选候选但不能读原始偏好。可见候选仍按现有快照供 AI 使用。成员和密码保护沿用现有入口，未更改页面、快照或能力表。
