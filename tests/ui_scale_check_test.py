"""Size checker regressions use in-memory C fragments, never product edits."""
from pathlib import Path
import runpy
import unittest
from unittest.mock import patch

CHECK = runpy.run_path(str(Path(__file__).resolve().parents[1] / 'tools/ui_scale_check.py'))['check']


class ScaleCheck(unittest.TestCase):
    def check_source(self, text):
        with patch.object(Path, 'read_text', return_value=text):
            return CHECK(Path('probe.c'))

    def test_illegal_sizes(self):
        for text in (
            'lv_obj_set_width(o, 123);',
            'lv_obj_set_style_width(o, 123, 0);',
            'lv_style_set_pad_all(&style, 123);',
            'lv_style_set_radius(&style, 8);',
            'const int width=123; lv_obj_set_width(o,width);',
            'const int width=\n123; lv_obj_set_width(o,width);',
            'const int base=120; const int width=base+3; lv_obj_set_width(o,width);',
            '#define BAD 123\nlv_obj_set_height(o, BAD);',
            'enum { BAD = 123 }; lv_obj_set_height(o, BAD);',
            'lv_obj_set_width(o, 123 * 1);',
            'lv_obj_set_width(o, (124 - 1));',
            'ink_ui_font_px(123, INK_WEIGHT_REGULAR);',
            'const int font=84; ink_ui_font_px(font, INK_WEIGHT_REGULAR);',
            'ink_ui_font_px((yes ? 32 : 123), INK_WEIGHT_REGULAR);',
            'ink_ui_icon(o, "ai", 44, INK_ICON_LINE, 0, 0, false);',
            'lv_obj_set_style_radius(o, INK_GAP_S, 0);',
            'lv_obj_set_style_border_width(o, INK_MARGIN, 0);',
            'ink_ui_label(o, "x", 84, INK_WEIGHT_REGULAR, 0, 0, 125);',
            'const int size=123; lv_obj_set_size(o, ink_ui_px(size), 125);',
            'lv_obj_set_width(o, INK_MARGIN + 123);',
            'int w=125; w=123; lv_obj_set_width(o,w);',
            'int w; w=123; lv_obj_set_width(o,w);',
            'int w=125; if (flag) {w=123;} lv_obj_set_width(o,w);',
            'int w=125; if (flag) {w=123;} else {w=125;} lv_obj_set_width(o,w);',
            'int w=125; w+=17; lv_obj_set_width(o,w);',
            'int w=123; int h=w; w=125; lv_obj_set_width(o,h);',
            'int base=125, w=123; lv_obj_set_width(o,w);',
            'static const int sizes[]={32,123}; ink_ui_font_px(sizes[index],INK_WEIGHT_REGULAR);',
            'static const int sizes[]={32,84}; ink_ui_font_px(sizes[0],INK_WEIGHT_REGULAR);',
            'static const int sizes[]={32,123}; int px=sizes[index]; ink_ui_font_px(px,INK_WEIGHT_REGULAR);',
            'int px; ink_ui_font_px(px,INK_WEIGHT_REGULAR);',
            'ink_ui_font_px(unknown(),INK_WEIGHT_REGULAR);',
            'lv_obj_set_width(o, unknown());',
            'lv_obj_set_width(o, ink_ui_px(INK_MARGIN) + 123);',
            'lv_obj_set_width(o, ink_dp(125) + 123);',
            'ink_ui_font_px(LV_PCT(32), INK_WEIGHT_REGULAR);',
            'ink_ui_font_px(LV_SIZE_CONTENT, INK_WEIGHT_REGULAR);',
            'ink_ui_icon(o,"ai",LV_RADIUS_CIRCLE,INK_ICON_LINE,0,0,false);',
            'void f(int w) {lv_obj_set_width(o,w);}',
            '/* ui-scale-runtime: dimension w Forwarded theme component argument. */\nint w=123; lv_obj_set_width(o,w);',
        ):
            with self.subTest(text=text): self.assertTrue(self.check_source(text), text)

    def test_legal_sizes_and_non_size_parameters(self):
        for text in (
            'lv_obj_set_width(o, 125);',
            'const int width=125; lv_obj_set_width(o, width);',
            'lv_obj_set_width(o, INK_CANVAS_W - 2 * INK_MARGIN);',
            'const int w=(INK_CANVAS_W - 2 * INK_MARGIN)/3; lv_obj_set_width(o,w);',
            'lv_obj_set_size(o, LV_PCT(100), LV_SIZE_CONTENT);',
            'ink_ui_font_px(38, INK_WEIGHT_REGULAR);',
            'lv_obj_set_height(o, ink_ui_line_height(INK_FONT_LIST) + INK_GAP);',
            'ink_ui_font_px(yes ? INK_FONT_BODY : INK_FONT_LIST, INK_WEIGHT_REGULAR);',
            'lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);',
            'ink_ui_slider(o, "slider", 7, 38, 125, 192, 0, 0);',
            'ink_ui_segments(o, labels, 7, 3, 38, 125, 192, 0, 0);',
            'ink_ui_pagebar(o, 7, 13, 125, 0);',
            'const int width=123; void f(void) { const int width=125; lv_obj_set_width(o,width); }',
            '/* ui-scale-runtime: dimension width Forwarded component geometry checked at callers. */\nvoid f(void) {const int width=123;} void g(int width) {lv_obj_set_width(o,width);}',
            'int w; w=125; lv_obj_set_width(o,w);',
            'int w=123; w=125; lv_obj_set_width(o,w);',
            'int w=125; int h=w; w=123; lv_obj_set_width(o,h);',
            'int w=125; {int w=123;} lv_obj_set_width(o,w);',
            'static const int sizes[]={INK_FONT_BODY,INK_FONT_LIST}; ink_ui_font_px(sizes[index],INK_WEIGHT_REGULAR);',
            'lv_obj_set_width(o, lv_obj_get_width(parent));',
            '/* ui-scale-exception: 123 legacy reading line, unchanged in this task */\nlv_obj_set_width(o,123);',
            '// lv_obj_set_width(o, 123);\nconst char *s="ink_ui_font_px(123, weight)";',
        ):
            with self.subTest(text=text): self.assertFalse(self.check_source(text), text)

    def test_conditional_writes_keep_reachable_values(self):
        for body in (
            'if(flag) w=125;',
            'if(flag) {w=125;}',
            'if(flag) w=123; else w=125;',
            'if(flag) {w=123;} else w=125;',
            'if(flag) w=123; else {w=125;}',
            'if(flag) w=125; else if(other) w=125;',
            'if(flag) if(other) w=123; else w=125;',
            'if(flag) {if(other) w=123; else w=125;}',
            'if(flag) for(int i=0;i<2;i++) w=125;',
            'for(int i=0;i<count;i++) w=125;',
            'while(flag) w=125;',
            'switch(flag) {case 0: w=125; break;}',
            'if(flag) do w=125; while(other);',
            'if(flag) w+=17; else w=125;',
            'if(flag) w-=17;',
            'if(flag) w=125; int alias=w; w=125; w=alias;',
        ):
            for initial in ('125', '123'):
                # Every body has an illegal or uninitialized reachable path.
                if initial == '125' and 'w=123' not in body and 'w+=17' not in body: continue
                source = f'int w={initial}; {body} lv_obj_set_width(o,w);'
                with self.subTest(source=source): self.assertTrue(self.check_source(source))
        for source in (
            'int px=123; if(flag) px=32; ink_ui_font_px(px,INK_WEIGHT_REGULAR);',
            'int w; if(flag) w=125; lv_obj_set_width(o,w);',
            'int w=123; if(flag) w=125; int h=w; lv_obj_set_height(o,h);',
            'int w=125; if(flag) w=123; else w=125; lv_obj_set_width(o,w);',
            'int w=123;\n#if FLAG\nw=125;\n#endif\nlv_obj_set_width(o,w);',
            'int w=125;\n#if FLAG\nw=123;\n#else\nw=125;\n#endif\nlv_obj_set_width(o,w);',
            'int w=125; flag ? w=123 : w=125; lv_obj_set_width(o,w);',
            '#if FLAG\n#define BAD 123\n#else\n#define BAD 125\n#endif\nlv_obj_set_width(o,BAD);',
            '#define BAD 123\n#ifdef FLAG\n#define BAD 125\n#endif\nlv_obj_set_width(o,BAD);',
        ):
            with self.subTest(source=source): self.assertTrue(self.check_source(source))

    def test_legal_conditional_writes(self):
        for body in (
            'if(flag) w=192;',
            'if(flag) {w=192;} else w=125;',
            'if(flag) w=192; else {w=125;}',
            'if(flag) if(other) w=192; else w=125;',
            'while(flag) w=192;',
            'for(int i=0;i<count;i++) w=192;',
            'do w=192; while(flag);',
        ):
            source = f'int w=125; {body} lv_obj_set_width(o,w);'
            with self.subTest(source=source): self.assertFalse(self.check_source(source))

        for source in (
            'int w=123; if(flag) w=192; w=125; lv_obj_set_width(o,w);',
            'int w=123; if(flag) {w=125; lv_obj_set_width(o,w);}',
            '#if FLAG\n#define WIDTH 125\n#else\n#define WIDTH 192\n#endif\nlv_obj_set_width(o,WIDTH);',
        ):
            with self.subTest(source=source): self.assertFalse(self.check_source(source))

    def test_pages_do_not_size_textfields(self):
        import re
        root = Path(__file__).resolve().parents[1]
        pages = {
            'system/ui/search.c': 'S.field', 'system/ui/app_packages.c': 'obj',
            'system/ui/shell/ai_chat.c': 'field', 'system/ui/shell/boot_flow.c': 'B.field',
            'system/ui/shell/mobile_network.c': 'f', 'system/ui/shell/settings_family.c': 'S.field',
            'system/ui/shell/navigation.c': 'search|password_field',
            'system/apps/calendar/calendar.c': 'E.name', 'system/apps/clock/clock.c': 'C.name',
            'system/apps/files/files.c': 'F.field', 'system/apps/weather/weather.c': 'W.field',
            'system/apps/remote/remote.c': 'RS.field', 'system/apps/memo/memo.c': 'M.field',
        }
        for page, names in pages.items():
            source = (root / page).read_text()
            for name in names.split('|'):
                self.assertIsNone(re.search(r'lv_obj_set_(?:size|width|height)\s*\(\s*' + re.escape(name) + r'\s*,', source), page)
            self.assertIn('ink_textfield_style(', source, page)


if __name__ == '__main__': unittest.main()
