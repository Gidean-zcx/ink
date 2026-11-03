#!/usr/bin/env python3
"""Build an otadata image that boots a given OTA slot, from the device's current otadata.

The bootloader boots slot (seq - 1) % n_slots of the valid entry with the highest seq. This
keeps the current entry and writes seq + k into the other 4 KiB sector, so restoring is
writing the original otadata back.

  make_otadata.py private/parts/otadata.bin --slot 1 --slots 2 -o otadata_ota1.bin
"""

import argparse
import binascii
import struct

SECTOR = 0x1000
VALID = 2  # ESP_OTA_IMG_VALID


def entry(data, off):
    seq, _label, state, crc = struct.unpack_from("<I20sII", data, off)
    ok = seq != 0xFFFFFFFF and crc == binascii.crc32(struct.pack("<I", seq), 0xFFFFFFFF)
    return seq, state, ok


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("otadata")
    ap.add_argument("--slot", type=int, required=True)
    ap.add_argument("--slots", type=int, default=2)
    ap.add_argument("-o", "--out", required=True)
    a = ap.parse_args()
    data = bytearray(open(a.otadata, "rb").read())
    if len(data) != 2 * SECTOR:
        raise SystemExit(f"otadata must be {2 * SECTOR} bytes, got {len(data)}")
    entries = [entry(data, s * SECTOR) for s in (0, 1)]
    valid = [(seq, i) for i, (seq, _, ok) in enumerate(entries) if ok]
    top_seq, top_sector = max(valid) if valid else (0, 1)
    seq = top_seq + 1
    while (seq - 1) % a.slots != a.slot:
        seq += 1
    other = 1 - top_sector
    data[other * SECTOR:(other + 1) * SECTOR] = b"\xff" * SECTOR
    crc = binascii.crc32(struct.pack("<I", seq), 0xFFFFFFFF)
    struct.pack_into("<I20sII", data, other * SECTOR, seq, b"\xff" * 20, VALID, crc)
    open(a.out, "wb").write(data)
    picked = max((s, i) for i, (s, _, ok) in enumerate([entry(data, 0), entry(data, SECTOR)]) if ok)[0]
    print(f"kept seq {top_seq} in sector {top_sector}; wrote seq {seq} in sector {other}; "
          f"bootloader picks slot {(picked - 1) % a.slots}")


if __name__ == "__main__":
    main()
