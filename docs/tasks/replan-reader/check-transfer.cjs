/* Execute the unmodified upstream browser TXT TOC functions with synthetic files. */
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const upstream = path.resolve(process.env.INK_DEPS, '../ink-hw/upstream/oss/kiikoread-xiugai');
const html = fs.readFileSync(path.join(upstream, 'components/read_pico_transfer/upload.html'), 'utf8');
const start = html.indexOf('const TOC_RULES=');
const end = html.indexOf('async function sendToc(');
assert.ok(start >= 0 && end > start);
const scope = {TextEncoder, TextDecoder};
vm.runInNewContext(html.slice(start, end) + '\nthis.generate=buildToc;', scope);
(async () => {
  const text = Array.from({length:8}, (_,i) => `第${i+1}章 测试章节\n${'这是合成的测试正文。'.repeat(130)}\n`).join('');
  const utf8 = Buffer.from(text), le = Buffer.from(text, 'utf16le'), be = Buffer.from(le).swap16();
  const gbk = Buffer.concat(Array.from({length:8},(_,i)=>Buffer.concat([Buffer.from([0xb5,0xda]),Buffer.from(String(i+1)),Buffer.from([0xd5,0xc2,0x20,0xb2,0xe2,0xca,0xd4]),Buffer.from('\n'+'FAKE BODY '.repeat(150)+'\n')])));
  const cases = [['UTF8','utf-8',utf8],['UTF8-BOM','utf-8',Buffer.concat([Buffer.from([239,187,191]),utf8])],['UTF16LE-BOM','utf-16le',Buffer.concat([Buffer.from([255,254]),le])],['UTF16BE-BOM','utf-16be',Buffer.concat([Buffer.from([254,255]),be])],['UTF16LE-no-BOM','utf-16le',le],['UTF16BE-no-BOM','utf-16be',be],['GBK','gbk',gbk]];
  const results = [];
  for (const [name,encoding,bytes] of cases) {
    const toc = await scope.generate({arrayBuffer:async()=>Uint8Array.from(bytes).buffer});
    assert.ok(toc, name);
    assert.equal(toc.encoding, encoding, name);
    assert.equal(toc.entries.length, 8, name);
    assert.ok(toc.entries.every(e=>new TextDecoder(encoding).decode(bytes.subarray(e.o)).startsWith(e.t)), name);
    results.push({name,detected:toc.encoding,chapters:toc.entries.length,byteOffsetsValid:true});
  }
  console.log(JSON.stringify({source:'kiikoread-xiugai 746ce316, upload.html functions unchanged',networkCalls:0,results},null,2));
})().catch(e=>{console.error(e);process.exit(1)});
