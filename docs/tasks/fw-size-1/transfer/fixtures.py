from pathlib import Path
import sys
import zlib

out = Path(sys.argv[1])
out.mkdir(exist_ok=True)
pattern = bytes((i * 23 ^ (i >> 8)) & 255 for i in range(70000))
cases = [b'', b'SYNTHETIC upload data\n' * 5000, pattern]
for index, plain in enumerate(cases):
    (out / f'{index}.raw').write_bytes(plain)
    (out / f'{index}.z').write_bytes(zlib.compress(plain, 0 if index == 2 else 9))
compressor = zlib.compressobj()
packed = compressor.compress(pattern) + compressor.flush(zlib.Z_SYNC_FLUSH)
packed += compressor.compress(pattern) + compressor.flush()
(out / '3.raw').write_bytes(pattern * 2)
(out / '3.z').write_bytes(packed)
