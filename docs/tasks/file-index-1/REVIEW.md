# file-index-1 第 1 轮复查

日期：2026-11-08。基线：`ed3d122665cf704bb77a962f1fe38d66f6febe01`；被审提交：`e757df46e8a4193f097190eea40f36c23a9a405b`。

**结论：严重 0、高 0、中 2。** 基本功能成立；共享目录查询有可复现的路径处理错误，后台删除测试有反向对照证实的缺口，建议修正后验收。只修改本报告，不修改产品代码、测试或设计文档，不提交；测试产物在忽略的构建目录。

## 验收核对

| 项 | 结论 | 独立证据 |
| --- | --- | --- |
| 1. 改名、移动保号，删除重建换号 | 已测场景通过，后台删除断言需补 | 【实测】T1 的改名、目录移动、单文件删除重建、跨进程重启、首次查询前移动通过。后台删除只断言作业结束，见 R2。 |
| 2. 图片、音频、文本、其它 | 通过 | 【实测】T1 覆盖大小写扩展名及四种分类；【实现】按扩展名分类，没有声称检测内容或保证可解码。 |
| 3. 按类型查询、分页 | 部分通过 | 【实测】T1 的 C 接口和能力调用首末页通过；合法的无关成员长路径会使共享子目录查询失败，见 R1。 |
| 4. 拔卡、换卡状态与旧号隔离 | 已测场景通过 | 【实测】T1 中拔卡返回 `NO_CARD/unavailable`，换卡首次查询返回 `CARD_CHANGED/changed`，同路径不同编号，换回恢复原号；孩子查询成人编号被拒绝。 |
| 5. 索引位置及上限 | 通过 | 【实现】设备集合 `file.index` 位于 `/user/sys/file.index.*`；记录和排序索引由数据服务以 `OSAL_MEM_EXTERNAL` 分配。所有卡合计最多 512 个活跃记录，单条内容 216 字节，内容上限 110592 字节，不含数据服务头、排序表和墓碑等开销。卡上 `/content/.file-index-card` 存 16 字节身份。 |
| 6. 能力注册、全量/内存检查、250 行预算 | 注册和预算通过；全量有已披露的生成文档前置条件 | 【实测】普通、ASan/UBSan 构建均退出 0；两套全量各 205/206、无跳过，唯一失败均为 `capability-table-doc`。在构建目录生成能力表并检查运行时，253 项定义、193 个能力、16 个设置匹配。【实现】新增产品代码 217 行，未改预算。 |

能力表失败与 `REPORT.md:64` 已说明的“生成后测试、恢复生成文档、不提交”一致，也符合任务书不提交生成文档的要求，**不算虚报或新增缺陷**。本次遵守只修改 `REVIEW.md` 的范围，没有修改正式生成文档，因此没有把实际的 205/206 写成 206/206。

## 问题

### R1｜中｜无关成员的长路径会阻断共享目录查询

- **位置**：`system/services/file/file_service.c:1517`、`:1520`。
- **复现命令**：执行下文 T4 的 `long-path` 探针。创建并索引合法的 121 字节 `/me/...txt` 路径，再创建 `/family/review/public.txt`，分别以成人和现有 `kid` 查询该共享子目录。
- **结果【实测】**：成人查询成功，孩子查询返回 `INK_FS_ERR_PATH (-1)`。`test.pass=6`、`test.fail=1`、`test.finished=1`；失败为 `child can query unrelated shared subtree (r=-1)`。原始证据：`build/services-finish-j9jtwfl2/long-path/out/metrics.json`。
- **原因/影响【实现】**：循环先转换每条全局记录的虚拟路径，再检查是否属于查询目录。成人长路径变成更长的 `/members/owner/...`，超出 128 字节就中止查询，尽管它不属于共享目录。孩子查询自己的目录也会受同一记录影响。这是查询可用性问题，未发现因此泄露成人内容。
- **最小修法**：将已有的 `root && !index_under(e.path, phys)` 过滤移到虚拟路径转换之前。只改 `fs_index_for`，不增加缓存或其它机制；用上述场景补回归。

### R2｜中｜后台删除的编号失效没有被回归测试覆盖

