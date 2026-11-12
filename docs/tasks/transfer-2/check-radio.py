"""Compile the actual radio glue against the existing fake SDK and undo SSID wiring once."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
build = Path(sys.argv[1]).resolve()
source = root / 'ports/esp32s31/vendor/read_pico_transfer/ink_radio.cc'
fixture = root / 'ports/esp32s31/tests/transfer_radio.cxx'
prefix = ['taskpolicy', '-c', 'utility'] if sys.platform == 'darwin' else []
with tempfile.TemporaryDirectory(prefix='transfer-radio-', dir=build) as directory:
    work = Path(directory)
    common = prefix + [os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I' + str(root / 'ports/esp32s31/tests/transfer_stub')]
    for name, sanitizer in [('normal', []), ('asan', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'])]:
        binary = work / name
        subprocess.run([*common, *sanitizer, str(fixture), '-o', str(binary)], cwd=root, check=True, capture_output=True)
        subprocess.run([*prefix, str(binary)], cwd=root, check=True)
    mutant = work / 'mutant.cc'
    before = '    FillSsid(ssid, sizeof ssid);'
    text = source.read_text()
    assert text.count(before) == 1
    mutant.write_text(text.replace(before, '    std::strcpy(ssid, "ink-transfer");'))
    binary = work / 'reverse'
    subprocess.run([*common, '-DINK_RADIO_SOURCE="' + str(mutant) + '"', str(fixture), '-o', str(binary)], cwd=root, check=True, capture_output=True)
    result = subprocess.run([*prefix, str(binary)], cwd=root, capture_output=True, text=True)
    assert result.returncode and 'configured ==' in result.stderr, result.stderr[-1000:]
    print('original AP name regression: expected failure, exit=' + str(result.returncode))
