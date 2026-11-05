"""Required icons include tables, branches, helper calls and app declarations."""
import json
from pathlib import Path
import runpy
import unittest

ROOT = Path(__file__).resolve().parents[1]
CHECKER = runpy.run_path(str(ROOT / 'tools/icongen/check.py'))
NAMES = set(json.loads((ROOT / 'assets/icons/manifest.json').read_text())['names'])


class IconCheck(unittest.TestCase):
    def check_source(self, source):
        return CHECKER['check_sources']({Path('probe.c'): source}, NAMES)

    def test_unknown_names(self):
        for source in (
            'ink_ui_icon(o, "missingReviewIcon", px, look, x, y, false);',
            'ink_ui_circle(o, flag ? "book" : "missingReviewIcon", x,y,d,px,s,0,0);',
            'ink_ui_list_row(o, "missingReviewIcon", title,value,x,y,w,false,0,0);',
            'static const char *const APP_ICONS[]={"missingReviewIcon","book"};',
            'static const char *const GROUP_ICONS[][2]={{"book","missingReviewIcon"}};',
            'static const char *const icons[]={"book","missingReviewIcon"};',
            'const char *icon=flag ? "book" : "missingReviewIcon"; ink_ui_circle(o,icon,x,y,d,px,s,0,0);',
            'const char *icon=INK_UI_ICON(missingReviewIcon);',
            'const tool_t MORE[]={{"clock","时钟",INK_UI_ICON(missingReviewIcon)}};',
            'const ink_app_def_t a={.id="tool",.icon="missingReviewIcon",.entry=&page};',
            'ink_ui_icon(o, "missing" "ReviewIcon", px,look,x,y,false);',
            'ink_ui_icon(o, (flag ? "book" : "missingReviewIcon"), px,look,x,y,false);',
            'const char *choice="missingReviewIcon"; ink_ui_circle(o,choice,x,y,d,px,s,0,0);',
            'const char *choice="book"; if(flag) choice="missingReviewIcon"; ink_ui_circle(o,choice,x,y,d,px,s,0,0);',
            'const char *a="missingReviewIcon"; const char *b=a; ink_ui_circle(o,b,x,y,d,px,s,0,0);',
            'typedef struct {const char *id,*title,*icon;} tool_t; static const tool_t MORE[]={{"clock","时钟","missingReviewIcon"}};',
            'typedef struct {const char *id,*title,*icon;} tool_t; out[n++]=(tool_t){"clock","时钟","missingReviewIcon"};',
        ):
            with self.subTest(source=source): self.assertTrue(self.check_source(source))

    def test_legal_names(self):
        for source in (
            'ink_ui_circle(o, flag ? "book" : "gear", x,y,d,px,s,0,0);',
            'ink_ui_icon(o,!strcmp(net,"mobile") ? "signal" : "wifi",px,look,x,y,false);',
            'const char *icon=!strcmp(title,"亮度") ? INK_UI_ICON(sun) : NULL;',
            'static const char *const APP_ICONS[]={INK_UI_ICON(book),INK_UI_ICON(gear)};',
            'static const char *const GROUP_ICONS[][2]={{"book","gear"}};',
            'const ink_app_def_t a={.icon="book",.title="任意标题"};',
            '// ink_ui_icon(o,"missingReviewIcon");\n/* INK_UI_ICON(missingReviewIcon) */',
        ):
            with self.subTest(source=source): self.assertFalse(self.check_source(source))

    def test_forwarded_arguments(self):
        helper = 'static void tile(const char *title, const char *icon, int x) {ink_ui_circle(o,icon,x,y,d,px,s,0,0);}'
        self.assertTrue(self.check_source(helper + 'tile("任意标题","missingReviewIcon",0);'))
        self.assertFalse(self.check_source(helper + 'tile("任意标题","book",0);'))
        sources = {Path('helper.c'): helper, Path('caller.c'): 'tile("x","missingReviewIcon",0);'}
        self.assertTrue(CHECKER['check_sources'](sources, NAMES))
        sources = {Path('helper.c'): helper, Path('caller.c'): 'const char *choice="missingReviewIcon"; tile("x",choice,0);'}
        self.assertTrue(CHECKER['check_sources'](sources, NAMES))
        for parameter in ('glyph', 'resource'):
            renamed = helper.replace('icon', parameter)
            sources = {Path('helper.c'): renamed,
                       Path('forward.c'): 'void forward(const char *name){tile("任意标题",name,0);}',
                       Path('caller.c'): 'forward("missingReviewIcon");'}
            with self.subTest(parameter=parameter):
                self.assertTrue(CHECKER['check_sources'](sources, NAMES))
                sources[Path('caller.c')] = 'forward("book");'
                self.assertFalse(CHECKER['check_sources'](sources, NAMES))

    def test_real_declared_origins(self):
        for file, needle in (
            ('system/ui/shell/navigation.c', 'APP_ICONS[] = {INK_UI_ICON(headphones)'),
            ('system/ui/shell/navigation.c', 'GROUP_ICONS[][4] = {{INK_UI_ICON(book)'),
            ('system/ui/shell/navigation.c', 'icons[] = {INK_UI_ICON(phone)'),
            ('system/ui/shell/navigation.c', '"时钟", INK_UI_ICON(clock)'),
            ('system/ui/theme.c', '? INK_UI_ICON(sun)'),
        ):
            path = ROOT / file
            source = path.read_text()
            self.assertEqual(source.count(needle), 1)
            import re
            mutant = source.replace(needle, re.sub(r'INK_UI_ICON\(\w+\)', 'INK_UI_ICON(missingReviewIcon)', needle), 1)
            errors = CHECKER['check_sources']({Path(file): mutant}, NAMES)
            with self.subTest(file=file, needle=needle):
                self.assertTrue(any('missingReviewIcon' in error for error in errors))

    def test_generated_names_are_closed(self):
        generated = CHECKER['header'](sorted(NAMES))
        self.assertIn('#define INK_UI_ICON(name) INK_UI_ICON_##name', generated)
        for name in NAMES: self.assertIn(f'#define INK_UI_ICON_{name} "{name}"', generated)
        self.assertNotIn('INK_UI_ICON_missingReviewIcon', generated)


if __name__ == '__main__': unittest.main()