- **位置**：`tests/services_finish/file_index.c:62`、`:63`；对应产品代码 `system/services/file/file_service.c:835`、`:857`。
- **复现命令**：执行 T4 的 `reverse-job-delete` 反向对照；仅从编译输入撤掉 `delete_step` 两处 `index_edit(..., NULL)`，运行原有四组用例。
- **结果【实测】**：四组仍全部通过，检查数仍为 **29 + 13 + 11 + 518 = 571**。原始证据：`build/services-finish-j9jtwfl2/reverse-job-delete/` 下四组 `metrics.json`。
- **原因/影响【实现】**：目录删除只验证作业结束，没有保存子文件编号，也没有验证旧编号失效及同路径重建后换号；单文件后台删除分支没有覆盖。即使这两条正常删除路径完全不维护编号，现有测试仍报通过。当前产品代码确实有清理，不将测试缺口冒充产品故障。
- **最小修法**：扩展已有用例，保存编号，后台删除并重建同路径，断言旧编号不可查、新编号不同；单文件后台删除使用相同断言。不新增测试框架。

## 品味和规模

【实测】基线差异中产品代码新增 217 行：公开接口 10、内部声明 1、能力与处理函数 44、文件服务 162；低于 250 行。无删除测试、放宽既有断言/预算或跳过测试。新增用例沿用 1800 ms 虚拟启动等待；撤掉编号维护后仍会失败，没有证据表明等待掩盖了维护缺失。100 s 虚拟时长只用于新增用例。

【实现】持久集合、16 字节卡身份和索引状态直接对应任务要求。未新增线程、常驻任务、执行器、后台重试或配置项；没有应为本任务删除的新机制。R1 只调整既有过滤顺序，R2 只补已有测试。

## 架构与范围

- 【实现】文件身份、类型、权限归文件服务；持久化和 PSRAM 管理复用数据服务；卡操作走 HAL。`file.index`、`file.lookup` 共用服务实现并注册在后台执行器，按钮和 AI 走同一路径；原始集合未向 AI 开放，避免直接暴露物理路径。
- 【实测】构建边界、确定性、既有成员隔离和能力清单相关测试均通过。未改并行任务的 policy、UI、图片/缩略图代码；设计文档只补能力/状态清单，没有修改约束迁就实现。没有新增页面，截图对照不适用。
- 【实测】指定隐私正则检查基线补丁，无命中；`git diff --check` 通过。没有设备操作、外部服务调用、push 或提交；没有读取硬件备份或设计稿。

## 命令与结果

使用用户给定的 `INK_DEPS`，不在报告保存本机绝对路径。配置使用总规则 §5 的字体、Python、LVGL 参数，另指定已有的 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`；ASan 目录另加 `-DINK_SANITIZE=ON`，两者均保留 `INK_WERROR=ON`。各轮构建和全量测试串行执行。

### T1：任务验收

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/services_finish/run.py --build build file_index
```

【实测】四组通过，共 571 个检查；两套全量中的 `file-index-regressions` 也通过。

### T2、T3：普通及内存检查

```sh
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

【实测】两次构建退出 0；两次 CTest 退出 8，均只有 `capability-table-doc` 失败，分别用时 94.73 s、185.21 s。日志：`build/file-index-review-ctest.log`、`build-asan/file-index-review-ctest.log`。ASan/UBSan 没有报错。

正式生成文档保持不动，另外验证生成结果和运行时：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output build/file-index-review-capabilities.md
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --check --output build/file-index-review-capabilities.md --runtime build/test-out/capability-table/capability-inventory.json
```

【实测】两条命令均退出 0。

### T4：边界探针与反向对照

下面命令读取本报告中的探针，复用现有测试编译/链接参数。源码只在内存替换并从标准输入编译，不改工作树；产物写入新的 `build/services-finish-*`。模拟器退出 0 不等于断言通过，脚本同时核对 `test.fail` 和 `test.finished`。

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" - <<'CHECK'
from pathlib import Path
report = Path('docs/tasks/file-index-1/REVIEW.md').read_text()
marker = '<!-- review-probes -->' + '\n```python\n'
code = report.split(marker, 1)[1].split('\n```', 1)[0]
exec(compile(code, '<review-probes>', 'exec'))
CHECK
```

<!-- review-probes -->
```python
from pathlib import Path
import json, subprocess, sys

