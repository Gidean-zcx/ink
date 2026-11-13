// SPDX-FileCopyrightText: 2026 chopper1026
// SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
// 中文：从固定版本的 MIT 历法库生成离线表，不在固件内执行 JavaScript。
// English: Generate offline tables from a pinned MIT calendar library; no firmware JavaScript runtime.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const root = path.resolve(__dirname, '..');
const input = path.join(__dirname, 'vendor/lunar-javascript/lunar.cjs');
const {Solar, LunarYear, LunarUtil} = require(input);
const serial = s => Math.round(s.getJulianDay() - 2440587.5);
const base = serial(Solar.fromYmd(2000, 1, 1));
const terms = '小寒 大寒 立春 雨水 惊蛰 春分 清明 谷雨 立夏 小满 芒种 夏至 小暑 大暑 立秋 处暑 白露 秋分 寒露 霜降 立冬 小雪 大雪 冬至'.split(' ');
const quote = s => JSON.stringify(s);
const array = (type, name, rows) => `static const ${type} ${name}[] = {\n${rows.join(',\n')}\n};\n`;
// ink almanac-2: size-only format change (word tables + 1-byte word codes); rules and results unchanged, checked below.
const fail = message => {throw new Error(`Calendar self-check: ${message}`);};
let months = new Map(), runs = 0, termDays = [], oracle = [], last = -1;
for (let year = 1999; year <= 2100; ++year) {
  for (const m of LunarYear.fromYear(year).getMonths()) {
    months.set(m.getFirstJulianDay(), [Math.floor(m.getFirstJulianDay()-2440587.5), m.getYear(), m.getMonth(), m.getDayCount()]);
  }
}
// One word table per kind; each list is a run of 1-byte word codes ending in 0xFF, stored once, reached by a uint16 start.
function wordList(name) {
  const words = [], wordIds = new Map(), codes = [], starts = new Map(), lists = [];
  const word = w => {if (!wordIds.has(w)) {wordIds.set(w, words.length); words.push(w);} return wordIds.get(w);};
  const list = items => {
    const key = items.join(' ');
    if (!starts.has(key)) {starts.set(key, codes.length); lists.push(key); codes.push(...items.map(word), 0xFF);}
    return starts.get(key);
  };
  const decode = start => {const out = []; for (let i = start; codes[i] !== 0xFF; ++i) out.push(words[codes[i]]); return out.join(' ');};
  return {name, words, codes, lists, word, list, decode};
}
const yiji = wordList('Yi/Ji'), folk = wordList('folk');
const pairs = [], fullPairs = [];
for (let month = 0; month < 12; ++month) for (const d of LunarUtil.JIA_ZI) {
  const m = LunarUtil.JIA_ZI[month];
  pairs.push([yiji.list(LunarUtil.getDayYi(m,d)), yiji.list(LunarUtil.getDayJi(m,d))]);
}
for (const m of LunarUtil.JIA_ZI) for (const d of LunarUtil.JIA_ZI)
  fullPairs.push([LunarUtil.getDayYi(m,d).join(' '), LunarUtil.getDayJi(m,d).join(' ')]);
