"""Shim for Mogo's tools/run_reader_baseline.py: the lock-style host tests import only toolchain(work) from it.

Mogo prepares its own compiler folder there; here it is this machine's C compiler, as tests/almanac does."""
import os
import shlex
import shutil


def toolchain(work):
    del work
    compiler = shlex.split(os.environ.get('CC', '')) or [shutil.which('cc') or 'cc']
    return dict(os.environ), compiler
