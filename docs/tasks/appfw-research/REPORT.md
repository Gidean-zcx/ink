# 应用框架调研和设计报告

基线 `2163603`。仅四个文档：原样任务书、设计、调研、本报告；不写产品代码、不 push。开工 2026-11-05 21:15 左右（Asia/Singapore）；设计与测试已完成；终检时间及范围审查见末条。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| A1 | 通过 | 【实现】设计第 0～9 节；一页结论直接回答控件、语言、输入和 AI；最多五项候选 | `python3 work/appfw-research/check_docs.py` |
| A2 | 通过 | 【文档】调研 S01～S24 逐组列官方链接、日期、版本／快照边界；无来源的 S31 体积和时间写“没查到” | 同上；外部来源实际查询见调研索引 |
| A3 | 通过 | 【实测】177 个 PNG 全纳入浏览与文件名清单，重点组件放大；设计 §2 控件对照与逐项表；清单摘要在调研 §3 | 同上；图像浏览结果为人工核对 |
| A4 | 通过 | 【实现】设计 §4 字段规则、输入页面覆盖、四种避让比较、打断／恢复／成员／秘密规则和规范差异 | 同上；人工对照规范 §4、§7、§10、§12 |
| A5 | 通过 | 【实测】普通 91/91、ASan／UBSan 91/91，均无失败／跳过；仅允许的四个文档；耗时约一小时，终检见下 | `cmake --build build -j4`；`ctest --test-dir build --output-on-failure -j3`；内存版见下 |

【实测】构建使用总规则所列参数，普通／内存版仅构建目录与 `-DINK_SANITIZE=ON` 不同；两边额外相同配置 `-DINK_HUB_PYTHON="$PWD/work/appfw-research/test-python/bin/python"`。测试设 `PYTHONPATH="$PWD/hub"`，ctest 实际使用 `$INK_DEPS/build-deps/pyenv/bin/ctest`。构建和全量测试始终顺序执行；最大并行度分别 4、3。

- 普通：`cmake --build build -j4`；`PYTHONPATH="$PWD/hub" "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3`。最终 91/91，用时 33.54 秒。
- 内存：同配置加 `-DINK_SANITIZE=ON`；`cmake --build build-asan -j4`；同一测试命令换 `--test-dir build-asan`。最终 91/91，用时 82.07 秒。
- 首轮因 Python 缺依赖跳过三项；在 `work/` 建独立 Python 3.11.15 环境，从 `hub/uv.lock` 提取 18 个确切版本和哈希，以 `pip install --require-hashes --only-binary=:all:` 安装。随后一轮端到端因找不到 `ink_hub` 失败；设置上述 `PYTHONPATH` 后全量重跑通过。未改依赖清单、锁文件或测试，不把前两轮算全过。
- `gui-smoke` 是既有全量测试，用 `SDL_VIDEODRIVER=dummy`（`tests/simulator/gui_smoke.cmake:10`）；本次没有启动带窗口的模拟器。
- 原始日志在 `work/appfw-research/` 的 `configure.log`、`build.log`、`ctest-all.log`、`configure-asan.log`、`build-asan.log`、`ctest-asan.log`；构建、测试、联系表和临时 Python 环境均忽略且不提交。

## 没做到的

- 未在 S31 测运行时、刷新、输入或功耗；本任务是设计，已将对应测量列在设计 §8。Linux/gcc 未在本机运行；仅 macOS 构建和两轮测试实测，本轮没有新增 C 调用。
- 决策板无法读取其集合；不把建议冒充用户选择。Apple 部分网页仅有官方检索摘要，其他墨水屏设备的避让与刷新细节没查到，均在调研列缺口。
- 没有进行产品代码修复，故无“撤掉修复”的反向实验。后续原型反例与准入检查另有计划。

## 和设计不一致的

- 不改现行合同。设计 §4.5 单列：`ConfirmSecret` 与本人输入边界、声明式目录排除项、AI 联想已有画稿但未实现、全局搜索范围、D-105 与旧代码差距。处理均为建议或交原定任务。

## 改过的已有测试

- 无；无断言、预算、等待、跳过条件的改动。

## 要用户定的

- 仅设计 §7 的候选 1～5：语言组合、生成页组件目录、输入避让、秘密代填边界、脚本测量顺序。没有占用 D 编号或修改决定文件。

## 顺手发现

- 本地有 177 个 PNG，规范写 176；已列全并记录摘要，未删图或改规范。
- Moddable 当前许可含 GPL/LGPL，不能把 XS 的历史 Apache 标识套给整套 SDK；本次只比较，不引入。
- 基线 CMake 未使用任务命令中的 medium／bold 字体参数，配置有对应提示；不扩范围改字体管线。
- `confirm_for` 的旧规则、远程页缺语义、缺长文分页等仅作为现状和后续依赖，不在文档任务夹带修复。
- 【实测】文档校验通过：任务书逐字相同、第 0～9 节、五项候选、24 组来源、177 张图及摘要、46 个重点输入页、全部本地链接／锚点、JSON 示例和文件大小。没有改需求、想法、决定、backlog 或现有调研文件。
- 最终提交范围和提交差异隐私扫描：待记录提交后的复核结果。
