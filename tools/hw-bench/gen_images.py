#!/usr/bin/env python3
"""Design boards as 4-bit pictures for the panel test. The output stays in the build directory."""
import sys
from PIL import Image
design, out, names = sys.argv[1], sys.argv[2], sys.argv[3:]
with open(out, "w") as f:
    f.write("#define INK_BENCH_IMAGES %d\n" % len(names))
    for n in names:
        im = Image.open(f"{design}/{n}.png").convert("L").resize((684, 1216))
        px = im.tobytes()
        f.write(f"static const unsigned char IMG_{n}[] = {{")
        for i in range(0, len(px), 2):
            b = px[i] >> 4 << 4 | (px[i + 1] >> 4 if i + 1 < len(px) else 0)
            f.write(f"{b},")
        f.write("};\n")
    f.write("static const struct { const char *name; const unsigned char *data; } BENCH_IMAGES[] = {")
    f.write(",".join(f'{{"{n}", IMG_{n}}}' for n in names) + "};\n")
