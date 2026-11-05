#!/usr/bin/env python3
"""Generate the capability catalogue from designated C initializers, without C dependencies."""
import argparse
import ast
from dataclasses import dataclass
import difflib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
DOC = Path("docs/system/capabilities.md")
TYPES = {"ink_cap_t": "cap", "ink_setting_def_t": "setting", "ink_state_def_t": "state"}
EFFECTS = {
    "INK_FX_READ": "低 / read",
    "INK_FX_NAVIGATE": "低 / navigate",
    "INK_FX_LOCAL_WRITE": "中 / local_write",
    "INK_FX_LOCAL_DESTRUCTIVE": "高 / local_destructive",
    "INK_FX_EXTERNAL": "高 / external",
    "INK_FX_SYSTEM": "高 / system",
}
FORMS = ["INK_FORM_CALL", "INK_FORM_STREAM", "INK_FORM_EVENT", "INK_FORM_SESSION"]
EXPOSE = {"INK_EXPOSE_APP": 1, "INK_EXPOSE_AI": 2, "INK_EXPOSE_REMOTE": 4, "INK_EXPOSE_OWNER": 8}
# Strings, character literals and comments must be tokens before punctuation is considered.
LEX = re.compile(r'(?P<comment>/\*[\s\S]*?\*/|//[^\n]*)|(?P<string>"(?:\\.|[^"\\])*")|'
                 r"(?P<char>'(?:\\.|[^'\\])*')|(?P<word>[A-Za-z_]\w*|[0-9]+)|(?P<other>\S)")


@dataclass
class Entry:
    kind: str
    fields: dict
    path: str
    line: int

    def value(self, name, default=""):
        return "".join(self.fields.get(name, [default]))

    def string(self, name, default=""):
        if name not in self.fields:
            return default
        tokens = self.fields[name]
        if not tokens or any(not t.startswith('"') for t in tokens):
            raise ValueError(f"{self.path}:{self.line}: {name} must be literal strings")
        return "".join(ast.literal_eval(t) for t in tokens)

    def flag(self, name):
        value = self.value(name, "false")
        if value not in ("true", "false", "0", "1"):
            raise ValueError(f"{self.path}:{self.line}: unsupported boolean {name}={value}")
        return value in ("true", "1")

    @property
    def id(self):
        return self.string("id")

    @property
    def group(self):
        path = Path(self.path)
        return str(path.parent) if path.parts[:2] in (("system", "services"), ("system", "apps")) or path.parts[0] == "apps" else str(path.with_suffix(""))


def split(tokens, delimiter):
    """Split at the outer level only, retaining nested initializer/schema expressions."""
    parts, start, depth = [], 0, 0
    for i, token in enumerate(tokens):
        if token in ("{", "(", "["):
            depth += 1
        elif token in ("}", ")", "]"):
            depth -= 1
        elif token == delimiter and not depth:
            parts.append(tokens[start:i])
            start = i + 1
    return parts + ([tokens[start:]] if tokens[start:] else [])


def parse_source(path, source):
    macros = {}
    for match in re.finditer(r"^\s*#define\s+(\w+)\(([^)\n]*)\)\s+([^\n]+)", source, re.MULTILINE):
        parameters = [p.strip() for p in match[2].split(",")]
        body = [m.group() for m in LEX.finditer(match[3]) if m.lastgroup != "comment"]
        macros[match[1]] = (parameters, body)
    matches = [m for m in LEX.finditer(source) if m.lastgroup != "comment"]
    tokens = [m.group() for m in matches]
    entries = []
    for i, token in enumerate(tokens):
        if token not in TYPES or i + 2 >= len(tokens) or not re.fullmatch(r"[A-Za-z_]\w*", tokens[i + 1]):
            continue
        end = i + 2
        while end < len(tokens) and tokens[end] not in ("=", ";", "(", ","):
            end += 1
        if end + 1 >= len(tokens) or tokens[end:end + 2] != ["=", "{"]:
            continue
        opening = end + 1
        end, depth = opening + 1, 1
        while end < len(tokens) and depth:
            depth += (tokens[end] == "{") - (tokens[end] == "}")
            end += 1
        if depth:
            raise ValueError(f"{path}: unterminated {token} initializer")
        body = tokens[opening + 1:end - 1]
        records = split(body, ",") if "[" in tokens[i + 2:opening] else [["{", *body, "}"]]
        for record in records:
            if record and record[0] in macros and record[1] == "(" and record[-1] == ")":
                parameters, expansion = macros[record[0]]
                arguments = split(record[2:-1], ",")
                if len(parameters) != len(arguments):
                    raise ValueError(f"{path}: invalid initializer macro arguments")
                values = dict(zip(parameters, arguments))
                record = [part for token in expansion for part in values.get(token, [token])]
            if len(record) < 2 or record[0] != "{" or record[-1] != "}":
                raise ValueError(f"{path}: {token} requires explicit designated initializers")
            fields = {}
            for field in split(record[1:-1], ","):
                if len(field) < 4 or field[0] != "." or field[2] != "=" or field[1] in fields:
                    raise ValueError(f"{path}: invalid or duplicate designated field {field}")
                fields[field[1]] = field[3:]
            line = source.count("\n", 0, matches[i].start()) + 1
            entries.append(Entry(TYPES[token], fields, str(path), line))
    return entries


