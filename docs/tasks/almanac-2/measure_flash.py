"""Measure the linked S31 calendar tables, including pooled string bytes, and the calendar code (pyelftools).

Round two: finds the table group by calendar_term_days (present in both formats), knows the word
tables, and also counts the functions of calendar.c and almanac.c (code bytes)."""
import argparse
import json
from pathlib import Path
import struct
from elftools.elf.elffile import ELFFile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--elf', type=Path, default=Path('build-s31/ink-s31.elf'))
p.add_argument('--before', type=Path)
p.add_argument('--after', type=Path, default=Path('build-s31/ink-s31.bin'))
a = p.parse_args()


def union_length(intervals):
    merged = []
    for lo, hi in sorted(intervals):
        if merged and lo <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(hi, merged[-1][1]))
        else:
            merged.append((lo, hi))
    return sum(hi - lo for lo, hi in merged)


with a.elf.open('rb') as f:
    elf = ELFFile(f)
    assert elf.elfclass == 32 and elf.little_endian
    sections = list(elf.iter_sections())
    groups, current, funcs, code, source_file = [], [], [], [], ''
    for symbol in elf.get_section_by_name('.symtab').iter_symbols():
        if symbol['st_info']['type'] == 'STT_FILE':
            if current or funcs:
                groups.append((current, funcs, source_file))
            current, funcs, source_file = [], [], symbol.name
        elif symbol['st_info']['type'] == 'STT_OBJECT' and isinstance(symbol['st_shndx'], int):
            current.append(symbol)
        elif symbol['st_info']['type'] == 'STT_FUNC' and symbol['st_info']['bind'] == 'STB_LOCAL':
            funcs.append((symbol.name, symbol['st_size']))
        elif symbol['st_info']['type'] == 'STT_FUNC' and symbol.name.startswith(('calendar_', 'ink_almanac_')):
            code.append((symbol.name, symbol['st_size']))  # globals come after every file's locals
    groups.append((current, funcs, source_file))
    group, local_funcs, _ = next(g for g in groups if any(s.name == 'calendar_term_days' for s in g[0]))
    code += local_funcs + [f for g in groups if g[2] == 'almanac.c' for f in g[1]]
    pointers = {'calendar_terms', 'calendar_ganzhi', 'calendar_yiji', 'calendar_folk',
                'calendar_yiji_words', 'calendar_folk_words',
                'calendar_joy', 'calendar_fortune', 'wealth.3', 'animals.2', 'sha.1',
                'month_names', 'day_names'}
    ranges, strings, objects = [], [], []
    for symbol in group:
        address, size = symbol['st_value'], symbol['st_size']
        section = sections[symbol['st_shndx']]
        assert section.name == '.flash.rodata', (symbol.name, section.name)
        ranges.append((address, address + size))
        objects.append((symbol.name, size))
        if symbol.name in pointers:
            start = address - section['sh_addr']
            for (ptr,) in struct.iter_unpack('<I', section.data()[start:start + size]):
                source = next(s for s in sections if s.name == '.flash.rodata' and
                              s['sh_addr'] <= ptr < s['sh_addr'] + s['sh_size'])
                payload = source.data()
                start = ptr - source['sh_addr']
                end = ptr + payload.index(0, start) - start + 1
                strings.append((ptr, end))
                ranges.append((ptr, end))
    result = {'flash_table_objects': sum(size for _, size in objects),
              'flash_strings_union': union_length(strings),
              'flash_all_table_bytes_union': union_length(ranges), 'objects': objects,
              'code_bytes': sum(size for _, size in code), 'functions': sorted(code)}
    if a.before:
        result.update(before_bin=a.before.stat().st_size, after_bin=a.after.stat().st_size,
                      firmware_delta=a.after.stat().st_size - a.before.stat().st_size)
    print(json.dumps(result, indent=2))
