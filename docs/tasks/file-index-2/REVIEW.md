# file-index-2 第 1 轮复查

复查对象：`63f95b1a078e051e754437b4c1210f09699e09d4`；基线：`291da6760931601e4783ebaddabc08e5bcab22eb`。

复查日期：2026-11-08。结论：**验收 1 部分通过，不能按原报告认定全部完成；发现 3 项中级问题，严重 0、高 0。** 普通与 ASan/UBSan 全量各 209 项通过。

本文件覆盖本轮旧内容。受版本管理的改动仅本报告；未修改产品、测试或依赖源码，未提交。构建及定向验证产物仅在已忽略的 `build/`、`build-asan/`。

## 验收核查

| 编号 | 状态 | 独立证据 |
| --- | --- | --- |
| 1 PNG、基线 JPEG、坏文件和超限输入 | 部分通过 | 【实测】已有 image-decode 通过，但保留 EOI 的残缺 JPEG 被成功解码，见 FI2-R1-001。 |
| 2 基于 PANEL_LEVEL 的 16 级灰度抖动 | 通过 | 【实测】image-dither 通过，渐变平均亮度误差 `0.0003515518044976407 < 0.01`，256 种纯灰块与重复结果检查通过；【实现】HAL 直接读取既有面板表。 |
| 3 缩略图与缓存上限、最旧淘汰 | 通过 | 【实测】image-cache 通过，含 FIFO、持续插入、内部堆不增长和释放检查。 |
| 4 对象版本变化失效 | 通过 | 【实测】image-version 通过，含失败新版本清除旧版本及独立上下文。 |
| 5 全量测试、内存检查、250 行预算 | 本机通过 | 【实测】普通、ASan/UBSan 构建退出码均为 0；两套全量均 209/209，通过耗时分别为 99.16 s、188.50 s，未跳过。ASan/UBSan 日志无诊断。【实现】产品接入 182 行，加第三方局部改动 22 行共 204 行，低于 250。Linux 未运行。 |

## 独立运行与范围核对

复用已有构建目录，检查了 `CMakeCache.txt`：两者 `INK_WERROR=ON`，内存检查目录另有 `INK_SANITIZE=ON`；解码器目标也确实带 `-fsanitize=address,undefined`。依赖来自用户指定的 `$INK_DEPS`，未安装或升级依赖。运行命令如下（每轮串行，构建 `-j3`、测试 `-j2`）：

