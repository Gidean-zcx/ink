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
            'ink_ui_icon(o, "ai", 32, INK_ICON_LINE, 0, 0, false);',
            'lv_obj_set_style_radius(o, INK_GAP_S, 0);',
            'lv_obj_set_style_border_width(o, INK_MARGIN, 0);',
            'ink_ui_label(o, "x", 84, INK_WEIGHT_REGULAR, 0, 0, 104);',
            'const int size=123; lv_obj_set_size(o, ink_ui_px(size), 104);',
            'lv_obj_set_width(o, INK_MARGIN + 123);',
        ):
            with self.subTest(text=text): self.assertTrue(self.check_source(text), text)

    def test_legal_sizes_and_non_size_parameters(self):
        for text in (
            'lv_obj_set_width(o, 104);',
            'const int width=104; lv_obj_set_width(o, width);',
            'lv_obj_set_width(o, INK_CANVAS_W - 2 * INK_MARGIN);',
            'const int w=(INK_CANVAS_W - 2 * INK_MARGIN)/3; lv_obj_set_width(o,w);',
            'lv_obj_set_size(o, LV_PCT(100), LV_SIZE_CONTENT);',
            'ink_ui_font_px(32, INK_WEIGHT_REGULAR);',
            'ink_ui_font_px(yes ? INK_FONT_BODY : INK_FONT_LIST, INK_WEIGHT_REGULAR);',
            'lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);',
            'ink_ui_slider(o, "slider", 7, 32, 104, 160, 0, 0);',
            'ink_ui_segments(o, labels, 7, 3, 32, 104, 160, 0, 0);',
            'ink_ui_pagebar(o, 7, 13, 104, 0);',
            'const int width=123; void f(void) { const int width=104; lv_obj_set_width(o,width); }',
            'void f(void) {const int width=123;} void g(int width) {lv_obj_set_width(o,width);}',
            '/* ui-scale-exception: 123 legacy reading line, unchanged in this task */\nlv_obj_set_width(o,123);',
            '// lv_obj_set_width(o, 123);\nconst char *s="ink_ui_font_px(123, weight)";',
        ):
            with self.subTest(text=text): self.assertFalse(self.check_source(text), text)


if __name__ == '__main__': unittest.main()