def exposure(entry):
    value = entry.value("expose", "0")
    if value == "0":
        return 0
    parts = value.split("|")
    if any(part not in EXPOSE for part in parts):
        raise ValueError(f"{entry.path}:{entry.line}: unsupported exposure {value}")
    return sum(set(EXPOSE[part] for part in parts))


def validate(entries):
    seen = set()
    for entry in entries:
        key = (entry.kind, entry.id)
        if not re.fullmatch(r"[a-z][a-z0-9_.]*", entry.id) or key in seen:
            raise ValueError(f"{entry.path}:{entry.line}: invalid or duplicate {entry.kind} id {entry.id}")
        seen.add(key)
        if not entry.string("title").strip():
            raise ValueError(f"{entry.path}:{entry.line}: {entry.id} has no description")
        if entry.kind == "cap":
            if entry.value("effect") not in EFFECTS:
                raise ValueError(f"{entry.id}: missing or invalid explicit risk/effect")
            if entry.value("form", FORMS[0]) not in FORMS:
                raise ValueError(f"{entry.id}: unsupported form")
            if entry.value("form") != "INK_FORM_EVENT" and entry.value("run", "NULL") == "NULL":
                raise ValueError(f"{entry.id}: callable capability has no handler")
            for field in ("child_ok", "while_locked", "parent_pin", "ask_by_hand"):
                entry.flag(field)
            exposure(entry)
            if "activity" in entry.fields:
                entry.string("activity")
        elif entry.kind == "setting":
            if entry.value("type") not in ("INK_SET_BOOL", "INK_SET_INT", "INK_SET_ENUM"):
                raise ValueError(f"{entry.id}: invalid setting type")
            for field in ("system", "child_may_change", "ask_by_hand"):
                entry.flag(field)
        else:
            if entry.value("type") not in ("INK_ST_BOOL", "INK_ST_INT", "INK_ST_STR"):
                raise ValueError(f"{entry.id}: invalid state type")
            exposure(entry)
    caps = {e.id: e for e in entries if e.kind == "cap"}
    if any(e.kind == "setting" for e in entries):
        for id in ("settings.get", "settings.set"):
            if id not in caps or caps[id].value("form") == "INK_FORM_EVENT":
                raise ValueError(f"every setting needs the shared callable capability {id}")
            if exposure(caps[id]) and not exposure(caps[id]) & EXPOSE["INK_EXPOSE_AI"]:
                raise ValueError(f"{id}: settings must be accessible to AI")
        if caps["settings.get"].value("effect") != "INK_FX_READ" or caps["settings.set"].value("effect") != "INK_FX_LOCAL_WRITE":
            raise ValueError("shared settings capabilities changed their effects; review the setting policy")


def load(root):
    sources = {str(path.relative_to(root)): path.read_text(encoding="utf-8")
               for folder in ("system", "apps") for path in sorted((root / folder).rglob("*.c"))}
    entries = [entry for path, source in sources.items() for entry in parse_source(path, source)]
    validate(entries)
    return sorted(entries, key=lambda e: (e.kind, e.id)), sources


