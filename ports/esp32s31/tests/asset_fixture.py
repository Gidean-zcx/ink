import platform
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'tools'))
from asset_pack import pack

out = Path(sys.argv[1])
first = pack([('fonts/a.inkf', b'font')])
second = pack([('icons/z.inki', b'icon')])
(out / 'first.bin').write_bytes(first)
section = '.section __TEXT,__const' if platform.system() == 'Darwin' else '.section .rodata'
(out / 'embedded.S').write_text(section + '\n.globl _binary_ink_embedded_bin_start\n'
    '_binary_ink_embedded_bin_start:\n.byte ' + ','.join(map(str, second)) +
    '\n.globl _binary_ink_embedded_bin_end\n_binary_ink_embedded_bin_end:\n' +
    ('' if platform.system() == 'Darwin' else '.section .note.GNU-stack,"",@progbits\n'))
