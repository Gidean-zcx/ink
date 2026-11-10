#!/usr/bin/env python3
"""Check the build mirror against the factory and reverse the SDK size check; no device writes."""
import argparse
import contextlib
import importlib
import io
from pathlib import Path
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("idf", type=Path)
parser.add_argument("factory_csv", type=Path)
parser.add_argument("build", type=Path)
args = parser.parse_args()
sys.dont_write_bytecode = True
sys.path.insert(0, str(args.idf / "components/partition_table"))
gen = importlib.import_module("gen_esp32part")
check = importlib.import_module("check_sizes")
gen.offset_part_table = 0x8000
port = Path(__file__).resolve().parents[1]
mirror = gen.PartitionTable.from_csv((port / "partitions-trial.csv").read_text())
factory = gen.PartitionTable.from_csv(args.factory_csv.read_text())
mirror.verify()
assert mirror.to_binary() == factory.to_binary(), "the build layout must match the factory exactly"
apps = [p for p in mirror if p.type == 0]
assert [(p.name, p.offset, p.size) for p in apps] == [
    ("ota_0", 0x80000, 5 * 1024 * 1024), ("ota_1", 0x580000, 5 * 1024 * 1024)]

def size_check(table):
    output = io.StringIO()
    status = 0
    with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
        try:
            with (args.build / "ink-s31.bin").open("rb") as image:
                check.check_partition("app", None, io.BytesIO(table.to_binary()), image)
        except SystemExit:
            status = 1
    return status, output.getvalue()

status, output = size_check(mirror)
assert status == 0 and "Warning:" not in output, output
print("factory mirror: all 9 entries identical; SDK size check passed without warnings")
skeleton = gen.PartitionTable.from_csv((args.idf / "components/partition_table/partitions_singleapp_large.csv").read_text())
status, output = size_check(skeleton)
assert status != 0 or "Warning:" in output, "reverting to the skeleton must fail the zero-warning check"
print("reverse control: skeleton 1500 KiB layout fails the zero-warning size check")
