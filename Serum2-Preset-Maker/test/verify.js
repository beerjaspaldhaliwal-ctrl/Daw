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

let n = 0;
const round = (v) => JSON.parse(JSON.stringify(v, (k, x) => (typeof x === 'number' ? Math.fround(x) : x)));

// Every design must build, decode back to exactly what was meant, and keep Serum's layout.
function roundTrip(design, label) {
  const { bytes, fileName } = engine.presetFile(design);
  const { meta, state } = unpack(bytes);
  assert.deepStrictEqual(round(state), round(JSON.parse(JSON.stringify(engine.buildState(design)))), label);
  assert.deepStrictEqual(Object.keys(state), Object.keys(engine.INIT).sort(), label + ': all sections kept, in byte order');
  assert.ok(fileName.endsWith('.SerumPreset'), label);
  assert.strictEqual(meta.fileType, 'SerumPreset');
  assert.strictEqual(meta.presetName, state.presetName);
  checkSorted(state, label);
  n++;
  return { meta, state };
}
// Serum writes every map with its keys in byte order; the files do the same.
function checkSorted(v, label) {
  if (Array.isArray(v)) return v.forEach((x) => checkSorted(x, label));
  if (!v || typeof v !== 'object') return;
  const keys = Object.keys(v);
  assert.deepStrictEqual(keys, [...keys].sort(), label + ': keys sorted');
  for (const k of keys) checkSorted(v[k], label);
}

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
  'dubstep wobble bass', 'future bass chords', 'lofi chill keys', 'synthwave lead with glide 80ms',
  'sidechain pumping pad', 'phaser pad', 'flanger lead', 'trap 808', 'techno bell', 'vibrato lead with reverb 30% and delay 1/4 dotted',
];
for (const p of prompts) roundTrip(engine.textToDesign(p).design, p);

// Text reading spot checks.
const d1 = engine.textToDesign('square arp, attack 5ms, decay 300ms, sustain 20%, release 0.4s, unison 4').design;
assert.strictEqual(d1.oscA.wave, 'square');
assert.strictEqual(d1.ampEnv.attack, 0.005);
assert.strictEqual(d1.ampEnv.decay, 0.3);
assert.strictEqual(d1.ampEnv.sustain, 0.2);
assert.strictEqual(d1.ampEnv.release, 0.4);
assert.strictEqual(d1.oscA.unison, 4);
assert.strictEqual(engine.textToDesign('808 sub bass called "Deep 808"').design.name, 'Deep 808');
assert.ok(engine.textToDesign('dark bass with sub').design.sub.on, 'bass with sub keeps bass base and adds sub');
assert.ok(engine.textToDesign('dark wide reese bass with distortion').design.fx.dist.on);
assert.strictEqual(engine.textToDesign('future bass chords').design.category, 'supersaw');
assert.strictEqual(engine.textToDesign('dubstep wobble bass').design.lfo.target, 'cutoff');
const d2 = engine.textToDesign('vibrato lead with reverb 30% and delay 1/4 dotted').design;
assert.strictEqual(d2.fx.reverb.wet, 30);
assert.strictEqual(d2.fx.delay.time, '1/4 dotted');
assert.strictEqual(engine.textToDesign('synthwave lead with glide 80ms').design.glide, 0.08);

// The fix for quiet, dull presets: brighter filters and full levels by default.
for (const type of engine.CATEGORIES) {
  const d = engine.textToDesign(type).design;
  if (d.filter.on && !d.filterEnv.on && !d.lfo.on) assert.ok(d.filter.cutoff >= 1400, type + ' static cutoff not parked low');
  assert.ok(d.oscA.level >= 0.7, type + ' main oscillator near full level');
  if (d.oscB.on) assert.ok(d.oscB.level >= 0.45, type + ' layer not buried');
}

// Values Serum 2 itself saved (single-change presets): the engine must write the same numbers.
assert.ok(Math.abs(engine.lfoRate('1/2') - 3.3735944697480575) < 1e-12, '1/2');
assert.ok(Math.abs(engine.lfoRate('1 bar') - 1.6269264358721542) < 1e-12, '1 bar');
assert.ok(Math.abs(engine.lfoRate('4 bar') - 0.2108496543592536) < 1e-12, '4 bar');
assert.ok(Math.abs(engine.lfoRate('1/32') - 26.030822973954468) < 1e-12, '1/32');
assert.ok(Math.abs(engine.lfoRate('1/4') - 6.25) < 1e-12, '1/4');