```sh
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility env PYTHONPATH="$PWD/hub/src" PYTHONDONTWRITEBYTECODE=1 \
  "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
taskpolicy -c utility env PYTHONPATH="$PWD/hub/src" PYTHONDONTWRITEBYTECODE=1 \
  "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

- 日志：`build/review-build.log`、`build/review-ctest.log` 及 `build-asan/` 下同名文件；另扫描了 `build-asan/Testing/Temporary/LastTest.log`，没有 AddressSanitizer/UBSan 错误。
- 【实测】四项图片测试均在现有媒体执行器上执行。普通构建最大任务栈估计 7056 B，小于 8192 B；内外部堆分配失败均为 0，释放和不增长断言通过。此值仍是宿主机估计。
- 【实测】基线到 HEAD 为报告所列的 13 个相关文件；`system/services/file/`、既有测试、预算及共享文档未修改。`git diff --check` 通过。规定隐私模式用 `git diff 291da6760931601e4783ebaddabc08e5bcab22eb..HEAD | rg -n '/Users/|/private/tmp|[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}'` 扫描，退出码 1，零命中。
- 【实测】新 C/H 逐行计数 172，CMake 产品接入 7、HAL 查询 3；原始和本地 stb 文件的字节数、SHA-256 均与执行报告一致，MIT 条款保留。对比只增加/替换 22 行，用于把 PNG 工作区移入分配钩子；单文件约 304 KB，未超过 5 MB。
- 【实现】未发现通过放宽已有断言、提高预算、增加 sleep 或跳过测试取得通过的改动。

## 问题

### FI2-R1-001（中）：残缺 JPEG 仍返回成功

- 文件和行号：`system/services/media/image.c:14`、`system/services/media/image.c:35`；上游容错路径 `third_party/stb/stb_image.h:3611`、`:3631`；测试 `tests/media/image.c:42`。
- 【实测】对本仓库生成的 636 B 彩色 JPEG，保留前 177 B 再接 `FF D9`，解码仍返回成功；此时没有图像扫描数据。按所有截断点复核，共 445 个缩短输入返回成功，其中 444 个与原图像素不同。普通与 ASan/UBSan 探针结果一致。探针使用原产品静态库和真实模拟器 OSAL，未修改产品源码。
- 结果：末尾标记只证明两个字节存在，不能证明扫描完成；依赖还会在部分坏标记路径直接返回成功。验收 1 的“坏文件如实返回错误”不成立，当前测试只覆盖删掉尾标记的截断。
- 最小修法：在现有 JPEG 解码入口统一拒绝未完整完成图像扫描的输入，关闭这些容错成功路径；补一条保留 EOI、扫描数据缺失的回归，不在调用方加过滤器。
- 复现命令：运行下方“定向探针”命令。读取 `review.first_cut`、`review.accepted_cuts`、`review.changed_images`。

### FI2-R1-002（中）：图片计算没有计入确定性模拟器 CPU 成本

- 文件和行号：`system/services/media/image.c:35`、`:37`、`:55`、`:110`；对照既有成本接口 `platform/include/ink/osal.h:136`。
- 【实测】在任务默认的 annotated 模式中，1024×1024 图片执行完整抖动前后 `osal_now_us()` 差为 **0 μs**。同一探针中 17×9 PNG 解码为 55 μs，1024×1024 PNG 为 258 μs；后两项包含已有 CRC、分配的计费，不包含新增解码和逐像素转换工作。
- 结果：逐行调用 `osal_preempt_point()` 不等于 CPU 成本计费；确定性测试能验证结果重复，却不能证明新增图片负载下的响应或调度。按复查规则，模拟覆盖缺口列中级，不推断实机卡死。
- 最小修法：在媒体模块现有处理循环和解码边界补上按像素/字节计费的 OSAL 成本调用，复用现有机制，不新增任务、配置或调度框架；增加一次大图计算消耗非零模拟 CPU 时间的断言。
- 复现命令：运行下方同一探针，读取 `review.dither_large_us`；指标位于 `build/test-out/review-image/metrics.json`。

### FI2-R1-003（中）：坏 PNG 块长度测试实际只触发前面的 CRC 错误

- 文件和行号：`tests/media/image.c:46`～`:49`，对应保护 `system/services/media/image.c:18`。
- 【实现】第 46 行先损坏 IHDR 的 CRC；第 48 行继续在同一缓冲上损坏下一块长度，没有恢复 CRC。因此在读取下一块之前，完整性检查就已失败。
- 【实测】仅在内存中的源码副本删除 `len > n - pos - 12 || `，编译成单独对象并链接原测试；`image-decode` 仍全部通过，程序退出码 **0**。真实产品的长度保护还在，此处是测试覆盖缺口，不是已发生的越界。
- 最小修法：第 48 行前重新从 `color_png` 复制完整夹具，独立测试坏长度；撤掉长度保护时，该用例必须失败。
- 复现命令：下方“坏长度反向对照”；结果见 `build/review-negative-chunk_length.log`。

## 反向对照

没有直接运行会写回源码的 `tests/media/negative.py`。本轮将它的六种变体只在 Python 内存中生成，经编译器标准输入编成独立对象，再链接原测试对象和库；未覆盖正式构建对象。输出日志为 `build/review-negative-<名称>.log`。

| 撤掉的实现 | 独立结果（直接运行程序的退出码） |
| --- | --- |
| 完整性入口 | 2，`truncated JPEG` |
| 总像素上限 | 2，`oversized PNG area` |
| 面板非均匀亮度 | 2，`mean luminance error` |
| 满缓存淘汰 | 2，`over-capacity insertion succeeds` |
| 版本匹配 | 2，`expected decoded/dithered pixel value` |
| PNG 工作区移入 PSRAM（改用依赖中的原始头文件） | 1，`BUDGET FAIL warnings.count` |
| **另撤掉 PNG 块长度保护** | **0，原测试仍通过，见 FI2-R1-003** |

前六项支持执行报告中相应断言有效，但不能扩大为所有坏文件或所有资源负载都已覆盖。

### 坏长度反向对照（不修改源码）

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
from pathlib import Path
import subprocess, shlex, re, os
root = Path.cwd()
build = root / 'build'
source = (root/'system/services/media/image.c').read_text()
source = source.replace('len > n - pos - 12 || ', '')
flags = (build/'CMakeFiles/ink_system.dir/flags.make').read_text()
opts = []
for key in ['C_DEFINES', 'C_INCLUDES', 'C_FLAGS']:
    opts += shlex.split(re.search(r'^'+key+r' = (.*)$', flags, re.M)[1])
obj, exe = build/'review-negative-chunk_length.o', build/'review-negative-chunk_length'
subprocess.run(['cc', *opts, '-x', 'c', '-c', '-o', str(obj), '-'],
               input=source, text=True, check=True)
link = shlex.split((build/'CMakeFiles/sim-image.dir/link.txt').read_text())
link.insert(1, str(obj))
link = [str(exe) if x == 'sim-image' else x for x in link]
subprocess.run(link, cwd=build, check=True)
result = subprocess.run([str(exe), '--profile', 'simulator/profiles/metalio-plus.json',
    '--budget', 'tests/media/image.budget.json', '--out',
    'build/test-out/review-negative-chunk_length', '--end', '10s', '--quiet',
    '--no-random-outages'], env=dict(os.environ, INK_IMAGE_TEST='decode'))
print('exit =', result.returncode)
PY
```

## 定向探针（不修改源码）

