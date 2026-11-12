"""Give the unchanged Mogo source test a stable fixture order."""
from pathlib import Path
import subprocess
import sys

books = Path(sys.argv[2]) / "books"
fixtures = sorted(books.glob("*.txt")) + sorted(books.glob("*.epub"))
if not fixtures:
    raise SystemExit("Mogo fixtures missing")
subprocess.run([sys.argv[1], *map(str, fixtures)], check=True)