const full = engine.normalizeDesign({
  name: 'Everything', category: 'lead', mono: true, legato: true, glide: 0.07, gainDb: 4,
  oscA: { wave: 'saw', unison: 5, detune: 0.3, width: 80, semi: 7, fine: 10, level: 0.8 },
  oscB: { on: true, wave: 'square', octave: 1, level: 0.6 },
  sub: { on: true, level: 0.6, octave: -1, shape: 'kTriangle' }, noise: { on: true, level: 0.2 },
  filter: { on: true, type: 'MgL24', cutoff: 1200, reso: 30, drive: 25, keytrack: 50 },
  filterEnv: { on: true, amount: 40, attack: 0.001, decay: 0.3, sustain: 0.2, release: 0.3 },
  lfo: { on: true, target: 'cutoff', rate: '1/8', amount: 30, shape: 'triangle' },
  fx: {
    dist: { on: true, mode: 'kOverdrive', drive: 60, wet: 50 }, chorus: { on: true }, flanger: { on: true }, phaser: { on: true },
    hyper: { on: true }, eq: { on: true, bass: 2, treble: 3 }, comp: { on: true, amount: 50 },
    delay: { on: true, time: '1/8 dotted', pingpong: true }, reverb: { on: true, type: 'kHall', size: 60, wet: 25 },
  },
});
const st = roundTrip(full, 'everything').state;
const rack = st.FXRack0.FX;
const kinds = rack.map((m) => [m.type, Object.keys(m).find((k) => k.startsWith('FX'))]);
assert.deepStrictEqual(kinds, [[0, 'FXDistortion'], [3, 'FXChorus'], [1, 'FXFlanger'], [2, 'FXPhaser'], [9, 'FXHyperD'], [7, 'FXEQ'], [5, 'FXComp'], [4, 'FXDelay'], [6, 'FXReverb'], [5, 'FXComp']]);
const delay = rack[7].FXDelay.plainParams;
assert.strictEqual(delay.kParamTimeL, 0.03869670710330132, '1/8 as Serum saved it');
assert.strictEqual(delay.kParamOffsetL, 1.5, 'dotted');
assert.strictEqual(delay.kParamMode, 1, 'ping-pong');
assert.strictEqual(rack[8].FXReverb.plainParams.kParamType, 'kHall');
const out = rack[9].FXComp.plainParams;
assert.strictEqual(out.kParamRatio, 1);
assert.strictEqual(out.kParamThresh, 0);
assert.ok(Math.abs(out.kParamMakeup - Math.pow(10, 4 / 20)) < 1e-6, 'output gain as makeup');
assert.strictEqual(st.VoiceFilter0.plainParams.kParamType, 'MgL24');
assert.strictEqual(st.VoiceFilter0.plainParams.kParamKeyTrack, 50);
assert.strictEqual(st.Oscillator0.plainParams.kParamPitch, 7);
assert.strictEqual(st.Oscillator0.plainParams.kParamDetuneWid, 80);
assert.strictEqual(st.Oscillator4.SubOsc4.plainParams.kParamShape, 'kTriangle');
assert.strictEqual(st.Global0.plainParams.kParamPortamentoTime, 0.07);
assert.strictEqual(st.Global0.plainParams.kParamLegato, 1);
assert.ok(Math.abs(st.LFO0.plainParams.kParamRate - engine.lfoRate('1/8')) < 1e-9);
assert.deepStrictEqual(st.LFO0.curveData.yVals, [1, 0, 1]);
assert.strictEqual(st.Env1.plainParams.kParamDecay, 0.3);
// Modulation slots: Env 2 and LFO 1 on the cutoff, then the four macros.
const slots = Object.keys(st).filter((k) => /^ModSlot\d+$/.test(k) && st[k].source).sort((a, b) => a.slice(7) - b.slice(7)).map((k) => st[k]);
const brief = slots.map((m) => [m.source[0], m.destModuleTypeString, m.destModuleID, m.destModuleParamName, m.destModuleParamID, m.plainParams.kParamAmount]);
assert.deepStrictEqual(brief, [
  [3, 'VoiceFilter', 0, 'kParamFreq', 3, 40],
  [6, 'VoiceFilter', 0, 'kParamFreq', 3, 30],
  [25, 'VoiceFilter', 0, 'kParamFreq', 3, 35],
  [26, 'FXDistortion', 0, 'kParamWet', 1, 50],
  [27, 'FXReverb', 8, 'kParamWet', 1, 40],
  [27, 'FXDelay', 7, 'kParamWet', 1, 30],
  [28, 'FXHyperD', 4, 'kParamWet', 1, 60],
]);
assert.deepStrictEqual([st.Macro0.name, st.Macro1.name, st.Macro2.name, st.Macro3.name], engine.MACRO_NAMES);

