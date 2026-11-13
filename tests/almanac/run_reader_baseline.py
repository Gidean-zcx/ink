"""Shim for Mogo's tools/calendar_host_test.py (ctest almanac-mogo-host puts this folder on PYTHONPATH).

The original imports only toolchain(work) from Mogo's tools/run_reader_baseline.py, which is not
copied: here it returns this machine's C compiler. The original first regenerates the oracle with
node, so without node this exits 77 (ctest skip) on import and says why, like almanac-oracle."""
import os
import shlex
import shutil

if not shutil.which('node'):
    print("SKIP: node is unavailable; Mogo's host test regenerates its oracle with lunar-javascript")
    raise SystemExit(77)


def toolchain(work):
    del work  # Mogo prepares its own toolchain folder here; the host compiler needs none.
    compiler = shlex.split(os.environ.get('CC', '')) or [shutil.which('cc') or 'cc']
    return dict(os.environ), compiler
