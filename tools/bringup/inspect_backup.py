#!/usr/bin/env python3
"""Offline look at a full flash dump: partitions, images, OTA slot, NVS keys.

Works on the file only. Prints a report that is safe to share: NVS is listed
as namespaces, key names and types, never values. With --extract DIR each
partition is also written to DIR/<label>.bin (those files are private).

  inspect_backup.py private/flash_full_1.bin --extract private/parts
"""

import argparse
import binascii
import os
import struct

PART_TABLE = 0x8000
BOOTLOADER = 0x2000

TYPES = {0x00: "app", 0x01: "data"}
APP_SUB = {0x00: "factory", 0x20: "test", **{0x10 + i: f"ota_{i}" for i in range(16)}}
DATA_SUB = {0x00: "ota", 0x01: "phy", 0x02: "nvs", 0x03: "coredump",
            0x04: "nvs_keys", 0x05: "efuse", 0x06: "undefined", 0x80: "esphttpd",
            0x81: "fat", 0x82: "spiffs", 0x83: "littlefs"}
NVS_TYPES = {0x01: "u8", 0x02: "u16", 0x04: "u32", 0x08: "u64", 0x11: "i8",
             0x12: "i16", 0x14: "i32", 0x18: "i64", 0x21: "str", 0x41: "blob(v1)",
             0x42: "blob_data", 0x48: "blob_index"}


def cstr(b):
    return b.split(b"\0", 1)[0].decode("utf-8", "replace")


def partitions(img):
    out = []
    for off in range(PART_TABLE, PART_TABLE + 0xC00, 32):
        e = img[off:off + 32]
        magic, ptype, sub, poff, size = struct.unpack_from("<HBBII", e)
        if magic == 0xEBEB:  # MD5 of the table
            out.append({"md5": e[16:32].hex()})
            continue
        if magic != 0x50AA:
            break
        flags = struct.unpack_from("<I", e, 28)[0]
        out.append({"label": cstr(e[12:28]), "type": ptype, "subtype": sub,
                    "offset": poff, "size": size, "flags": flags})
    return out


def used_bytes(data):
    """Bytes up to the last non-0xFF byte, and how many bytes are not 0xFF."""
    stripped = data.rstrip(b"\xff")
    return len(stripped), len(data) - data.count(b"\xff")


def image_info(data):
    """ESP image header, segments, appended SHA-256, total size."""
    if not data or data[0] != 0xE9:
        return None
    nseg, mode, sizefreq = data[1], data[2], data[3]
    entry = struct.unpack_from("<I", data, 4)[0]
    chip_id = struct.unpack_from("<H", data, 12)[0]
    hash_appended = data[23]
    off, segs = 24, []
    for _ in range(nseg):
        addr, length = struct.unpack_from("<II", data, off)
        segs.append((addr, off + 8, length))
        off += 8 + length
    off += 1  # checksum byte
    off = (off + 15) & ~15
    if hash_appended:
        off += 32
    return {"segments": segs, "size": off, "entry": entry, "chip_id": chip_id,
            "flash_mode": mode, "flash_size_freq": sizefreq,
            "hash_appended": hash_appended}


def app_desc(data):
    i = data.find(struct.pack("<I", 0xABCD5432), 0, 4096)
    if i < 0:
        return None
    d = data[i:i + 256]
    return {"secure_version": struct.unpack_from("<I", d, 4)[0],
            "version": cstr(d[16:48]), "project": cstr(d[48:80]),
            "time": cstr(d[80:96]), "date": cstr(d[96:112]),
            "idf": cstr(d[112:144]), "elf_sha256": d[144:176].hex(),
            "min_efuse_blk_rev": struct.unpack_from("<H", d, 176)[0],
            "max_efuse_blk_rev": struct.unpack_from("<H", d, 178)[0],
            "mmu_page_size": 1 << d[180] if d[180] else None}


def bootloader_desc(data, info):
    """esp_bootloader_desc_t sits at the start of the first loaded segment."""
    if not info:
        return None
    _, off, _ = info["segments"][0]
    d = data[off:off + 80]
    if d[0] != 0x50:
        return None
    return {"version": struct.unpack_from("<I", d, 4)[0], "idf": cstr(d[8:40]),
            "date_time": cstr(d[40:64])}


def otadata(data):
    """Return (entries, index of the slot the bootloader picks or None)."""
    entries = []
    for sector in (0, 0x1000):
        seq, label, state, crc = struct.unpack_from("<I20sII", data, sector)
        want = binascii.crc32(struct.pack("<I", seq), 0xFFFFFFFF)  # as otatool.py
        valid = seq != 0xFFFFFFFF and crc == want
        entries.append({"seq": seq, "state": state, "crc_ok": crc == want,
                        "valid": valid})
    seqs = [e["seq"] for e in entries if e["valid"]]
    return entries, (max(seqs) - 1 if seqs else None)


