"""Prepare a real source copy with the proposed translations, without editing the original table."""
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
source = ROOT / "work/ui-fix-a-validation/source"
source.mkdir(parents=True, exist_ok=True)
for name in subprocess.check_output(["git", "ls-tree", "--name-only", "HEAD"], cwd=ROOT, text=True).splitlines():
    target, original = source / name, ROOT / name
    if name == "docs":
        if not target.exists():
            target.symlink_to(original, target_is_directory=True)
        continue
    if target.is_symlink():
        target.unlink()
    if original.is_dir():
        if target.exists():
            for link in target.rglob("*"):
                if link.is_symlink():
                    link.unlink()
        shutil.copytree(original, target, dirs_exist_ok=True, ignore=shutil.ignore_patterns("__pycache__", ".DS_Store"))
    else:
        shutil.copy2(original, target)
table = source / "assets/i18n/en.tsv"
if table.is_symlink():
    table.unlink()
table.write_text((ROOT / "assets/i18n/en.tsv").read_text().rstrip("\n") + "\n" +
    (ROOT / "docs/tasks/ui-fix-a/en-additions.tsv").read_text())
print("Candidate source: work/ui-fix-a-validation/source")
