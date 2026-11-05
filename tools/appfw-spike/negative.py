"""Remove the Lua quota once, prove the unchanged check fails, restore and rebuild."""
import argparse
import json
import subprocess
from pathlib import Path

from measure import ROOT, run


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, default=Path("build-spike"))
    parser.add_argument("--out", type=Path, default=Path("work/appfw-spike/negative"))
    args = parser.parse_args()
    build, out = args.build.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    path = ROOT / "tools/appfw-spike/candidate_lua.c"
    original = path.read_bytes()
    guard = b"    if (new_size > INK_SPIKE_VM_LIMIT - (used - old_size)) return NULL;\n"
    assert original.count(guard) == 1

    def rebuild(name):
        with (out / name).open("w") as log:
            subprocess.run(["cmake", "--build", str(build), "--target", "appfw-lua", "-j4"],
                           cwd=ROOT, check=True, stdout=log, stderr=subprocess.STDOUT, timeout=120)

    caught = False
    try:
        path.write_bytes(original.replace(guard, b""))
        rebuild("without-quota-build.log")
        try:
            run(build, out / "without-quota", "lua", "annotated")
        except AssertionError as exc:
            assert str(exc) == "quota_rejected", str(exc)
            caught = True
        assert caught, "the unchanged validation must reject a missing quota"
        metrics = json.loads((out / "without-quota/metrics.json").read_text())
        assert metrics["metrics"]["spike.quota_rejected"]["min"] == 0
    finally:
        path.write_bytes(original)
        rebuild("restored-build.log")
    run(build, out / "restored", "lua", "annotated")
    assert path.read_bytes() == original
    (out / "result.json").write_text(json.dumps({"removed": "Lua allocator quota guard only", "unchanged_check_failed": caught,
                                                "restored_check_passed": True, "source_restored_byte_for_byte": True}, indent=2) + "\n")
    print("negative control: quota removed -> check failed; restored -> passed; source restored byte for byte")


if __name__ == "__main__":
    main()