// Macros on a dry sound add the effects they need at 0% mix, so the sound itself is unchanged.
const dry = roundTrip(engine.normalizeDesign({ name: 'Dry', oscA: { wave: 'sine' } }), 'dry').state;
const wets = Object.fromEntries(dry.FXRack0.FX.map((m) => { const k = Object.keys(m).find((x) => x.startsWith('FX')); return [k, m[k].plainParams.kParamWet]; }));
assert.deepStrictEqual(wets, { FXDistortion: 0, FXHyperD: 0, FXDelay: 0, FXReverb: 0 });
const noMacros = roundTrip(engine.normalizeDesign({ name: 'Plain', macros: false }), 'no macros').state;
assert.deepStrictEqual(noMacros.FXRack0.FX, []);
assert.strictEqual(noMacros.ModSlot0.plainParams, 'default');

// Negative output gain lowers every source level instead (amplitude follows level^2).
const quiet = roundTrip(engine.normalizeDesign({ name: 'Quiet', gainDb: -6, macros: false, sub: { on: true, level: 0.6 } }), 'quiet').state;
assert.ok(Math.abs(quiet.Oscillator0.plainParams.kParamVolume - 0.75 * Math.pow(10, -6 / 40)) < 1e-9);
assert.ok(Math.abs(quiet.Oscillator4.plainParams.kParamVolume - 0.6 * Math.pow(10, -6 / 40)) < 1e-9);

// normalizeDesign: garbage in, valid design out.
const junk = engine.normalizeDesign({
  name: 'bad/name:*?', category: 'nope', mono: 1, glide: -3, gainDb: 99,
  oscA: { wave: 'wobble', octave: 99, unison: -3, detune: 7, level: NaN, semi: 40, pan: 400 },
  oscB: { on: true, unison: 40 }, sub: { level: 9, octave: -9, shape: 'x' }, noise: { sample: 'x' },
  filter: { cutoff: 1e9, reso: -5, type: 'Laser' }, ampEnv: { attack: -1, decay: 0, sustain: 2, release: 'slow' },
  filterEnv: { amount: 500 }, lfo: { rate: '1/3', target: 'moon', amount: -900 },
  fx: { hyper: { wet: 500 }, dist: { on: false, mode: 'kNope' }, comp: true, reverb: { type: 'kSpace' }, delay: { time: '3/7' }, eq: { lowCut: 77 } },
});
assert.strictEqual(junk.name, 'badname');
assert.strictEqual(junk.category, 'lead');
assert.deepStrictEqual([junk.oscA.wave, junk.oscA.octave, junk.oscA.unison, junk.oscA.detune, junk.oscA.level, junk.oscA.semi, junk.oscA.pan], ['saw', 4, 1, 1, 0.75, 12, 50]);
assert.strictEqual(junk.oscB.unison, 16);
assert.deepStrictEqual([junk.sub.level, junk.sub.octave, junk.sub.shape], [1, -3, 'kSine']);
assert.strictEqual(junk.noise.sample, 'air');
assert.deepStrictEqual([junk.filter.cutoff, junk.filter.reso, junk.filter.type], [20000, 0, 'MgL12']);
assert.deepStrictEqual([junk.ampEnv.attack, junk.ampEnv.sustain, junk.ampEnv.release], [0, 1, 0.12]);
assert.strictEqual(junk.filterEnv.amount, 100);
assert.deepStrictEqual([junk.lfo.rate, junk.lfo.target, junk.lfo.amount], ['1/4', 'cutoff', -100]);
assert.deepStrictEqual([junk.glide, junk.gainDb], [0, 12]);
assert.strictEqual(junk.fx.hyper.wet, 100);
assert.deepStrictEqual([junk.fx.dist.on, junk.fx.dist.mode], [false, 'kTube']);
assert.strictEqual(junk.fx.comp.on, true);
assert.strictEqual(junk.fx.reverb.type, 'kPlate');
assert.strictEqual(junk.fx.delay.time, '1/8 dotted');
assert.strictEqual(junk.fx.eq.lowCut, 0);
assert.deepStrictEqual(engine.normalizeDesign(junk), junk, 'normalizing twice changes nothing');
roundTrip(junk, 'junk design');

