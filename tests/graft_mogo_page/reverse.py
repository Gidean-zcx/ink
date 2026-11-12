"""Disconnect the real graft input and run the same native page assertions; no Git history needed."""
import argparse
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
from run import ROOT, exercise


def reverse(build, carrier_close=False):
    source = ROOT / ('system/graft/graft.c' if carrier_close else 'apps/mogo/mogo.c')
    entry = next(x for x in json.loads((build/'compile_commands.json').read_text()) if Path(x['file']) == source)
    with tempfile.TemporaryDirectory(prefix='mogo-input-control-',dir=build) as directory:
        area = Path(directory)
        mutant = area / source.name
        original = source.read_text()
        if carrier_close:
            changed = original.replace('lv_obj_t *image;', 'lv_obj_t *image, *status;').replace(
                '    ink_shell_statusbar(screen, true);', '    G.status = ink_shell_statusbar(screen, true);').replace(
                '    osal_free(G.pixels);', '    if (G.status) lv_obj_delete(G.status);\n    osal_free(G.pixels);')
        else:
            changed = original.replace('if (!dir || M.busy) return false;', 'if (dir || M.busy) return false;')
        assert changed != original
        mutant.write_text(changed)
        obj = area / (source.name + '.o')
        args = shlex.split(entry['command'])
        command, skip = [], False
        for a in args:
            if skip: skip = False
            elif a in ('-o','-MT','-MF','-MQ'): skip = True
            elif a != '-MD': command.append(str(mutant) if a == str(source) else a)
        subprocess.run(command+['-o',str(obj)],cwd=entry['directory'],check=True,capture_output=True)
        library = area / ('libink_system.a' if carrier_close else 'libink_apps.a')
        shutil.copyfile(build/library.name,library)
        subprocess.run(['ar','r',str(library),str(obj)],check=True,capture_output=True)
        binary = area / 'sim-mogo-page'
        link = shlex.split((build/'CMakeFiles/sim-mogo-page.dir/link.txt').read_text())
        link[link.index('-o')+1] = str(binary)
        link = [str(library) if Path(x).name == library.name else x for x in link]
        subprocess.run(link,cwd=build,check=True,capture_output=True)
        exercise(build,binary,reverse='status' if carrier_close else True)

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--carrier-close',action='store_true')
    a=p.parse_args();reverse(a.build.resolve(),a.carrier_close)
