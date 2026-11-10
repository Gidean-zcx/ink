#!/usr/bin/env python3
"""Check fixed dependency, offline diagnostics, and corruption without networking."""
from pathlib import Path
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
helper = Path(__file__).resolve().parents[1] / "opus.cmake"
with tempfile.TemporaryDirectory(prefix="opus-fetch-", dir=build) as temp:
    work = Path(temp)
    script = work / "check.cmake"
    script.write_text(f'''set(CMAKE_BINARY_DIR "{work.as_posix()}")
function(file)
  if(ARGV0 STREQUAL "DOWNLOAD")
    set(ink_opus_status "7;synthetic offline" PARENT_SCOPE)
  elseif(ARGV0 STREQUAL "SHA256")
    _file(${{ARGV}})
    set(${{ARGV2}} "${{${{ARGV2}}}}" PARENT_SCOPE)
  else()
    _file(${{ARGV}})
  endif()
endfunction()
include("{helper.as_posix()}")
''')
    offline = subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True)
    assert offline.returncode and "无法下载 Opus 1.0.5" in offline.stderr
    assert "f04982787072427616f2ef86e228844d4912f0a6952a2ee597bb4dc6e5ced829" in offline.stderr
    print("synthetic offline: clear fatal diagnostic, exit", offline.returncode)
    (work / "esp-opus-1.0.5.zip").write_bytes(b"synthetic corrupt archive")
    corrupt = subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True)
    assert corrupt.returncode and "SHA256 不符" in corrupt.stderr
    print("corrupt archive: rejected, exit", corrupt.returncode)
    (work / "esp-opus-1.0.5.zip").write_bytes((build / "esp-opus-1.0.5.zip").read_bytes())
    valid = subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True)
    assert not valid.returncode, valid.stderr
    assert (work / "esp-opus/include/opus.h").is_file()
    print("verified offline archive: hash and extraction passed, exit 0")