以下命令复用当前构建的编译参数和静态库，只把探针对象、可执行文件和运行结果写进已忽略的构建目录。全部图片来自已有合成夹具，不读取真实家庭文件。先完成本文的普通构建与全量命令，再在仓库根目录运行：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
from pathlib import Path
import subprocess, shlex, re, json
root = Path.cwd()
build = root / 'build'
source = r'''
#define ink_main previous_ink_main
#include "tests/media/image.c"
#undef ink_main
static void review_run(void *arg) {
    (void)arg;
    ink_image_t expected, got;
    CHECK(DECODE(color_jpeg, &expected) == 0, "reference JPEG");
    uint8_t *bad = osal_malloc(sizeof color_jpeg, OSAL_MEM_EXTERNAL);
    CHECK(bad != NULL, "probe input");
    unsigned accepted=0, different=0, first=0;
    for (size_t cut=2; cut < sizeof color_jpeg-2; ++cut) {
        memcpy(bad, color_jpeg, cut); bad[cut]=255; bad[cut+1]=217;
        if (ink_image_decode(bad, cut+2, &got) == 0) {
            accepted++;
            if (!first) first=(unsigned)cut;
            if (got.width != expected.width || got.height != expected.height ||
                memcmp(got.pixels, expected.pixels, expected.width*expected.height)) different++;
            ink_image_free(&got);
        }
    }
    ink_probe_value("review.jpeg_size", sizeof color_jpeg);
    ink_probe_value("review.accepted_cuts", accepted);
    ink_probe_value("review.changed_images", different);
    ink_probe_value("review.first_cut", first);
    osal_free(bad); ink_image_free(&expected);
    uint64_t t=osal_now_us();
    CHECK(DECODE(color_png, &got) == 0, "small PNG");
    ink_probe_value("review.decode_small_us", osal_now_us()-t); ink_image_free(&got);
    t=osal_now_us();
    CHECK(DECODE(max_png, &got) == 0, "large PNG");
    ink_probe_value("review.decode_large_us", osal_now_us()-t);
    t=osal_now_us(); ink_image_dither(&got);
    ink_probe_value("review.dither_large_us", osal_now_us()-t); ink_image_free(&got);
    osal_sem_give(done);
}
void ink_main(void) {
    static ink_exec_job_t job;
    CHECK(ink_exec_init()==0 && ink_exec_ready(INK_EXEC_MEDIA)==0, "media ready");
    done=osal_sem_create(0,1);
    CHECK(done != NULL, "semaphore");
    CHECK(ink_exec_post(INK_EXEC_MEDIA,&job,review_run,NULL)==0,"post review");
    CHECK(osal_sem_take(done,9000000)==OSAL_OK,"review completed");
    osal_sem_delete(done);
}
'''
flags = (build/'CMakeFiles/sim-image.dir/flags.make').read_text()
opts = []
for key in ['C_DEFINES', 'C_INCLUDES', 'C_FLAGS']:
    opts += shlex.split(re.search(r'^'+key+r' = (.*)$', flags, re.M)[1])
obj, exe = build/'review-image.o', build/'review-image'
subprocess.run(['cc', *opts, '-I'+str(root), '-x', 'c', '-c', '-o', str(obj), '-'],
               input=source, text=True, check=True)
link = shlex.split((build/'CMakeFiles/sim-image.dir/link.txt').read_text())
link = [str(obj) if x == 'CMakeFiles/sim-image.dir/tests/media/image.c.o'
        else str(exe) if x == 'sim-image' else x for x in link]
subprocess.run(link, cwd=build, check=True)
subprocess.run([str(exe), '--profile', 'simulator/profiles/metalio-plus.json',
    '--out', 'build/test-out/review-image', '--end', '10s', '--quiet',
    '--no-random-outages'], check=True)
metrics = json.loads(Path('build/test-out/review-image/metrics.json').read_text())['metrics']
print({k: v for k, v in metrics.items() if k.startswith('review.')})
PY
```

## 过度设计与架构审视

【实现】未发现需删除的新机制。解码、完整性入口、抖动、4 槽 FIFO 和版本键均对应任务书；无额外任务、线程、重试、持久格式或配置层。本地 stb 副本虽大，但来源、许可与小范围工作区修改可核对，且反向验证支持栈问题，不能仅凭文件行数要求删除。

模块是内部图片机制：经 OSAL 分配、经 HAL 查询面板亮度，不依赖模拟器头文件或 UI 状态，分层测试通过。文件服务接线被任务明确留待合并；本轮没有新增用户动作或页面，未把内部解码另做成软总线能力是合理的。成员隔离目前只能确认独立缓存上下文；真实换成员/换卡销毁仍由未来接线负责，不能据此宣称整条产品链路已经验证。

## 证据边界

本任务没有新增用户界面，不适用 10 屏设计对照。只验证本地 macOS 代码与模拟器；Linux、设备画质/耗时/功耗未验证。本轮未碰设备、未调用外部服务、未更新共享决策与需求文档。上述问题均不依赖人为卡住线程；未将模拟器覆盖缺口升级为实机故障。

SEVERE=0 HIGH=0
