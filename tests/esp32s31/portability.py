#!/usr/bin/env python3
"""Check format fixes with the S31 compiler, then compile their baseline versions."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', type=Path)
parser.add_argument('--baseline', default='2228df7de99de9fea15fddc22868a0de1caae770')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
build = args.build.resolve()
evidence = build / 'portability-check'
evidence.mkdir(exist_ok=True)
changed = subprocess.check_output(['git', 'diff', '--name-only', args.baseline, '--', 'system'], cwd=root, text=True).splitlines()
commands = {Path(c['file']).resolve(): c for c in json.loads((build / 'compile_commands.json').read_text())}
for name in changed:
    source = root / name
    command = commands[source]
    words = shlex.split(command['command'])
    flags, index = [], 0
    while index < len(words):
        word = words[index]
        if word in ('-o', '-MF', '-MT', '-MQ'):
            index += 2
            continue
        if word not in ('-c', '-MD', '-MMD', '-fstack-usage', '-fcallgraph-info=su', str(source)):
            flags.append(word)
        index += 1
    flags += ['-fsyntax-only', '-Werror=format', '-fdiagnostics-color=never', '-iquote', str(source.parent)]
    original = evidence / (source.parent.name + '-' + source.name)
    original.write_bytes(subprocess.check_output(['git', 'show', args.baseline + ':' + name], cwd=root))
    for version, path, expected in [('fixed', source, 0), ('baseline', original, 1)]:
        result = subprocess.run(flags + [str(path)], cwd=command['directory'], text=True, capture_output=True)
        (evidence / (original.name + '.' + version + '.log')).write_text(result.stderr)
        errors = result.stderr.count('error: format')
        print(name, version, 'exit=' + str(result.returncode), 'format_errors=' + str(errors))
        if (result.returncode == 0) != (expected == 0) or (expected and not errors):
            raise SystemExit('Unexpected result: ' + name + ' ' + version)
