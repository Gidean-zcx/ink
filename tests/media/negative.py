"""Temporarily remove each image guarantee; tests must fail, then restore it."""
import argparse
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    build = (root / args.build).resolve()
    work = root / "work" / "image-negative"
    work.mkdir(parents=True, exist_ok=True)
    source = root / "system/services/media/image.c"
    vendor = root / "third_party/stb/stb_image.h"
    original = source.read_text()
    decoder = vendor.read_text()
    deps = Path(os.environ["INK_DEPS"])
    ctest = str(deps / "build-deps/pyenv/bin/ctest")
    upstream = deps / "build/_deps/lvgl-src/src/libs/gltf/stb_image/stb_image.h"
    objects = [build / "CMakeFiles/ink_system.dir/system/services/media/image.c.o",
               build / "CMakeFiles/ink_image_codec.dir/system/services/media/image_codec.c.o"]
    cases = [
        ("integrity", source, original.replace("if (!complete(data, size))", "if (false && !complete(data, size))"),
         "decode", "PNG CRC error"),
        ("jpeg_markers", vendor, decoder.replace("if(!stbi__process_marker(j, m)) return 0;",
                                                "if(!stbi__process_marker(j, m)) return 1;"),
         "decode", "JPEG truncated before EOI"),
        ("jpeg_scans", vendor, decoder.replace(
            'if(scanned != (1 << j->s->img_n) - 1) return stbi__err("missing scan", "Corrupt JPEG");', ''),
         "decode", "JPEG truncated before EOI"),
        ("jpeg_entropy", vendor, decoder.replace("if(j->nomore) return;\n        b = stbi__get8(j->s);",
                                                "b = j->nomore ? 0 : stbi__get8(j->s);"),
         "decode", "JPEG truncated before EOI"),
        ("jpeg_restart", vendor, decoder.replace("return i == w - 1 && j == h - 1;", "return 1;").replace(
            "return i == z->img_mcu_x - 1 && j == z->img_mcu_y - 1;", "return 1;"),
         "decode", "JPEG truncated before EOI"),
        ("decode_cost", source, original.replace(
            "osal_cost_cycles((uint64_t)size * 80 + (uint64_t)w * h * 200);", ''),
         "decode", "large decode and conversion charge modeled CPU time"),
        ("convert_cost", source, original.replace("osal_cost_cycles((uint64_t)w * 64);", ''),
         "decode", "large decode and conversion charge modeled CPU time"),
        ("dither_cost", source, original.replace("osal_cost_cycles((uint64_t)image->width * 128);",
                                                "osal_preempt_point();"),
         "dither", "dither charges modeled CPU time"),
        ("chunk_length", source, original.replace("len > n - pos - 12 || ", ''),
         "decode", "Segmentation fault"),
        ("size", source, original.replace("(size_t)w * (size_t)h > INK_IMAGE_MAX_PIXELS", "false"),
         "decode", "oversized PNG area"),
        ("luminance", source, original.replace("hal_display_level_luminance(l)", ".20f + .80f * l / 15.f"),
         "dither", "mean luminance error"),
        ("eviction", source, original.replace("if (cache->count == INK_THUMB_SLOTS) drop(cache, 0);",
                                            "if (cache->count == INK_THUMB_SLOTS) return INK_IMAGE_ERROR;"),
         "cache", "over-capacity insertion succeeds"),
        ("version", source, original.replace("if (e->version == version)", "if (true)"),
         "version", "expected decoded/dithered pixel value"),
        ("stack", vendor, upstream.read_text(), "decode", "BUDGET FAIL warnings.count"),
    ]
    for name, path, changed, test, evidence in cases:
        saved = path.read_text()
        assert changed != saved, name
        try:
            path.write_text(changed)
            # Make may compare mtimes at one-second resolution; never reuse a mutant object.
            for obj in objects:
                obj.unlink(missing_ok=True)
            with (work / f"{name}-build.log").open("w") as log:
                subprocess.run(["cmake", "--build", str(build), "--target", "sim-image", "-j3"],
                               cwd=root, stdout=log, stderr=subprocess.STDOUT, check=True)
            result = subprocess.run([ctest, "--test-dir", str(build), "--output-on-failure",
                                     "-R", f"^image-{test}$", "-j2"], cwd=root,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            (work / f"{name}-test.log").write_text(result.stdout)
            assert result.returncode == 8 and evidence in result.stdout, (name, result.stdout)
            print(f"{name}: expected failure, exit 8, {evidence}", flush=True)
        finally:
            path.write_text(saved)
            for obj in objects:
                obj.unlink(missing_ok=True)
    subprocess.run(["cmake", "--build", str(build), "--target", "sim-image", "-j3"], cwd=root, check=True,
                   stdout=subprocess.DEVNULL)
    subprocess.run([ctest, "--test-dir", str(build), "--output-on-failure", "-R", "^image-", "-j2"],
                   cwd=root, check=True)


if __name__ == "__main__":
    main()
