"""The copied InfiniTime text against the original: hashes, line ranges, and only the changes SOURCE.md lists.

Needs a read-only copy of InfiniTime at 9ad16304 in work/infinitime (see system/apps/clock/vendor/infinitime/SOURCE.md);
without it the test says so and is skipped (exit 77): the CI checkout has none.
"""
import hashlib
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
CLONE = ROOT / "work/infinitime"
COPY = ROOT / "system/apps/clock/vendor/infinitime"
SHA = {
    "LICENSE": "8b1ba204bb69a0ade2bfcf65ef294a920f6bb361b317dba43c7ef29d96332b9b",
    "src/components/stopwatch/StopWatchController.cpp": "a48c950a05e80733958a16fc94ce8ce890e41f237d4223c295688ef4c5960a1d",
    "src/components/stopwatch/StopWatchController.h": "f21b29f0fe35ed73fbb995c58af59919622b4c9d5efaef36aa42e830ea2bb7c0",
    "src/utility/CircularBuffer.h": "ed65afa603890ab0866345aa770a9cbc8f5d66c5b7d2e3556461875fe60d27e4",
    "src/utility/DirtyValue.h": "259e7186768091a8f4413449e1cae17ba0669c9af932bda2619d8fadf798d495",
    "src/components/timer/Timer.cpp": "9d5bf76d3a43b887052d85a9883d171a07347b7e6af17d1aa0461949ad46d189",
    "src/components/timer/Timer.h": "4fc78e15585bfac0e1df48a6e105737fdcc27f21891f96b321510e9ceb6e3b9d",
    "src/displayapp/screens/Timer.cpp": "7fd3fd9dc13f8002776b25d5619e085d3570b18ed1093172dba8f81cedf2820b",
    "src/displayapp/widgets/Counter.cpp": "d1f83129bcac55b218de4f3ce83a2fbe750ce0c902ab72ac6e14d10c2d25a05a",
}
WHOLE = [name for name in SHA if not name.startswith("src/displayapp/")]
# SOURCE.md changes 1, 2, 4 and 5: original line -> copied line(s).
CHANGED = {
    "      static constexpr uint8_t histSize = 4;": ["      static constexpr uint8_t histSize = 99;"],
    "    if (timerStatus->distanceToExpiry.count() > 10000 && motorController.IsRinging()) {":
        ["    if (timerStatus->distanceToExpiry.count() > 60000 && motorController.IsRinging()) {"],
    "    std::chrono::duration_cast<std::chrono::seconds>(timer.GetTimerState().value_or(Controllers::Timer::TimerStatus {}).distanceToExpiry);":
        ["    std::chrono::ceil<std::chrono::seconds>(timer.GetTimerState().value_or(Controllers::Timer::TimerStatus {}).distanceToExpiry);"],
    "    minuteCounter.SetValue(displaySeconds.Get().count() / 60);":
        ["    hourCounter.SetValue(displaySeconds.Get().count() / 3600);", "    minuteCounter.SetValue(displaySeconds.Get().count() / 60 % 60);"],
    "  } else if (secondCounter.GetValue() + minuteCounter.GetValue() > 0) {":
        ["  } else if (secondCounter.GetValue() + minuteCounter.GetValue() + hourCounter.GetValue() > 0) {"],
    "    auto timerDuration = std::chrono::minutes(minuteCounter.GetValue()) + std::chrono::seconds(secondCounter.GetValue());":
        ["    auto timerDuration = std::chrono::hours(hourCounter.GetValue()) + std::chrono::minutes(minuteCounter.GetValue()) + std::chrono::seconds(secondCounter.GetValue());"],
}
# Whole functions: (original, copy, [ranges]); between ranges the copy has one empty line. Changes 2 and 3 drop
# 131-134 and 137-145 of Timer.cpp, which is why Refresh is three ranges with nothing between them.
PARTS = [
    ("src/displayapp/screens/Timer.cpp", "src/displayapp/screens/timer_screen_part.cpp",
     [[(7, 7)], [(118, 130), (135, 136), (146, 147)], [(149, 156)], [(181, 196)], [(198, 202)]]),
    ("src/displayapp/widgets/Counter.cpp", "src/displayapp/widgets/counter_part.cpp", [[(5, 5)], [(35, 45)], [(47, 57)], [(59, 62)]]),
]


def changed(lines):
    out = []
    for line in lines:
        out += CHANGED.get(line, [line])
    return out


def main():
    if not CLONE.exists():
        print("clock-3-source: SKIP, no read-only InfiniTime copy at work/infinitime (see SOURCE.md)")
        return 77
    errors, checked, used = [], 0, set()
    for name, digest in SHA.items():
        got = hashlib.sha256((CLONE / name).read_bytes()).hexdigest()
        if got != digest:
            errors.append(f"{name}: SHA-256 {got}, SOURCE.md says {digest}")
    for name in WHOLE:
        original = (CLONE / name).read_text().split("\n")
        expected = changed(original)
        used |= {line for line in original if line in CHANGED}
        checked += len(original)
        if (COPY / name).read_text().split("\n") != expected:
            errors.append(f"{name}: differs from the original beyond the listed changes")
    for name, part, functions in PARTS:
        original = (CLONE / name).read_text().split("\n")
        expected = []
        for i, ranges in enumerate(functions):
            expected += [""] if i else []
            for first, last in ranges:
                lines = original[first - 1:last]
                used |= {line for line in lines if line in CHANGED}
                checked += len(lines)
                expected += changed(lines)
        expected += [""]  # the copy ends with a newline
        got = (COPY / part).read_text().split("\n")
        if got != expected:
            bad = next((i for i, (a, b) in enumerate(zip(got, expected)) if a != b), min(len(got), len(expected)))
            errors.append(f"{part}: line {bad + 1} is {got[bad] if bad < len(got) else None!r}, expected "
                          f"{expected[bad] if bad < len(expected) else None!r}")
    if used != set(CHANGED):
        errors.append(f"listed changes not found in the original: {sorted(set(CHANGED) - used)}")
    print(f"clock-3-source: {checked} original lines checked, {len(errors)} problems")
    for e in errors[:20]:
        print("  " + e)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
