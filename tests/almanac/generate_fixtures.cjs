// Manual fixture generation from the pinned lunar-javascript, never a CI dependency.
const fs = require('node:fs');
const path = require('node:path');
const vendor = path.resolve(__dirname, '../../system/services/time/vendor/mogo-calendar');
const {Solar, LunarUtil} = require(path.join(vendor, 'tools/vendor/lunar-javascript/lunar.cjs'));
const rows = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
const dates = [
  '2000-01-01', '2099-12-31', '2000-02-04', '2000-02-05', '2000-02-06',
  '2026-02-16', '2026-02-17', '2026-02-18',
  '2001-05-23', '2004-03-21', '2012-05-21', '2014-10-24',
  '2017-07-23', '2020-05-23', '2023-03-22', '2025-07-25',
  '2033-11-22', '2033-11-23', '2033-12-20', '2033-12-21', '2033-12-22', '2033-12-23',
  '2034-01-19', '2034-01-20', '2034-02-19', '2000-02-29', '2026-11-12'
];
for (const s of Object.values(Solar.fromYmd(2026,7,1).getLunar().getJieQiTable()))
  if (s.getYear() === 2026) dates.push(s.toYmd());
const all = process.argv.includes('--all');
const dateKey = row => row.slice(0,3).map((v,i) => i ? String(v).padStart(2,'0') : v).join('-');
const unique = all ? rows.map(dateKey) : [...new Set(dates)].sort();
if (!all && unique.length !== 50) throw new Error(`Expected 50 dates, got ${unique.length}`);
const keys = ['year','month','day','lunar_year','lunar_month','lunar_day','month_cycle','day_cycle',
  'yi','ji','wealth','clash','sha','joy','fortune','auspicious','inauspicious','officer','guardian','guardian_type'];
const byDate = new Map(rows.map(row => [dateKey(row), row]));
const fixtures = unique.map(date => {
  const row = byDate.get(date), v = Object.fromEntries(keys.map((k,i) => [k,row[i]]));
  const s = Solar.fromYmd(...row.slice(0,3)), l = s.getLunar();
  v.clash = [...v.clash].at(-1);
  v.weekday = s.getWeek();
  v.days = new Date(Date.UTC(v.year,v.month,0)).getUTCDate();
  v.first_weekday = Solar.fromYmd(v.year,v.month,1).getWeek();
  v.year_cycle = (v.lunar_year-4)%60;
  v.year_ganzhi = LunarUtil.JIA_ZI[v.year_cycle];
  v.month_ganzhi = l.getMonthInGanZhi(); v.day_ganzhi = l.getDayInGanZhi();
  v.solar_term = l.getJieQi();
  v.lunar_text = '农历'+(v.lunar_month<0?'闰':'')+l.getMonthInChinese()+'月'+l.getDayInChinese();
  // getMonthInChinese includes the leap marker itself.
  if (v.lunar_month<0) v.lunar_text = '农历'+l.getMonthInChinese()+'月'+l.getDayInChinese();
  return v;
});
fs.writeFileSync(all ? process.argv[3] : path.join(__dirname,'fixtures.json'),JSON.stringify(fixtures,null,2)+'\n');
console.log(`Wrote ${fixtures.length} ${all ? 'daily' : 'fixed'} fixtures from lunar-javascript 1.7.7 oracle`);
