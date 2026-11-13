#!/usr/bin/env python3
"""tools/device/trial_run.py without a device: --image without --fonts stops before anything is read or written.

An app reads only the font pack of its own build (font-pack-1: images before it cannot read the compressed INKF v2
fonts), so installing an image over whatever font_data the device holds must not be possible by leaving --fonts out."""
import contextlib
import importlib.util
import io
import os
import tempfile
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
spec = importlib.util.spec_from_file_location("trial_run", os.path.join(ROOT, "tools", "device", "trial_run.py"))
trial_run = importlib.util.module_from_spec(spec)
spec.loader.exec_module(trial_run)


class FontsWithImage(unittest.TestCase):
    def run_main(self, args):
        err = io.StringIO()
        with contextlib.redirect_stderr(err), self.assertRaises(SystemExit) as stop:
            trial_run.main(args)
        return stop.exception.code, err.getvalue()

    def test_image_without_fonts_stops(self):
        with tempfile.TemporaryDirectory() as d:
            image, out = os.path.join(d, "ink-s31.bin"), os.path.join(d, "out")
            with open(image, "wb") as f:
                f.write(b"\xe9")
            for extra in ([], ["--install"]):
                code, err = self.run_main(["--port", "/dev/cu.fake", "--backup", os.path.join(d, "none.bin"),
                                           "--image", image, "--out", out, "--dry-run", *extra])
                self.assertEqual(code, 2)
                self.assertIn("--fonts is needed with --image", err)
                self.assertNotIn("beside", err)
                self.assertFalse(os.path.exists(out), "nothing may be created before the arguments are right")
            with open(os.path.join(d, "ink-fonts.bin"), "wb") as f:
                f.write(b"INKA")
            code, err = self.run_main(["--port", "/dev/cu.fake", "--backup", os.path.join(d, "none.bin"),
                                       "--image", image, "--out", out, "--install"])
            self.assertEqual(code, 2)
            self.assertIn("the pack beside the image is " + os.path.join(d, "ink-fonts.bin"), err)
            self.assertFalse(os.path.exists(out))

    def test_restore_only_needs_neither(self):
        with tempfile.TemporaryDirectory() as d:
            backup, out = os.path.join(d, "short.bin"), os.path.join(d, "out")
            with open(backup, "wb") as f:
                f.write(b"\xff" * 16)
            with contextlib.redirect_stdout(io.StringIO()) as said:
                code = trial_run.main(["--port", "/dev/cu.fake", "--backup", backup, "--out", out, "--restore-only",
                                       "--dry-run"])
            self.assertEqual(code, 2)  # past the arguments, stopped by the short backup before writing anything
            self.assertIn("STOP before writing anything: backup is 16 bytes", said.getvalue())


if __name__ == "__main__":
    unittest.main()
