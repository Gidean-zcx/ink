"""The `cc` Mogo's lock-style host tests call (through PATH and CC), for build lines Mogo wrote on macOS with Apple clang.

Mogo's tests stay as they are (D-140); only the compiler line changes, and only for what clang never showed Mogo:
- gcc warns where clang does not, in Mogo's test code, none of it a fault: -Wmisleading-indentation on the generated
  tests' one-line loops (`for(...)a;b;`, b meant to run once) and -Warray-parameter on lock_collage's stub
  book_store_roots(book_store_root_t *r, ...) against the header's out[BOOK_STORE_ROOT_MAX]. These two are turned off
  for gcc only. The shipped Mogo files on these lines are still checked for both: the product build compiles them with
  gcc -Wall -Wextra -Werror. -Wformat-truncation stays on: the product build sends Mogo's snprintf to
  ink_mogo_snprintf (apps/mogo/port/runtime.h), so these tests are where gcc checks Mogo's snprintf calls.
- lock_clock_ui links with Apple's -Wl,-dead_strip, which GNU ld refuses, and needs it: ttf_font.c's settings-backed
  functions (app_settings_font_path, app_settings_fonts_dir) have no definition in that test. Off macOS it gets
  -ffunction-sections -fdata-sections -Wl,--gc-sections, the choice Mogo's own lock_collage test makes."""
import os
import platform
import shlex
import subprocess
import sys

real = shlex.split(os.environ['MOGO_HOST_CC'])
args = sys.argv[1:]
if platform.system() != 'Darwin' and '-Wl,-dead_strip' in args:
    args = ['-ffunction-sections', '-fdata-sections'] + ['-Wl,--gc-sections' if arg == '-Wl,-dead_strip' else arg
                                                         for arg in args]
if 'clang' not in subprocess.run(real + ['--version'], capture_output=True, text=True).stdout:
    args += ['-Wno-misleading-indentation', '-Wno-array-parameter']
os.execvp(real[0], real + args)
