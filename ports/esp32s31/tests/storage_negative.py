"""Run isolated reverse controls without Git history or device access."""
import argparse
import json
from pathlib import Path
import resource
import shlex
import subprocess
import tempfile


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('build', type=Path)
    args = ap.parse_args()
    build = args.build.resolve()
    tests = Path(__file__).resolve().parent
    root = tests.parents[2]
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    db = json.loads((build / 'compile_commands.json').read_text())
    source = (tests.parent / 'hal_storage_s31.c').read_text()
    mutations = [
        ('geometry', '.block_count = 100', '.block_count = 101', 'rw'),
        ('read-only', 'v && CONFIG_INK_SD_READONLY && flags', 'v && false && flags', 'ro'),
        ('sd-write-barrier', 'CONFIG_INK_SD_READONLY || !__atomic_load_n', 'false || !__atomic_load_n', 'ro'),
        ('card-notify', 'if (changed && cb) cb(ctx);', '(void)changed; (void)cb; (void)ctx;', 'rw'),
    ]
    with tempfile.TemporaryDirectory(prefix='storage-negative-', dir=build) as temp:
        work = Path(temp)
        (work / 'tests').mkdir()
        (work / 'tests/storage.c').write_text((tests / 'storage.c').read_text())
        for name, old, new, mode in [('baseline-rw', '', '', 'rw'), ('baseline-ro', '', '', 'ro')] + mutations:
            assert not old or source.count(old) == 1, name
            (work / 'hal_storage_s31.c').write_text(source.replace(old, new) if old else source)
            entry = next(e for e in db if e['file'] == str(tests / 'storage.c') and f'storage-{mode}.dir' in e['command'])
            command = shlex.split(entry['command'])
            for flag in ('-c', '-o'):
                if flag == '-o':
                    i = command.index(flag); del command[i:i + 2]
                else:
                    command.remove(flag)
            command[command.index(str(tests / 'storage.c'))] = str(work / 'tests/storage.c')
            command += [str(root / 'third_party/littlefs/lfs.c'), str(root / 'third_party/littlefs/lfs_util.c'), '-o', str(work / 'test')]
            compiled = subprocess.run(command, capture_output=True, text=True)
            assert compiled.returncode == 0, compiled.stderr
            result = subprocess.run([str(work / 'test')], cwd=work, capture_output=True, text=True)
            assert (result.returncode != 0) == bool(old), f'{name}: unexpected {result.returncode}\n{result.stderr}'
            print(f'{name}: compile=0 test={result.returncode}')
        # Mutate a source copy and rerun the actual two-pack lookup executable.
        asset_source = (tests.parent / 'hal_assets_s31.c').read_text()
        for name, old, new in [
            ('first-pack', 'find(g_fonts, 4608 * 1024, name, size)', 'find(NULL, 4608 * 1024, name, size)'),
            ('second-pack', 'return p ? p : find(ink_embedded_start,', 'return p ? p : find(NULL,'),
            ('inflate', 'if (stored & 0x80000000u)', 'if (false)'),
            ('asset-unmap', 'osal_free((void *)data)', '(void)data'),
        ]:
            changed = work / 'assets.c'; changed.write_text(asset_source.replace(old, new))
            command = ['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-I', str(tests / 'storage_stub'),
                       '-I', str(root / 'platform/include'), str(tests / 'assets.c'), str(changed),
                       '-I', str(root / 'third_party'), str(build / 'libtest-asset-miniz.a'),
                       str(build / 'embedded.S'), '-o', str(work / 'test')]
            c = subprocess.run(command, capture_output=True, text=True); assert c.returncode == 0, c.stderr
            r = subprocess.run([str(work / 'test'), str(build / 'first.bin')], capture_output=True, text=True)
            assert r.returncode != 0, name
            print(f'{name}: compile=0 test={r.returncode}')
        for name, product, testfile, old, new in [
            ('asset-budget', root / 'tools/asset_pack.py', tests / 'asset_pack_test.py', 'if len(data) > limit:', 'if False:'),
            ('backup-hash', root / 'tools/device/flash_plan.py', tests / 'flash_plan_test.py',
             'hashlib.sha256(image).hexdigest() != digest', 'False'),
        ]:
            target = work / product.name
            target.write_text(product.read_text().replace(old, new))
            script = work / testfile.name
            text = testfile.read_text()
            line = next(line for line in text.splitlines() if line.startswith('spec = '))
            text = text.replace(line, f'spec = importlib.util.spec_from_file_location("subject", {str(target)!r})')
            script.write_text(text)
            r = subprocess.run(['python3', str(script)], capture_output=True, text=True)
            assert r.returncode != 0 and 'FAIL:' in r.stderr, f'{name}: {r.stderr}'
            print(f'{name}: unittest={r.returncode}')
    print('reverse controls: 10/10 rejected; both storage baselines passed')


if __name__ == '__main__':
    main()
