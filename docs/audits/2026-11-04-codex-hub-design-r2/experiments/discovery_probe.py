#!/usr/bin/env python3
"""Extract a registered subset and run the unchanged production ID serializer.

No firmware build, network, hardware or production-file writes. Temporary C and
binary files live in this directory and are removed. This is a function probe,
not a complete simulator integration test.
"""
import json
import pathlib
import re
import subprocess
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[3]
REV = "a81c1b8ed805c044bcc24a120c14bb8e3a4d011e"


def source(path):
    return subprocess.check_output(["git", "show", REV + ":" + path], cwd=ROOT, text=True)


def balanced(text, start):
    """Return balanced braces, ignoring quoted C strings and comments."""
    depth, quoted, escape, line_comment, block_comment = 0, None, False, False, False
    i = start
    while i < len(text):
        c, nxt = text[i], text[i:i + 2]
        if line_comment:
            if c == "\n":
                line_comment = False
        elif block_comment:
            if nxt == "*/":
                block_comment = False
                i += 1
        elif quoted:
            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == quoted:
                quoted = None
        elif nxt == "//":
            line_comment = True
            i += 1
        elif nxt == "/*":
            block_comment = True
            i += 1
        elif c in "\"'":
            quoted = c
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1], i + 1
        i += 1
    raise AssertionError("unbalanced source")


def function(text, prefix):
    start = text.index(prefix)
    body = text.index("{", start)
    _, end = balanced(text, body)
    return text[start:end]


paths = [
    "system/policy/cap.c",
    "system/policy/confirm.c",
    "system/policy/system_actions.c",
    "system/services/data/data_caps.c",
    "system/services/file/file_caps.c",
    "system/services/time/time_caps.c",
    "system/services/notify/notify_caps.c",
    "system/services/comm/comm_service.c",
    "system/services/audio/audio_service.c",
    "system/services/camera/camera_service.c",
    "system/services/net/net_service.c",
]
names, evidence = [], []
for path in paths:
    text = source(path)
    for match in re.finditer(r"static const ink_cap_t\s+(\w+)\s*\[\]\s*=\s*\{", text):
        table = match.group(1)
        assert re.search(r"ink_cap_register\(\s*" + table + r"\s*,", text), (path, table)
        body, _ = balanced(text, match.end() - 1)
        pos = 1
        count = 0
        while pos < len(body) - 1:
            start = body.find("{", pos)
            if start < 0:
                break
            entry, pos = balanced(body, start)
            name = re.search(r'\.id\s*=\s*"([^"\n]+)"', entry)
            if not name:
                continue
            # Conservative subset: only global callables with default exposure.
            if re.search(r"\.(activity|expose)\s*=", entry) or "INK_FORM_EVENT" in entry:
                continue
            names.append(name.group(1))
            count += 1
        evidence.append({"path": path, "table": table, "selected": count})
assert len(set(names)) == len(names)
serialized = json.dumps(names, separators=(",", ":"))
assert len(serialized.encode()) >= 768
cap = source("system/policy/cap.c")
fn = function(cap, "int ink_cap_ids_json(")
stub = r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ink/sys/json.h"
enum { INK_SRC_AI = 2, INK_FORM_EVENT = 2 };
typedef struct { const char *id; int form; } ink_cap_t;
static bool on_its_page(const ink_cap_t *c) { (void)c; return true; }
static bool exposed(const ink_cap_t *c, int src) { (void)c; (void)src; return true; }
void osal_cost_units(const char *name, uint64_t n) { (void)name; (void)n; }
'''
stub += "static const ink_cap_t CAPS[] = {" + ",".join("{" + json.dumps(n) + ",0}" for n in names) + "};\n"
stub += "static const ink_cap_t *REG[256];\nstatic int nreg;\n" + fn
stub += r'''
int main(void) {
    nreg = (int)(sizeof CAPS / sizeof CAPS[0]);
    for (int i = 0; i < nreg; i++) REG[i] = &CAPS[i];
    char small[768], big[16384];
    int a = ink_cap_ids_json(small, sizeof small);
    int b = ink_cap_ids_json(big, sizeof big);
    printf("{\"count\":%d,\"capacity_768_rc\":%d,\"required_bytes_without_nul\":%d,\"ai_actions\":%s}\n",
           nreg, a, b, a > 0 ? small : "[]");
    return a == -1 && b >= 768 ? 0 : 1;
}
'''
with tempfile.TemporaryDirectory(prefix="discovery-", dir=HERE) as tmp:
    tmp = pathlib.Path(tmp)
    (tmp / "probe.c").write_text(stub)
    (tmp / "ink_json.c").write_text(source("system/util/ink_json.c"))
    command = ["cc", "-std=c11", "-I" + str(ROOT / "system/include"),
               "-I" + str(ROOT / "platform/include"), "-I" + str(ROOT / "third_party/jsmn"),
               str(tmp / "probe.c"), str(tmp / "ink_json.c"), "-o", str(tmp / "probe")]
    subprocess.run(command, check=True, cwd=ROOT)
    observed = json.loads(subprocess.check_output([str(tmp / "probe")], text=True))
assert observed["required_bytes_without_nul"] == len(serialized.encode())
print(json.dumps({"revision": REV, "evidence": evidence, "observed": observed,
                  "boundary": "production serializer + registered global subset; not full firmware"},
                 ensure_ascii=False, indent=2))
