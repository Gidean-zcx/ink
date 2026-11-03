#!/usr/bin/env python3
"""Redact device logs for sharing, and check files for leaked identifiers.

  redact.py IN OUT          write a redacted copy of IN to OUT
  redact.py --check PRIVATE_DIR FILE...
                            collect this device's real identifiers from the
                            raw logs in PRIVATE_DIR and report any FILE that
                            still contains one (values are never printed)

Redaction replaces MAC addresses, IMEI/IMSI/ICCID and other long digit runs,
UUIDs, activation codes, Wi-Fi names and passwords, tokens, phone numbers and
public IP addresses with <redacted>, keeping the last 2 characters so entries
can still be told apart. ANSI colour codes are stripped.
"""

import ipaddress
import os
import re
import sys

ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
MAC = re.compile(r"\b(?:[0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})\b")
UUID = re.compile(r"\b[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-"
                  r"[0-9a-fA-F]{10}([0-9a-fA-F]{2})\b")
LONG_DIGITS = re.compile(r"(?<![0-9A-Za-z])\d{12,}(\d{2})(?![0-9A-Za-z])")
LABELLED = re.compile(r"(?i)\b(imei|imsi|iccid|meid|serial(?: number)?|sn|ssid|password|"
                      r"passwd|pwd|psk|token|secret|authorization|bearer|client[-_]id|"
                      r"device[-_]id|uuid)(\s*[:=]\s*|\s+)([^\s,;]+)")
ACTIVATION = re.compile(r"(验证码|(?i:activation code)|(?i:code))([:：=]\s*)\d{4,8}")
PHONE = re.compile(r"(?<!\d)(?:\+?86)?1[3-9]\d{9}(?!\d)")
IPV4 = re.compile(r"\b(?:\d{1,3}\.){3}\d{1,3}\b")


def keep2(value):
    return "<redacted>" + value[-2:]


def public_ip(m):
    try:
        ip = ipaddress.ip_address(m.group(0))
    except ValueError:
        return m.group(0)
    return m.group(0) if ip.is_private or ip.is_loopback or ip.is_unspecified \
        or ip.is_reserved or ip.is_multicast else "<redacted-ip>"


def redact(line):
    line = ANSI.sub("", line)
    line = MAC.sub(lambda m: "<redacted>:" + m.group(1), line)
    line = UUID.sub(lambda m: "<redacted-uuid>" + m.group(1), line)
    line = ACTIVATION.sub(lambda m: m.group(1) + m.group(2) + "<redacted>", line)
    line = LABELLED.sub(lambda m: m.group(1) + m.group(2) + keep2(m.group(3)), line)
    line = PHONE.sub("<redacted-phone>", line)
    line = LONG_DIGITS.sub(lambda m: "<redacted>" + m.group(1), line)
    return IPV4.sub(public_ip, line)


def known_identifiers(private_dir):
    """This device's real identifiers, from the raw logs."""
    found = set()
    for name in os.listdir(private_dir):
        if not name.endswith((".txt", ".log", ".json")):
            continue
        text = open(os.path.join(private_dir, name), encoding="utf-8",
                    errors="replace").read()
        found.update(m.group(0).lower() for m in MAC.finditer(text))
        found.update(m.group(0).lower() for m in UUID.finditer(text))
        for m in re.finditer(r"(?i)\b(?:imei|imsi|iccid)\s*[:=]\s*([0-9A-Fa-f]{10,})", text):
            found.add(m.group(1).lower())
        for m in ACTIVATION.finditer(text):
            found.add(re.sub(r"\D", "", m.group(0)[-8:]))
    macs = {v for v in found if MAC.fullmatch(v)}
    found.update(v.replace(":", "").replace("-", "") for v in macs)  # bare-hex form
    # all-zero or all-0xFF placeholders identify nothing and match padding
    return {v for v in found if len(v) >= 6 and len(set(v.replace(":", ""))) > 1}


def check(private_dir, files):
    ids = known_identifiers(private_dir)
    print(f"checking {len(files)} files against {len(ids)} known identifiers")
    leaks = 0
    for path in files:
        text = open(path, encoding="utf-8", errors="replace").read().lower()
        for i, value in enumerate(sorted(ids)):
            if value in text:
                leaks += 1
                print(f"LEAK: {path} contains identifier #{i} "
                      f"(length {len(value)}, ends ...{value[-2:]})")
    print("no leaks" if not leaks else f"{leaks} leaks")
    return 1 if leaks else 0


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "--check":
        sys.exit(check(sys.argv[2], sys.argv[3:]))
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    with open(sys.argv[1], encoding="utf-8", errors="replace") as src, \
            open(sys.argv[2], "w", encoding="utf-8") as dst:
        for line in src:
            dst.write(redact(line))


if __name__ == "__main__":
    main()