def nvs_keys(data):
    """Namespaces and keys (no values) from all written entries."""
    spaces, keys = {}, []
    for page in range(0, len(data), 4096):
        state, seq = struct.unpack_from("<II", data, page)
        if state == 0xFFFFFFFF:
            continue
        bitmap = data[page + 32:page + 64]
        i = 0
        while i < 126:
            st = (bitmap[i // 4] >> ((i % 4) * 2)) & 3
            e = data[page + 64 + i * 32:page + 96 + i * 32]
            span = e[2] or 1
            if st == 2:  # written
                ns, typ, chunk = e[0], e[1], e[3]
                key = cstr(e[8:24])
                if ns == 0 and typ == 0x01:
                    spaces[e[24]] = key
                else:
                    size = struct.unpack_from("<H", e, 24)[0] if typ in (0x21, 0x41, 0x42) else None
                    keys.append((ns, key, NVS_TYPES.get(typ, hex(typ)), chunk, size))
                i += span
            else:
                i += 1
    return spaces, keys


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("image")
    ap.add_argument("--extract", metavar="DIR")
    a = ap.parse_args()
    img = open(a.image, "rb").read()
    print(f"image: {len(img)} bytes ({len(img) >> 20} MiB)")
    print(f"0x0000-0x{BOOTLOADER:04x}: {used_bytes(img[:BOOTLOADER])[1]} non-0xFF bytes")

    bl = img[BOOTLOADER:PART_TABLE]
    bl_info = image_info(bl)
    print(f"\nbootloader @0x{BOOTLOADER:x}: ", end="")
    if bl_info:
        print(f"chip_id={bl_info['chip_id']} size={bl_info['size']} "
              f"flash_mode={bl_info['flash_mode']} size_freq=0x{bl_info['flash_size_freq']:02x}"
              f" segments={len(bl_info['segments'])} desc={bootloader_desc(bl, bl_info)}")
    else:
        print("no image header")

    parts = partitions(img)
    print("\npartitions @0x8000:")
    print(f"{'label':<12}{'type':<6}{'subtype':<10}{'offset':>10}{'size':>10}  used / non-0xFF")
    for p in parts:
        if "md5" in p:
            print(f"(table md5 {p['md5']})")
            continue
        data = img[p["offset"]:p["offset"] + p["size"]]
        sub = (APP_SUB if p["type"] == 0 else DATA_SUB).get(p["subtype"], hex(p["subtype"]))
        last, nonff = used_bytes(data)
        print(f"{p['label']:<12}{TYPES.get(p['type'], hex(p['type'])):<6}{sub:<10}"
              f"{p['offset']:>#10x}{p['size']:>#10x}  {last} / {nonff}"
              + (f" flags=0x{p['flags']:x}" if p["flags"] else ""))
        if a.extract:
            os.makedirs(a.extract, exist_ok=True)
            with open(os.path.join(a.extract, p["label"] + ".bin"), "wb") as f:
                f.write(data)
        p["data"], p["sub"] = data, sub

    apps = [p for p in parts if p.get("type") == 0]
    for p in apps:
        info = image_info(p["data"])
        print(f"\n{p['label']}: ", end="")
        if not info:
            print("empty (no image header)")
            continue
        print(f"image size {info['size']} bytes, chip_id={info['chip_id']}, "
              f"entry=0x{info['entry']:08x}, hash_appended={info['hash_appended']}")
        for addr, off, length in info["segments"]:
            print(f"  segment vaddr=0x{addr:08x} len=0x{length:x} ({length})")
        print(f"  app_desc: {app_desc(p['data'])}")

    for p in parts:
        if p.get("type") == 1 and p.get("sub") == "ota":
            entries, seq_slot = otadata(p["data"])
            print(f"\notadata: {entries}")
            ota_apps = [q for q in apps if q["sub"].startswith("ota_")]
            if seq_slot is None:
                print("  no valid entry: bootloader boots the first app (factory or ota_0)")
            else:
                print(f"  bootloader picks {ota_apps[seq_slot % len(ota_apps)]['label']}")
        if p.get("type") == 1 and p.get("sub") == "nvs":
            spaces, keys = nvs_keys(p["data"])
            print(f"\nNVS '{p['label']}': {len(spaces)} namespaces, {len(keys)} entries (names only)")
            for ns_index, name in sorted(spaces.items()):
                names = sorted({(k, t) for ns, k, t, _, _ in keys if ns == ns_index})
                print(f"  [{name}] " + ", ".join(f"{k}:{t}" for k, t in names))
            orphans = sorted({(ns, k) for ns, k, *_ in keys if ns not in spaces and ns != 0})
            if orphans:
                print(f"  entries in unknown namespaces: {orphans}")
        if p.get("type") == 1 and p.get("sub") == "coredump":
            last, nonff = used_bytes(p["data"])
            print(f"\ncoredump: {nonff} non-0xFF bytes, last at {last}; "
                  f"first 16 bytes {p['data'][:16].hex()}")


if __name__ == "__main__":
    main()