// Variations and random sounds: repeatable from a seed, always valid.
const src = engine.textToDesign('warm evolving pad with slow attack and long release').design;
const v1 = engine.varyDesign(src, { seed: 42, name: 'Pad 2' });
assert.deepStrictEqual(engine.varyDesign(src, { seed: 42, name: 'Pad 2' }), v1, 'same seed, same variation');
assert.notDeepStrictEqual(v1, engine.varyDesign(src, { seed: 43, name: 'Pad 2' }), 'different seeds differ');
assert.notDeepStrictEqual(Object.assign({}, v1, { name: src.name }), src, 'a variation changes something');
assert.deepStrictEqual(engine.varyDesign(src, { seed: 5, amount: 0 }).ampEnv, src.ampEnv, 'amount 0 keeps the envelope');
for (let seed = 1; seed <= 40; seed++) {
  const v = engine.varyDesign(engine.textToDesign(prompts[seed % prompts.length]).design, { seed, amount: 1 });
  assert.deepStrictEqual(engine.normalizeDesign(v), v, 'variation ' + seed + ' is already normalized');
  if (seed <= 6) roundTrip(v, 'variation ' + seed);
  const r = engine.randomDesign(seed * 7919);
  assert.deepStrictEqual(engine.randomDesign(seed * 7919), r, 'random sound repeatable');
  if (seed <= 6) roundTrip(r.design, 'random ' + seed);
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
assert.ok(engine.aiSinglePrompt('glassy pluck').includes('glassy pluck'));
assert.ok(engine.aiPackPrompt(['one', 'two']).includes('2. two'));

// Claude's answers map onto designs.
const answer = {
  name: 'Glass Pluck', category: 'pluck', mono: false, glideMs: 0,
  oscA: { wave: 'triangle', octave: 1, semitones: 0, unison: 3, detune: 0.12, level: 0.8 },
  oscB: { enabled: true, wave: 'sine', octave: 2, semitones: 7, unison: 1, detune: 0.1, level: 0.5 },
  sub: { enabled: false, level: 0.5, octave: -1 }, noise: { enabled: true, level: 0.15 },
  filter: { enabled: true, type: 'MgL24', cutoffHz: 1200, resonance: 25, drive: 10 },
  ampEnv: { attackMs: 1, decayMs: 350, sustainPercent: 0, releaseMs: 400 },
  filterEnv: { amount: 50, attackMs: 0, decayMs: 250, sustainPercent: 0, releaseMs: 200 },
  lfo: { target: 'none', rate: '1/4', shape: 'triangle', amount: 0 },
  fx: { distortionMode: 'kTube', distortionDrive: 30, distortionMix: 0, chorusMix: 25, phaserMix: 0, flangerMix: 0, hyperMix: 30,
    eqBassDb: 0, eqTrebleDb: 2, punch: 40, delayTime: '1/8 dotted', delayFeedback: 40, delayMix: 20, reverbType: 'kHall', reverbSize: 60, reverbMix: 25 },
  notes: ['Bright triangle pluck.'],
};
const a1 = engine.fromAi(answer, 'glassy pluck');
assert.strictEqual(a1.design.name, 'Glass Pluck');
assert.strictEqual(a1.design.oscB.semi, 7);
assert.deepStrictEqual([a1.design.filter.type, a1.design.filter.cutoff, a1.design.filter.drive], ['MgL24', 1200, 10]);
assert.deepStrictEqual([a1.design.filterEnv.on, a1.design.filterEnv.amount, a1.design.filterEnv.decay], [true, 50, 0.25]);
assert.strictEqual(a1.design.lfo.on, false);
assert.deepStrictEqual([a1.design.fx.dist.on, a1.design.fx.chorus.on, a1.design.fx.chorus.wet, a1.design.fx.eq.treble, a1.design.fx.comp.amount], [false, true, 25, 2, 40]);
assert.deepStrictEqual([a1.design.fx.reverb.on, a1.design.fx.reverb.type, a1.design.fx.delay.wet], [true, 'kHall', 20]);
assert.deepStrictEqual(a1.notes, ['Bright triangle pluck.']);
roundTrip(a1.design, 'AI design');
const a3 = engine.fromAi('not json', 'dark reese bass');
assert.strictEqual(a3.design.category, 'reese');
const a4 = engine.fromAi({ name: 7, oscA: { wave: 'laser', unison: 400 }, filter: { cutoffHz: 'high', type: 'Laser' }, lfo: { target: 'moon', amount: 50 } }, 'dark reese bass');
assert.strictEqual(a4.design.oscA.wave, 'saw');
assert.strictEqual(a4.design.oscA.unison, 16);
roundTrip(a4.design, 'malformed AI design');

console.log('OK: ' + n + ' presets built and verified');
