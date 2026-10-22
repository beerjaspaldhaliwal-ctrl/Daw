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

// Every design the page can produce must build and decode, so check the other sources too.
function roundTrip(design, label) {
  const { bytes, fileName } = engine.presetFile(design);
  const { meta, state } = unpack(bytes);
  const round = (v) => JSON.parse(JSON.stringify(v, (k, x) => (typeof x === 'number' ? Math.fround(x) : x)));
  assert.deepStrictEqual(round(state), round(JSON.parse(JSON.stringify(engine.buildState(design)))), label);
  assert.ok(fileName.endsWith('.SerumPreset'), label);
  n++;
  return { meta, state };
}

// normalizeDesign: garbage in, valid design out.
const junk = engine.normalizeDesign({
  name: 'bad/name:*?', category: 'nope', mono: 1,
  oscA: { wave: 'wobble', octave: 99, unison: -3, detune: 7, level: NaN },
  oscB: { on: true, unison: 40 }, sub: { level: 9, octave: -9 }, noise: { sample: 'x' },
  filter: { cutoff: 1e9, reso: -5 }, ampEnv: { attack: -1, decay: 0, sustain: 2, release: 'slow' },
  fx: { hyper: { wet: 500 }, dist: { on: false }, comp: true, reverb: {}, delay: null },
});
assert.strictEqual(junk.name, 'badname');
assert.strictEqual(junk.category, 'lead');
assert.strictEqual(junk.oscA.wave, 'saw');
assert.strictEqual(junk.oscA.octave, 4);
assert.strictEqual(junk.oscA.unison, 1);
assert.strictEqual(junk.oscA.detune, 1);
assert.strictEqual(junk.oscA.level, 0.75);
assert.strictEqual(junk.oscB.unison, 16);
assert.strictEqual(junk.sub.level, 1);
assert.strictEqual(junk.sub.octave, -3);
assert.strictEqual(junk.noise.sample, 'air');
assert.strictEqual(junk.filter.cutoff, 20000);
assert.strictEqual(junk.filter.reso, 0);
assert.strictEqual(junk.ampEnv.attack, 0);
assert.strictEqual(junk.ampEnv.sustain, 1);
assert.strictEqual(junk.ampEnv.release, 0.1);
assert.strictEqual(junk.fx.hyper.wet, 100);
assert.strictEqual(junk.fx.dist.on, false);
assert.strictEqual(junk.fx.comp.on, true);
assert.deepStrictEqual(engine.normalizeDesign(junk), junk, 'normalizing twice changes nothing');
roundTrip(junk, 'junk design');

// The sub's octave goes into the file; an Init-level oscillator writes no volume.
const subState = roundTrip(engine.textToDesign('dark bass with sub').design, 'bass with sub').state;
assert.strictEqual(subState.Oscillator4.plainParams.kParamOctave, -1);
assert.ok(!('kParamVolume' in subState.Oscillator0.plainParams), 'osc A at Init level writes no volume');

// Variations: repeatable from a seed, different from the source, always valid.
const src = engine.textToDesign('warm evolving pad with slow attack and long release').design;
const v1 = engine.varyDesign(src, { seed: 42, name: 'Pad 2' });
assert.deepStrictEqual(engine.varyDesign(src, { seed: 42, name: 'Pad 2' }), v1, 'same seed, same variation');
assert.notDeepStrictEqual(v1, engine.varyDesign(src, { seed: 43, name: 'Pad 2' }), 'different seeds differ');
assert.strictEqual(v1.name, 'Pad 2');
assert.notDeepStrictEqual(Object.assign({}, v1, { name: src.name }), src, 'a variation changes something');
assert.deepStrictEqual(engine.varyDesign(src, { seed: 5, amount: 0 }).ampEnv, src.ampEnv, 'amount 0 keeps the envelope');
for (let seed = 1; seed <= 40; seed++) {
  const v = engine.varyDesign(src, { seed, amount: 1 });
  assert.deepStrictEqual(engine.normalizeDesign(v), v, 'variation ' + seed + ' is already normalized');
  if (seed <= 5) roundTrip(v, 'variation ' + seed);
}

