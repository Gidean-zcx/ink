"""Check every shared clock-field access is enclosed by the existing identity mutex."""
import argparse
from pathlib import Path
import re

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1] / 'system/services/hub/hub_link.c')
a = p.parse_args()
held = False
accesses = 0
for n, line in enumerate(a.source.read_text().splitlines(), 1):
    if 'osal_mutex_lock(H.id_lock,' in line:
        assert not held, f'line {n}: nested identity lock'
        held = True
    if re.search(r'H\.(server_ms|hello_sent_us|hello_rtt_ms)\b', line):
        assert held, f'line {n}: shared clock access without id_lock: {line.strip()}'
        accesses += 1
    if 'osal_mutex_unlock(H.id_lock)' in line:
        assert held, f'line {n}: identity unlock without lock'
        held = False
assert not held and accesses >= 6, f'incomplete clock coverage: {accesses}'
print(f'audit14 hub clock: {accesses} access lines under id_lock')
