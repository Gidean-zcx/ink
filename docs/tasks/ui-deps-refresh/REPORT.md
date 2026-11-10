# ui-deps-refresh 执行报告

基线：`d6dbdce0a060429a7f4d4a58bee1057084d239e3`。日期：2026-11-11。

范围：只更新 `docs/system/ui-dependencies.md` 的 §4.2、§4.3 和本报告；不改产品代码，不访问设备，不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实现／实测】文档核对通过 | [§4.2、§4.3](../../system/ui-dependencies.md#42-界面框架)：13 行框架、64 行页面／浮层，每行有代码或报告出处；138 个登记名称全部列入，152 处本地链接及源码行号有效 | 下方文档检查；`rg -n 'PAGE\("|OVERLAY\("|\.name[[:space:]]*=' system/ui/shell system/apps system/ui/{search,file_picker,lock_mark}.c apps/home/home.c` |
| 2 | 【实现／实测】已列改动数与核查结果 | 依赖文档 +84／−26，共 110 行；原表 23 行改为 77 行，净增 54 行表项；“文档说有、代码没有”0 项，详见下节 | `git diff --numstat d6dbdce0 -- docs/system/ui-dependencies.md` |

## 没做到的

- ASan/UBSan 全量正在准备，尚未作为完成证据。
- 没有验证真实硬件、运营商、在线天气或真实中枢；此任务仅核对本地基线实现与已有报告，不修改缺失功能。

## 和设计不一致的

- 本次没有改设计合同或产品实现。页面存在但尚未完成的画板差异、语义迁移、真实后端与实机验收，均在对应行保留。

## 文档说有、代码没有

- **0 项（仅限任务指定的 §4.2、§4.3）**。原文的字体、快照、服务卡片、AI 面板、锁标记、设置表及 `call.*`／`message.*`／`camera.capture` 都能定位；没有把“没找到”写成完成。主要问题是文档落后于代码，而不是已有实现凭空消失。
- 纠正的旧说法包括：主题／图标／搜索为“无”；主页、备忘、日历、文件、壁纸为“全部”；电话缺联系人和通话记录；短信仅内存 16 条且限 118 字；相机只采集不保存；远程只有点坐标和一行快照。这些均已改为实际实现与剩余边界。
- 明确保留：两段提交仅测试显式调用；“听”无独立应用；`AiHome` 无历史对话；名字／打字接话、AI 记忆／运行记录、自动备份、更新说明及真实自动时区来源尚未接齐。它们不是本任务新实现的功能。

## 改过的已有测试

- 无。

## 要用户定的

- 暂无。

## 顺手发现

- §4.1 仍有旧状态，例如 Wi-Fi 行写“真机 HAL 没写”，[端口 Wi-Fi 实现](../../../ports/esp32s31/hal_wifi_s31.c)已存在；按任务边界未改。这不构成真实联网已验收。

## 新增的机制

- 无；产品代码预算使用 0 行。

## 要改的文档

- 已更新任务明确允许的 §4.2、§4.3。§4.1 及其他节保持基线原文；后续若继续刷新通用底座，应另行核对，避免把本次范围扩大成全篇实机验收。

## 验证记录

- 【实测】开工时 HEAD 等于指定基线，工作区干净；在现有任务分支继续。
- 【实测】§4.2、§4.3 以外原文逐字未变，产品代码 0 行，现有测试 0 行，没有新增任务、配置项、文件格式、状态机或框架。
- 【实测】文档反向对照：对指定基线执行相同出处检查，23 行中 21 行没有代码／任务报告链接；更新后 77／77 有出处。用源码的 `.name` 和 `PAGE`／`OVERLAY` 登记检查 138 个名称，当前无遗漏；六种远程画面状态另在对应行列出。没有撤改产品代码或测试来做对照。
- 【实测】普通配置、构建、全量均退出 0；原话 `100% tests passed out of 347`，381.11 秒。日志 `work/ui-deps-refresh/{configure,build,ctest}.log`；未启动窗口。
- S31：本次未编（只改文档）。`ink-codex/tools/gcccheck.py`：已读使用说明，本次无 C 文件变更，未运行，不能写成 GCC 通过。未运行 Linux CI。
- ASan/UBSan、最终差异／隐私复核：待完成。

复现配置沿用总规则第 5 节字体、Python、LVGL 参数；两套额外均指定现有 `INK_HUB_PYTHON` 和只读 SDL2／ImGui 源码。内存目录仅改为 `build-asan` 并增加 `INK_SANITIZE=ON`，两套都为 `INK_WERROR=ON`。构建和全量测试依次执行，utility 优先级，最多 `-j2`。

```sh
taskpolicy -c utility cmake --build build -j2
PYTHONPATH="$PWD/hub" SDL_VIDEODRIVER=dummy taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
```

文档检查（只读，不注册进 CTest，不依赖基线历史）：

```sh
python3 - <<'PY'
from pathlib import Path
import re
p = Path('docs/system/ui-dependencies.md')
s = p.read_text().split('### 4.2 ', 1)[1].split('\n## 5.', 1)[0]
rows = [l for l in s.splitlines() if l.startswith('| ') and not re.match(r'\| (?:---|能力 \||界面)', l)]
assert len(rows) == 77
assert all(re.search(r'\]\((?:../../(?:system|apps|tools|tests|drivers)/|../tasks/)', l) for l in rows)
links = re.findall(r'\]\(([^)]+)\)', s)
for href in links:
    f, _, anchor = href.partition('#')
    target = p.parent / f
    assert target.is_file(), href
    if re.fullmatch(r'L[0-9]+', anchor):
        assert 0 < int(anchor[1:]) <= len(target.read_text().splitlines()), href
files = [*Path('system/ui/shell').glob('*.c'), *Path('system/apps').glob('*/*.c')]
files += [Path(f) for f in ('system/ui/search.c', 'system/ui/file_picker.c', 'system/ui/lock_mark.c', 'apps/home/home.c')]
names = set()
for f in files:
    for m in re.finditer(r'\.name\s*=\s*"([A-Za-z_][A-Za-z0-9_]*)"|\b(?:PAGE|OVERLAY)\("([A-Za-z_][A-Za-z0-9_]*)"', f.read_text()):
        names.add(m.group(1) or m.group(2))
assert all('`' + n + '`' in s for n in names)
print(len(rows), 'rows;', len(links), 'links;', len(names), 'UI names; PASS')
PY
```