// Yi/Ji depend on the month branch only: the folded 720 pairs must equal all 3600 month-cycle × day-cycle pairs.
fullPairs.forEach(([yi, ji], i) => {
  const folded = pairs[(Math.floor(i/60)%12)*60 + i%60];
  if (yiji.decode(folded[0]) !== yi || yiji.decode(folded[1]) !== ji) fail(`Yi/Ji pair ${i} differs after folding`);
});
const spiritPairs = [];
for (let month = 0; month < 12; ++month) for (const day of LunarUtil.JIA_ZI) {
  const pair = [folk.list(LunarUtil.getDayJiShen(month,day)), folk.list(LunarUtil.getDayXiongSha(month,day))];
  if (folk.decode(pair[0]) !== LunarUtil.getDayJiShen(month,day).join(' ') || folk.decode(pair[1]) !== LunarUtil.getDayXiongSha(month,day).join(' '))
    fail(`spirit pair ${spiritPairs.length} does not decode back`);
  spiritPairs.push(pair);
}
const dayRoles = new Array(144), joy = new Array(10), fortune = new Array(10);
for (let year = 2000; year <= 2099; ++year) {
  const table = Solar.fromYmd(year,7,1).getLunar().getJieQiTable();
  const actual = Object.values(table).filter(s => s.getYear() === year).sort((a,b) => serial(a)-serial(b));
  if (actual.length !== 24) throw new Error(`Expected 24 terms in ${year}`);
  termDays.push(actual.map(s => s.getDay()));
  for (let month = 1; month <= 12; ++month) {
    for (let day = 1; day <= 31; ++day) {
      const js = new Date(Date.UTC(year,month-1,day));
      if (js.getUTCMonth() !== month-1) break;
      const s = Solar.fromYmd(year,month,day), l = s.getLunar();
      const cycle = LunarUtil.JIA_ZI.indexOf(l.getMonthInGanZhi());
      if (cycle !== last) {++runs; last=cycle;}
      // The month cycle follows the month's first solar term (jie) by civil day; calendar.c computes it the same way.
      if ((12 + (year-2000)*12 + (month-1) + (day >= termDays[year-2000][(month-1)*2] ? 1 : 0)) % 60 !== cycle)
        fail(`month cycle formula differs on ${year}-${month}-${day}`);
      const roleIndex = l.getMonthZhiIndex()*12+l.getDayZhiIndex(), stem = l.getDayGanIndex();
      if (!dayRoles[roleIndex]) dayRoles[roleIndex] = [folk.word(l.getZhiXing()),folk.word(l.getDayTianShen()),folk.word(l.getDayTianShenType())];
      joy[stem] = l.getDayPositionXiDesc();fortune[stem] = l.getDayPositionFuDesc();
      if (process.argv.includes('--oracle')) oracle.push([year,month,day,l.getYear(),l.getMonth(),l.getDay(),cycle,LunarUtil.JIA_ZI.indexOf(l.getDayInGanZhi()),l.getDayYi().join(' '),l.getDayJi().join(' '),l.getDayPositionCaiDesc(),l.getDayChongDesc(),l.getDaySha(),l.getDayPositionXiDesc(),l.getDayPositionFuDesc(),l.getDayJiShen().join(' '),l.getDayXiongSha().join(' '),l.getZhiXing(),l.getDayTianShen(),l.getDayTianShenType()]);
    }
  }
}
// Lunar months: {start relative to the first month, year - 1998, month}; days come from the next start (sentinel last).
const lunar = [...months.values()].sort((a,b)=>a[0]-b[0]);
const lunarFirst = lunar[0][0], lunarEnd = lunar[lunar.length-1][0] + lunar[lunar.length-1][3];
lunar.forEach((m, i) => {
  const next = lunar[i+1];
  if (m[1] < 1998 || m[1] - 1998 > 255 || m[2] < -12 || m[2] > 12 || !m[2]) fail(`lunar month ${m} out of field range`);
  if (next && next[0] !== m[0] + m[3]) fail(`lunar months ${m} and ${next} do not meet`);
  if (next && next[1] !== m[1] + (next[2] === 1 ? 1 : 0)) fail(`lunar year changes outside the first month at ${next}`);
});
if (lunarEnd - lunarFirst > 65535) fail('lunar month starts exceed 16 bits');
// The four list fields are fixed char arrays in calendar.h; the longest decoded list must fit with its NUL.
const header = fs.readFileSync(path.join(root, 'main/time/calendar.h'), 'utf8');
const capacity = name => {const m = header.match(new RegExp(`\\b${name}\\[(\\d+)\\]`)); if (!m) fail(`calendar.h has no ${name}[] array`); return +m[1];};
const longest = (table, column, rows) => Math.max(...rows.map(r => Buffer.byteLength(table.decode(r[column])) + 1));
for (const [name, table, column, rows] of [['yi', yiji, 0, pairs], ['ji', yiji, 1, pairs], ['auspicious', folk, 0, spiritPairs], ['inauspicious', folk, 1, spiritPairs]])
  if (longest(table, column, rows) > capacity(name)) fail(`longest ${name} list needs ${longest(table, column, rows)} bytes, calendar.h has ${capacity(name)}`);