def ai_allowed(entry):
    bits = exposure(entry)
    return (not bits or bool(bits & EXPOSE["INK_EXPOSE_AI"])) and (entry.value("form") == "INK_FORM_EVENT" or "upgrade" not in entry.id)


def callers(entry):
    if entry.value("form") == "INK_FORM_EVENT":
        return "不可调用；AI 可订阅" if ai_allowed(entry) else "不可调用；不向 AI 转发"
    return "人、AI" if ai_allowed(entry) else "仅人（设备）"


def confirmation(entry):
    if entry.value("form") == "INK_FORM_EVENT":
        return "不适用"
    if entry.id == "settings.set":
        return "按设置项（见设置表）"
    if entry.id == "system.undo":
        return "设置撤销按设置项重新判断（见设置表）；其他撤销无需"
    if entry.flag("parent_pin"):
        return "人、AI 均确认；已设成人 PIN 时还需 PIN"
    if entry.flag("ask_by_hand"):
        return "人、AI 均在设备确认" if ai_allowed(entry) else "人在设备确认"
    if entry.value("effect") == "INK_FX_LOCAL_DESTRUCTIVE":
        return "人无需；AI 需设备确认"
    if entry.value("effect") == "INK_FX_EXTERNAL":
        return "人无需；AI 仅 asked=true 时无需"
    return "无需"


def confirm_value(entry, pin, src, asked):
    """Numeric policy result, cross-checked against the compiled confirm_for in CTest."""
    if src == 6:  # CONFIRMED
        return 0
    if entry.flag("parent_pin") and pin:
        return 2
    if entry.flag("ask_by_hand"):
        return 1
    if src in (0, 1, 3):  # TOUCH, KEY, SYSTEM
        return 0
    return int(entry.value("effect") == "INK_FX_LOCAL_DESTRUCTIVE" or
               (entry.value("effect") == "INK_FX_EXTERNAL" and not asked))


def member_scope(entry):
    if entry.value("form") == "INK_FORM_EVENT":
        return "按原事件成员及代际投递"
    if entry.value("effect") == "INK_FX_SYSTEM":
        rule = "成人；儿童仅人工" if entry.flag("child_ok") else "仅成人"
    else:
        rule = "成人、儿童（无需白名单）" if entry.flag("child_ok") else "成人；儿童须白名单"
    if entry.flag("while_locked"):
        rule += "；锁定时仅人工入口"
    if "activity" in entry.fields:
        rule += "；页面 " + entry.string("activity")
    if entry.id == "settings.set":
        rule += "；设置写入另受设置项限制"
    if entry.id == "system.undo":
        rule += "；设置撤销另受设置项限制"
    return rule


def code(value):
    return f"`{value}`"


def codes(values):
    return "、".join(code(v) for v in sorted(set(values))) or "—"


def row(*values):
    return "| " + " | ".join(str(v).replace("|", "\\|").replace("\n", " ") for v in values) + " |"


