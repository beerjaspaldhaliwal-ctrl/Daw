// Builds presets from sample descriptions and checks that each file decodes
// back to exactly the state the engine meant to write.
// Run: node test/verify.js   (Node 22.15+ for zlib.zstdDecompressSync)
'use strict';
const assert = require('assert');
const zlib = require('zlib');
const engine = require('../engine.js');

// Minimal CBOR decoder, independent of the engine's encoder.
function cborDecode(buf) {
  let i = 0;
  function len(ai) {
    if (ai < 24) return ai;
    if (ai === 24) return buf[i++];
    if (ai === 25) { const v = buf.readUInt16BE(i); i += 2; return v; }
    if (ai === 26) { const v = buf.readUInt32BE(i); i += 4; return v; }
    throw new Error('unsupported length ' + ai);
  }
  function item() {
    const ib = buf[i++], mt = ib >> 5, ai = ib & 31;
    if (mt === 0) return len(ai);
    if (mt === 1) return -1 - len(ai);
    if (mt === 3) { const n = len(ai), s = buf.toString('utf8', i, i + n); i += n; return s; }
    if (mt === 4) { const n = len(ai), a = []; for (let k = 0; k < n; k++) a.push(item()); return a; }
    if (mt === 5) { const n = len(ai), o = {}; for (let k = 0; k < n; k++) { const key = item(); o[key] = item(); } return o; }
    if (mt === 7) {
      if (ai === 20) return false;
      if (ai === 21) return true;
      if (ai === 22) return null;
      if (ai === 26) { const v = buf.readFloatBE(i); i += 4; return v; }
      if (ai === 27) { const v = buf.readDoubleBE(i); i += 8; return v; }
    }
    throw new Error('unsupported CBOR item 0x' + ib.toString(16) + ' at ' + (i - 1));
  }
  const v = item();
  assert.strictEqual(i, buf.length, 'trailing bytes after CBOR');
  return v;
}

function unpack(bytes) {
  const b = Buffer.from(bytes);
  assert.strictEqual(b.toString('latin1', 0, 9), 'XferJson\0');
  const jlen = b.readUInt32LE(9);
  assert.strictEqual(b.readUInt32LE(13), 0);
  const meta = JSON.parse(b.toString('utf8', 17, 17 + jlen));
  let o = 17 + jlen;
  const clen = b.readUInt32LE(o), ver = b.readUInt32LE(o + 4);
  assert.strictEqual(ver, 2);
  const z = b.subarray(o + 8);
  assert.strictEqual(meta.hash, engine.md5Hex(z), 'hash is MD5 of the zstd frame');
  const cbor = zlib.zstdDecompressSync(z);
  assert.strictEqual(cbor.length, clen);
  return { meta, state: cborDecode(cbor) };
}

// MD5 known answers.
assert.strictEqual(engine.md5Hex(Buffer.from('')), 'd41d8cd98f00b204e9800998ecf8427e');
assert.strictEqual(engine.md5Hex(Buffer.from('The quick brown fox jumps over the lazy dog')), '9e107d9d372bb6826bd81d3542a419d6');
// Filter cutoff mapping: Serum's default 425 Hz sits at the middle of the knob.
assert.ok(Math.abs(engine.hzToNorm(425) - 0.5) < 0.001);

const prompts = [
  'dark wide reese bass with distortion',
  'huge supersaw lead, bright, detuned',
  'warm evolving pad with slow attack and long release',
  'short bright pluck, punchy',
  '808 sub bass called "Deep 808"',
  'acid lead with resonance 70 and cutoff 600hz',
  'airy bell keys',
  'dreamy pad with reverb and delay',
  'square arp, attack 5ms, decay 300ms, sustain 20%, release 0.4s, unison 4',
];

let n = 0;
for (const p of prompts) {
  for (const experimentalFx of [false, true]) {
    const { bytes, design } = engine.makePreset(p, { experimentalFx });
    const { meta, state } = unpack(bytes);
    const expected = JSON.parse(JSON.stringify(engine.buildState(design)));
    // Floats that fit in 32 bits are stored as float32; compare at that precision.
    const round = (v) => JSON.parse(JSON.stringify(v, (k, x) => (typeof x === 'number' ? Math.fround(x) : x)));
    assert.deepStrictEqual(round(state), round(expected), p);
    assert.deepStrictEqual(Object.keys(state).sort(), Object.keys(engine.INIT).sort(), 'all top-level sections kept');
    assert.strictEqual(meta.presetName, design.name);
    assert.strictEqual(meta.fileType, 'SerumPreset');
    const types = state.FXRack0.FX.map((f) => f.type);
    if (!experimentalFx) assert.ok(!types.includes(4) && !types.includes(6), 'no experimental FX unless asked');
    n++;
  }
}

// A few spot checks on how text is read.
const d1 = engine.textToDesign('square arp, attack 5ms, decay 300ms, sustain 20%, release 0.4s, unison 4').design;
assert.strictEqual(d1.oscA.wave, 'square');
assert.strictEqual(d1.ampEnv.attack, 0.005);
assert.strictEqual(d1.ampEnv.decay, 0.3);
assert.strictEqual(d1.ampEnv.sustain, 0.2);
assert.strictEqual(d1.ampEnv.release, 0.4);
assert.strictEqual(d1.oscA.unison, 4);
assert.strictEqual(engine.textToDesign('808 sub bass called "Deep 808"').design.name, 'Deep 808');
assert.ok(engine.textToDesign('dark bass with sub').design.sub, 'bass with sub keeps bass base and adds sub');
assert.ok(engine.textToDesign('dark wide reese bass with distortion').design.fx.dist);

console.log('OK: ' + n + ' presets built and verified');
