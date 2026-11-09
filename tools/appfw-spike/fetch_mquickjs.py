"""Fetch a pinned, unmodified MIT source archive into the ignored work tree."""
import hashlib
import subprocess
import tarfile
from pathlib import Path

COMMIT = "4119f6819ccb5bae2d21a18a106e89522d18ad45"
SHA256 = "911ebe922ec84acf530a4db64ea3963fbcc4f9ae84e176ecfa819273fb5fa638"
WORK = Path("work/appfw-spike")
ARCHIVE = WORK / "mquickjs-4119f681.tar.gz"
SOURCE = WORK / f"mquickjs-{COMMIT}"


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    if not ARCHIVE.exists():
        subprocess.run(["curl", "-fL", "--connect-timeout", "15", "--max-time", "60",
                        f"https://codeload.github.com/bellard/mquickjs/tar.gz/{COMMIT}",
                        "-o", str(ARCHIVE)], check=True)
    assert hashlib.sha256(ARCHIVE.read_bytes()).hexdigest() == SHA256
    with tarfile.open(ARCHIVE) as archive:
        files = [m for m in archive.getmembers() if m.isfile()]
        assert all(not Path(m.name).is_absolute() and ".." not in Path(m.name).parts for m in files)
        if not SOURCE.exists():
            archive.extractall(WORK, filter="data")
        expected = {m.name for m in files}
        actual = {str(p.relative_to(WORK)) for p in SOURCE.rglob("*") if p.is_file()}
        assert actual == expected, "upstream file inventory differs"
        for member in files:
            assert (WORK / member.name).read_bytes() == archive.extractfile(member).read(), member.name
    # Normalize the upstream license's one whitespace-only line in our copy.
    license_text = (SOURCE / "LICENSE").read_bytes().replace(b"\n \n", b"\n\n")
    assert license_text == Path("tools/appfw-spike/licenses/MicroQuickJS.txt").read_bytes()
    print(f"MicroQuickJS {COMMIT}: archive and extracted files verified")


if __name__ == "__main__":
    main()
