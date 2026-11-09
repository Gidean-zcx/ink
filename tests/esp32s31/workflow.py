#!/usr/bin/env python3
"""Check S31 CI triggers, cache setup and shell failures without GitHub or IDF."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

import yaml

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--workflows', type=Path,
                    default=Path(__file__).resolve().parents[2] / '.github/workflows')
args, remaining = parser.parse_known_args()


class WorkflowTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workflow = yaml.load((args.workflows / 's31-port.yml').read_text(), Loader=yaml.BaseLoader)
        cls.job = cls.workflow['jobs']['compile']
        cls.steps = cls.job['steps']
        cls.cache = next(s for s in cls.steps if s.get('id') == 'idf-cache')
        cls.install = next(s for s in cls.steps if s.get('name') == 'Install ESP-IDF on a cache miss')
        cls.compile = next(s for s in cls.steps if s.get('name') == 'Compile the ink ESP32-S31 port')

    def test_triggers_and_manual_baselines(self):
        events = self.workflow['on']
        self.assertIn('workflow_dispatch', events)
        required = ['system/**', 'platform/**', 'drivers/**', 'boards/**', 'apps/**',
                    'ports/esp32s31/**', 'third_party/**', 'tests/esp32s31/**',
                    '.github/workflows/s31-port.yml']
        for event in ('push', 'pull_request'):
            self.assertTrue(set(required) <= set(events[event]['paths']))
            self.assertNotIn('branches', events[event])
        baseline = yaml.load((args.workflows / 's31-size.yml').read_text(), Loader=yaml.BaseLoader)
        self.assertEqual(set(baseline['on']), {'workflow_dispatch'})
        self.assertNotIn('tools/s31-size/run.sh', str(self.steps))

    def test_cache_hit_skips_install_but_not_compile(self):
        self.assertEqual(self.cache['uses'], 'actions/cache@v4')
        self.assertEqual(set(self.cache['with']['path'].splitlines()),
                         {'${{ env.IDF_PATH }}', '${{ env.IDF_TOOLS_PATH }}'})
        for field in ('runner.os', 'runner.arch', 'env.IDF_VER', 'env.IDF_COMMIT'):
            self.assertIn('${{ ' + field + ' }}', self.cache['with']['key'])
        self.assertNotIn('restore-keys', self.cache['with'])
        self.assertEqual(self.install['if'], "steps.idf-cache.outputs.cache-hit != 'true'")
        self.assertNotIn('if', self.compile)
        self.assertNotIn('continue-on-error', self.job)
        for step in (self.cache, self.install, self.compile):
            self.assertNotIn('continue-on-error', step)
        self.assertLess(self.steps.index(self.cache), self.steps.index(self.install))
        self.assertLess(self.steps.index(self.install), self.steps.index(self.compile))

    def run_install(self, *, commit=None, exit_code=0):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            git = work / 'git'
            git.write_text('''#!/bin/bash
case "$1" in
  clone)
    printf '%s\\n' "$*" >> "$CALLS"
    mkdir -p "$IDF_PATH"
    cp "$FAKE_INSTALL" "$IDF_PATH/install.sh" ;;
  -C) printf '%s\\n' "$FAKE_COMMIT" ;;
  *) exit 99 ;;
esac
''')
            installer = work / 'install.sh'
            installer.write_text('''#!/bin/bash
printf 'install %s\\n' "$*" >> "$CALLS"
exit "$INSTALL_EXIT"
''')
            git.chmod(0o755)
            installer.chmod(0o755)
            env = {**os.environ, **self.workflow['env'], 'PATH': str(work) + os.pathsep + os.environ['PATH'],
                   'IDF_PATH': str(work / 'esp-idf'), 'IDF_TOOLS_PATH': str(work / 'tools'),
                   'CALLS': str(work / 'calls'), 'FAKE_INSTALL': str(installer),
                   'FAKE_COMMIT': commit or self.workflow['env']['IDF_COMMIT'], 'INSTALL_EXIT': str(exit_code)}
            result = subprocess.run(['bash', '-eo', 'pipefail', '-c', self.install['run']],
                                    cwd=work, env=env, capture_output=True, text=True)
            return result.returncode, (work / 'calls').read_text()

    def test_cache_miss_clones_submodules_and_installs_only_s31(self):
        code, calls = self.run_install()
        self.assertEqual(code, 0)
        self.assertIn('--branch ' + self.workflow['env']['IDF_VER'], calls)
        self.assertIn('--recurse-submodules --shallow-submodules', calls)
        self.assertIn('install esp32s31\n', calls)

    def test_wrong_revision_stops_before_install(self):
        code, calls = self.run_install(commit='0' * 40)
        self.assertNotEqual(code, 0)
        self.assertNotIn('install esp32s31', calls)

    def test_install_failure_propagates(self):
        code, _ = self.run_install(exit_code=23)
        self.assertEqual(code, 23)

    def test_compile_failure_propagates(self):
        self.assertEqual(self.compile['run'], 'bash ports/esp32s31/build.sh')
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / 'ports/esp32s31/build.sh'
            script.parent.mkdir(parents=True)
            script.write_text('exit 23\n')
            result = subprocess.run(['bash', '-eo', 'pipefail', '-c', self.compile['run']], cwd=directory)
            self.assertEqual(result.returncode, 23)


if __name__ == '__main__':
    unittest.main(argv=[__file__, *remaining])