for (const table of [yiji, folk]) {
  if (table.words.length >= 255) fail(`${table.name} has ${table.words.length} words; codes are one byte with 0xFF as the end`);
  if (table.codes.length >= 65536) fail(`${table.name} codes take ${table.codes.length} bytes; starts are 16 bits`);
}
const digest = crypto.createHash('sha256').update(fs.readFileSync(input)).digest('hex');
let out = `// Generated by tools/generate_calendar_data.cjs; lunar-javascript 1.7.7 (MIT), Copyright (c) 2018 6tail.\n// Input SHA-256: ${digest}\n// 中文：生成数据勿手改，许可见 tools/vendor/lunar-javascript/LICENSE。\n// English: Do not edit generated tables; see the upstream license.\n#pragma once\n`;
const codes = (name, table) => `static const uint8_t ${name}[${table.codes.length}] = {\n${Array.from({length: Math.ceil(table.codes.length/32)}, (_, i) => table.codes.slice(i*32, i*32+32).join(',')).join(',\n')}\n};\n`;
out += `#define CALENDAR_LUNAR_FIRST ${lunarFirst}\n`;
out += array('calendar_lunar_month_t', 'calendar_lunar_months', [...lunar.map(v=>`{${v[0]-lunarFirst},${v[1]-1998},${v[2]}}`), `{${lunarEnd-lunarFirst},0,0}`]);
out += `static const uint8_t calendar_term_days[100][24] = {\n${termDays.map(v=>`{${v}}`).join(',\n')}\n};\n`;
out += array('char *const', 'calendar_terms', terms.map(quote));
out += array('char *const', 'calendar_ganzhi', LunarUtil.JIA_ZI.map(quote));
out += array('char *const', 'calendar_yiji_words', yiji.words.map(quote));
out += codes('calendar_yiji_codes', yiji);
out += `static const uint16_t calendar_yiji_pairs[720][2] = {\n${pairs.map(v=>`{${v}}`).join(',\n')}\n};\n`;
if (dayRoles.filter(Boolean).length !== 144 || joy.filter(Boolean).length !== 10 || fortune.filter(Boolean).length !== 10)
  throw new Error('Incomplete daily folk tables');
out += array('char *const', 'calendar_folk_words', folk.words.map(quote));
out += codes('calendar_folk_codes', folk);
out += `static const uint16_t calendar_spirit_pairs[720][2] = {\n${spiritPairs.map(v=>`{${v}}`).join(',\n')}\n};\n`;
out += `static const uint8_t calendar_day_roles[144][3] = {\n${dayRoles.map(v=>`{${v}}`).join(',\n')}\n};\n`;
out += array('char *const', 'calendar_joy', joy.map(quote));
out += array('char *const', 'calendar_fortune', fortune.map(quote));
const dest = path.join(root, 'main/assets/calendar_data.h');
if (process.argv.includes('--check')) {if (fs.readFileSync(dest,'utf8') !== out) throw new Error('Stale calendar data');}
else fs.writeFileSync(dest,out);
const index = process.argv.indexOf('--oracle');
if (index >= 0) fs.writeFileSync(process.argv[index+1],JSON.stringify(oracle));
console.log(`Calendar 2000–2099: ${runs} month transitions, ${months.size} lunar months, ${yiji.lists.length} Yi/Ji lists of ${yiji.words.length} words, ${folk.lists.length} spirit lists of ${folk.words.length} folk words; ${Buffer.byteLength(out)} generated bytes`);