// The structured-output schemas must follow the API's rules: every object closed and fully required.
function checkSchema(sch, path) {
  if (sch.type === 'object') {
    assert.strictEqual(sch.additionalProperties, false, path + ' additionalProperties');
    assert.deepStrictEqual([...sch.required].sort(), Object.keys(sch.properties).sort(), path + ' required');
    for (const [k, v] of Object.entries(sch.properties)) checkSchema(v, path + '.' + k);
  } else if (sch.type === 'array') checkSchema(sch.items, path + '[]');
  for (const bad of ['minimum', 'maximum', 'minLength', 'maxLength', 'multipleOf']) assert.ok(!(bad in sch), path + ' uses ' + bad);
}
checkSchema(engine.AI_DESIGN_SCHEMA, 'design');
checkSchema(engine.AI_PACK_SCHEMA, 'pack');
assert.ok(engine.aiSinglePrompt('glassy pluck', {}).includes('glassy pluck'));
assert.ok(engine.aiSinglePrompt('x', {}).includes('always set reverbMix and delayMix to 0'));
assert.ok(engine.aiSinglePrompt('x', { experimentalFx: true }).includes('Reverb and delay are available'));
assert.ok(engine.aiPackPrompt(['one', 'two'], {}).includes('2. two'));

// Claude's answers map onto designs; experimental effects stay off unless allowed.
const answer = {
  name: 'Glass Pluck', category: 'pluck', mono: false,
  oscA: { wave: 'triangle', octave: 1, unison: 3, detune: 0.12, level: 0.8 },
  oscB: { enabled: true, wave: 'sine', octave: 2, unison: 1, detune: 0.1, level: 0.3 },
  sub: { enabled: false, level: 0.5 }, noise: { enabled: true, level: 0.1 },
  filter: { enabled: true, cutoffHz: 4200, resonance: 25 },
  ampEnv: { attackMs: 1, decayMs: 350, sustainPercent: 0, releaseMs: 400 },
  fx: { hyperWet: 30, distortionDrive: 0, distortionMix: 0, compressor: true, reverbMix: 35, delayMix: 20 },
  notes: ['Bright triangle pluck.'],
};
const a1 = engine.fromAi(answer, 'glassy pluck', {});
assert.strictEqual(a1.design.name, 'Glass Pluck');
assert.strictEqual(a1.design.category, 'pluck');
assert.strictEqual(a1.design.oscA.wave, 'triangle');
assert.strictEqual(a1.design.oscB.on, true);
assert.strictEqual(a1.design.filter.cutoff, 4200);
assert.strictEqual(a1.design.ampEnv.decay, 0.35);
assert.strictEqual(a1.design.ampEnv.sustain, 0);
assert.strictEqual(a1.design.fx.hyper.on, true);
assert.strictEqual(a1.design.fx.dist.on, false);
assert.strictEqual(a1.design.fx.comp.on, true);
assert.strictEqual(a1.design.fx.reverb.on, false);
assert.strictEqual(a1.design.fx.delay.on, false);
assert.deepStrictEqual(a1.notes, ['Bright triangle pluck.']);
roundTrip(a1.design, 'AI design');
const a2 = engine.fromAi(answer, 'glassy pluck', { experimentalFx: true });
assert.strictEqual(a2.design.fx.reverb.on, true);
assert.strictEqual(a2.design.fx.reverb.wet, 35);
assert.strictEqual(a2.design.fx.delay.on, true);
const types = roundTrip(a2.design, 'AI design with experimental FX').state.FXRack0.FX.map((f) => f.type);
assert.deepStrictEqual(types, [9, 5, 4, 6]);
// A broken answer falls back to the keyword reading.
const a3 = engine.fromAi('not json', 'dark reese bass', {});
assert.strictEqual(a3.design.category, 'reese');
const a4 = engine.fromAi({ name: 7, oscA: { wave: 'laser', unison: 400 }, filter: { cutoffHz: 'high' } }, 'dark reese bass', {});
assert.strictEqual(a4.design.oscA.wave, 'saw');
assert.strictEqual(a4.design.oscA.unison, 16);
roundTrip(a4.design, 'malformed AI design');

console.log('OK: ' + n + ' presets built and verified');
