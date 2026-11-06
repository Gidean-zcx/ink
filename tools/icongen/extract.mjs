// Extract only the icon geometry from the user's read-only design export.
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import crypto from 'node:crypto';

const source = process.argv[2] || process.env.INK_DESIGN;
if (!source) throw Error('set INK_DESIGN or pass the design export directory');
const dest = process.argv[3] || 'assets/icons';
const load = (name) => fs.readFileSync(path.join(source, 'boards', `${name}.dc.html`), 'utf8');
const icon = load('Icon'), sheet = load('IconSheet');
const code = (s) => s.match(/<script type="text\/x-dc"[^>]*>([\s\S]*?)<\/script>/)[1];
const run = (s, props) => vm.runInNewContext(`${code(s)}\nnew Component(props).renderVals()`, {
  props, DCLogic: class { constructor(p) { this.props = p; } }
}, { timeout: 1000 });
const listed = run(sheet, {});
const names = [...new Set([...listed.apps.map(a => a.icon), ...listed.glyphs,
  ...[...icon.matchAll(/^      (\w+): \{/gm)].map(m => m[1])])].sort();
fs.mkdirSync(dest, { recursive: true });
const hashes = {};
for (const name of names) for (const look of ['line', 'duo', 'solid']) {
  const p = run(icon, { icon: name, look, ink: '#000000', paper: '#ffffff', tone: '#ffffff' });
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48" fill="none" stroke-linecap="round" stroke-linejoin="round"><path d="${p.b}" fill="${p.bFill}" stroke="${p.ink}" stroke-width="${p.bw}"/><path d="${p.d}" stroke="${p.dStroke}" stroke-width="${p.dw}"/><path d="${p.f}" fill="${p.fFill}"/><path d="${p.s}" stroke="${p.ink}" stroke-width="${p.sw}"/><path d="${p.g}" fill="${p.ink}"/></svg>\n`;
  const file = `${name}-${look}.svg`;
  fs.writeFileSync(path.join(dest, file), svg);
  hashes[file] = crypto.createHash('sha256').update(svg).digest('hex');
}
fs.writeFileSync(path.join(dest, 'manifest.json'), JSON.stringify({
  source: 'Icon / IconSheet design boards',
  source_sha256: { Icon: crypto.createHash('sha256').update(icon).digest('hex'), IconSheet: crypto.createHash('sha256').update(sheet).digest('hex') },
  sizes: [28, 36, 44, 64, 88], names, files: hashes
}, null, 2) + '\n');
console.log(`extracted ${names.length} icons in three looks`);