def render(entries, sources):
    caps = [e for e in entries if e.kind == "cap"]
    settings = [e for e in entries if e.kind == "setting"]
    states = [e for e in entries if e.kind == "state"]
    events = [e for e in caps if e.value("form") == "INK_FORM_EVENT"]
    groups = sorted({e.group for e in entries})
    group_ids = {group: f"s{i + 1:02d}" for i, group in enumerate(groups)}
    lines = ["# 能力总表（代码生成）", "",
             "由 `tools/capabilities.py` 读取 `system/`、`apps/` 的 `ink_cap_t`、`ink_setting_def_t`、`ink_state_def_t` 定义生成；请修改代码后重新生成，不直接编辑本文件。",
             "", "```sh", "python3 tools/capabilities.py", "python3 tools/capabilities.py --check", "```", "",
             f"当前共 {len(caps)} 项：可调用能力 {len(caps) - len(events)}、事件 {len(events)}；设置 {len(settings)} 项、状态 {len(states)} 项。本表列设备固件的静态定义，不含中枢运行时的远端目录。静态目录包含按硬件条件注册的能力；实际可用性仍取决于硬件、当前页面及服务状态。CTest 另外与 Plus 模拟器启动后的完整注册表核对，不使用按成员或页面过滤的 AI 清单代替完整目录，并核对实际 C 确认策略的来源、PIN 和 asked 组合。", "",
             "风险按代码的 effect 归类：read / navigate 为低，local_write 为中，local_destructive / external / system 为高；这是副作用等级，不等于所有高等级操作都弹确认。描述和 effect 必须显式填写，遗漏、未知值、重复 id 或文档过期均使检查失败。", "",
             "调用和确认规则来自 `system/policy/cap.c` 的 exposed / gate / confirm_for，以及 `system/policy/settings.c` 的 setting_set。表中人指触摸、实体键；AI 指 INK_SRC_AI，外部调用还须各自的 expose 权限。AI 不能代输 PIN 或替人按确认。asked=true 只取消普通对外动作的确认，不绕过 ask_by_hand、成人 PIN、成员、页面版本或服务内部校验。", "",
             "成员范围指能力闸门允许的调用成员，均绑定调用时的当前成员及代际，不表示能访问其他成员数据。非 system 能力的 child_ok 直接放行儿童，白名单按 id 前缀额外放行 child_ok=false 的能力；system 能力则只允许 child_ok=true 的人工儿童调用，白名单不能绕过。成人空间关闭时 AI、外部应用都不能调用；while_locked 只放行人工入口。数据集合、文件路径、通知、闹钟、后台作业仍检查各自的归属；设置是设备级值，儿童写权限另列。事件只能订阅，不能当工具调用，投递仍按成员隔离。", "",
             "system.undo 的外层 confirm_for 无需确认，不代表整个撤销无需确认。设置撤销保留当前来源，重新经过 settings.set 及设置项的儿童、确认、版本检查；其他撤销以已确认来源调用逆操作，仍检查逆操作的成员及服务条件。", "",
             "关联面按同一服务目录（policy / ui 按源文件）及该提供方源码引用的已知 id 汇集，表示服务上下文，**不表示每次调用都会修改列出的所有设置、状态或发出所有事件**。完整设置、状态和事件定义均列在后文。没有定义的关联显示 —，不据命名前缀猜测。", "",
             "## 可调用能力", "",
             row("能力 id", "描述", "谁能调", "风险 / effect", "是否确认", "成员范围", "关联面", "定义"),
             row(*(["---"] * 8))]
    for entry in caps:
        if entry in events:
            continue
        group = group_ids[entry.group]
        source = f"[{entry.path}](../../{entry.path})"
        lines.append(row(code(entry.id), entry.string("title"), callers(entry), EFFECTS[entry.value("effect")],
                         confirmation(entry), member_scope(entry), f"[{group}](#{group})", source))
    lines += ["", "## 关联的设置、状态和事件", ""]
    for group in groups:
        anchor = group_ids[group]
        own_sources = [source for path, source in sources.items()
                       if Entry("", {}, path, 0).group == group]
        literals = {ast.literal_eval(m.group()) for source in own_sources for m in LEX.finditer(source) if m.lastgroup == "string"}
        lines += [f'<a id="{anchor}"></a>', "", f"### {anchor} — {code(group)}", ""]
        for label, pool in (("设置", settings), ("状态", states), ("事件", events)):
            related = [e.id for e in pool if e.group == group or e.id in literals]
            if group == "system/policy/settings" and label == "设置":
                related = [e.id for e in settings]
            if group == "system/policy/system_actions" and label in ("状态", "设置"):
                related = [e.id for e in (states if label == "状态" else settings)]
            lines.append(f"{label}：{codes(related)}。")
            lines.append("")
    lines += ["## 每个设置项的能力覆盖", "",
              "所有已定义设置都通过 `settings.get` 读取、`settings.set` 写入，由设置表按 id 查找；`settings.list` 列出类型、范围和值。写入继续检查范围、儿童限制、确认和应用执行器，通用能力不跳过这些检查。", "",
              row("设置 id", "描述", "类型", "读取 / 写入能力", "允许修改的成员", "风险 / 确认", "定义"), row(*(["---"] * 7))]
    for entry in settings:
        children = entry.flag("child_may_change") and not entry.flag("system")
        risk = "高" if entry.flag("system") or entry.flag("ask_by_hand") else "中"
        ask = "人、AI 均需确认" if entry.flag("ask_by_hand") else "无需"
        lines.append(row(code(entry.id), entry.string("title"), code(entry.value("type")),
                         "`settings.get` / `settings.set`", "成人、儿童（无需白名单）" if children else "仅成人",
                         risk + " / " + ask, f"[{entry.path}](../../{entry.path})"))
    lines += ["", "## 状态", "", row("状态 id", "描述", "类型", "AI 可读", "定义"), row(*(["---"] * 5))]
    for entry in states:
        bits = exposure(entry)
        lines.append(row(code(entry.id), entry.string("title"), code(entry.value("type")),
                         "是" if not bits or bits & EXPOSE["INK_EXPOSE_AI"] else "否", f"[{entry.path}](../../{entry.path})"))
    lines += ["", "## 事件", "", row("事件 id", "描述", "谁能调 / 订阅", "风险 / effect", "确认", "成员范围", "关联面", "定义"), row(*(["---"] * 8))]
    for entry in events:
        group = group_ids[entry.group]
        lines.append(row(code(entry.id), entry.string("title"), callers(entry), EFFECTS[entry.value("effect")],
                         confirmation(entry), member_scope(entry), f"[{group}](#{group})", f"[{entry.path}](../../{entry.path})"))
    return "\n".join(lines) + "\n"


