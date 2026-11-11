"""Remove just the EPUB hook in a temporary object; source files and history stay untouched."""
import argparse
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
from run import ROOT, run

p=argparse.ArgumentParser(description=__doc__); p.add_argument('--build',type=Path,required=True); p.add_argument('--segments',action='store_true'); p.add_argument('--unsaved-toc',action='store_true'); p.add_argument('--anchor',action='store_true'); a=p.parse_args(); build=a.build.resolve()
source=ROOT/('apps/reader/epub_source.cc' if a.anchor else 'apps/reader/reader.c')
entry=next(x for x in json.loads((build/'compile_commands.json').read_text()) if Path(x['file'])==source)
with tempfile.TemporaryDirectory(prefix='reader-epub-control-',dir=build) as temp:
    temp=Path(temp); mutant=temp/source.name; obj=temp/(source.name+'.o')
    original=source.read_text()
    if a.anchor: changed='\n'.join(line for line in original.split('\n') if 'before_finish' not in line)
    elif a.segments: changed=original.replace('2u * 1024u * 1024u','2048u')
    elif a.unsaved_toc: changed=original.replace('RD.dirty_pos=true; RD.dirty_since_us=ink_now_us();','/* TOC position persistence removed. */')
    else: changed=original.replace('return load_section(0,0);','return false; /* EPUB hook removed by the reverse control. */')
    assert changed!=original
    mutant.write_text(changed)
    args=shlex.split(entry['command']); compile_args=[]; skip=False
    for arg in args:
        if skip: skip=False; continue
        if arg in ('-o','-MT','-MF','-MQ'): skip=True
        elif arg!='-MD': compile_args.append(str(mutant) if arg==str(source) else arg)
    subprocess.run(compile_args+['-o',str(obj)],cwd=entry['directory'],check=True,capture_output=True)
    library=temp/'libink_apps.a'; shutil.copyfile(build/library.name,library)
    subprocess.run(['ar','r',str(library),str(obj)],check=True,capture_output=True)
    binary=temp/'sim-reader-epub'
    link=shlex.split((build/'CMakeFiles/sim-reader-epub.dir/link.txt').read_text()); link[link.index('-o')+1]=str(binary)
    link=[str(library) if Path(x).name==library.name else x for x in link]
    subprocess.run(link,cwd=build,check=True,capture_output=True)
    run(build,binary,reverse=not a.segments,name='reader-epub-anchor-reverse' if a.anchor else 'reader-epub-segments' if a.segments else 'reader-epub-unsaved-toc' if a.unsaved_toc else None,
        expected='fragment body' if a.anchor else 'TOC navigation persists' if a.unsaved_toc else 'reader.open_book returns done')