root = Path.cwd()
runner = root / 'tests/services_finish/run.py'
sys.argv = [str(runner), '--build', 'build', 'file_index']
scope = {'__file__': str(runner), '__name__': 'review_driver'}
exec(compile(runner.read_text().split('\nfailed = False\n')[0], str(runner), 'exec'), scope)
real_run = subprocess.run
mode = ''
probe = r"""
static void review_probe(void) {
    char path[INK_FS_PATH_MAX];
    snprintf(path, sizeof path, "/me/%058d/%054d.txt", 0, 0);
    CHECK(strlen(path) < INK_FS_PATH_MAX && ink_fs_write_atomic(path, "SYNTH_PRIVATE", 13) == 0, "create valid 121-byte private path");
    uint32_t id = find("/me", path, INK_FS_TEXT);
    CHECK(id != 0, "index long private path");
    CHECK(ink_fs_write_atomic("/family/review/public.txt", "SYNTH_PUBLIC", 12) == 0, "create shared file");
    uint32_t cursor = 0; ink_fs_entry_t entry; int r;
    r = ink_fs_index("/family/review", INK_FS_TEXT, &cursor, &entry);
    CHECK(r == 1, "owner can query shared subtree (r=%d)", r);
    CHECK(ink_member_switch("kid") == 0, "switch to existing child");
    cursor = 0;
    r = ink_fs_index("/family/review", INK_FS_TEXT, &cursor, &entry);
    CHECK(r == 1, "child can query unrelated shared subtree (r=%d)", r);
}
"""

def intercepted(cmd, *args, **kwargs):
    if isinstance(cmd, list) and '-c' in cmd:
        at = cmd.index('-c') + 1
        path = Path(cmd[at])
        source = path.read_text()
        if mode == 'long-path' and path.name == 'file_index.c':
            source = source.replace('static void run(void *arg) {', probe + '\nstatic void run(void *arg) {')
            source = source.replace('    const char *stage = getenv("INK_INDEX_STAGE");', '    review_probe(); ink_probe_count("test.finished"); return;\n    const char *stage = getenv("INK_INDEX_STAGE");')
        if mode.startswith('reverse-') and path.name == 'file_index_boundary.c':
            service = (root / 'system/services/file/file_service.c').read_text()
            if mode == 'reverse-all':
                service = service.replace('static int index_edit(const char *from, const char *to) {', 'static int index_edit(const char *from, const char *to) { return 0;')
            else:
                service = service.replace('        else if (index_edit(j->from, NULL)) job_failed(j, INK_FS_ERR_IO);', '')
                service = service.replace('        if (index_edit(child, NULL)) { job_failed(j, INK_FS_ERR_IO); hal_fs_closedir(d); return true; }', '')
            source = source.replace('#include INK_TEST_FILE_SOURCE', service)
        cmd = cmd[:]
        cmd[at] = '-'
        cmd[at:at] = ['-x', 'c']
        cmd += ['-I' + str(path.parent)]
        kwargs.update(input=source, text=True)
    return real_run(cmd, *args, **kwargs)

subprocess.run = intercepted
base = Path(scope['work'])
for mode in ('long-path', 'reverse-all', 'reverse-job-delete'):
    work = base / mode
    work.mkdir()
    scope['work'] = work
    binary, end = scope['compile_suite']('file_index')
    print('MODE', mode, 'ARTIFACTS', work.relative_to(root), flush=True)
    if mode == 'long-path':
        out = work / 'out'
        cmd = [str(binary), '--profile', str(root / 'simulator/profiles/metalio-plus.json'), '--assets', str(root / 'build/assets'), '--content', str(root / 'build/test-content'), '--out', str(out), '--end', end, '--quiet', '--no-random-outages']
        result = real_run(cmd, cwd=root, capture_output=True, text=True, timeout=180)
        (work / 'stderr.log').write_text(result.stderr)
        metrics = json.loads((out / 'metrics.json').read_text())
        m = metrics['metrics']
        print('exit', result.returncode, 'pass', m.get('test.pass'), 'fail', m.get('test.fail'), 'finished', m.get('test.finished'))
        for warning in metrics.get('warnings', []):
            if 'FAIL' in str(warning): print(warning)
        assert result.returncode == 0 and m.get('test.finished') == 1 and m.get('test.fail') == 1
    else:
        ok = scope['review_matrix']('file_index', binary)
        print('MATRIX_OK', ok, flush=True)
        assert ok == (mode == 'reverse-job-delete')
subprocess.run = real_run
```

【实测】全撤 `index_edit` 时，改名、目录移动、重启前置移动、满额删除恢复四处断言失败，证明主维护路径测试有效；只撤后台删除维护时四组仍全过，证明 R2。反向变体均未写入产品源码。

## 证据边界

本机 macOS 模拟器验证；未做 Linux、真实 FAT、实机性能和真实拔卡测试。任务报告已披露的电脑端直接改名/删除重建、跨卷断电原子性，本轮没有扩展成新开发要求。最终工作树仅本报告变化，未修复上述问题。

SEVERE=0 HIGH=0
