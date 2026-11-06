#!/usr/bin/env python3
"""Compile both slider translation units and reject their gate-3 preimages."""
import argparse
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", required=True, help="GNU C compiler (native or cross)")
    parser.add_argument("--build", default="build")
    parser.add_argument("--baseline", default="c536f0dc")
    args = parser.parse_args()
    version = subprocess.check_output([args.cc, "--version"], text=True)
    if "Free Software Foundation" not in version or "clang" in version.lower():
        raise SystemExit("This regression requires GCC; clang misses the original diagnostic")
    print(version.splitlines()[0], flush=True)
    build = (ROOT / args.build).resolve()
    work = Path(tempfile.mkdtemp(prefix="gate3-reverse-", dir=build))
    for target, source, statement in (
        ("ink_system", "system/ui/shell/navigation.c", "if (v < 0) v = 0; if (v > 10) v = 10;"),
        ("sim-ui-regressions", "tests/ui_shell_regressions.c", "if (value == 0) x++; if (value == 10) x--;"),
    ):
        makefile = build / "CMakeFiles" / (target + ".dir") / "flags.make"
        settings = dict(re.findall(r"^(C_\w+) = (.*)$", makefile.read_text(), re.M))
        flags = shlex.split(settings["C_DEFINES"] + " " + settings["C_INCLUDES"] + " " + settings["C_FLAGS"])
        # Cross GCC uses its own headers instead of the host's Darwin SDK.
        if "-isysroot" in flags:
            at = flags.index("-isysroot")
            del flags[at:at + 2]
        flags = [flag for flag in flags if not flag.startswith("-mmacosx-version-min=")]
        if "-Werror" not in flags:
            raise SystemExit("Configure the build with INK_WERROR=ON first")
        for variant in ("fixed", "baseline"):
            text = ((ROOT / source).read_text() if variant == "fixed" else subprocess.check_output(
                ["git", "show", args.baseline + ":" + source], cwd=ROOT, text=True))
            # GCC needs an on-disk source to track indentation diagnostics.
            copy = work / (target + "-" + variant + ".c")
            copy.write_text(text)
            command = [args.cc, *flags, "-I" + str((ROOT / source).parent), "-fsyntax-only", str(copy)]
            result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, timeout=60)
            output = result.stdout + result.stderr
            (work / (target + "-" + variant + ".log")).write_text(shlex.join(command) + "\n" + output)
            if variant == "fixed":
                if result.returncode:
                    raise SystemExit(output)
            elif (result.returncode != 1 or "[-Werror=misleading-indentation]" not in output
                  or statement not in output or len(re.findall(r"\berror:", output)) != 1):
                raise SystemExit("Baseline failed for an unexpected reason:\n" + output)
            print(f"{source} {variant}: exit {result.returncode}", flush=True)
    print("PASS: both fixed units compile; both preimages fail on misleading indentation")
    print("Evidence directory:", work.relative_to(ROOT))


if __name__ == "__main__":
    main()
