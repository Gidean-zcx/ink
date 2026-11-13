"""Break one thing in the reader's glue in a temporary object; source files and history stay untouched.
Default: EPUBs no longer open. --unsaved-toc: a position reached from the contents is not saved."""
import argparse
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
from run import ROOT, run

p=argparse.ArgumentParser(description=__doc__); p.add_argument('--build',type=Path,required=True); p.add_argument('--unsaved-toc',action='store_true'); a=p.parse_args(); build=a.build.resolve()
source=ROOT/'third_party/mogo/main/apps/ink_book_page.c'
header=ROOT/('apps/mogo/port/ink_page_port.h' if a.unsaved_toc else 'apps/mogo/port/ink_page_exports.h')
entry=next(x for x in json.loads((build/'compile_commands.json').read_text()) if Path(x['file'])==source)
with tempfile.TemporaryDirectory(prefix='reader-epub-control-',dir=build) as temp:
    temp=Path(temp); mutant=temp/source.name; obj=temp/(source.name+'.o')
    original=header.read_text()
    if a.unsaved_toc:
        # A position reached from a contents entry is never saved (at the jump, later turns or the close).
        changed=original.replace('    if (!s_text || !book_layout_page_count()) return;','    if (!s_text || !book_layout_page_count() || s_selected_toc != SIZE_MAX) return;')
    else:
        changed=original.replace('bool locked=ink_mogo_encrypted(path);','bool locked=ink_mogo_encrypted(path) || strstr(path,".epub");')
    assert changed!=original
    (temp/header.name).write_text(changed)
    mutant.write_text(source.read_text())
    args=shlex.split(entry['command']); compile_args=[]; skip=False
    for arg in args:
        if skip: skip=False; continue
        if arg in ('-o','-MT','-MF','-MQ'): skip=True
        elif arg!='-MD': compile_args.append(str(mutant) if arg==str(source) else arg)
    subprocess.run(compile_args+['-o',str(obj)],cwd=entry['directory'],check=True,capture_output=True)
    library=temp/'libink_mogo_native.a'; shutil.copyfile(build/library.name,library)
    subprocess.run(['ar','r',str(library),str(obj)],check=True,capture_output=True)
    binary=temp/'sim-reader-epub'
    link=shlex.split((build/'CMakeFiles/sim-reader-epub.dir/link.txt').read_text()); link[link.index('-o')+1]=str(binary)
    link=[str(library) if Path(x).name==library.name else x for x in link]
    subprocess.run(link,cwd=build,check=True,capture_output=True)
    run(build,binary,reverse=True,name='reader-epub-unsaved-toc' if a.unsaved_toc else None,
        expected='TOC navigation persists' if a.unsaved_toc else 'reader.open_book returns done')
