# 只读复核记录

2026-11-11。以下为源码、已有构建物和文档检查，不是编译、功能测试或上机测试。命令均从项目根目录运行；使用用户提供的 `INK_DEPS`，原厂只读克隆记为 `INK_UPSTREAM`，IDF 目录记为 `IDF_PATH`。

## E1 固定版本、寄存器表、分区及许可证

```bash
export INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus"
python3 - <<'PY'
import hashlib, os, re, subprocess
from pathlib import Path
u = os.environ['INK_UPSTREAM']
def git(*args):
    return subprocess.check_output(['git', '-C', u, *args])
def show(path):
    return git('show', '874538e:' + path)
print('upstream_HEAD', git('rev-parse', 'HEAD').decode().strip())
print('upstream_ref', git('rev-parse', '874538e').decode().strip())
prefix = 'main/boards/common/power_policy/s31_sleep/'
for name in ('s31_sleep.cc', 's31_sleep.h', 's31_sleep_sdk.cmake'):
    a = show(prefix + name)
    b = (Path('ports/esp32s31/vendor/s31_sleep') / name).read_bytes()
    print(name, 'byte_equal', a == b, 'vendor_sha256', hashlib.sha256(a).hexdigest())
def pairs(data):
    return [(int(a, 0), int(b, 0)) for a, b in re.findall(
        r'\{\s*(0x[\da-fA-F]+|\d+)\s*,\s*(0x[\da-fA-F]+|\d+)\s*\}', data)]
table = pairs(show('main/boards/common/camera/gc2145_vga_settings.h').decode())
for path in ('ports/esp32s31/vendor/gc2145/gc2145_vga_settings.h',
             'drivers/chips/gc2145/gc2145_vga_table.h'):
    values = pairs(Path(path).read_text())
    print(path, 'pairs', len(values), 'numeric_equal', values == table)
def partitions(data):
    return [tuple(x.strip() for x in line.split(',')[:5])
            for line in data.splitlines() if line.strip() and not line.startswith('#')]
ours = partitions(Path('ports/esp32s31/partitions.csv').read_text())
for name in ('partitions.csv', 'partitions/v1/16m.csv'):
    print(name, 'entries_equal', partitions(show(name).decode()) == ours)
for name in ('LICENSE', 'main/boards/common/camera/GC2145-LICENSE'):
    print(name, hashlib.sha256(show(name)).hexdigest())
PY
```

【实测】退出码0。HEAD为 `690a023c66862af103a3187464373ee7c5efcd14`，目标为 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`。睡眠h/CMake字节一致；cc的两处适配见报告C06。两份相机表各586对且均与原厂相同；两份原厂分区表均与项目九个分区条目相同。根MIT许可证SHA-256为 `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05`，相机表Apache许可证为 `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30`。

## E2 留存基线 ELF 的 Cache 候选路径

`B:` 为 `$INK_DEPS/build-s31/`。本次未生成/修改其中任何文件，未从设备取日志。

| 留存证据 | 本次读到的值 |
| --- | --- |
| project_description.json 的 project_version | `g87bf4865-261111110131` |
| CMakeCache.txt | `IDF_TARGET:STRING=esp32s31`；`INK_S31_LIGHT_SLEEP:BOOL=OFF` |
| IDF版本/提交 | `v6.1` / `fff9895c82d744c7237be8847347bdd1b07c6643` |
| ink-s31.elf SHA-256 | `958551f0a36593e57b6703f2a6781fb96e08f4c22f901eef4099f89dd0e0f85f` |
| sdkconfig SHA-256 | `1df547f8ac19e91adb84041d35e5b3fa1d3db389a745e507aad9e6c0547e467f` |

```bash
python3 - <<'PY'
import hashlib, json, os, re, subprocess
from pathlib import Path
b = Path(os.environ['INK_DEPS']) / 'build-s31'
print('version', json.loads((b / 'project_description.json').read_text())['project_version'])
for name in ('ink-s31.elf', 'sdkconfig'):
    h = hashlib.sha256()
    with (b / name).open('rb') as f:
        for chunk in iter(lambda: f.read(1048576), b''):
            h.update(chunk)
    print(name, h.hexdigest())
cache = (b / 'CMakeCache.txt').read_text()
objdump = re.search(r'^CMAKE_OBJDUMP:FILEPATH=(.+)$', cache, re.M).group(1)
names = ('rtos_int_enter', 'pxPortGetCoprocArea', 'hwlp_restore_if_used',
         'spi_flash_op_block_func', 'SysTickIsrHandler', 'esp_crosscore_isr',
         'esp_pm_impl_isr_hook', 'rtc_clk_cpu_freq_set_config_fast')
symbols = subprocess.check_output([objdump, '-t', str(b / 'ink-s31.elf')], text=True)
for line in symbols.splitlines():
    if line.split() and line.split()[-1] in names:
        print(line)
for name in ('rtos_int_enter', 'hwlp_restore_if_used'):
    text = subprocess.check_output([objdump, '-d', '--disassemble=' + name,
                                    str(b / 'ink-s31.elf')], text=True)
    print('DISASSEMBLY', name)
    for line in text.splitlines():
        if re.match(r'^[0-9a-f]+:', line):
            print(line)
PY
```

【实测】退出码0，段表/反汇编显示：

| 符号 | 地址 | 段 |
| --- | --- | --- |
| spi_flash_op_block_func | `0x2f00054c` | `.iram0.text` |
| esp_pm_impl_isr_hook | `0x2f005288` | `.iram0.text` |
| esp_crosscore_isr | `0x2f00576a` | `.iram0.text` |
| rtc_clk_cpu_freq_set_config_fast | `0x2f00ab8a` | `.iram0.text` |
| SysTickIsrHandler | `0x2f00eb18` | `.iram0.text` |
| hwlp_restore_if_used | `0x2f00ebd8` | `.iram0.text` |
| rtos_int_enter | `0x2f00ef44` | `.iram0.text` |
| pxPortGetCoprocArea | `0x40204c78` | `.flash.text` |

`rtos_int_enter` 在 `0x2f00efc8` 读CSR `0x7f1`，减3后非零则跳过；相等时在 `0x2f00efde` 调用 `0x40204c78`。返回地址为 `0x2f00efe2`。`hwlp_restore_if_used` 的调用点为 `0x2f00ebe8`。这只证明条件调用路径存在；没有故障现场CSR/PC，不能证明条件发生过。报告建议的后续IRAM映射、固定频率、优化级别对照均未执行。

## E3 来源行号复核

报告中的 `U:` 取 `git -C "$INK_UPSTREAM" show 874538e:<路径> | nl -ba`；项目路径取 `nl -ba <路径>`；IDF路径取 `nl -ba "$IDF_PATH/<路径>"`。行号来自上述固定版本，不依赖原厂克隆当前HEAD。本次只读文件核查/摘要命令正常完成；不存在的猜测路径已改用实际文件路径（如 `bq27220_gauge.cc`、`esp_cam_ctlr_dvp_cam.c`）。

## E4 在线版本

只读查询 [原厂main API](https://api.github.com/repos/CloudZao/Metalio-E-INK4-Plus/branches/main) 返回目标提交 `874538e`；没有新主分支提交可列入本块。GitHub网页抓取未成功，版本结论依据API成功响应。未查询或披露任何设备标识、网络凭据和真实密钥。
