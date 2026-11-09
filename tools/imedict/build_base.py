#!/usr/bin/env python3
"""Build the card dictionary from hash-locked, permissively licensed sources."""
import argparse
from collections import defaultdict
import hashlib
import json
from math import isqrt
from pathlib import Path
import re
import sys
import urllib.request
import zipfile

from imedict import build

ROOT = Path(__file__).resolve().parent
DICT_MAX = 1024 * 1024
INITIALS_LIMIT = 32
DAILY_BONUS = 16384
sys.path.insert(0, str(ROOT.parent / "fontgen"))
from fontgen import charset


def source_files(cache):
    locked = json.loads((ROOT / "sources.lock.json").read_text())
    cache.mkdir(parents=True, exist_ok=True)
    for item in locked["downloads"]:
        path = cache / item["name"]
        if not path.exists():
            with urllib.request.urlopen(item["url"], timeout=60) as response:
                raw = response.read()
            if hashlib.sha256(raw).hexdigest() != item["sha256"]:
                raise SystemExit("SHA-256 mismatch: " + item["name"])
            path.write_bytes(raw)
        if hashlib.sha256(path.read_bytes()).hexdigest() != item["sha256"]:
            raise SystemExit("SHA-256 mismatch: " + item["name"])
    # Extract only the hash-verified package; no pip install or ambient packages.
    package = cache / "pypinyin-0.55.0"
    with zipfile.ZipFile(cache / locked["downloads"][1]["name"]) as wheel:
        for name in wheel.namelist():
            if name.startswith("pypinyin/") and not name.endswith("/"):
                path = package / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(wheel.read(name))
    sys.path.insert(0, str(package))
    return cache / locked["downloads"][0]["name"]


def generate(dictionary):
    from pypinyin import Style, lazy_pinyin, pinyin

    words = {}
    readings = {}
    for line in dictionary.read_text(encoding="utf-8").splitlines():
        word, frequency, _ = line.split()
        if re.fullmatch(r"[\u4e00-\u9fff]{1,12}", word):
            words[word] = max(words.get(word, 0), int(frequency))
    for line in (ROOT / "common.tsv").read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        syllables, word, frequency = line.split("\t")
        words[word] = max(words.get(word, 0), int(frequency))
        readings[word] = syllables.split()
    source_words = len(words)
    supported = set(charset("ascii,punct,pinyin,gb2312"))
    words = {word: frequency for word, frequency in words.items()
             if all(ord(char) in supported for char in word)}
    keys, lines = set(), []
    initials = defaultdict(list)
    size, kept, characters, abbreviations = 32, 0, 0, 0
    for word, frequency in sorted(words.items(), key=lambda row: (-row[1], row[0])):
        if len(word) == 1:
            full = pinyin(word, style=Style.NORMAL, heteronym=True)[0]
            primary = full[0]
            pairs = [(key, False) for key in sorted(set(full))]
        else:
            syllables = readings.get(word) or lazy_pinyin(word, style=Style.NORMAL)
            full = "".join(syllables)
            pairs = [(full, False), ("".join(s[0] for s in syllables), True)]
        pairs = [(key, short) for key, short in pairs if re.fullmatch("[a-z]{1,40}", key)]
        if not pairs:
            continue
        cost = sum(8 + len(word.encode()) + (12 + len(key) if key not in keys else 0) for key, _ in pairs)
        if size + cost > DICT_MAX:
            break
        # Integer square root preserves useful frequency differences within u16.
        score = min(65535, max(1, isqrt(frequency * 4096)))
        for key, short in pairs:
            keys.add(key)
            # Secondary character readings must not dominate common homophones.
            weight = max(1, score // 4) if len(word) == 1 and key != primary else score
            if short:
                # Conversation/device priors apply only to initials, never full pinyin.
                weight = min(65535, weight + (DAILY_BONUS if word in readings else 0))
                initials[key].append((word, weight))
            else:
                lines.append(f"{key}\t{word}\t{weight}\n")
            abbreviations += short
        size += cost
        kept += 1
        characters += len(word) == 1
        minimum = frequency
    abbreviations = 0
    for key, candidates in initials.items():
        # Keep the admission cut unchanged so every full-pinyin entry survives.
        for word, weight in sorted(candidates, key=lambda row: -row[1])[:INITIALS_LIMIT]:
            lines.append(f"{key}\t{word}\t{weight}\n")
            abbreviations += 1
    data = build(lines)
    assert len(data) <= size <= DICT_MAX
    stats = {"bytes": len(data), "words": kept, "characters": characters,
             "source_words": source_words, "font_filtered_words": source_words - len(words),
             "font_supported_words": len(words),
             "keys": len(keys), "entries": len(lines), "abbreviations": abbreviations,
             "initials_limit": INITIALS_LIMIT, "daily_bonus": DAILY_BONUS,
             "minimum_source_frequency": minimum,
             "sha256": hashlib.sha256(data).hexdigest()}
    return data, stats


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    data, stats = generate(source_files(args.cache))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(data)
    args.out.with_suffix(".json").write_text(json.dumps(stats, indent=2) + "\n")
    # Distribute notices with the generated card resource.
    for license_file in (ROOT / "licenses").glob("*.txt"):
        (args.out.parent / license_file.name).write_bytes(license_file.read_bytes())
    print(json.dumps(stats, sort_keys=True))


if __name__ == "__main__":
    main()
