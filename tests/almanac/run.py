"""Check the actual C adapter against fixed or pinned JavaScript oracle data."""
import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
VENDOR = ROOT / 'system/services/time/vendor/mogo-calendar'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary', type=Path, required=True)
p.add_argument('--mode', choices=['fixtures', 'oracle', 'compare'], default='fixtures')
p.add_argument('--fixtures', type=Path, default=HERE / 'fixtures.json')
p.add_argument('--cnlunar', type=Path)
p.add_argument('--out', type=Path)
a = p.parse_args()


def actual(expected):
    dates = ''.join(f'{r["year"]} {r["month"]} {r["day"]}\n' for r in expected)
    result = subprocess.run([str(a.binary.resolve())], input=dates, capture_output=True,
                            text=True, check=True, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    found = [json.loads(line) for line in result.stdout.splitlines()]
    assert len(found) == len(expected), (len(found), len(expected))
    differences = []
    for want, got in zip(expected, found):
        legacy = got.pop('legacy')
        assert got == want, (want['year'], want['month'], want['day'],
                             {k: [v, got.get(k)] for k, v in want.items() if got.get(k) != v})
        modern = [got[k] for k in ('lunar_year', 'lunar_month', 'lunar_day')]
        if legacy != modern:
            differences.append([f'{got["year"]:04}-{got["month"]:02}-{got["day"]:02}', legacy, modern])
    print(f'PASS: {len(found)} dates, all C interface fields match the lunar-javascript oracle')
    return differences


if a.mode == 'oracle':
    node = shutil.which('node')
    if not node:
        print('SKIP: node is unavailable; fixed 50-day fixtures still run without node')
        raise SystemExit(77)
    with tempfile.TemporaryDirectory(prefix='almanac-oracle-', dir=a.binary.resolve().parent) as folder:
        area = Path(folder)
        oracle, expanded = area / 'oracle.json', area / 'expanded.json'
        subprocess.run([node, str(VENDOR / 'tools/generate_calendar_data.cjs'),
                        '--check', '--oracle', str(oracle)], check=True)
        subprocess.run([node, str(HERE / 'generate_fixtures.cjs'), str(oracle),
                        str(expanded), '--all'], check=True)
        expected = json.loads(expanded.read_text())
        differences = actual(expected)
        print(f'Legacy lunar: {len(expected)-len(differences)} agree, {len(differences)} differ')
        if a.out:
            a.out.mkdir(parents=True, exist_ok=True)
            lines = ['# 现有农历与 Mogo 逐日对照', '',
                     f'【实测】2000–2099 共 {len(expected)} 天；一致 {len(expected)-len(differences)} 天，不一致 {len(differences)} 天。',
                     '负月份表示闰月。未修改现有 `lunar.c`。', '',
                     '| 公历 | 现有农历（年/月/日） | Mogo（年/月/日） |', '| --- | --- | --- |']
            lines += [f'| {date} | {old} | {new} |' for date, old, new in differences]
            (a.out / 'LUNAR-DIFF.md').write_text('\n'.join(lines) + '\n')
else:
    expected = json.loads(a.fixtures.read_text())
    assert len(expected) == 50
    actual(expected)
    if a.mode == 'compare':
        assert a.cnlunar and a.out, '--compare requires --cnlunar and --out'
        sys.path.insert(0, str(a.cnlunar.resolve()))
        from cnlunar import Lunar
        lines = ['# cnlunar 与 lunar-javascript 固定 50 天对照', '',
                 '【实测】cnlunar `3f674fe`：`Lunar(datetime(...), godType="8char", year8Char="year", yeargod="duty")`；当地日期 00:00。',
                 '宜忌、吉神凶煞按集合比较（忽略展示顺序），只列真实词项差异；未改动 Mogo 数据。', '',
                 '| 公历 | 字段 | 仅 lunar-javascript | 仅 cnlunar |', '| --- | --- | --- | --- |']
        affected = set()
        for r in expected:
            date = datetime(r['year'], r['month'], r['day'])
            c = Lunar(date, godType='8char', year8Char='year', yeargod='duty')
            directions = c.get_luckyGodsDirection()
            values = {'lunar_year': c.lunarYear, 'lunar_month': -c.lunarMonth if c.isLunarLeapMonth else c.lunarMonth,
                      'lunar_day': c.lunarDay, 'year_ganzhi': c.year8Char, 'month_ganzhi': c.month8Char,
                      'day_ganzhi': c.day8Char, 'yi': c.goodThing, 'ji': c.badThing,
                      'joy': directions[0][2:], 'wealth': directions[1][2:], 'fortune': directions[2][2:],
                      'auspicious': c.goodGodName, 'inauspicious': c.badGodName,
                      'officer': c.today12DayOfficer, 'guardian': c.today12DayGod,
                      'guardian_type': c.get_today12DayOfficer()[2][:-1], 'clash': c.zodiacLose,
                      'solar_term': '' if c.todaySolarTerms == '无' else c.todaySolarTerms}
            for key, theirs in values.items():
                ours = r[key]
                if isinstance(theirs, list):
                    left, right = sorted(set(ours.split()) - set(theirs)), sorted(set(theirs) - set(ours.split()))
                    if not left and not right:
                        continue
                    ours, theirs = '、'.join(left) or '—', '、'.join(right) or '—'
                elif ours == theirs:
                    continue
                affected.add(date.date().isoformat())
                lines.append(f'| {date:%Y-%m-%d} | {key} | {ours} | {theirs} |')
        lines.insert(4, f'50 天中 {len(affected)} 天至少一项不同，共 {len(lines)-8} 个字段差异。')
        lines += ['', '黄道/黑道比较时去掉 cnlunar 的“日”后缀；冲取其 `zodiacLose`。cnlunar 没有同名的“煞”字段，本对照不自造规则。',
                  '这里只记录差异，不判断两个流派谁正确，也没有据此改数据。']
        a.out.mkdir(parents=True, exist_ok=True)
        (a.out / 'CNLUNAR-DIFF.md').write_text('\n'.join(lines) + '\n')
        print(f'cnlunar: {len(affected)}/50 dates differ; complete field table written')
