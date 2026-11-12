"""撤掉增量列表更新，要求既有页面夹具报告对应断言失败。"""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
build = Path(sys.argv[1]).resolve()
fixture = root / 'tests/ui_fix_a/transfer.c'
source = root / 'system/ui/shell/transfer_pages.c'
entry = next(e for e in json.loads((build / 'compile_commands.json').read_text()) if Path(e['file']) == fixture)
with tempfile.TemporaryDirectory(prefix='transfer-ui-reverse-', dir=build) as directory:
    work = Path(directory)
    text = source.read_text()
    assert text.count('if (row_only) {') == 1
    mutant = work / 'mutant.c'
    mutant.write_text(text.replace('if (row_only) {', 'if (false && row_only) {'))
    args = shlex.split(entry['command'])
    obj = work / 'mutant.o'
    original_object = args[args.index('-o') + 1]
    args[args.index('-o') + 1] = str(obj)
    args.insert(1, '-DINK_UI_FIX_SOURCE="' + str(mutant) + '"')
    subprocess.run(args, cwd=build, check=True, capture_output=True)
    link = shlex.split((build / 'CMakeFiles/sim-ui-fix-a-transfer.dir/link.txt').read_text())
    link = [str(obj) if arg == original_object else arg for arg in link]
    binary = work / 'mutant'
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    result = subprocess.run([sys.executable, str(root / 'tests/ui_fix_a/run.py'), '--build', str(build), '--case', 'transfer', '--binary', str(binary)],
                            cwd=root, capture_output=True, text=True, env={**os.environ, 'INK_KEEP_EVIDENCE': '1'})
    metrics = json.loads((build / "test-out" / ("ui-fix-a-transfer-" + work.name) / "100/metrics.json").read_text())
    output = result.stdout + result.stderr + str(metrics.get("warnings", []))
    assert result.returncode and ('first receipt retains' in output or 'second receipt preserves' in output), output[-5000:]
    print('incremental receipt rows: expected failure, exit=' + str(result.returncode))
