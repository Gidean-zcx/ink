"""Remove only the native progress write, then rerun the same reopening assertions."""
import argparse
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
from run import ROOT, exercise

def reverse(build):
    source=ROOT/'third_party/mogo/main/apps/ink_book_page.c'
    entry=next(x for x in json.loads((build/'compile_commands.json').read_text()) if Path(x['file'])==source)
    with tempfile.TemporaryDirectory(prefix='mogo-save-control-',dir=build) as directory:
        area=Path(directory)
        header=(ROOT/'apps/mogo/port/ink_page_port.h').read_text()
        old='s_save_failed = book_progress_save(s_reader_session.book.path,&p) != ESP_OK;'
        assert old in header
        (area/'ink_page_port.h').write_text(header.replace(old,'(void)p; s_save_failed = false;'))
        mutant=area/source.name
        mutant.write_text(source.read_text())
        obj=area/(source.name+'.o')
        command=[]; skip=False
        for a in shlex.split(entry['command']):
            if skip: skip=False
            elif a in ('-o','-MT','-MF','-MQ'): skip=True
            elif a!='-MD': command.append(str(mutant) if a==str(source) else a)
        subprocess.run(command+['-o',str(obj)],cwd=entry['directory'],check=True,capture_output=True)
        library=area/'libink_mogo_native.a'
        shutil.copyfile(build/library.name,library)
        subprocess.run(['ar','r',str(library),str(obj)],check=True,capture_output=True)
        binary=area/'sim-mogo-flow'
        link=shlex.split((build/'CMakeFiles/sim-mogo-flow.dir/link.txt').read_text())
        link[link.index('-o')+1]=str(binary)
        link=[str(library) if Path(x).name==library.name else x for x in link]
        subprocess.run(link,cwd=build,check=True,capture_output=True)
        exercise(build,binary,reverse=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(); p.add_argument('--build',type=Path,required=True)
    a=p.parse_args(); reverse(a.build.resolve())
