"""Deterministically embed the adapted browser page; retain its upstream notices."""
import gzip
from pathlib import Path
import sys
Path(sys.argv[2]).write_bytes(gzip.compress(Path(sys.argv[1]).read_bytes(), compresslevel=9, mtime=0))