def check_runtime(entries, path):
    runtime = json.loads(path.read_text(encoding="utf-8"))
    caps = {e.id: e for e in entries if e.kind == "cap"}
    actual = runtime["capabilities"]
    if len({e["id"] for e in actual}) != len(actual) or {e["id"] for e in actual} != set(caps):
        raise ValueError("runtime capability ids differ from the source catalogue")
    for item in actual:
        entry = caps[item["id"]]
        expected = {"id": entry.id, "title": entry.string("title"), "effect": list(EFFECTS).index(entry.value("effect")),
                    "form": FORMS.index(entry.value("form", FORMS[0])), "expose": exposure(entry),
                    "activity": entry.string("activity"), **{name: entry.flag(name) for name in ("child_ok", "while_locked", "ask_by_hand", "parent_pin")},
                    "confirmations": [confirm_value(entry, pin, src, asked) for pin in (False, True) for src in range(7) for asked in (False, True)]}
        if item != expected:
            raise ValueError(f"runtime metadata differs: {entry.id}")
    settings = {e.id: e for e in entries if e.kind == "setting"}
    if {e["id"] for e in runtime["settings"]} != set(settings) or len(runtime["settings"]) != len(settings):
        raise ValueError("runtime setting ids differ from the source catalogue")
    for item in runtime["settings"]:
        entry = settings[item["id"]]
        setting_type = {"INK_SET_BOOL": "bool", "INK_SET_INT": "int", "INK_SET_ENUM": "enum"}[entry.value("type")]
        if item["title"] != entry.string("title") or item["type"] != setting_type or any(
                type(item.get(flag)) is not bool or item[flag] != entry.flag(flag)
                for flag in ("system", "ask_by_hand", "child_may_change")):
            raise ValueError(f"runtime setting metadata differs: {entry.id}")
    print(f"Runtime matches: {len(actual)} capabilities, {len(settings)} settings")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, default=DOC)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--runtime", type=Path)
    args = parser.parse_args()
    try:
        entries, sources = load(args.root)
        generated = render(entries, sources)
        output = args.root / args.output
        if args.check:
            existing = output.read_text(encoding="utf-8") if output.exists() else ""
            if existing != generated:
                print("".join(difflib.unified_diff(existing.splitlines(True), generated.splitlines(True), fromfile=str(args.output), tofile="generated")))
                raise ValueError("capability catalogue is stale; run python3 tools/capabilities.py")
        else:
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(generated, encoding="utf-8")
        if args.runtime:
            check_runtime(entries, args.runtime)
        print(f"PASS: {len(entries)} definitions, {args.output}")
    except (ValueError, OSError) as exc:
        parser.exit(1, f"FAIL: {exc}\n")


if __name__ == "__main__":
    main()
