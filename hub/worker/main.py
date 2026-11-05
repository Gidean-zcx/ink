"""Step 1 worker probe: bounded synthetic file hashing, not a content converter."""
import argparse
import hashlib
import json
from pathlib import Path


def run(source, output):
    source, output = Path(source), Path(output)
    path = source / "example.txt"
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 1024 * 1024:
        raise ValueError("invalid example input")
    data = path.read_bytes()
    if len(data) > 1024 * 1024:
        raise ValueError("input too large")
    output.mkdir(parents=True, exist_ok=True)
    result = {"sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data)}
    (output / "result.json").write_text(json.dumps(result))
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    run(args.input, args.output)
