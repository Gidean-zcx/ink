"""Lock policy must use declarations rather than capability-name exceptions."""
from pathlib import Path
import re


def check(source):
    gate = source.split('static ink_rc_t gate(', 1)[1].split('\n}\n', 1)[0]
    assert not re.search(r'\b(?:strcmp|strncmp|memcmp)\s*\(\s*c->id\b', gate), 'lock gate contains a name-prefix exception'
    opener = source.split('static bool opens_emergency(', 1)[1].split('}', 1)[0]
    assert 'c->id' not in opener, 'public entry is selected by name rather than a capability marker'


if __name__ == '__main__':
    check((Path(__file__).resolve().parents[2] / 'system/policy/cap.c').read_text())
    print('PASS: lock gate and public entry contain no capability-name exceptions')
