/*
 * Serum 2 Preset Maker - engine
 *
 * Turns a plain-English description into a Serum 2 .SerumPreset file.
 * Runs in the browser (loaded by index.html) and in Node (used by the tests).
 *
 * File layout (reverse engineered by the community, see README):
 *   "XferJson\0" + u32le(len(json)) + u32le(0) + metadata JSON
 *   u32le(len(cbor)) + u32le(2) + zstd frame of the CBOR state
 * metadata.hash is the MD5 of the zstd frame.
 *
 * The state starts from Serum 2's own INIT preset; only parameters that were
 * seen in real Serum 2 files are written, plus a few clearly marked guesses.
 */
(function (root) {
  'use strict';

  // Serum 2 INIT preset state (decoded from a preset saved by Serum 2.0.19).
  const INIT = {"Arp0":{"plainParams":"default"},"ArpClip0":{"clip":{},"plainParams":"default"},"ArpClip1":{"clip":{},"plainParams":"default"},"ArpClip10":{"clip":{},"plainParams":"default"},"ArpClip11":{"clip":{},"plainParams":"default"},"ArpClip2":{"clip":{},"plainParams":"default"},"ArpClip3":{"clip":{},"plainParams":"default"},"ArpClip4":{"clip":{},"plainParams":"default"},"ArpClip5":{"clip":{},"plainParams":"default"},"ArpClip6":{"clip":{},"plainParams":"default"},"ArpClip7":{"clip":{},"plainParams":"default"},"ArpClip8":{"clip":{},"plainParams":"default"},"ArpClip9":{"clip":{},"plainParams":"default"},"ClipPlayer":{"kUIParamPianoRollNotePreview":1.0,"kUIParamPreviewClip":0.0,"kUIParamSelectedClip":0.0},"ClipPlayer0":{"plainParams":"default"},"Env0":{"plainParams":{"kParamCurve1":50.0,"kParamCurve2":66.60000000000001,"kParamCurve3":66.60000000000001}},"Env1":{"plainParams":{"kParamCurve1":50.0,"kParamCurve2":66.60000000000001,"kParamCurve3":66.60000000000001}},"Env2":{"plainParams":{"kParamCurve1":50.0,"kParamCurve2":66.60000000000001,"kParamCurve3":66.60000000000001}},"Env3":{"plainParams":{"kParamCurve1":50.0,"kParamCurve2":66.60000000000001,"kParamCurve3":66.60000000000001}},"FXRack0":{"FX":[],"displayName":"","plainParams":"default"},"FXRack1":{"FX":[],"displayName":"","plainParams":"default"},"FXRack2":{"FX":[],"displayName":"","plainParams":"default"},"Filter":{"kUIParamMixOrGain":0.0},"Global0":{"plainParams":"default"},"GranularOsc":[{"kUIParamDisplayXYInput":0.0},{"kUIParamDisplayXYInput":0.0},{"kUIParamDisplayXYInput":0.0}],"LFO0":{"curveData":{},"pathData":{},"plainParams":"default"},"LFO1":{"curveData":{},"pathData":{},"plainParams":"default"},"LFO2":{"curveData":{},"pathData":{},"plainParams":"default"},"LFO3":{"curveData":{},"pathData":{},"plainParams":"default"},"LFO4":{"curveData":{},"pathData":{},"plainParams":"default"},"LFO5":{"curveData":{},"pathData":{},"plainParams":"default"},"LFO6":{"curveData":{},"pathData":{},"plainParams":"default"},"LFO7":{"curveData":{},"pathData":{},"plainParams":"default"},"LFO8":{"curveData":{},"pathData":{},"plainParams":"default"},"LFO9":{"curveData":{},"pathData":{},"plainParams":"default"},"LFOPointModBus0":{"plainParams":"default"},"LFOPointModBus1":{"plainParams":"default"},"LFOPointModBus10":{"plainParams":"default"},"LFOPointModBus11":{"plainParams":"default"},"LFOPointModBus12":{"plainParams":"default"},"LFOPointModBus13":{"plainParams":"default"},"LFOPointModBus14":{"plainParams":"default"},"LFOPointModBus15":{"plainParams":"default"},"LFOPointModBus2":{"plainParams":"default"},"LFOPointModBus3":{"plainParams":"default"},"LFOPointModBus4":{"plainParams":"default"},"LFOPointModBus5":{"plainParams":"default"},"LFOPointModBus6":{"plainParams":"default"},"LFOPointModBus7":{"plainParams":"default"},"LFOPointModBus8":{"plainParams":"default"},"LFOPointModBus9":{"plainParams":"default"},"Macro0":{"plainParams":"default"},"Macro1":{"plainParams":"default"},"Macro2":{"plainParams":"default"},"Macro3":{"plainParams":"default"},"Macro4":{"plainParams":"default"},"Macro5":{"plainParams":"default"},"Macro6":{"plainParams":"default"},"Macro7":{"plainParams":"default"},"MidiClip0":{"clip":{},"plainParams":"default"},"MidiClip1":{"clip":{},"plainParams":"default"},"MidiClip10":{"clip":{},"plainParams":"default"},"MidiClip11":{"clip":{},"plainParams":"default"},"MidiClip2":{"clip":{},"plainParams":"default"},"MidiClip3":{"clip":{},"plainParams":"default"},"MidiClip4":{"clip":{},"plainParams":"default"},"MidiClip5":{"clip":{},"plainParams":"default"},"MidiClip6":{"clip":{},"plainParams":"default"},"MidiClip7":{"clip":{},"plainParams":"default"},"MidiClip8":{"clip":{},"plainParams":"default"},"MidiClip9":{"clip":{},"plainParams":"default"},"ModSlot0":{"plainParams":"default"},"ModSlot1":{"plainParams":"default"},"ModSlot10":{"plainParams":"default"},"ModSlot11":{"plainParams":"default"},"ModSlot12":{"plainParams":"default"},"ModSlot13":{"plainParams":"default"},"ModSlot14":{"plainParams":"default"},"ModSlot15":{"plainParams":"default"},"ModSlot16":{"plainParams":"default"},"ModSlot17":{"plainParams":"default"},"ModSlot18":{"plainParams":"default"},"ModSlot19":{"plainParams":"default"},"ModSlot2":{"plainParams":"default"},"ModSlot20":{"plainParams":"default"},"ModSlot21":{"plainParams":"default"},"ModSlot22":{"plainParams":"default"},"ModSlot23":{"plainParams":"default"},"ModSlot24":{"plainParams":"default"},"ModSlot25":{"plainParams":"default"},"ModSlot26":{"plainParams":"default"},"ModSlot27":{"plainParams":"default"},"ModSlot28":{"plainParams":"default"},"ModSlot29":{"plainParams":"default"},"ModSlot3":{"plainParams":"default"},"ModSlot30":{"plainParams":"default"},"ModSlot31":{"plainParams":"default"},"ModSlot32":{"plainParams":"default"},"ModSlot33":{"plainParams":"default"},"ModSlot34":{"plainParams":"default"},"ModSlot35":{"plainParams":"default"},"ModSlot36":{"plainParams":"default"},"ModSlot37":{"plainParams":"default"},"ModSlot38":{"plainParams":"default"},"ModSlot39":{"plainParams":"default"},"ModSlot4":{"plainParams":"default"},"ModSlot40":{"plainParams":"default"},"ModSlot41":{"plainParams":"default"},"ModSlot42":{"plainParams":"default"},"ModSlot43":{"plainParams":"default"},"ModSlot44":{"plainParams":"default"},"ModSlot45":{"plainParams":"default"},"ModSlot46":{"plainParams":"default"},"ModSlot47":{"plainParams":"default"},"ModSlot48":{"plainParams":"default"},"ModSlot49":{"plainParams":"default"},"ModSlot5":{"plainParams":"default"},"ModSlot50":{"plainParams":"default"},"ModSlot51":{"plainParams":"default"},"ModSlot52":{"plainParams":"default"},"ModSlot53":{"plainParams":"default"},"ModSlot54":{"plainParams":"default"},"ModSlot55":{"plainParams":"default"},"ModSlot56":{"plainParams":"default"},"ModSlot57":{"plainParams":"default"},"ModSlot58":{"plainParams":"default"},"ModSlot59":{"plainParams":"default"},"ModSlot6":{"plainParams":"default"},"ModSlot60":{"plainParams":"default"},"ModSlot61":{"plainParams":"default"},"ModSlot62":{"plainParams":"default"},"ModSlot63":{"plainParams":"default"},"ModSlot7":{"plainParams":"default"},"ModSlot8":{"plainParams":"default"},"ModSlot9":{"plainParams":"default"},"MultiSampleOsc":[{"kUIParamMultiSampleOverviewMouseTag":0.0},{"kUIParamMultiSampleOverviewMouseTag":0.0},{"kUIParamMultiSampleOverviewMouseTag":0.0}],"Osc":[{"kUIParamAutoSyncSlicing":0.0,"kUIParamShowMarkerAnimations":0.0,"kUIParamZoomToStartEnd":0.0},{"kUIParamAutoSyncSlicing":0.0,"kUIParamShowMarkerAnimations":0.0,"kUIParamZoomToStartEnd":0.0},{"kUIParamAutoSyncSlicing":0.0,"kUIParamShowMarkerAnimations":0.0,"kUIParamZoomToStartEnd":0.0},{"kUIParamAutoSyncSlicing":0.0,"kUIParamShowMarkerAnimations":0.0,"kUIParamZoomToStartEnd":0.0},{"kUIParamAutoSyncSlicing":0.0,"kUIParamShowMarkerAnimations":0.0,"kUIParamZoomToStartEnd":0.0}],"Oscillator0":{"GranularOsc0":{"plainParams":"default"},"MultiSampleOsc0":{"plainParams":"default"},"SampleOsc0":{"plainParams":"default"},"SpectralOsc0":{"plainParams":"default"},"WTOsc0":{"flex":{},"numChannels":1,"numFrames":18432,"plainParams":"default","relativePathToWT":"S2 Tables/Default Shapes.wav","sampleRate":44100},"plainParams":"default"},"Oscillator1":{"GranularOsc1":{"plainParams":"default"},"MultiSampleOsc1":{"plainParams":"default"},"SampleOsc1":{"plainParams":"default"},"SpectralOsc1":{"plainParams":"default"},"WTOsc1":{"flex":{},"numChannels":1,"numFrames":18432,"plainParams":"default","relativePathToWT":"S2 Tables/Default Shapes.wav","sampleRate":44100},"plainParams":"default"},"Oscillator2":{"GranularOsc2":{"plainParams":"default"},"MultiSampleOsc2":{"plainParams":"default"},"SampleOsc2":{"plainParams":"default"},"SpectralOsc2":{"plainParams":"default"},"WTOsc2":{"flex":{},"numChannels":1,"numFrames":18432,"plainParams":"default","relativePathToWT":"S2 Tables/Default Shapes.wav","sampleRate":44100},"plainParams":"default"},"Oscillator3":{"NoiseOsc3":{"detuneFactor":0.4758218478413068,"numChannels":1,"numFrames":78241,"plainParams":"default","relativePathToNoiseSample":"Organics/AC hum1.wav","sampleRate":44100},"plainParams":"default"},"Oscillator4":{"SubOsc4":{"plainParams":"default"},"plainParams":"default"},"PitchQuantizer0":{"plainParams":"default","scale":[0,2,0,2,0,0,2,0,2,0,2,0],"scaleName":"Major"},"RetriggerState0":{"plainParams":"default"},"RoutingSlot0":{"plainParams":"default"},"RoutingSlot1":{"plainParams":"default"},"RoutingSlot2":{"plainParams":"default"},"RoutingSlot3":{"plainParams":"default"},"RoutingSlot4":{"plainParams":"default"},"RoutingSlot5":{"plainParams":"default"},"RoutingSlot6":{"plainParams":"default"},"SerumGUI":{"kUIParamShowKeyboard":1.0,"kUIParamShowMidiOut":0.0},"SpectralOsc":[{"kUIParamDisplayXYInput":0.0,"kUIParamShowWaveformDisplay":0.0},{"kUIParamDisplayXYInput":0.0,"kUIParamShowWaveformDisplay":0.0},{"kUIParamDisplayXYInput":0.0,"kUIParamShowWaveformDisplay":0.0}],"VoiceFilter0":{"plainParams":"default"},"VoiceFilter1":{"plainParams":"default"},"VoicePanel0":{"displayName":"","plainParams":"default"},"WTOsc":[{"kUIParamWTOverviewMouseTag":0.0},{"kUIParamWTOverviewMouseTag":0.0},{"kUIParamWTOverviewMouseTag":0.0}],"arpBankDisplayName":"","clipBankDisplayName":"","fileType":"SerumPreset","lockOversampling":false,"lockTuning":false,"mpeConfig":0,"mpeEnabled":false,"mpePitchBendRange":48,"presetAuthor":"","presetDescription":"","presetName":"","product":"Serum2","productVersion":"2.0.19","scalars":{"note":{},"velo":{}},"tags":["Wavetable","Poly"],"url":"https://xferrecords.com/","vendor":"Xfer Records","version":7.0};

  // Copied from a real Serum 2 preset: HyperDimension's internal LFO table.
  const HYPER_LFO = [[0.9194614291191101, 0.08975490182638168, 0.5845403671264648, 0.286531537771225, 0.8271082043647766, 0.6682987809181213, 0.08552838116884232, 0.7215851545333862], [0.9865029454231262, 0.8898591995239258, 0.6562260389328003, 0.600176215171814, 0.5416178703308105, 0.5513129234313965, 0.8703054189682007, 0.1764964610338211]];
  const FLAT_CURVE = { curveVals: [0.4999999701976776, 0.4999999701976776], numPoints: 1, xVals: [0.0, 1.0], yVals: [1.0, 0.0] };

  // Keys whose values Serum stores as CBOR integers (everything else is a float).
  const INT_KEYS = new Set(['numChannels', 'numFrames', 'sampleRate', 'mpeConfig', 'mpePitchBendRange',
    'destModuleID', 'destModuleParamID', 'loopbackPointNum', 'numPoints', 'type', 'source', 'scale']);

  // Factory wavetable "Analog/Basic Shapes" (7 frames of 2048).
  // Frame order: sine, saw, triangle, square, pulse, narrow pulse, last.
  const BASIC_SHAPES = { path: '/Analog/Basic Shapes.wav', numFrames: 14336 };
  const WAVE_FRAME = { sine: 0, saw: 1, triangle: 2, square: 3, pulse: 4, narrowpulse: 5 };
  const NOISE_SAMPLES = {
    hum: { path: 'Organics/AC hum1.wav', numFrames: 78241 },
    air: { path: '/Organics/Air Can 2.wav', numFrames: 40855 },
  };

  /* ------------------------------------------------------------------ CBOR */

  function cborEncode(value) {
    const out = [];
    const f32 = new DataView(new ArrayBuffer(4));
    const f64 = new DataView(new ArrayBuffer(8));
    function head(major, n) {
      const m = major << 5;
      if (n < 24) out.push(m | n);
      else if (n < 0x100) out.push(m | 24, n);
      else if (n < 0x10000) out.push(m | 25, n >> 8, n & 255);
      else if (n < 0x100000000) out.push(m | 26, (n >>> 24) & 255, (n >>> 16) & 255, (n >>> 8) & 255, n & 255);
      else throw new Error('integer too large');
    }
    function str(s) {
      const b = new TextEncoder().encode(s);
      head(3, b.length);
      for (const x of b) out.push(x);
    }
    function num(v, asInt) {
      if (asInt && Number.isInteger(v)) {
        if (v >= 0) head(0, v); else head(1, -1 - v);
        return;
      }
      f32.setFloat32(0, v);
      if (f32.getFloat32(0) === v || Number.isNaN(v)) {
        out.push(0xfa);
        for (let i = 0; i < 4; i++) out.push(f32.getUint8(i));
      } else {
        f64.setFloat64(0, v);
        out.push(0xfb);
        for (let i = 0; i < 8; i++) out.push(f64.getUint8(i));
      }
    }
    function enc(v, asInt) {
      if (v === null || v === undefined) out.push(0xf6);
      else if (v === true) out.push(0xf5);
      else if (v === false) out.push(0xf4);
      else if (typeof v === 'number') num(v, asInt);
      else if (typeof v === 'string') str(v);
      else if (Array.isArray(v)) { head(4, v.length); for (const x of v) enc(x, asInt); }
      else {
        const keys = Object.keys(v);
        head(5, keys.length);
        for (const k of keys) { str(k); enc(v[k], INT_KEYS.has(k)); }
      }
    }
    enc(value, false);
    return Uint8Array.from(out);
  }

  /* ------------------------------------------- zstd (uncompressed blocks) */

  // A valid zstd frame made of raw blocks. Any zstd decoder (including
  // Serum's) reads it; it is just not compressed, which is fine for ~12 KB.
  function zstdRawFrame(data) {
    const out = [0x28, 0xb5, 0x2f, 0xfd, 0xa0]; // magic, single segment + 4-byte content size
    const n = data.length;
    out.push(n & 255, (n >>> 8) & 255, (n >>> 16) & 255, (n >>> 24) & 255);
    const MAX = 128 * 1024;
    let off = 0;
    do {
      const size = Math.min(MAX, n - off);
      const last = off + size >= n ? 1 : 0;
      const h = (size << 3) | last; // block type 0 = raw
      out.push(h & 255, (h >>> 8) & 255, (h >>> 16) & 255);
      for (let i = 0; i < size; i++) out.push(data[off + i]);
      off += size;
    } while (off < n);
    return Uint8Array.from(out);
  }

  /* ------------------------------------------------------------------- MD5 */

  function md5Hex(bytes) {
    const K = new Uint32Array(64), S = [7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21];
    for (let i = 0; i < 64; i++) K[i] = Math.floor(Math.abs(Math.sin(i + 1)) * 4294967296);
    const len = bytes.length, total = ((len + 8) >> 6) + 1;
    const w = new Uint32Array(total * 16);
    for (let i = 0; i < len; i++) w[i >> 2] |= bytes[i] << ((i % 4) * 8);
    w[len >> 2] |= 0x80 << ((len % 4) * 8);
    w[total * 16 - 2] = (len * 8) >>> 0;
    w[total * 16 - 1] = Math.floor(len / 0x20000000);
    let a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
    for (let blk = 0; blk < total * 16; blk += 16) {
      let a = a0, b = b0, c = c0, d = d0;
      for (let i = 0; i < 64; i++) {
        let f, g;
        if (i < 16) { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
        else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
        else { f = c ^ (b | ~d); g = (7 * i) % 16; }
        const s = S[(i >> 4) * 4 + (i % 4)];
        const t = d; d = c; c = b;
        const x = (a + f + K[i] + w[blk + g]) >>> 0;
        b = (b + ((x << s) | (x >>> (32 - s)))) >>> 0;
        a = t;
      }
      a0 = (a0 + a) >>> 0; b0 = (b0 + b) >>> 0; c0 = (c0 + c) >>> 0; d0 = (d0 + d) >>> 0;
    }
    let hex = '';
    for (const v of [a0, b0, c0, d0]) for (let i = 0; i < 4; i++) hex += ((v >>> (i * 8)) & 255).toString(16).padStart(2, '0');
    return hex;
  }

  /* ------------------------------------------------------------- container */

  function u32le(n) { return [n & 255, (n >>> 8) & 255, (n >>> 16) & 255, (n >>> 24) & 255]; }

  function packPreset(metadata, state) {
    const cbor = cborEncode(state);
    const z = zstdRawFrame(cbor);
    const meta = Object.assign({}, metadata, { hash: md5Hex(z) });
    const keys = Object.keys(meta).sort();
    const sorted = {};
    for (const k of keys) sorted[k] = meta[k];
    // Serum writes "version":7.0; JSON.stringify would write 7.
    const json = new TextEncoder().encode(JSON.stringify(sorted).replace('"version":7}', '"version":7.0}'));
    const head = [...new TextEncoder().encode('XferJson'), 0, ...u32le(json.length), 0, 0, 0, 0];
    const out = new Uint8Array(head.length + json.length + 8 + z.length);
    let o = 0;
    out.set(head, o); o += head.length;
    out.set(json, o); o += json.length;
    out.set([...u32le(cbor.length), ...u32le(2)], o); o += 8;
    out.set(z, o);
    return out;
  }

  /* ------------------------------------------------------------ the design */

  // A design is the editable description of a preset. Times are in seconds,
  // levels and sustain 0..1, resonance/mix/drive 0..100, cutoff in Hz.
  //   { name, description, category, tags, mono, polyCount,
  //     oscA: {wave, octave, unison, detune, level, pan},
  //     oscB: {on, wave, octave, unison, detune, level, pan},
  //     sub: {on, level}, noise: {on, level, sample}, filter: {on, cutoff, reso},
  //     ampEnv: {attack, hold, decay, sustain, release},
  //     fx: {hyper: {on, wet}, dist: {on, drive, wet}, comp: {on},
  //          delay: {on, wet}, reverb: {on, wet, size}} }   (delay and reverb are experimental)

  const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));
  const num = (v, fallback) => (typeof v === 'number' && Number.isFinite(v) ? v : fallback);

  const WAVES = ['sine', 'triangle', 'saw', 'square', 'pulse', 'narrowpulse'];
  const OSC_LEVEL_DEFAULT = 0.75; // Init level; not written to the file unless changed

  const MODULE_DEFAULTS = {
    oscA: { wave: 'saw', octave: 0, unison: 1, detune: 0.25, level: OSC_LEVEL_DEFAULT, pan: 0 },
    oscB: { on: false, wave: 'saw', octave: 0, unison: 1, detune: 0.25, level: OSC_LEVEL_DEFAULT, pan: 0 },
    sub: { on: false, level: 0.5625, octave: -1 },
    noise: { on: false, level: 0.2, sample: 'air' },
    filter: { on: false, cutoff: 2000, reso: 10 },
    ampEnv: { attack: 0.001, hold: 0, decay: 1.0, sustain: 1.0, release: 0.1 },
    hyper: { on: false, wet: 40 },
    dist: { on: false, drive: 50, wet: 60 },
    comp: { on: false },
    delay: { on: false, wet: 25 },
    reverb: { on: false, wet: 30, size: 70 },
  };

  function normOsc(o, def) {
    o = o || {};
    return {
      wave: WAVES.includes(o.wave) ? o.wave : def.wave,
      octave: clamp(Math.round(num(o.octave, def.octave)), -4, 4),
      unison: clamp(Math.round(num(o.unison, def.unison)), 1, 16),
      detune: clamp(num(o.detune, def.detune), 0, 1),
      level: clamp(num(o.level, def.level), 0, 1),
      pan: clamp(num(o.pan, def.pan), -100, 100),
    };
  }

  // Older designs mark a module as on by its presence; newer ones use `on`.
  function isOn(m) { return !!m && m.on !== false; }

  // Fills in and clamps every field, so the editor, the preview and the file
  // writer can rely on the full shape whatever produced the design.
  function normalizeDesign(raw) {
    raw = raw || {};
    const fx = raw.fx || {};
    const env = raw.ampEnv || {};
    const D = MODULE_DEFAULTS;
    const category = BASES[raw.category] ? raw.category : 'lead';
    const d = {
      name: String(raw.name || 'Text Preset').replace(/[\\/:*?"<>|]/g, '').trim().slice(0, 60) || 'Text Preset',
      description: String(raw.description || '').slice(0, 300),
      category,
      mono: !!raw.mono,
      polyCount: clamp(Math.round(num(raw.polyCount, 16)), 1, 32),
      oscA: normOsc(raw.oscA, D.oscA),
      oscB: Object.assign({ on: isOn(raw.oscB) }, normOsc(raw.oscB, D.oscB)),
      sub: {
        on: isOn(raw.sub),
        level: clamp(num(raw.sub && raw.sub.level, D.sub.level), 0, 1),
        octave: clamp(Math.round(num(raw.sub && raw.sub.octave, D.sub.octave)), -3, 3),
      },
      noise: {
        on: isOn(raw.noise),
        level: clamp(num(raw.noise && raw.noise.level, D.noise.level), 0, 1),
        sample: raw.noise && NOISE_SAMPLES[raw.noise.sample] ? raw.noise.sample : D.noise.sample,
      },
      filter: {
        on: isOn(raw.filter),
        cutoff: clamp(Math.round(num(raw.filter && raw.filter.cutoff, D.filter.cutoff)), 20, 20000),
        reso: clamp(num(raw.filter && raw.filter.reso, D.filter.reso), 0, 100),
      },
      ampEnv: {
        attack: clamp(num(env.attack, D.ampEnv.attack), 0, 32),
        hold: clamp(num(env.hold, D.ampEnv.hold), 0, 32),
        decay: clamp(num(env.decay, D.ampEnv.decay), 0.001, 32),
        sustain: clamp(num(env.sustain, D.ampEnv.sustain), 0, 1),
        release: clamp(num(env.release, D.ampEnv.release), 0.001, 32),
      },
      fx: {
        hyper: { on: isOn(fx.hyper), wet: clamp(num(fx.hyper && fx.hyper.wet, D.hyper.wet), 0, 100) },
        dist: {
          on: isOn(fx.dist),
          drive: clamp(num(fx.dist && fx.dist.drive, D.dist.drive), 0, 100),
          wet: clamp(num(fx.dist && fx.dist.wet, D.dist.wet), 0, 100),
        },
        comp: { on: fx.comp === true || (typeof fx.comp === 'object' && isOn(fx.comp)) },
        delay: { on: isOn(fx.delay), wet: clamp(num(fx.delay && fx.delay.wet, D.delay.wet), 0, 100) },
        reverb: {
          on: isOn(fx.reverb),
          wet: clamp(num(fx.reverb && fx.reverb.wet, D.reverb.wet), 0, 100),
          size: clamp(num(fx.reverb && fx.reverb.size, D.reverb.size), 0, 100),
        },
      },
    };
    if (raw.author) d.author = String(raw.author).slice(0, 60);
    d.tags = [BASES[category].tag, 'Wavetable', d.mono ? 'Mono' : 'Poly'];
    tidy(d);
    return d;
  }

  // Rounds to the precision a person would set by hand.
  function tidy(d) {
    const r = (v, places) => Math.round(v * Math.pow(10, places)) / Math.pow(10, places);
    for (const o of [d.oscA, d.oscB]) { o.detune = r(o.detune, 3); o.level = r(o.level, 3); o.pan = r(o.pan, 1); }
    d.sub.level = r(d.sub.level, 3);
    d.noise.level = r(d.noise.level, 3);
    d.filter.reso = r(d.filter.reso, 1);
    for (const k of ['attack', 'hold', 'decay', 'release']) d.ampEnv[k] = r(d.ampEnv[k], 4);
    d.ampEnv.sustain = r(d.ampEnv.sustain, 3);
    for (const m of Object.values(d.fx)) for (const k of ['wet', 'drive', 'size']) if (k in m) m[k] = r(m[k], 1);
  }

  /* ---------------------------------------------------- design -> preset */

  // Filter cutoff is stored 0..1 on a log scale. 0.5 = 425 Hz (Serum's default).
  const F_MIN = 8.19, F_MAX = 22050;
  function hzToNorm(hz) { return clamp(Math.log(hz / F_MIN) / Math.log(F_MAX / F_MIN), 0, 1); }

  function wavePos(wave) {
    const frame = WAVE_FRAME[wave] !== undefined ? WAVE_FRAME[wave] : 1;
    return 1 + frame * 255 / 6;
  }

  function oscParams(o, enable) {
    const p = {};
    if (enable) p.kParamEnable = 1.0;
    if (o.octave) p.kParamOctave = o.octave;
    if (o.pan) p.kParamPan = o.pan;
    if (o.unison > 1) { p.kParamUnison = o.unison; p.kParamDetune = o.detune; }
    if (o.level !== OSC_LEVEL_DEFAULT) p.kParamVolume = o.level;
    return Object.keys(p).length ? p : 'default';
  }

  function setWavetableOsc(state, idx, o) {
    const osc = state['Oscillator' + idx];
    const wt = osc['WTOsc' + idx];
    wt.relativePathToWT = BASIC_SHAPES.path;
    wt.numFrames = BASIC_SHAPES.numFrames;
    wt.flex = JSON.parse(JSON.stringify(FLAT_CURVE));
    wt.plainParams = { kParamTablePos: wavePos(o.wave) };
    osc.plainParams = oscParams(o, idx !== 0);
  }

  function buildState(design) {
    const d = normalizeDesign(design);
    const s = JSON.parse(JSON.stringify(INIT));
    s.presetName = d.name;
    s.presetAuthor = d.author || 'Serum 2 Preset Maker';
    s.presetDescription = d.description;
    s.tags = d.tags;

    setWavetableOsc(s, 0, d.oscA);
    if (d.oscB.on) setWavetableOsc(s, 1, d.oscB);

    if (d.sub.on) {
      // Serum's sub defaults to the played note's octave; -1 puts it under the oscillators.
      s.Oscillator4.plainParams = { kParamEnable: 1.0, kParamVolume: d.sub.level };
      if (d.sub.octave) s.Oscillator4.plainParams.kParamOctave = d.sub.octave;
    }
    if (d.noise.on) {
      const n = s.Oscillator3.NoiseOsc3, smp = NOISE_SAMPLES[d.noise.sample];
      n.relativePathToNoiseSample = smp.path;
      n.numFrames = smp.numFrames;
      s.Oscillator3.plainParams = { kParamEnable: 1.0, kParamVolume: d.noise.level };
    }

    if (d.filter.on) {
      s.VoiceFilter0.plainParams = { kParamEnable: 1.0, kParamFreq: hzToNorm(d.filter.cutoff), kParamReso: d.filter.reso };
      // Send A, B, C and noise through the filter; the sub stays direct.
      for (let i = 0; i < 4; i++) s['RoutingSlot' + i].plainParams = { kParamRoutingDest: 'kRoutingDestFilter' };
      if (d.sub.on) s.RoutingSlot4.plainParams = { kParamRoutingDest: 'kRoutingDestDirect' };
    }

    const e = d.ampEnv;
    s.Env0.plainParams = Object.assign({}, s.Env0.plainParams, {
      kParamAttack: e.attack, kParamHold: e.hold, kParamDecay: e.decay,
      kParamSustain: e.sustain, kParamRelease: e.release,
    });

    s.Global0.plainParams = d.mono
      ? { kParamMonoToggle: 1.0, kParamLimitSameNotePolyphony: 1.0 }
      : { kParamPolyCount: d.polyCount };

    const fx = [], f = d.fx;
    if (f.hyper.on) fx.push({
      FXHyperD: { lfo: HYPER_LFO, plainParams: { kParamDetune: 50.0, kParamDimESize: 50.0, kParamRate: 4.5, kParamUnison: 4.0, kParamWet: f.hyper.wet } },
      kUIParamMixOrGainDimE: 0.0, kUIParamMixOrGainHyper: 0.0, type: 9,
    });
    if (f.dist.on) fx.push({
      FXDistortion: { plainParams: { kParamDrive: f.dist.drive, kParamFreq: 0.5, kParamMode: 'kDiode2', kParamWet: f.dist.wet } },
      flex: [JSON.parse(JSON.stringify(FLAT_CURVE)), JSON.parse(JSON.stringify(FLAT_CURVE))],
      kUIParamMixOrGain: 0.0, type: 0,
    });
    if (f.comp.on) fx.push({
      FXComp: { plainParams: { kParamAttack: 90.0, kParamCompensatedWetDry: 0.0, kParamGain0: 4.6, kParamGain2: 4.6, kParamMakeup: 1.79, kParamRatioBelow: 0.75, kParamRelease: 90.0 } },
      kUIParamMixOrGain: 0.0, type: 5,
    });
    // Experimental: effect type numbers and parameter names below are guesses
    // that have not been seen in a real Serum 2 file.
    if (f.delay.on) fx.push({ FXDelay: { plainParams: { kParamWet: f.delay.wet } }, kUIParamMixOrGain: 0.0, type: 4 });
    if (f.reverb.on) fx.push({ FXReverb: { plainParams: { kParamWet: f.reverb.wet, kParamSize: f.reverb.size } }, kUIParamMixOrGain: 0.0, type: 6 });
    s.FXRack0.FX = fx;

    return s;
  }

  function buildPreset(design) {
    const state = buildState(design);
    const meta = {
      fileType: 'SerumPreset', presetAuthor: state.presetAuthor, presetDescription: state.presetDescription,
      presetName: state.presetName, product: 'Serum2', productVersion: '2.0.19', tags: state.tags,
      url: 'https://xferrecords.com/', vendor: 'Xfer Records', version: 7,
    };
    return packPreset(meta, state);
  }

  function fileNameFor(name) { return String(name).replace(/[\\/:*?"<>|]/g, '').trim() + '.SerumPreset'; }

  function presetFile(design) {
    const d = normalizeDesign(design);
    return { bytes: buildPreset(d), fileName: fileNameFor(d.name) };
  }

  /* ------------------------------------------------------------ variations */

  // Small deterministic PRNG so a variation can be made again from its seed.
  function rng(seed) {
    let a = seed >>> 0;
    return () => {
      a = (a + 0x6d2b79f5) >>> 0;
      let t = a;
      t = Math.imul(t ^ (t >>> 15), t | 1);
      t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  const RELATED_WAVES = { sine: ['triangle'], triangle: ['sine', 'square'], saw: ['square', 'pulse'], square: ['saw', 'pulse'], pulse: ['square', 'narrowpulse'], narrowpulse: ['pulse'] };

  // A new design near `design`. amount 0..1 sets how far it may move.
  function varyDesign(design, opts) {
    opts = opts || {};
    const amount = clamp(num(opts.amount, 0.5), 0, 1);
    const r = rng(num(opts.seed, Date.now()));
    const pick = (list) => list[Math.floor(r() * list.length)];
    const spread = (v, rel) => v * Math.pow(2, (r() * 2 - 1) * rel * amount); // multiplicative
    const nudge = (v, by) => v + (r() * 2 - 1) * by * amount;                    // additive
    const chance = (p) => r() < p * amount;
    const d = normalizeDesign(JSON.parse(JSON.stringify(design)));

    for (const o of [d.oscA, d.oscB]) {
      if (chance(0.35)) o.wave = pick(RELATED_WAVES[o.wave]);
      if (o.unison > 1) {
        o.unison = clamp(Math.round(nudge(o.unison, 2)), 2, 16);
        o.detune = clamp(spread(o.detune, 0.8), 0.02, 0.9);
      }
    }
    if (d.oscB.on) {
      if (chance(0.3)) d.oscB.octave = clamp(d.oscB.octave + pick([-1, 1]), -2, 2);
      d.oscB.level = clamp(nudge(d.oscB.level, 0.15), 0.1, 1);
    }
    if (d.sub.on) d.sub.level = clamp(nudge(d.sub.level, 0.15), 0.1, 1);
    if (chance(0.15)) d.noise.on = !d.noise.on;
    if (d.filter.on) {
      d.filter.cutoff = clamp(Math.round(spread(d.filter.cutoff, 1.2)), 60, 18000);
      d.filter.reso = clamp(nudge(d.filter.reso, 20), 0, 85);
    }
    const e = d.ampEnv;
    e.attack = clamp(e.attack < 0.01 ? e.attack : spread(e.attack, 1), 0.0005, 10);
    e.decay = clamp(spread(e.decay, 1), 0.02, 10);
    e.release = clamp(spread(e.release, 1), 0.01, 10);
    if (e.sustain > 0 && e.sustain < 1) e.sustain = clamp(nudge(e.sustain, 0.2), 0, 1);
    if (d.fx.hyper.on) d.fx.hyper.wet = clamp(nudge(d.fx.hyper.wet, 20), 5, 90);
    if (chance(0.2)) d.fx.dist.on = !d.fx.dist.on;
    if (d.fx.dist.on) d.fx.dist.drive = clamp(nudge(d.fx.dist.drive, 20), 10, 95);

    if (opts.name) d.name = opts.name;
    return normalizeDesign(d);
  }

  /* ---------------------------------------------------- text -> design */

  const has = (t, ...words) => words.some((w) => new RegExp('\\b' + w + '\\b', 'i').test(t));

  // Starting points per sound type. Times are in seconds, levels 0..1.
  const BASES = {
    bass: { tag: 'Bass', mono: true, oscA: { wave: 'saw', octave: -1, unison: 1 }, sub: { level: 0.5625 },
      filter: { cutoff: 900, reso: 15 }, ampEnv: { attack: 0.001, decay: 1.0, sustain: 1.0, release: 0.08 } },
    sub: { tag: 'Bass', mono: true, oscA: { wave: 'sine', octave: -1, unison: 1 },
      ampEnv: { attack: 0.002, decay: 1.0, sustain: 1.0, release: 0.12 } },
    reese: { tag: 'Bass', mono: true, oscA: { wave: 'saw', octave: -1, unison: 2, detune: 0.18 },
      oscB: { wave: 'saw', octave: -1, unison: 2, detune: 0.22 }, sub: { level: 0.45 },
      filter: { cutoff: 1200, reso: 10 }, ampEnv: { attack: 0.003, decay: 1.0, sustain: 1.0, release: 0.1 } },
    lead: { tag: 'Lead', mono: true, oscA: { wave: 'saw', unison: 3, detune: 0.15 }, oscB: { wave: 'square', octave: 1, level: 0.35 },
      filter: { cutoff: 5000, reso: 20 }, ampEnv: { attack: 0.003, decay: 1.0, sustain: 1.0, release: 0.15 } },
    supersaw: { tag: 'Lead', oscA: { wave: 'saw', unison: 7, detune: 0.3 }, oscB: { wave: 'saw', octave: 1, unison: 5, detune: 0.25, level: 0.4 },
      filter: { cutoff: 8000, reso: 5 }, ampEnv: { attack: 0.005, decay: 1.0, sustain: 1.0, release: 0.35 }, fx: { hyper: { wet: 30 } } },
    pad: { tag: 'Pad', oscA: { wave: 'saw', unison: 5, detune: 0.2 }, oscB: { wave: 'triangle', octave: 1, unison: 3, detune: 0.15, level: 0.4 },
      filter: { cutoff: 2500, reso: 10 }, ampEnv: { attack: 0.8, decay: 2.0, sustain: 0.85, release: 2.0 }, fx: { hyper: { wet: 45 } } },
    pluck: { tag: 'Pluck', oscA: { wave: 'saw', unison: 3, detune: 0.12 }, oscB: { wave: 'square', octave: 1, level: 0.3 },
      filter: { cutoff: 3500, reso: 15 }, ampEnv: { attack: 0.001, decay: 0.3, sustain: 0.0, release: 0.25 } },
    keys: { tag: 'Keys', oscA: { wave: 'triangle', unison: 2, detune: 0.08 }, oscB: { wave: 'sine', octave: 1, level: 0.35 },
      filter: { cutoff: 3000, reso: 5 }, ampEnv: { attack: 0.002, decay: 1.2, sustain: 0.4, release: 0.5 } },
    bell: { tag: 'Keys', oscA: { wave: 'sine' }, oscB: { wave: 'triangle', octave: 2, level: 0.4 },
      ampEnv: { attack: 0.001, decay: 1.5, sustain: 0.0, release: 1.5 } },
    stab: { tag: 'Synth', oscA: { wave: 'saw', unison: 5, detune: 0.2 }, oscB: { wave: 'square', level: 0.4 },
      filter: { cutoff: 4000, reso: 10 }, ampEnv: { attack: 0.001, decay: 0.25, sustain: 0.0, release: 0.15 } },
    arp: { tag: 'Synth', oscA: { wave: 'square', unison: 2, detune: 0.1 }, oscB: { wave: 'saw', octave: 1, level: 0.3 },
      filter: { cutoff: 4000, reso: 20 }, ampEnv: { attack: 0.001, decay: 0.4, sustain: 0.2, release: 0.2 } },
  };

  function detectBase(t) {
    if (has(t, 'reese')) return 'reese';
    if (has(t, 'supersaw', 'super saw', 'trance', 'hoover')) return 'supersaw';
    if (has(t, '808', 'sub ?bass', 'subbass')) return 'sub';
    if (has(t, 'bass', 'bassline', 'wobble', 'growl')) return 'bass';
    if (has(t, 'pad', 'pads', 'atmosphere', 'ambient', 'drone', 'texture')) return 'pad';
    if (has(t, 'pluck', 'plucks', 'plucky', 'pizzicato')) return 'pluck';
    if (has(t, 'bell', 'bells', 'chime', 'mallet')) return 'bell';
    if (has(t, 'keys', 'piano', 'rhodes', 'organ', 'ep')) return 'keys';
    if (has(t, 'stab', 'stabs', 'chord', 'chords')) return 'stab';
    if (has(t, 'arp', 'arpeggio', 'sequence')) return 'arp';
    if (has(t, 'lead', 'leads', 'solo', 'melody')) return 'lead';
    return 'lead';
  }

  // Reads "attack 200ms", "release 2s", "cutoff 800hz", "unison 7", etc.
  function readNumber(t, word, unit) {
    const m = t.match(new RegExp(word + '\\s*(?:of|at|=|:)?\\s*(-?\\d+(?:\\.\\d+)?)\\s*(' + unit + ')?', 'i'));
    if (!m) return null;
    return { v: parseFloat(m[1]), unit: (m[2] || '').toLowerCase() };
  }
  function readTime(t, word) {
    const n = readNumber(t, word, 'ms|s|sec|seconds|milliseconds');
    if (!n) return null;
    return n.unit === 'ms' || n.unit === 'milliseconds' ? n.v / 1000 : (n.unit ? n.v : (n.v > 20 ? n.v / 1000 : n.v));
  }

  function textToDesign(text, opts) {
    opts = opts || {};
    const t = ' ' + text.toLowerCase().replace(/[^a-z0-9.%\s-]/g, ' ') + ' ';
    const notes = [];
    const baseName = detectBase(t);
    const d = JSON.parse(JSON.stringify(BASES[baseName]));
    d.fx = d.fx || {};
    notes.push('Sound type: ' + baseName);

    // Waveform
    for (const w of ['sine', 'triangle', 'square', 'saw', 'pulse']) {
      const re = w === 'saw' ? 'saw|sawtooth' : w === 'triangle' ? 'triangle|tri' : w;
      if (has(t, re) && !(w === 'saw' && baseName === 'supersaw')) { d.oscA.wave = w; notes.push('Main wave: ' + w); break; }
    }

    // Brightness
    if (d.filter) {
      if (has(t, 'dark', 'muffled', 'deep', 'underwater', 'mellow', 'lofi', 'lo-fi')) { d.filter.cutoff *= 0.35; notes.push('Darker: lower cutoff'); }
      if (has(t, 'warm', 'soft', 'smooth', 'round')) { d.filter.cutoff *= 0.6; notes.push('Warmer: lower cutoff'); }
      if (has(t, 'bright', 'airy', 'crisp', 'shiny', 'sharp', 'open')) { d.filter.cutoff *= 2.5; notes.push('Brighter: higher cutoff'); }
      if (has(t, 'resonant', 'squelchy', 'acid', 'squelch')) { d.filter.reso = 65; notes.push('High resonance'); }
    } else if (has(t, 'dark', 'warm', 'muffled', 'mellow')) {
      d.filter = { cutoff: 1500, reso: 5 }; notes.push('Added a low-pass filter for a darker tone');
    }

    // Width / detune
    if (has(t, 'wide', 'huge', 'massive', 'big', 'stereo', 'lush', 'thick', 'fat')) {
      d.oscA.unison = Math.max(d.oscA.unison || 1, d.mono && baseName !== 'lead' ? 3 : 7);
      d.oscA.detune = Math.max(d.oscA.detune || 0, 0.25);
      if (!(baseName === 'sub' || baseName === 'bass' || baseName === 'reese')) d.fx.hyper = { wet: Math.max(d.fx.hyper ? d.fx.hyper.wet : 0, 40) };
      notes.push('Wider: more unison voices and detune');
    }
    if (has(t, 'detuned', 'detune')) { d.oscA.unison = Math.max(d.oscA.unison || 1, 5); d.oscA.detune = 0.35; notes.push('Extra detune'); }
    if (has(t, 'thin', 'narrow')) { d.oscA.unison = 1; if (d.oscB) d.oscB.unison = 1; delete d.fx.hyper; notes.push('Narrow: unison off'); }
    if (has(t, 'chorus', 'chorused', 'dimension')) { d.fx.hyper = { wet: 50 }; notes.push('Chorus-style width (HyperDimension)'); }

    // Dirt
    if (has(t, 'distorted', 'distortion', 'dirty', 'gritty', 'grit', 'aggressive', 'growl', 'angry', 'heavy', 'saturated', 'saturation', 'crunchy', 'drive', 'overdrive')) {
      const hard = has(t, 'aggressive', 'heavy', 'angry', 'growl', 'distorted', 'distortion');
      d.fx.dist = { drive: hard ? 70 : 40, wet: hard ? 80 : 55 };
      notes.push(hard ? 'Heavy distortion' : 'Light saturation');
    }
    if (has(t, 'clean', 'pure')) { delete d.fx.dist; notes.push('Clean: no distortion'); }
    if (has(t, 'punchy', 'punch', 'compressed', 'tight', 'ott')) { d.fx.comp = true; notes.push('Multiband compression'); }

    // Layers
    if (has(t, 'sub', 'subby', 'deep', 'weight', 'heavy') && baseName !== 'sub' && !d.sub) { d.sub = { level: 0.5 }; notes.push('Added sub oscillator'); }
    if (has(t, 'no sub')) { delete d.sub; notes.push('Sub oscillator off'); }
    if (has(t, 'noise', 'noisy', 'breath', 'breathy', 'air', 'airy', 'hiss')) { d.noise = { level: 0.2, sample: 'air' }; notes.push('Added a noise layer'); }
    if (has(t, 'single', 'simple', 'one oscillator', 'one osc')) { delete d.oscB; notes.push('Single oscillator'); }

    // Envelope words
    const e = d.ampEnv;
    if (has(t, 'slow attack', 'swell', 'swelling', 'fade in', 'soft attack', 'evolving')) { e.attack = Math.max(e.attack, 0.9); notes.push('Slow attack'); }
    if (has(t, 'snappy', 'percussive', 'short', 'staccato', 'tight')) { e.decay = Math.min(e.decay, 0.25); e.sustain = Math.min(e.sustain, 0.1); e.release = Math.min(e.release, 0.12); notes.push('Short, snappy envelope'); }
    if (has(t, 'long', 'sustained', 'held')) { e.release = Math.max(e.release, 1.5); notes.push('Long release'); }

    // Pitch / voicing
    if (has(t, 'high', 'higher')) { d.oscA.octave = (d.oscA.octave || 0) + 1; notes.push('Up one octave'); }
    if (has(t, 'low(?!\\s?pass)', 'lower') && baseName !== 'bass' && baseName !== 'sub' && baseName !== 'reese') { d.oscA.octave = (d.oscA.octave || 0) - 1; notes.push('Down one octave'); }
    if (has(t, 'poly', 'polyphonic', 'chords?')) { d.mono = false; }
    if (has(t, 'mono', 'monophonic', 'legato', 'glide')) { d.mono = true; }

    // Space (experimental)
    if (has(t, 'reverb', 'reverby', 'hall', 'spacey', 'spacious', 'roomy', 'dreamy', 'ethereal')) {
      if (opts.experimentalFx) { d.fx.reverb = { wet: has(t, 'huge', 'hall', 'spacey', 'ethereal') ? 45 : 25, size: 70 }; notes.push('Reverb (experimental)'); }
      else notes.push('Reverb asked for but skipped: turn on "experimental reverb and delay" to include it');
    }
    if (has(t, 'delay', 'echo', 'echoes')) {
      if (opts.experimentalFx) { d.fx.delay = { wet: 25 }; notes.push('Delay (experimental)'); }
      else notes.push('Delay asked for but skipped: turn on "experimental reverb and delay" to include it');
    }

    // Exact numbers override the words
    const atk = readTime(t, 'attack'); if (atk !== null) { e.attack = atk; notes.push('Attack ' + atk + ' s'); }
    const dec = readTime(t, 'decay'); if (dec !== null) { e.decay = dec; notes.push('Decay ' + dec + ' s'); }
    const rel = readTime(t, 'release'); if (rel !== null) { e.release = rel; notes.push('Release ' + rel + ' s'); }
    const sus = readNumber(t, 'sustain', '%'); if (sus) { e.sustain = clamp(sus.v > 1 ? sus.v / 100 : sus.v, 0, 1); notes.push('Sustain ' + Math.round(e.sustain * 100) + '%'); }
    const cut = readNumber(t, '(?:cutoff|filter)', 'khz|hz|k'); if (cut) {
      const hz = cut.unit === 'khz' || cut.unit === 'k' ? cut.v * 1000 : cut.v;
      d.filter = d.filter || { reso: 10 }; d.filter.cutoff = hz; notes.push('Cutoff ' + Math.round(hz) + ' Hz');
    }
    const res = readNumber(t, '(?:resonance|reso|res)', '%'); if (res) { d.filter = d.filter || { cutoff: 2000 }; d.filter.reso = clamp(res.v, 0, 100); notes.push('Resonance ' + d.filter.reso); }
    const uni = readNumber(t, '(?:unison|voices)', ''); if (uni) { d.oscA.unison = clamp(uni.v, 1, 16); d.oscA.detune = d.oscA.detune || 0.25; notes.push('Unison ' + d.oscA.unison); }
    const oct = readNumber(t, 'octave', ''); if (oct) { d.oscA.octave = clamp(oct.v, -4, 4); notes.push('Octave ' + d.oscA.octave); }

    if (d.filter) d.filter.cutoff = clamp(Math.round(d.filter.cutoff), 20, 20000);
    e.sustain = clamp(e.sustain, 0, 1);

    // Name: text in quotes, "called X" / "named X", otherwise the first words.
    const q = text.match(/["“']([^"”']{2,40})["”']/) || text.match(/\b(?:called|named|name)\s*:?\s+([\w -]{2,40})/i);
    const FILLER = /^(a|an|the|with|and|of|for|in|on|some|very|really|that|is|but|plus|make|me|i|want|like)$/i;
    d.name = (q ? q[1] : text.replace(/[^\w\s-]/g, ' ').trim().split(/\s+/).filter((w) => !FILLER.test(w)).slice(0, 4).join(' ')).trim() || 'Text Preset';
    d.name = d.name.replace(/\b\w/g, (c) => c.toUpperCase());
    d.description = text.trim().slice(0, 300);
    d.category = baseName;
    if (opts.author) d.author = opts.author;
    return { design: normalizeDesign(d), notes };
  }

  function makePreset(text, opts) {
    const { design, notes } = textToDesign(text, opts);
    return Object.assign({ design, notes }, presetFile(design));
  }

  /* ------------------------------------------------- AI (Claude) -> design */

  // What Claude fills in. Kept flat with every field required so it works as a
  // structured-output schema; fromAi() maps it onto a design and clamps it.
  const AI_OSC_SCHEMA = (withEnabled) => ({
    type: 'object',
    additionalProperties: false,
    required: (withEnabled ? ['enabled'] : []).concat(['wave', 'octave', 'unison', 'detune', 'level']),
    properties: Object.assign(withEnabled ? { enabled: { type: 'boolean' } } : {}, {
      wave: { type: 'string', enum: WAVES },
      octave: { type: 'integer' },
      unison: { type: 'integer' },
      detune: { type: 'number' },
      level: { type: 'number' },
    }),
  });
  const AI_LAYER_SCHEMA = { type: 'object', additionalProperties: false, required: ['enabled', 'level'], properties: { enabled: { type: 'boolean' }, level: { type: 'number' } } };

  const AI_DESIGN_SCHEMA = {
    type: 'object',
    additionalProperties: false,
    required: ['name', 'category', 'mono', 'oscA', 'oscB', 'sub', 'noise', 'filter', 'ampEnv', 'fx', 'notes'],
    properties: {
      name: { type: 'string' },
      category: { type: 'string', enum: ['bass', 'sub', 'reese', 'lead', 'supersaw', 'pad', 'pluck', 'keys', 'bell', 'stab', 'arp'] },
      mono: { type: 'boolean' },
      oscA: AI_OSC_SCHEMA(false),
      oscB: AI_OSC_SCHEMA(true),
      sub: AI_LAYER_SCHEMA,
      noise: AI_LAYER_SCHEMA,
      filter: {
        type: 'object', additionalProperties: false, required: ['enabled', 'cutoffHz', 'resonance'],
        properties: { enabled: { type: 'boolean' }, cutoffHz: { type: 'number' }, resonance: { type: 'number' } },
      },
      ampEnv: {
        type: 'object', additionalProperties: false, required: ['attackMs', 'decayMs', 'sustainPercent', 'releaseMs'],
        properties: { attackMs: { type: 'number' }, decayMs: { type: 'number' }, sustainPercent: { type: 'number' }, releaseMs: { type: 'number' } },
      },
      fx: {
        type: 'object', additionalProperties: false,
        required: ['hyperWet', 'distortionDrive', 'distortionMix', 'compressor', 'reverbMix', 'delayMix'],
        properties: {
          hyperWet: { type: 'number' }, distortionDrive: { type: 'number' }, distortionMix: { type: 'number' },
          compressor: { type: 'boolean' }, reverbMix: { type: 'number' }, delayMix: { type: 'number' },
        },
      },
      notes: { type: 'array', items: { type: 'string' } },
    },
  };
  const AI_PACK_SCHEMA = {
    type: 'object', additionalProperties: false, required: ['presets'],
    properties: { presets: { type: 'array', items: AI_DESIGN_SCHEMA } },
  };

  const AI_EXAMPLE = {
    name: 'Grit Reese', category: 'reese', mono: true,
    oscA: { wave: 'saw', octave: -1, unison: 2, detune: 0.2, level: 0.75 },
    oscB: { enabled: true, wave: 'saw', octave: -1, unison: 2, detune: 0.3, level: 0.6 },
    sub: { enabled: true, level: 0.5 }, noise: { enabled: false, level: 0.2 },
    filter: { enabled: true, cutoffHz: 700, resonance: 12 },
    ampEnv: { attackMs: 3, decayMs: 1000, sustainPercent: 100, releaseMs: 90 },
    fx: { hyperWet: 0, distortionDrive: 65, distortionMix: 70, compressor: true, reverbMix: 0, delayMix: 0 },
    notes: ['Two detuned saws an octave down give the reese movement.', 'Serum has no filter LFO here; add LFO 1 to Filter 1 cutoff for a wobble.'],
  };

  function aiInstructions(opts) {
    opts = opts || {};
    return [
      'You design presets for the Xfer Serum 2 synthesizer. Turn the sound description into settings for the fixed preset template below.',
      'Everything the template does not set stays at Serum 2\'s Init value.',
      '',
      'Fields:',
      '- name: a short preset name, 2 to 4 words, no quotes. If the description gives a name, use it.',
      '- category: the closest of bass, sub (808 / sub bass), reese, lead, supersaw, pad, pluck, keys, bell, stab, arp.',
      '- mono: true for basses and most leads; false for pads, chords, keys and plucks.',
      '- oscA (always on) and oscB (enabled true/false) are wavetable oscillators. wave: sine, triangle, saw, square, pulse or narrowpulse.',
      '  octave: -3 to 3, relative to the played note (basses usually -1). unison: 1 to 16 voices.',
      '  detune: 0 to 1 (0.1 subtle, 0.25 normal, 0.5 very wide; only matters when unison is above 1).',
      '  level: 0 to 1 (0.75 is Serum\'s default; set oscB lower to blend it under oscA).',
      '- sub: a sine sub oscillator. enabled, level 0 to 1.',
      '- noise: a noise layer for air or texture. enabled, level 0 to 1 (0.1 to 0.3 is typical).',
      '- filter: one 12 dB low-pass filter on oscA, oscB and noise (the sub bypasses it). enabled, cutoffHz 20 to 20000, resonance 0 to 100.',
      '- ampEnv: the volume envelope. attackMs, decayMs, sustainPercent 0 to 100, releaseMs.',
      '- fx: hyperWet 0 to 100 (stereo width and chorus; 0 is off), distortionDrive 0 to 100 (0 is off), distortionMix 0 to 100,',
      '  compressor true/false (multiband, adds punch and loudness), reverbMix and delayMix 0 to 100 (0 is off).',
      opts.experimentalFx
        ? '  Reverb and delay are available.'
        : '  Reverb and delay are switched off in this tool: always set reverbMix and delayMix to 0.',
      '- notes: 2 to 5 short sentences for the musician: what you chose and why, and anything the template cannot do.',
      '',
      'The template cannot add modulation (LFO wobble, filter envelopes, pitch envelopes), glide, macros, other filter types or other wavetables.',
      'When the description asks for one of these, get as close as the fields allow and say in notes what to set in Serum 2 afterwards.',
      'Keep levels sensible: the sum of oscA, oscB, sub and noise levels should rarely go above 2.',
    ].join('\n');
  }

  function aiSinglePrompt(text, opts) {
    return aiInstructions(opts) + '\n\nReply with only one JSON object in exactly this shape (the values here are just an example):\n'
      + JSON.stringify(AI_EXAMPLE) + '\n\nSound description:\n' + String(text).slice(0, 2000);
  }

  function aiPackPrompt(lines, opts) {
    const list = lines.map((l, i) => (i + 1) + '. ' + String(l).slice(0, 500)).join('\n');
    return aiInstructions(opts) + '\n\nDesign one preset for each numbered description below, in the same order, with distinct names.'
      + '\nReply with only JSON in exactly this shape, one object per description (the values are just an example):\n'
      + JSON.stringify({ presets: [AI_EXAMPLE] }) + '\n\nDescriptions:\n' + list;
  }

  // Maps Claude's answer onto a design. Starts from the keyword reading of the
  // same text, so anything missing or malformed falls back to that.
  function fromAi(ai, text, opts) {
    opts = opts || {};
    const base = textToDesign(text, opts).design;
    const d = JSON.parse(JSON.stringify(base));
    const notes = [];
    if (!ai || typeof ai !== 'object') return { design: base, notes: ['Claude\'s answer couldn\'t be read, so the keyword reading is used.'] };

    if (typeof ai.name === 'string' && ai.name.trim()) d.name = ai.name.trim().slice(0, 40);
    if (BASES[ai.category]) d.category = ai.category;
    if (typeof ai.mono === 'boolean') d.mono = ai.mono;
    const osc = (target, src) => {
      if (!src || typeof src !== 'object') return;
      if (typeof src.enabled === 'boolean') target.on = src.enabled;
      for (const k of ['wave', 'octave', 'unison', 'detune', 'level']) if (src[k] !== undefined) target[k] = src[k];
    };
    osc(d.oscA, ai.oscA);
    osc(d.oscB, ai.oscB);
    for (const k of ['sub', 'noise']) {
      const src = ai[k];
      if (src && typeof src === 'object') {
        if (typeof src.enabled === 'boolean') d[k].on = src.enabled;
        if (src.level !== undefined) d[k].level = src.level;
      }
    }
    const f = ai.filter;
    if (f && typeof f === 'object') {
      if (typeof f.enabled === 'boolean') d.filter.on = f.enabled;
      if (f.cutoffHz !== undefined) d.filter.cutoff = num(f.cutoffHz, d.filter.cutoff);
      if (f.resonance !== undefined) d.filter.reso = num(f.resonance, d.filter.reso);
    }
    const e = ai.ampEnv;
    if (e && typeof e === 'object') {
      if (e.attackMs !== undefined) d.ampEnv.attack = num(e.attackMs, d.ampEnv.attack * 1000) / 1000;
      if (e.decayMs !== undefined) d.ampEnv.decay = num(e.decayMs, d.ampEnv.decay * 1000) / 1000;
      if (e.sustainPercent !== undefined) d.ampEnv.sustain = num(e.sustainPercent, d.ampEnv.sustain * 100) / 100;
      if (e.releaseMs !== undefined) d.ampEnv.release = num(e.releaseMs, d.ampEnv.release * 1000) / 1000;
    }
    const x = ai.fx;
    if (x && typeof x === 'object') {
      const hyper = num(x.hyperWet, -1), drive = num(x.distortionDrive, -1);
      if (hyper >= 0) d.fx.hyper = { on: hyper > 0, wet: hyper > 0 ? hyper : d.fx.hyper.wet };
      if (drive >= 0) d.fx.dist = { on: drive > 0, drive: drive > 0 ? drive : d.fx.dist.drive, wet: num(x.distortionMix, d.fx.dist.wet) || d.fx.dist.wet };
      if (typeof x.compressor === 'boolean') d.fx.comp = { on: x.compressor };
      const rev = num(x.reverbMix, 0), del = num(x.delayMix, 0);
      if (opts.experimentalFx) {
        d.fx.reverb = { on: rev > 0, wet: rev > 0 ? rev : d.fx.reverb.wet, size: d.fx.reverb.size };
        d.fx.delay = { on: del > 0, wet: del > 0 ? del : d.fx.delay.wet };
      } else {
        d.fx.reverb.on = false;
        d.fx.delay.on = false;
      }
    }
    if (Array.isArray(ai.notes)) for (const n of ai.notes.slice(0, 6)) if (typeof n === 'string' && n.trim()) notes.push(n.trim().slice(0, 300));
    const design = normalizeDesign(d);
    design.description = base.description;
    return { design, notes };
  }

  const api = {
    makePreset, textToDesign, normalizeDesign, varyDesign, presetFile, fileNameFor,
    buildPreset, buildState, packPreset, cborEncode, zstdRawFrame, md5Hex, hzToNorm,
    aiSinglePrompt, aiPackPrompt, aiInstructions, fromAi, AI_DESIGN_SCHEMA, AI_PACK_SCHEMA,
    WAVES, CATEGORIES: Object.keys(BASES), MODULE_DEFAULTS, INIT,
  };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.SerumPresetMaker = api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
