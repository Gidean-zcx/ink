#!/usr/bin/env python3
"""Make observer/scheduling-only copies of existing tests under build-audit.

surface: prints the actual tools/list replies before parsing them.
notify: delays the existing interleaving from the first notification unlock to
the fifth (last membership check, just before announce). No product logic changes.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[4]
dest = root / 'build-audit/observer-copies'
dest.mkdir(parents=True, exist_ok=True)
for name in ('ai_surface', 'races'):
    source = root / ('tests/integration/' + name + '.c')
    text = source.read_text()
    text = text.replace('#include "../../system/boot/main.c"', '#include "' + str(root / 'system/boot/main.c') + '"')
    text = text.replace('#include "whole_product.h"', '#include "' + str(root / 'tests/integration/whole_product.h') + '"')
    if name == 'ai_surface':
        old = '        cJSON *page = cJSON_Parse(reply);'
        assert text.count(old) == 1
        text = text.replace(old, '        fprintf(stderr, "AUDIT_PAGE %s\\n", reply);\n' + old)
        filename = 'surface_probe.c'
    else:
        text = text.replace('static int g_mode;', 'static int g_mode;\nstatic int g_notify_unlocks;')
        old = 'if (g_notify_armed && osal_task_self() == g_test_task)'
        assert text.count(old) == 1
        text = text.replace(old, 'if (g_notify_armed && osal_task_self() == g_test_task && ++g_notify_unlocks == 5)')
        filename = 'late_notify_probe.c'
    (dest / filename).write_text(text)
