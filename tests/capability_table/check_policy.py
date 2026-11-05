"""Compare catalogue wording with observed bus calls and the real member gate."""
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import capabilities as catalogue


def check(document, observed):
    def require(condition, message):
        if not condition:
            raise ValueError(message)

    def cells(identifier):
        rows = [line for line in document.splitlines() if line.startswith(f"| `{identifier}` |")]
        require(len(rows) == 1, f"missing or duplicate row: {identifier}")
        return rows[0].split(" | ")

    calls = {c["case"]: c for c in observed["calls"]}
    expected = {"child_volume": 0, "child_get": 0, "child_notify": 0, "child_restricted_setting": 4,
                "child_whitelist_extra": 0, "child_no_whitelist": 4, "ordinary_setting_undo": 9,
                "ordinary_operation_undo": 9, "permission_undo": 3}
    require(len(calls) == len(observed["calls"]) and set(calls) == set(expected), "policy cases differ")
    for name, status in expected.items():
        require(calls[name]["status"] == status, f"unexpected real call result: {name}")
    for name in ("child_volume", "child_notify"):
        require(calls[name]["whitelisted"] is False, f"case must have no whitelist match: {name}")
    require(calls["child_whitelist_extra"]["whitelisted"] is True, "extra whitelist must match")
    require(calls["child_volume"]["after"] == 35 and calls["ordinary_setting_undo"]["after"] == 35,
            "ordinary setting/undo did not restore the value")
    restricted = calls["child_restricted_setting"]
    require(restricted["before"] == restricted["after"], "rejected child write changed the setting")
    require(calls["permission_undo"]["after"] == 0, "permission changed before human confirmation")

    for identifier in ("settings.get", "settings.set", "notify.list", "system.undo"):
        require(cells(identifier)[5].startswith("成人、儿童（无需白名单）"),
                f"wording contradicts unlisted child call: {identifier}")
    require(cells("audio.volume")[4] == "成人、儿童（无需白名单）", "wording restricts child-writable setting")
    require(cells("audio.child_max")[4] == "仅成人", "wording permits child-restricted setting")
    require(cells("ai.read_clipboard")[5] == "高 / 人、AI 均需确认", "permission setting must require confirmation")
    require(cells("system.undo")[4] == "设置撤销按设置项重新判断（见设置表）；其他撤销无需",
            "wording contradicts ordinary/permission undo results")

    expected_gates = {("system.shutdown", 0): 0, ("system.shutdown", 2): 4,
                      ("system.reboot", 1): 0, ("system.reboot", 2): 4,
                      ("system.factory_reset", 0): 4, ("system.factory_reset", 2): 4,
                      ("reader.catalogue_system", 0): 4, ("reader.catalogue_system", 2): 4}
    gates = {(g["id"], g["source"]): g["status"] for g in observed["system_gates"]}
    require(len(gates) == len(observed["system_gates"]) and gates == expected_gates, "unexpected real system gate result")
    for identifier in ("system.shutdown", "system.reboot"):
        require(cells(identifier)[5].startswith("成人；儿童仅人工") and "白名单" not in cells(identifier)[5],
                f"wording contradicts child system gate: {identifier}")
    require(cells("system.factory_reset")[5] == "仅成人", "system whitelist must not grant child access")


def main():
    entries, sources = catalogue.load(ROOT)
    document = catalogue.render(entries, sources)
    observed = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    check(document, observed)
    negatives = {
        "child whitelist": document.replace("成人、儿童（无需白名单）", "成人；儿童须白名单"),
        "system whitelist": document.replace("成人；儿童仅人工", "成人；儿童仅人工且须白名单"),
        "setting member": document.replace("| `audio.child_max` | 孩子的音量上限 | `INK_SET_INT` | `settings.get` / `settings.set` | 仅成人",
                                           "| `audio.child_max` | 孩子的音量上限 | `INK_SET_INT` | `settings.get` / `settings.set` | 成人、儿童（无需白名单）"),
        "undo confirmation": document.replace("设置撤销按设置项重新判断（见设置表）；其他撤销无需", "无需"),
    }
    for label, changed in negatives.items():
        if changed == document:
            raise ValueError(f"negative control did not change wording: {label}")
        try:
            check(changed, observed)
        except ValueError:
            continue
        raise ValueError(f"negative control accepted: {label}")
    print(f"PASS: {len(observed['calls'])} real calls, {len(observed['system_gates'])} system gates, "
          f"{len(negatives)} incorrect explanations rejected")


if __name__ == "__main__":
    main()
