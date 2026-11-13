"""The copied AgentUI text against the original: hashes, line ranges, and only the changes SOURCE.md lists.

Needs the read-only clone at work/agentui (pinned commit, see docs/tasks/gallery-3/SOURCE.md); without it the test
says so and is skipped (exit 77): the CI checkout has no clone.
"""
import hashlib
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
CLONE = ROOT / "work/agentui/main/display/agent_ui/apps/camera"
PART = ROOT / "system/apps/gallery/vendor/agentui/camera_gallery_repository_part.h"
SHA = {"camera_gallery_repository.cc": "0a8dcf664db73014902f12aa025630d1dc595e9c04c900ac006a3c5cd26bd29e",
       "camera_view.cc": "859fd456ff060dc5037e9ea1a44fda43977f21fd196b219ffed39a4100c063f4"}
# (file, first line, last line); 1-based, inclusive, as SOURCE.md lists them
RANGES = [("camera_gallery_repository.cc", 16, 17), ("camera_gallery_repository.cc", 24, 41), ("camera_gallery_repository.cc", 64, 85),
          ("camera_gallery_repository.cc", 87, 92), ("camera_gallery_repository.cc", 94, 99), ("camera_gallery_repository.cc", 101, 112),
          ("camera_gallery_repository.cc", 154, 158), ("camera_view.cc", 1109, 1120), ("camera_gallery_repository.cc", 114, 114)]
# The listed changes: original line -> copied line (SOURCE.md, changes 1-3; change 4 only adds wrapping lines).
CHANGED = {
    '    std::snprintf(value, sizeof(value), "%d月%d日", local.tm_mon + 1,': '    std::snprintf(value, sizeof(value), TR("%d月%d日"), local.tm_mon + 1,',
    '    if (bytes == 0) return "大小未知";': '    if (bytes == 0) return TR("大小未知");',
    '    std::sort(result.begin(), result.end(), [](const GalleryPhoto& left,': '    std::sort(result.begin(), result.end(), [](const auto& left,',
    '                                               const GalleryPhoto& right) {': '                                               const auto& right) {',
}
WRAPPERS = {"template <typename Photos>", "void SortNewestFirst(Photos& result) {", "}", "template <typename Gallery>",
            "std::vector<std::pair<std::size_t, std::size_t>> GroupByDateKey(const Gallery& gallery) {", "    return groups;",
            "}  // namespace agent_ui::camera", "#pragma once", ""}


def main():
    if not CLONE.exists():
        print("gallery-3-source: SKIP, no read-only AgentUI clone at work/agentui (see SOURCE.md)")
        return 77
    errors = []
    for name, digest in SHA.items():
        got = hashlib.sha256((CLONE / name).read_bytes()).hexdigest()
        if got != digest:
            errors.append(f"{name}: SHA-256 {got}, SOURCE.md says {digest}")
    part = PART.read_text().split("\n")
    body = part[part.index("#pragma once"):]
    expected = []
    for name, first, last in RANGES:
        lines = (CLONE / name).read_text().split("\n")[first - 1:last]
        expected += [CHANGED.get(line, line).replace("state.gallery", "gallery") for line in lines]
    at = 0
    for line in expected:
        while at < len(body) and body[at] != line:
            if body[at].strip() and body[at] not in WRAPPERS:
                errors.append(f"line not from the original and not a listed change: {body[at]!r}")
            at += 1
        if at == len(body):
            errors.append(f"original line missing from the copy: {line!r}")
            break
        at += 1
    if sum(copied in body for copied in CHANGED.values()) != 4:
        errors.append("the four listed line changes are not all present")
    print(f"gallery-3-source: {len(expected)} original lines checked, {len(errors)} problems")
    for e in errors[:20]:
        print("  " + e)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
