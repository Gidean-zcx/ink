"""Check the S31 decode frame; optionally undo only the scratch patch in a temporary object."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--negative", action="store_true")
a = p.parse_args()
build = a.build.resolve()
source = ROOT / "third_party/quirc/decode.c"
if a.negative:
    work = build / "qr-stack-negative"
    work.mkdir(exist_ok=True)
    text = source.read_text()
    start = text.index("static quirc_decode_error_t decode_with_scratch(")
    end = text.index("/* ink: keep the 8896-byte scratch area")
    wrapper_end = text.index("void quirc_flip", end)
    block = text[start:end].replace("static quirc_decode_error_t decode_with_scratch(", "quirc_decode_error_t quirc_decode(")
    block = block.replace("struct quirc_data *data, struct datastream *ds)", "struct quirc_data *data)")
    block = block.replace("quirc_decode_error_t err;", "quirc_decode_error_t err;\n\tstruct datastream ds;", 1)
    block = block.replace("memset(ds, 0, sizeof(*ds));", "memset(&ds, 0, sizeof(ds));").replace("ds->raw", "ds.raw")
    block = block.replace("read_data(code, data, ds)", "read_data(code, data, &ds)").replace("codestream_ecc(data, ds)", "codestream_ecc(data, &ds)").replace("decode_payload(data, ds)", "decode_payload(data, &ds)")
    mutant = work / "decode.c"
    mutant.write_text(text[:start] + block + text[wrapper_end:])
    db = json.loads((build / "compile_commands.json").read_text())
    entry = next(e for e in db if Path(e["file"]).resolve() == source)
    command = shlex.split(entry["command"])
    args, skip = [], False
    for word in command:
        if skip:
            skip = False
            continue
        if word in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
        elif word not in ("-MD", "-MMD", "-MP") and word != str(source):
            args.append(word)
    args += ["-I"+str(source.parent), "-o", str(work / "decode.o"), str(mutant)]
    subprocess.run(args, cwd=entry["directory"], check=True)
    paths = [work / "decode.su"]
else:
    paths = list((build / "esp-idf/ink_system").rglob("decode.c.su"))
assert len(paths) == 1
rows = [line.split("\t") for line in paths[0].read_text().splitlines()]
frame = next(int(row[1]) for row in rows if row[0].endswith(":quirc_decode"))
limit = 8192
assert (frame > limit) if a.negative else (frame < limit), f"decode frame {frame}, MEDIA stack {limit}"
print("negative control" if a.negative else "current", "decode frame", frame, "MEDIA", limit, "PASS")
