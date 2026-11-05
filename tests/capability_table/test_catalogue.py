"""Regression and negative controls for the source-derived capability catalogue."""
import copy
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import capabilities as catalogue


class CatalogueTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.entries, cls.sources = catalogue.load(ROOT)

    def entries_copy(self):
        return copy.deepcopy(self.entries)

    def test_missing_description_rejected(self):
        for title in (None, ['""'], ['"   "']):
            with self.subTest(title=title):
                entries = self.entries_copy()
                entry = next(e for e in entries if e.kind == "cap")
                if title is None:
                    entry.fields.pop("title")
                else:
                    entry.fields["title"] = title
                with self.assertRaisesRegex(ValueError, "no description"):
                    catalogue.validate(entries)

    def test_risk_must_be_explicit_and_known(self):
        for effect in (None, ["INK_FX_UNKNOWN"], ["99"]):
            with self.subTest(effect=effect):
                entries = self.entries_copy()
                entry = next(e for e in entries if e.kind == "cap")
                if effect is None:
                    entry.fields.pop("effect")
                else:
                    entry.fields["effect"] = effect
                with self.assertRaisesRegex(ValueError, "risk/effect"):
                    catalogue.validate(entries)

    def test_all_settings_need_read_and_write_capabilities(self):
        for id in ("settings.get", "settings.set"):
            with self.subTest(id=id):
                with self.assertRaisesRegex(ValueError, "every setting needs"):
                    catalogue.validate([e for e in self.entries if not (e.kind == "cap" and e.id == id)])

    def test_new_setting_is_automatically_covered(self):
        entries = self.entries_copy()
        setting = next(e for e in entries if e.kind == "setting")
        setting.fields["id"] = ['"test.new_setting"']
        catalogue.validate(entries)
        document = catalogue.render(entries, self.sources)
        self.assertRegex(document, r"\| `test.new_setting` \|[^\n]+`settings.get` / `settings.set`")

    def test_duplicate_id_rejected(self):
        entries = self.entries_copy()
        entries.append(copy.deepcopy(entries[0]))
        with self.assertRaisesRegex(ValueError, "duplicate"):
            catalogue.validate(entries)

    def test_unknown_flags_fail_closed(self):
        for field in ("expose", "child_ok", "form"):
            entries = self.entries_copy()
            next(e for e in entries if e.kind == "cap").fields[field] = ["UNKNOWN"]
            with self.subTest(field=field), self.assertRaises(ValueError):
                catalogue.validate(entries)

    def test_strings_comments_and_nested_schemas(self):
        source = '''
        /* ink_cap_t ignored[] = {{.id="bad"}}; */
        static const ink_cap_t CAPS[] = {
          {.id="test.one", .title="字 { 逗号, 和 \\"引号\\"" "相邻串",
           .effect=INK_FX_READ, .input=SCHEMA("x", (1 + 2)), .run=handler},
          {.id="test.event", .title="事件", .form=INK_FORM_EVENT, .effect=INK_FX_READ},
        };
        static const ink_state_def_t ST = {.id="test.state", .title="状态", .type=INK_ST_BOOL};
        static const ink_cap_t *pointer;
        #define CAP(id_, title_, run_) {.id=id_, .title=title_, .effect=INK_FX_NAVIGATE, .run=run_}
        static const ink_cap_t MACROS[] = {CAP("test.macro", "宏定义", handler)};
        '''
        entries = catalogue.parse_source("system/services/test/test.c", source)
        catalogue.validate(entries)
        self.assertEqual([e.id for e in entries], ["test.one", "test.event", "test.state", "test.macro"])
        self.assertEqual(entries[0].string("title"), '字 { 逗号, 和 "引号"相邻串')

    def test_confirmation_and_member_conditions(self):
        caps = {e.id: e for e in self.entries if e.kind == "cap"}
        self.assertIn("asked=true", catalogue.confirmation(caps["message.send"]))
        self.assertIn("均在设备确认", catalogue.confirmation(caps["file.delete"]))
        self.assertIn("PIN", catalogue.confirmation(caps["member.remove"]))
        self.assertFalse(catalogue.ai_allowed(caps["member.unlock"]))
        self.assertFalse(catalogue.ai_allowed(caps["confirm.accept"]))
        self.assertIn("仅人工", catalogue.member_scope(caps["system.shutdown"]))
        self.assertEqual(catalogue.confirmation(caps["call.incoming"]), "不适用")

    def test_check_rejects_stale_document(self):
        # Only the output copy changes; the repository document and sources remain intact.
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "catalogue.md"
            output.write_text(catalogue.render(self.entries, self.sources) + "stale\n", encoding="utf-8")
            result = subprocess.run([sys.executable, str(ROOT / "tools/capabilities.py"), "--check", "--output", str(output)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertIn("catalogue is stale", result.stderr)

    def test_source_inventory_contains_owner_only_and_page_capabilities(self):
        caps = {e.id: e for e in self.entries if e.kind == "cap"}
        self.assertIn("system.upgrade", caps)
        self.assertEqual(caps["reader.next_page"].string("activity"), "book")
        self.assertEqual(caps["confirm.result"].value("form"), "INK_FORM_EVENT")
        # Separate namespaces may use the same id, e.g. battery.low state and event.
        self.assertTrue(any(e.kind == "state" and e.id == "battery.low" for e in self.entries))
        self.assertIn("battery.low", caps)


if __name__ == "__main__":
    unittest.main(verbosity=2)
