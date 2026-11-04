#!/usr/bin/env python3
"""Compile a parser-only probe in a temporary directory; never modify source."""
from pathlib import Path
import json
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]

def count(v):
    if isinstance(v, dict):
        return 1 + sum(1 + count(x) for x in v.values())
    if isinstance(v, list):
        return 1 + sum(map(count, v))
    return 1

def caps(n, expanded=False, rev=False):
    item = {'name': 'voice.chat', 'title': 'voice', 'form': 'session'}
    if expanded:
        item.update(offline='fail', timeout_ms=8000)
    result = {'tools': [item] * n}
    if rev:
        result['rev'] = 18
    return {'jsonrpc': '2.0', 'id': 1, 'result': result}

fixtures = {
    'caps_17_three_fields': caps(17),
    'caps_17_plus_rev': caps(17, rev=True),
    'caps_15_five_fields_rev': caps(15, expanded=True, rev=True),
    'caps_10_five_fields_rev': caps(10, expanded=True, rev=True),
    'members_8': {'jsonrpc': '2.0', 'id': 1, 'result': {
        'members': [{'id': 'm1', 'name': 'n', 'role': 'adult', 'mark': 'x'}] * 8,
        'nextCursor': 'c', 'rev': 1}},
    'asr_text': {'jsonrpc': '2.0', 'method': 'rt.asr', 'params': {
        'sid': 1, 'seg': 1, 'text': 'a' * 480, 'final': False}},
    'asr_open': {'jsonrpc': '2.0', 'id': 1, 'method': 'rt.open', 'params': {
        'sid': 9, 'cap': 'asr.stream', 'params': {'format': 'opus', 'rate': 16000,
        'frame_ms': 60, 'kind': 'text', 'interim_ms': 800}}},
}
with tempfile.TemporaryDirectory(prefix='hub-audit-parser-') as tmp:
    binary = Path(tmp) / 'probe'
    cmd = ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
           '-I' + str(ROOT / 'system/include'), '-I' + str(ROOT / 'platform/include'),
           '-I' + str(ROOT / 'third_party/jsmn'), str(HERE / 'protocol_probe.c'),
           str(ROOT / 'system/util/ink_json.c'), '-o', str(binary)]
    subprocess.run(cmd, check=True, capture_output=True)
    print('compile_exit=0; unchanged system/util/ink_json.c')
    for name, payload in fixtures.items():
        result = subprocess.run([str(binary)], input=json.dumps(payload),
                                capture_output=True, text=True, check=True)
        rc, ntok = map(int, result.stdout.split())
        tokens = count(payload)
        assert (rc == 0) == (tokens <= 128), (name, tokens, rc)
        if rc == 0:
            assert ntok == tokens
        print(f'{name}: tokens={tokens}, parser_rc={rc}, parsed_tokens={ntok}')
