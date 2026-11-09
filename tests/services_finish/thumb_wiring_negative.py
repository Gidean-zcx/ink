"""Remove wiring guarantees one at a time; the same integration test must fail."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', default='build')
    args = parser.parse_args()
    source = ROOT / 'system/services/file/file_service.c'
    original = source.read_bytes()
    text = original.decode()
    work = ROOT / 'work' / 'thumb-negative'
    work.mkdir(parents=True, exist_ok=True)
    size_term = '((uint64_t)(r ? 0 : e.size) * UINT64_C(0x9e3779b97f4a7c15))'
    cases = [
        ('mtime', '(uint64_t)stamp ^ ' + size_term, size_term, 'mtime alone changes version'),
        ('size', '(uint64_t)stamp ^ ' + size_term, '(uint64_t)stamp', 'size alone changes version'),
        ('delete', '        if (!to) ink_thumb_cache_remove(FS.thumbs, rec.id);\n', '',
         'deletion immediately frees cached pixels'),
        ('identity', 'resolve_for(c, e.path, false, phys, sizeof phys)',
         'resolve_for(c, "/me/SYNTH/one.png", false, phys, sizeof phys)',
         'thumbnail status failed, expected done'),
    ]
    command = [sys.executable, str(ROOT / 'tests/services_finish/run.py'), '--build', args.build, 'thumb_wiring']
    try:
        for name, before, after, evidence in cases:
            assert text.count(before) == 1, name
            source.write_text(text.replace(before, after))
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180,
                                    env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
            output = result.stdout + result.stderr
            (work / (name + '.log')).write_text(output)
            assert result.returncode == 1 and evidence in output, (name, result.returncode, output[-3000:])
            print(f'{name}: expected failure, exit=1, {evidence}', flush=True)
            source.write_bytes(original)
    finally:
        source.write_bytes(original)
    subprocess.run(command, cwd=ROOT, check=True)


if __name__ == '__main__':
    main()
