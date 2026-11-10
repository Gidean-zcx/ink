#!/usr/bin/env python3
"""Compile the actual power task configuration and OSAL creation path in isolation."""
import argparse
from pathlib import Path
import re
import resource
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--negative", action="store_true")
    args = parser.parse_args()
    sdk_flags = []
    if sys.platform == "darwin":
        sdk_flags = ["-isysroot", subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()]
    root = Path(__file__).resolve().parents[3]
    service = (root / "system/services/power/power_service.c").read_text()
    osal = (root / "ports/esp32s31/osal.c").read_text()
    start = service.index('    osal_task_cfg_t cfg = {.name = "power"')
    config = service[start:service.index("    return osal_task_create", start)]
    defines = "\n".join(re.findall(r"^#define PW_(?:STACK|PRIO) .*$", service, re.M))
    start = osal.index("osal_err_t osal_task_create(")
    create = osal[start:osal.index("\nvoid osal_task_delay_us", start)]
    harness = r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "ink/osal.h"
#define configNUMBER_OF_CORES 2
#define tskNO_AFFINITY (-1)
#define pdPASS 1
#define MALLOC_CAP_8BIT 1u
#define MALLOC_CAP_INTERNAL 2u
#define MALLOC_CAP_SPIRAM 4u
struct osal_task { osal_task_fn fn; void *arg; char name[16]; };
static void power_task(void *arg) { (void)arg; }
static void task_entry(void *arg) { (void)arg; }
void *osal_calloc(size_t n, size_t size, uint32_t caps) {
    assert(caps == OSAL_MEM_INTERNAL);
    return calloc(n, size);
}
void osal_free(void *p) { free(p); }
static int xTaskCreatePinnedToCoreWithCaps(osal_task_fn fn, const char *name,
        uint32_t stack, void *arg, int priority, void *out, int core, uint32_t caps) {
    const osal_task_t *task = arg;
    assert(fn == task_entry && task->fn == power_task && !out);
    assert(strcmp(name, "power") == 0 && stack == 4096);
    assert(caps == (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
#ifdef ESP_PLATFORM
    assert(core == 0 && priority == OSAL_PRIO_IDLE + 3);
#else
    assert(core == tskNO_AFFINITY && priority == 4);
#endif
    return pdPASS;
}
'''
    main_code = r'''
    osal_task_t *task = NULL;
    assert(!(cfg.flags & OSAL_TASK_STACK_EXTERNAL));
    assert(osal_task_create(&cfg, &task) == OSAL_OK && task);
    osal_free(task);
    return 0;
}
'''
    cases = [("s31", config, create, True, False), ("sim-config", config, create, False, False)]
    if args.negative:
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        for name, old, new in [
            ("unpinned", "cfg.core = 0;", "cfg.core = -1;"),
            ("wrong-core", "cfg.core = 0;", "cfg.core = 1;"),
            ("priority", "cfg.priority = OSAL_PRIO_IDLE + 3;", "cfg.priority = 4;"),
            ("external-stack", ".stack_bytes = PW_STACK", ".flags = OSAL_TASK_STACK_EXTERNAL, .stack_bytes = PW_STACK"),
            ("stack-size", ".stack_bytes = PW_STACK", ".stack_bytes = 3072"),
        ]:
            assert config.count(old) == 1, name
            cases.append((name, config.replace(old, new), create, True, True))
        old = "? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL"
        assert create.count(old) == 1
        cases.append(("osal-caps", config, create.replace(old, "? MALLOC_CAP_INTERNAL : MALLOC_CAP_SPIRAM"), True, True))
    args.build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="task-config-", dir=args.build.resolve()) as temp:
        work = Path(temp)
        for name, cfg, body, s31, negative in cases:
            source = work / "task.c"
            source.write_text(harness + "\n" + defines + "\n" + body + "\nint main(void) {\n" + cfg + main_code)
            command = [args.cc, "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-Wall", "-Wextra", "-Werror", "-UNDEBUG",
                       "-I", str(root / "platform/include"), str(source), "-o", str(work / "test")]
            command += sdk_flags
            if s31:
                command.append("-DESP_PLATFORM")
            if args.sanitize:
                command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
            compiled = subprocess.run(command, capture_output=True, text=True)
            assert compiled.returncode == 0, f"{name}: compile={compiled.returncode}\n{compiled.stderr}"
            result = subprocess.run([str(work / "test")], cwd=work, capture_output=True, text=True)
            assert (result.returncode != 0) == negative, f"{name}: test={result.returncode}\n{result.stderr}"
            evidence = result.stderr.strip().splitlines()
            print(f"{name}: compile=0, test={result.returncode}" + (f"; {evidence[-1]}" if evidence else ""))


if __name__ == "__main__":
    main()
