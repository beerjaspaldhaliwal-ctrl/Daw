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
 * The state starts from Serum 2's own INIT preset. Parameter names, units,
 * effect types and modulation ids follow presets saved by Serum 2 (see the
 * README's "Sources"); the few that are not confirmed are marked there.
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

  // A design is the editable description of a preset. Levels are Serum's level
  // knobs (0..1, amplitude = knob^2, 0.75 is the Init level); times are seconds;
  // sustain 0..1; mix/drive/resonance 0..100 %; cutoff in Hz.
  //   { name, description, category, tags, mono, legato, glide, polyCount, gainDb, macros,
  //     oscA: {wave, octave, semi, fine, unison, detune, width, level, pan},
  //     oscB: {on, ...as oscA}, sub: {on, level, octave, shape}, noise: {on, level, sample},
  //     filter: {on, type, cutoff, reso, drive, keytrack},
  //     ampEnv: {attack, hold, decay, sustain, release},
  //     filterEnv: {on, amount, attack, decay, sustain, release},       (Env 2 -> filter cutoff)
  //     lfo: {on, target, rate, amount, shape},                         (LFO 1 -> target)
  //     fx: {dist, chorus, flanger, phaser, hyper, eq, comp, delay, reverb} }

  const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));
  const num = (v, fallback) => (typeof v === 'number' && Number.isFinite(v) ? v : fallback);
  const pick = (v, allowed, fallback) => (Object.prototype.hasOwnProperty.call(allowed, v) ? v : fallback);

  const WAVES = ['sine', 'triangle', 'saw', 'square', 'pulse', 'narrowpulse'];
  const OSC_LEVEL_DEFAULT = 0.75; // Init level; not written to the file unless changed

  // Menu values as Serum 2 stores them, with the names its menus show.
  const FILTER_TYPES = { MgL12: 'MG Low 12', MgL18: 'MG Low 18', MgL24: 'MG Low 24', L12: 'Low 12', H12: 'High 12', H24: 'High 24', DirtyMg: 'Dirty MG', CombP: 'Comb +' };
  const SUB_SHAPES = { kSine: 'Sine', kRoundRect: 'Round rect', kTriangle: 'Triangle', kSaw: 'Saw', kSquare: 'Square', kPulse: 'Pulse' };
  const DIST_MODES = {
    kTube: 'Tube', kSoftClip: 'Soft clip', kHardClip: 'Hard clip', kDiode1: 'Diode 1', kDiode2: 'Diode 2', kOverdrive: 'Overdrive',
    kTapeSat: 'Tape', kSoftSat: 'Soft sat', kAsym: 'Asym', kSinFold: 'Sine fold', kLinFold: 'Linear fold', kDownsample: 'Downsample', kRectify: 'Rectify',
  };
  const REVERB_TYPES = { kPlate: 'Plate', kHall: 'Hall', kVintage: 'Vintage' };
  // Synced LFO rates step through divisions at knob k/14 (a float32) and store
  // 100 * knob^4; 1/2, 1 bar, 4 bar and 1/32 match values Serum 2 saved (to 1e-15).
  const LFO_DIVISIONS = { '4 bar': 3, '2 bar': 4, '1 bar': 5, '1/2': 6, '1/4': 7, '1/8': 8, '1/16': 9, '1/32': 10 };
  const lfoRate = (div) => 100 * Math.pow(Math.fround(LFO_DIVISIONS[div] / 14), 4);
  // Synced delay times exactly as Serum 2 saved them.
  const DELAY_SYNC = { '1/32': 0.008377834674240726, '1/16': 0.01800540514814858, '1/8': 0.03869670710330132, '1/4': 0.08316586760018929, '1/2': 0.1787377286451886, '1 bar': 0.38413806725162286 };
  const DELAY_TIMES = { '1/16': '1/16', '1/8': '1/8', '1/8 dotted': '1/8 dotted', '1/4': '1/4', '1/4 dotted': '1/4 dotted', '1/2': '1/2' };
  const LFO_TARGETS = { cutoff: 'Filter cutoff', wtpos: 'Wavetable position', pitch: 'Pitch (vibrato)', volume: 'Volume (pump)' };
  const LFO_SHAPES = { triangle: 'Triangle', ramp: 'Ramp down' };
  const EQ_LOW_CUTS = { 0: 'Off', 60: '60 Hz', 100: '100 Hz', 150: '150 Hz', 250: '250 Hz' };
  const MACRO_NAMES = ['Brightness', 'Grit', 'Space', 'Width'];

  const OSC_DEFAULT = { wave: 'saw', octave: 0, semi: 0, fine: 0, unison: 1, detune: 0.2, width: 100, level: OSC_LEVEL_DEFAULT, pan: 0 };
  const MODULE_DEFAULTS = {
    oscA: OSC_DEFAULT,
    oscB: Object.assign({ on: false }, OSC_DEFAULT),
    sub: { on: false, level: 0.6, octave: -1, shape: 'kSine' },
    noise: { on: false, level: 0.2, sample: 'air' },
    filter: { on: false, type: 'MgL12', cutoff: 2000, reso: 10, drive: 0, keytrack: 0 },
    ampEnv: { attack: 0.002, hold: 0, decay: 1.0, sustain: 1.0, release: 0.12 },
    filterEnv: { on: false, amount: 30, attack: 0.001, decay: 0.4, sustain: 0.2, release: 0.3 },
    lfo: { on: false, target: 'cutoff', rate: '1/4', amount: 25, shape: 'triangle' },
    dist: { on: false, mode: 'kTube', drive: 40, wet: 50 },
    chorus: { on: false, rate: 0.6, depth: 6, wet: 35 },
    flanger: { on: false, rate: 0.2, depth: 50, feedback: 50, wet: 35 },
    phaser: { on: false, rate: 0.3, depth: 50, feedback: 50, wet: 40 },
    hyper: { on: false, wet: 35, detune: 35, size: 30 },
    eq: { on: false, bass: 0, treble: 0, lowCut: 0 },
    comp: { on: false, amount: 40 },
    delay: { on: false, time: '1/8 dotted', feedback: 35, pingpong: true, wet: 20 },
    reverb: { on: false, type: 'kPlate', size: 45, wet: 20 },
  };
  const FX_KEYS = ['dist', 'chorus', 'flanger', 'phaser', 'hyper', 'eq', 'comp', 'delay', 'reverb'];

  // Older designs mark a module as on by its presence (or `true`); newer ones use `on`.
  function isOn(m) { return m === true || (!!m && typeof m === 'object' && m.on !== false); }
  const obj = (m) => (m && typeof m === 'object' ? m : {});

  function normOsc(o, def) {
    o = obj(o);
    return {
      wave: WAVES.includes(o.wave) ? o.wave : def.wave,
      octave: clamp(Math.round(num(o.octave, def.octave)), -4, 4),
      semi: clamp(Math.round(num(o.semi, def.semi)), -12, 12),
      fine: clamp(num(o.fine, def.fine), -100, 100),
      unison: clamp(Math.round(num(o.unison, def.unison)), 1, 16),
      detune: clamp(num(o.detune, def.detune), 0, 1),
      width: clamp(num(o.width, def.width), 0, 100),
      level: clamp(num(o.level, def.level), 0, 1),
      pan: clamp(num(o.pan, def.pan), -50, 50),
    };
  }

  function normFx(raw) {
    const fx = obj(raw), D = MODULE_DEFAULTS, out = {};
    const pct = (v, def) => clamp(num(v, def), 0, 100);
    const m = (k) => obj(fx[k]);
    out.dist = { on: isOn(fx.dist), mode: pick(m('dist').mode, DIST_MODES, D.dist.mode), drive: pct(m('dist').drive, D.dist.drive), wet: pct(m('dist').wet, D.dist.wet) };
    out.chorus = { on: isOn(fx.chorus), rate: clamp(num(m('chorus').rate, D.chorus.rate), 0.01, 20), depth: clamp(num(m('chorus').depth, D.chorus.depth), 0, 26), wet: pct(m('chorus').wet, D.chorus.wet) };
    for (const k of ['flanger', 'phaser']) {
      out[k] = { on: isOn(fx[k]), rate: clamp(num(m(k).rate, D[k].rate), 0.01, 20), depth: pct(m(k).depth, D[k].depth), feedback: pct(m(k).feedback, D[k].feedback), wet: pct(m(k).wet, D[k].wet) };
    }
    out.hyper = { on: isOn(fx.hyper), wet: pct(m('hyper').wet, D.hyper.wet), detune: pct(m('hyper').detune, D.hyper.detune), size: pct(m('hyper').size, D.hyper.size) };
    out.eq = {
      on: isOn(fx.eq), bass: clamp(num(m('eq').bass, D.eq.bass), -12, 12), treble: clamp(num(m('eq').treble, D.eq.treble), -12, 12),
      lowCut: Object.prototype.hasOwnProperty.call(EQ_LOW_CUTS, String(m('eq').lowCut)) ? Number(m('eq').lowCut) : D.eq.lowCut,
    };
    out.comp = { on: isOn(fx.comp), amount: pct(m('comp').amount, D.comp.amount) };
    out.delay = {
      on: isOn(fx.delay), time: pick(m('delay').time, DELAY_TIMES, D.delay.time), feedback: clamp(num(m('delay').feedback, D.delay.feedback), 0, 95),
      pingpong: typeof m('delay').pingpong === 'boolean' ? m('delay').pingpong : D.delay.pingpong, wet: pct(m('delay').wet, D.delay.wet),
    };
    out.reverb = { on: isOn(fx.reverb), type: pick(m('reverb').type, REVERB_TYPES, D.reverb.type), size: pct(m('reverb').size, D.reverb.size), wet: pct(m('reverb').wet, D.reverb.wet) };
    return out;
  }

  // Fills in and clamps every field, so the editor, the preview and the file
  // writer can rely on the full shape whatever produced the design.
  function normalizeDesign(raw) {
    raw = obj(raw);
    const D = MODULE_DEFAULTS, env = obj(raw.ampEnv), fe = obj(raw.filterEnv), lfo = obj(raw.lfo);
    const category = BASES[raw.category] ? raw.category : 'lead';
    const sub = obj(raw.sub), noise = obj(raw.noise), filter = obj(raw.filter);
    const d = {
      name: String(raw.name || 'Text Preset').replace(/[\\/:*?"<>|]/g, '').trim().slice(0, 60) || 'Text Preset',
      description: String(raw.description || '').slice(0, 300),
      category,
      mono: !!raw.mono,
      legato: !!raw.legato,
      glide: clamp(num(raw.glide, 0), 0, 2),
      polyCount: clamp(Math.round(num(raw.polyCount, 16)), 1, 32),
      gainDb: clamp(num(raw.gainDb, 0), -12, 12),
      macros: raw.macros !== false,
      oscA: normOsc(raw.oscA, D.oscA),
      oscB: Object.assign({ on: isOn(raw.oscB) }, normOsc(raw.oscB, D.oscB)),
      sub: {
        on: isOn(raw.sub), level: clamp(num(sub.level, D.sub.level), 0, 1),
        octave: clamp(Math.round(num(sub.octave, D.sub.octave)), -3, 3), shape: pick(sub.shape, SUB_SHAPES, D.sub.shape),
      },
      noise: { on: isOn(raw.noise), level: clamp(num(noise.level, D.noise.level), 0, 1), sample: NOISE_SAMPLES[noise.sample] ? noise.sample : D.noise.sample },
      filter: {
        on: isOn(raw.filter), type: pick(filter.type, FILTER_TYPES, D.filter.type),
        cutoff: clamp(Math.round(num(filter.cutoff, D.filter.cutoff)), 20, 20000),
        reso: clamp(num(filter.reso, D.filter.reso), 0, 100), drive: clamp(num(filter.drive, D.filter.drive), 0, 100),
        keytrack: clamp(num(filter.keytrack, D.filter.keytrack), 0, 100),
      },
      ampEnv: {
        attack: clamp(num(env.attack, D.ampEnv.attack), 0, 32), hold: clamp(num(env.hold, D.ampEnv.hold), 0, 32),
        decay: clamp(num(env.decay, D.ampEnv.decay), 0.001, 32), sustain: clamp(num(env.sustain, D.ampEnv.sustain), 0, 1),
        release: clamp(num(env.release, D.ampEnv.release), 0.001, 32),
      },
      filterEnv: {
        on: isOn(raw.filterEnv), amount: clamp(num(fe.amount, D.filterEnv.amount), -100, 100),
        attack: clamp(num(fe.attack, D.filterEnv.attack), 0, 32), decay: clamp(num(fe.decay, D.filterEnv.decay), 0.001, 32),
        sustain: clamp(num(fe.sustain, D.filterEnv.sustain), 0, 1), release: clamp(num(fe.release, D.filterEnv.release), 0.001, 32),
      },
      lfo: {
        on: isOn(raw.lfo), target: pick(lfo.target, LFO_TARGETS, D.lfo.target), rate: pick(lfo.rate, LFO_DIVISIONS, D.lfo.rate),
        amount: clamp(num(lfo.amount, D.lfo.amount), -100, 100), shape: pick(lfo.shape, LFO_SHAPES, D.lfo.shape),
      },
      fx: normFx(raw.fx),
    };
    if (raw.author) d.author = String(raw.author).slice(0, 60);
    d.tags = [BASES[category].tag, 'Wavetable', d.mono ? 'Mono' : 'Poly'];
    tidy(d);
    return d;
  }

  // Rounds to the precision a person would set by hand.
  function tidy(d) {
    const r = (v, places) => Math.round(v * Math.pow(10, places)) / Math.pow(10, places);
    for (const o of [d.oscA, d.oscB]) { o.detune = r(o.detune, 3); o.level = r(o.level, 3); o.pan = r(o.pan, 1); o.fine = r(o.fine, 1); o.width = r(o.width, 1); }
    d.sub.level = r(d.sub.level, 3);
    d.noise.level = r(d.noise.level, 3);
    for (const k of ['reso', 'drive', 'keytrack']) d.filter[k] = r(d.filter[k], 1);
    for (const e of [d.ampEnv, d.filterEnv]) for (const k of ['attack', 'hold', 'decay', 'release']) if (k in e) e[k] = r(e[k], 4);
    d.ampEnv.sustain = r(d.ampEnv.sustain, 3);
    d.filterEnv.sustain = r(d.filterEnv.sustain, 3);
    d.filterEnv.amount = r(d.filterEnv.amount, 1);
    d.lfo.amount = r(d.lfo.amount, 1);
    d.glide = r(d.glide, 3);
    d.gainDb = r(d.gainDb, 1);
    for (const m of Object.values(d.fx)) for (const k of ['wet', 'drive', 'size', 'depth', 'feedback', 'detune', 'amount', 'bass', 'treble']) if (k in m) m[k] = r(m[k], 1);
    for (const k of ['chorus', 'flanger', 'phaser']) d.fx[k].rate = r(d.fx[k].rate, 3);
  }

  /* ---------------------------------------------------- design -> preset */

  // Filter cutoff is stored 0..1 on a log scale from 8 Hz to 22.05 kHz (0.5 = 425 Hz, Serum's default).
  const F_MIN = 8.19, F_MAX = 22050;
  function hzToNorm(hz) { return clamp(Math.log(hz / F_MIN) / Math.log(F_MAX / F_MIN), 0, 1); }

  // Basic Shapes has 7 frames; the position knob runs 1..256 across them.
  function wavePos(wave) {
    const frame = WAVE_FRAME[wave] !== undefined ? WAVE_FRAME[wave] : 1;
    return 1 + frame * 255 / 6;
  }

  // Serum writes every map with its keys in byte order.
  function sortKeys(v) {
    if (Array.isArray(v)) return v.map(sortKeys);
    if (!v || typeof v !== 'object') return v;
    const out = {};
    for (const k of Object.keys(v).sort()) out[k] = sortKeys(v[k]);
    return out;
  }

  function oscParams(o, enable, trim) {
    const p = {};
    if (enable) p.kParamEnable = 1.0;
    if (o.octave) p.kParamOctave = o.octave;
    if (o.semi) p.kParamPitch = o.semi;
    if (o.fine) p.kParamFine = o.fine;
    if (o.pan) p.kParamPan = o.pan;
    if (o.unison > 1) {
      p.kParamUnison = o.unison;
      p.kParamDetune = o.detune;
      if (o.width !== 100) p.kParamDetuneWid = o.width;
    }
    const level = o.level * trim;
    if (Math.abs(level - OSC_LEVEL_DEFAULT) > 1e-9) p.kParamVolume = level;
    return Object.keys(p).length ? p : 'default';
  }

  function setWavetableOsc(state, idx, o, trim) {
    const osc = state['Oscillator' + idx];
    const wt = osc['WTOsc' + idx];
    wt.relativePathToWT = BASIC_SHAPES.path;
    wt.numFrames = BASIC_SHAPES.numFrames;
    wt.flex = JSON.parse(JSON.stringify(FLAT_CURVE));
    wt.plainParams = { kParamTablePos: wavePos(o.wave) };
    osc.plainParams = oscParams(o, idx !== 0, trim);
  }

  // LFO shapes as Serum 2 stores them (the triangle is copied from a saved preset).
  const LFO_CURVES = {
    triangle: { curveVals: [0.4999999701976776, 0.4999999701976776, 0.4999999701976776], loopbackPointNum: 65, numPoints: 2, xVals: [0.0, 0.5, 1.0], yVals: [1.0, 0.0, 1.0] },
    ramp: { curveVals: [0.4999999701976776, 0.4999999701976776], loopbackPointNum: 65, numPoints: 1, xVals: [0.0, 1.0], yVals: [1.0, 0.0] },
  };

  // Modulation sources (Serum 2 menu ids) and destination parameter ids.
  const SRC = { env2: 3, lfo1: 6, macro: [25, 26, 27, 28] };
  const DEST = {
    cutoff: ['VoiceFilter', 'kParamFreq', 3],
    oscVolume: ['Oscillator', 'kParamVolume', 1],
    oscPitch: ['Oscillator', 'kParamPitch', 4],
    tablePos: ['WTOsc', 'kParamTablePos', 6],
    fxWet: 'kParamWet', // local id 1 on every effect
  };

  function buildState(design) {
    const d = normalizeDesign(design);
    const s = JSON.parse(JSON.stringify(INIT));
    s.presetName = d.name;
    s.presetAuthor = d.author || 'Serum 2 Preset Maker';
    s.presetDescription = d.description;
    s.tags = d.tags;

    // A negative output trim lowers every source (amplitude follows knob^2).
    const trim = d.gainDb < 0 ? Math.pow(10, d.gainDb / 40) : 1;
    setWavetableOsc(s, 0, d.oscA, trim);
    if (d.oscB.on) setWavetableOsc(s, 1, d.oscB, trim);
    if (d.sub.on) {
      s.Oscillator4.plainParams = { kParamEnable: 1.0, kParamVolume: d.sub.level * trim };
      if (d.sub.octave) s.Oscillator4.plainParams.kParamOctave = d.sub.octave;
      if (d.sub.shape !== 'kSine') s.Oscillator4.SubOsc4.plainParams = { kParamShape: d.sub.shape };
    }
    if (d.noise.on) {
      const n = s.Oscillator3.NoiseOsc3, smp = NOISE_SAMPLES[d.noise.sample];
      n.relativePathToNoiseSample = smp.path;
      n.numFrames = smp.numFrames;
      s.Oscillator3.plainParams = { kParamEnable: 1.0, kParamVolume: d.noise.level * trim };
    }

    if (d.filter.on) {
      const f = d.filter, p = { kParamEnable: 1.0, kParamType: f.type, kParamFreq: hzToNorm(f.cutoff), kParamReso: f.reso };
      if (f.drive) p.kParamDrive = f.drive;
      if (f.keytrack) p.kParamKeyTrack = f.keytrack;
      s.VoiceFilter0.plainParams = p;
      // Send A, B, C and noise through the filter; the sub stays direct.
      for (let i = 0; i < 4; i++) s['RoutingSlot' + i].plainParams = { kParamRoutingDest: 'kRoutingDestFilter' };
      if (d.sub.on) s.RoutingSlot4.plainParams = { kParamRoutingDest: 'kRoutingDestDirect' };
    }

    const envParams = (e) => ({ kParamAttack: e.attack, kParamHold: e.hold || 0, kParamDecay: e.decay, kParamSustain: e.sustain, kParamRelease: e.release });
    s.Env0.plainParams = Object.assign({}, s.Env0.plainParams, envParams(d.ampEnv));

    const g = d.mono ? { kParamMonoToggle: 1.0, kParamLimitSameNotePolyphony: 1.0 } : { kParamPolyCount: d.polyCount };
    if (d.mono && d.legato) g.kParamLegato = 1.0;
    if (d.glide > 0) g.kParamPortamentoTime = d.glide;
    s.Global0.plainParams = g;

    // Modulation matrix.
    const mods = [];
    const mod = (source, dest, moduleId, amount, bipolar) => {
      const pp = { kParamAmount: amount };
      if (bipolar) pp.kParamBipolar = 1.0;
      mods.push({ destModuleID: moduleId, destModuleParamID: dest[2], destModuleParamName: dest[1], destModuleTypeString: dest[0], plainParams: pp, source: [source, 0] });
    };
    if (d.filter.on && d.filterEnv.on && d.filterEnv.amount) {
      s.Env1.plainParams = Object.assign({}, s.Env1.plainParams, envParams(d.filterEnv));
      mod(SRC.env2, DEST.cutoff, 0, d.filterEnv.amount);
    }
    const lfo = d.lfo;
    if (lfo.on && lfo.amount && (lfo.target !== 'cutoff' || d.filter.on)) {
      s.LFO0.curveData = JSON.parse(JSON.stringify(LFO_CURVES[lfo.shape]));
      s.LFO0.plainParams = { kParamRate: lfoRate(lfo.rate) };
      const both = (dest, bipolar) => { mod(SRC.lfo1, dest, 0, lfo.amount, bipolar); if (d.oscB.on) mod(SRC.lfo1, dest, 1, lfo.amount, bipolar); };
      if (lfo.target === 'cutoff') mod(SRC.lfo1, DEST.cutoff, 0, lfo.amount);
      else if (lfo.target === 'wtpos') both(DEST.tablePos);
      else if (lfo.target === 'pitch') both(DEST.oscPitch, true);
      else both(DEST.oscVolume);
    }

    // Effects. Modules a macro needs are added at 0% mix when they are off.
    const f = d.fx, macro = d.macros;
    const need = {
      dist: f.dist.on || macro, chorus: f.chorus.on, flanger: f.flanger.on, phaser: f.phaser.on, hyper: f.hyper.on || macro,
      eq: f.eq.on, comp: f.comp.on, delay: f.delay.on || macro, reverb: f.reverb.on || macro,
    };
    const rack = [], pos = {};
    const add = (key, name, type, params, extra) => {
      pos[key] = rack.length;
      rack.push(Object.assign({ [name]: { plainParams: params }, type }, extra || {}));
    };
    const wet = (k) => (f[k].on ? f[k].wet : 0);
    if (need.dist) add('dist', 'FXDistortion', 0, { kParamMode: f.dist.mode, kParamDrive: f.dist.drive, kParamWet: wet('dist') },
      { flex: [JSON.parse(JSON.stringify(FLAT_CURVE)), JSON.parse(JSON.stringify(FLAT_CURVE))] });
    if (need.chorus) add('chorus', 'FXChorus', 3, { kParamBeatSync: 0.0, kParamRate: f.chorus.rate, kParamDepth: f.chorus.depth, kParamWet: f.chorus.wet });
    if (need.flanger) add('flanger', 'FXFlanger', 1, { kParamBeatSync: 0.0, kParamRate: f.flanger.rate, kParamDepth: f.flanger.depth, kParamFeedback: f.flanger.feedback, kParamWet: f.flanger.wet });
    if (need.phaser) add('phaser', 'FXPhaser', 2, { kParamBeatSync: 0.0, kParamRate: f.phaser.rate, kParamDepth: f.phaser.depth, kParamFeedback: f.phaser.feedback, kParamWet: f.phaser.wet });
    if (need.hyper) add('hyper', 'FXHyperD', 9, { kParamRate: 40.0, kParamUnison: 4.0, kParamDetune: f.hyper.detune, kParamDimESize: f.hyper.size, kParamDimEWet: f.hyper.on ? f.hyper.size : 0, kParamWet: wet('hyper') });
    if (need.eq) {
      const e = f.eq;
      const low = e.lowCut ? { kParamType1: 2.0, kParamFreq1: e.lowCut } : { kParamType1: 0.0, kParamFreq1: 150.0, kParamGain1: e.bass };
      add('eq', 'FXEQ', 7, Object.assign(low, { kParamType2: 0.0, kParamFreq2: 5000.0, kParamGain2: e.treble }));
    }
    // "Punch": an OTT-style multiband compressor (upward and downward) with some makeup.
    if (need.comp) add('comp', 'FXComp', 5, { kParamMultiband: 1.0, kParamThresh: 0.6, kParamRatio: 4.0, kParamRatioBelow: 0.5, kParamAttack: 8.0, kParamRelease: 90.0, kParamMakeup: 1.6, kParamWet: f.comp.amount });
    if (need.delay) {
      const dl = f.delay, dotted = / dotted$/.test(dl.time), t = DELAY_SYNC[dl.time.replace(/ dotted$/, '')];
      const p = { kParamTimeL: t, kParamTimeR: t, kParamLink: 1.0, kParamFeedback: dl.feedback, kParamWet: wet('delay') };
      if (dotted) { p.kParamOffsetL = 1.5; p.kParamOffsetR = 1.5; }
      if (dl.pingpong) p.kParamMode = 1.0;
      add('delay', 'FXDelay', 4, p);
    }
    if (need.reverb) add('reverb', 'FXReverb', 6, { kParamType: f.reverb.type, kParamSize: f.reverb.size, kParamFreq: 20.0, kParamWet: wet('reverb') });
    // Output: a compressor at 1:1 with a 0 dB threshold is a clean gain stage.
    if (d.gainDb > 0.05) add('output', 'FXComp', 5, { kParamThresh: 0.0, kParamRatio: 1.0, kParamMakeup: Math.pow(10, d.gainDb / 20), kParamWet: 100.0 });
    s.FXRack0.FX = rack;

    // Macros 1-4: Brightness, Grit, Space, Width.
    if (macro) {
      const used = [false, false, false, false];
      if (d.filter.on) { mod(SRC.macro[0], DEST.cutoff, 0, 35); used[0] = true; }
      mod(SRC.macro[1], ['FXDistortion', DEST.fxWet, 1], pos.dist, Math.min(70, 100 - wet('dist'))); used[1] = true;
      mod(SRC.macro[2], ['FXReverb', DEST.fxWet, 1], pos.reverb, Math.min(40, 100 - wet('reverb')));
      mod(SRC.macro[2], ['FXDelay', DEST.fxWet, 1], pos.delay, Math.min(30, 100 - wet('delay'))); used[2] = true;
      mod(SRC.macro[3], ['FXHyperD', DEST.fxWet, 1], pos.hyper, Math.min(60, 100 - wet('hyper'))); used[3] = true;
      used.forEach((u, i) => { if (u) s['Macro' + i].name = MACRO_NAMES[i]; });
    }
    mods.slice(0, 64).forEach((m, i) => { s['ModSlot' + i] = m; });

    return sortKeys(s);
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
  const NEIGHBOUR_RATES = { '4 bar': ['2 bar'], '2 bar': ['4 bar', '1 bar'], '1 bar': ['2 bar', '1/2'], '1/2': ['1 bar', '1/4'], '1/4': ['1/2', '1/8'], '1/8': ['1/4', '1/16'], '1/16': ['1/8', '1/32'], '1/32': ['1/16'] };

  // A new design near `design`. amount 0..1 sets how far it may move.
  function varyDesign(design, opts) {
    opts = opts || {};
    const amount = clamp(num(opts.amount, 0.5), 0, 1);
    const r = rng(num(opts.seed, Date.now()));
    const choose = (list) => list[Math.floor(r() * list.length)];
    const spread = (v, rel) => v * Math.pow(2, (r() * 2 - 1) * rel * amount); // multiplicative
    const nudge = (v, by) => v + (r() * 2 - 1) * by * amount;                    // additive
    const chance = (p) => r() < p * amount;
    const d = normalizeDesign(JSON.parse(JSON.stringify(design)));

    for (const o of [d.oscA, d.oscB]) {
      if (chance(0.35)) o.wave = choose(RELATED_WAVES[o.wave]);
      if (o.unison > 1) {
        o.unison = clamp(Math.round(nudge(o.unison, 2)), 2, 16);
        o.detune = clamp(spread(o.detune, 0.8), 0.02, 0.9);
      }
    }
    if (d.oscB.on) {
      if (chance(0.3)) d.oscB.octave = clamp(d.oscB.octave + choose([-1, 1]), -2, 2);
      d.oscB.level = clamp(nudge(d.oscB.level, 0.12), 0.3, 1);
    }
    if (d.sub.on) d.sub.level = clamp(nudge(d.sub.level, 0.12), 0.3, 1);
    if (chance(0.15)) d.noise.on = !d.noise.on;
    if (d.filter.on) {
      d.filter.cutoff = clamp(Math.round(spread(d.filter.cutoff, 1)), 200, 18000);
      d.filter.reso = clamp(nudge(d.filter.reso, 20), 0, 85);
      d.filter.drive = clamp(nudge(d.filter.drive, 20), 0, 80);
      if (d.filterEnv.on) {
        d.filterEnv.amount = clamp(nudge(d.filterEnv.amount, 20), 5, 80);
        d.filterEnv.decay = clamp(spread(d.filterEnv.decay, 1), 0.05, 4);
      }
    }
    const e = d.ampEnv;
    e.attack = clamp(e.attack < 0.01 ? e.attack : spread(e.attack, 1), 0.0005, 10);
    e.decay = clamp(spread(e.decay, 1), 0.02, 10);
    e.release = clamp(spread(e.release, 1), 0.01, 10);
    if (e.sustain > 0 && e.sustain < 1) e.sustain = clamp(nudge(e.sustain, 0.2), 0, 1);
    if (d.lfo.on) {
      d.lfo.amount = clamp(nudge(d.lfo.amount, 15), d.lfo.amount < 0 ? -90 : 2, d.lfo.amount < 0 ? -2 : 90);
      if (chance(0.3)) d.lfo.rate = choose(NEIGHBOUR_RATES[d.lfo.rate]);
    }
    const f = d.fx;
    if (f.hyper.on) f.hyper.wet = clamp(nudge(f.hyper.wet, 20), 5, 90);
    if (chance(0.2)) f.dist.on = !f.dist.on;
    if (f.dist.on) f.dist.drive = clamp(nudge(f.dist.drive, 20), 10, 95);
    if (f.reverb.on) { f.reverb.wet = clamp(nudge(f.reverb.wet, 10), 5, 60); f.reverb.size = clamp(nudge(f.reverb.size, 15), 10, 95); }
    if (f.delay.on) f.delay.wet = clamp(nudge(f.delay.wet, 8), 5, 45);
    if (f.chorus.on) f.chorus.wet = clamp(nudge(f.chorus.wet, 15), 10, 80);

    if (opts.name) d.name = opts.name;
    return normalizeDesign(d);
  }

  /* ---------------------------------------------------- text -> design */

  const has = (t, ...words) => words.some((w) => new RegExp('\\b' + w + '\\b', 'i').test(t));

  // Starting points per sound type, voiced bright and full: oscillator levels
  // near Serum's default, filters opened up and shaped by an envelope instead
  // of being parked low, and the usual effects for each kind of sound.
  const BASES = {
    bass: { tag: 'Bass', mono: true, oscA: { wave: 'saw', octave: -1, unison: 2, detune: 0.12, level: 0.8 }, sub: { level: 0.7, octave: -1 },
      filter: { type: 'MgL24', cutoff: 1800, reso: 18, drive: 20 }, filterEnv: { amount: 28, attack: 0.001, decay: 0.35, sustain: 0.35, release: 0.2 },
      ampEnv: { attack: 0.002, decay: 1.0, sustain: 1.0, release: 0.1 }, fx: { comp: { amount: 40 } } },
    sub: { tag: 'Bass', mono: true, legato: true, glide: 0.06, oscA: { wave: 'sine', octave: -1, level: 0.85 },
      ampEnv: { attack: 0.002, decay: 2.5, sustain: 1.0, release: 0.35 }, fx: { dist: { mode: 'kTube', drive: 25, wet: 45 } } },
    reese: { tag: 'Bass', mono: true, oscA: { wave: 'saw', octave: -1, unison: 4, detune: 0.2, level: 0.8 },
      oscB: { wave: 'saw', octave: -1, unison: 4, detune: 0.3, level: 0.7 }, sub: { level: 0.6, octave: -1 },
      filter: { type: 'MgL24', cutoff: 1400, reso: 15, drive: 30 }, lfo: { target: 'cutoff', rate: '2 bar', amount: 10 },
      ampEnv: { attack: 0.003, decay: 1.0, sustain: 1.0, release: 0.12 }, fx: { dist: { mode: 'kOverdrive', drive: 35, wet: 45 }, comp: { amount: 50 } } },
    lead: { tag: 'Lead', mono: true, legato: true, glide: 0.04, oscA: { wave: 'saw', unison: 5, detune: 0.18, level: 0.8 },
      oscB: { wave: 'square', octave: 1, level: 0.5 }, filter: { type: 'MgL12', cutoff: 7000, reso: 15 },
      filterEnv: { amount: 15, attack: 0.001, decay: 0.5, sustain: 0.5, release: 0.3 }, ampEnv: { attack: 0.003, decay: 1.0, sustain: 1.0, release: 0.2 },
      fx: { delay: { time: '1/8 dotted', feedback: 35, wet: 18 }, reverb: { type: 'kPlate', size: 40, wet: 15 }, comp: { amount: 30 } } },
    supersaw: { tag: 'Lead', oscA: { wave: 'saw', unison: 9, detune: 0.3, level: 0.8 }, oscB: { wave: 'saw', octave: 1, unison: 7, detune: 0.25, level: 0.55 },
      ampEnv: { attack: 0.005, decay: 1.0, sustain: 1.0, release: 0.4 },
      fx: { hyper: { wet: 35 }, delay: { time: '1/8', feedback: 30, wet: 12 }, reverb: { type: 'kHall', size: 55, wet: 22 }, comp: { amount: 35 } } },
    pad: { tag: 'Pad', oscA: { wave: 'saw', unison: 6, detune: 0.22, level: 0.7 }, oscB: { wave: 'triangle', octave: 1, unison: 4, detune: 0.18, level: 0.55 },
      filter: { type: 'MgL12', cutoff: 3500, reso: 12 }, lfo: { target: 'cutoff', rate: '2 bar', amount: 12 },
      ampEnv: { attack: 0.9, decay: 2.0, sustain: 0.85, release: 2.5 },
      fx: { chorus: { rate: 0.4, depth: 6, wet: 35 }, hyper: { wet: 30 }, reverb: { type: 'kHall', size: 70, wet: 35 } } },
    pluck: { tag: 'Pluck', oscA: { wave: 'saw', unison: 3, detune: 0.15, level: 0.8 }, oscB: { wave: 'square', octave: 1, level: 0.45 },
      filter: { type: 'MgL12', cutoff: 900, reso: 20 }, filterEnv: { amount: 55, attack: 0, decay: 0.28, sustain: 0, release: 0.25 },
      ampEnv: { attack: 0.001, decay: 0.45, sustain: 0.0, release: 0.3 },
      fx: { delay: { time: '1/8 dotted', feedback: 35, wet: 22 }, reverb: { type: 'kPlate', size: 45, wet: 20 }, comp: { amount: 35 } } },
    keys: { tag: 'Keys', oscA: { wave: 'triangle', unison: 2, detune: 0.08, level: 0.8 }, oscB: { wave: 'sine', octave: 1, level: 0.5 },
      filter: { type: 'MgL12', cutoff: 2500, reso: 8, keytrack: 50 }, filterEnv: { amount: 25, attack: 0.001, decay: 0.8, sustain: 0.2, release: 0.5 },
      ampEnv: { attack: 0.002, decay: 1.4, sustain: 0.45, release: 0.6 }, fx: { chorus: { rate: 0.5, depth: 5, wet: 30 }, reverb: { type: 'kPlate', size: 45, wet: 20 } } },
    bell: { tag: 'Keys', oscA: { wave: 'sine', level: 0.8 }, oscB: { wave: 'triangle', octave: 2, level: 0.5 },
      ampEnv: { attack: 0.001, decay: 1.8, sustain: 0.0, release: 1.6 },
      fx: { delay: { time: '1/4', feedback: 30, wet: 15 }, reverb: { type: 'kHall', size: 60, wet: 30 } } },
    stab: { tag: 'Synth', oscA: { wave: 'saw', unison: 5, detune: 0.2, level: 0.8 }, oscB: { wave: 'square', level: 0.5 },
      filter: { type: 'MgL12', cutoff: 2500, reso: 15 }, filterEnv: { amount: 40, attack: 0, decay: 0.2, sustain: 0.1, release: 0.2 },
      ampEnv: { attack: 0.001, decay: 0.3, sustain: 0.0, release: 0.2 }, fx: { reverb: { type: 'kPlate', size: 40, wet: 22 }, comp: { amount: 40 } } },
    arp: { tag: 'Synth', oscA: { wave: 'square', unison: 2, detune: 0.1, level: 0.8 }, oscB: { wave: 'saw', octave: 1, level: 0.45 },
      filter: { type: 'MgL12', cutoff: 1500, reso: 25 }, filterEnv: { amount: 45, attack: 0, decay: 0.22, sustain: 0.1, release: 0.2 },
      ampEnv: { attack: 0.001, decay: 0.35, sustain: 0.25, release: 0.2 },
      fx: { delay: { time: '1/8 dotted', feedback: 35, wet: 22 }, reverb: { type: 'kPlate', size: 40, wet: 15 } } },
  };

  // Genre and style words: a sound type to start from (if the text names none) and changes on top.
  const GENRES = [
    { words: ['dubstep', 'riddim', 'brostep'], base: 'bass', apply: (d, n) => { wobble(d, '1/8', 40); dirt(d, 'kOverdrive', 60, 70); d.fx.comp = { amount: 60 }; n.push('Dubstep: 1/8 wobble, heavy drive and OTT-style punch'); } },
    { words: ['future bass', 'futurebass'], base: 'supersaw', apply: (d, n) => { pump(d); d.ampEnv.attack = 0.01; n.push('Future bass: wide supersaw with a 1/4 volume pump'); } },
    { words: ['trap', 'drill'], base: 'sub', apply: (d, n) => { d.glide = 0.08; d.mono = true; d.legato = true; n.push('Trap: 808-style glide'); } },
    { words: ['house', 'deep house', 'tech house'], base: 'stab', apply: (d, n) => { d.fx.comp = { amount: 45 }; n.push('House: punchy, short and compressed'); } },
    { words: ['techno'], base: 'arp', apply: (d, n) => { filterOn(d, 1500).reso = 45; d.fx.dist = { mode: 'kTapeSat', drive: 35, wet: 40 }; n.push('Techno: resonant and saturated'); } },
    { words: ['trance', 'uplifting'], base: 'supersaw', apply: (d, n) => { d.fx.delay = { time: '1/8 dotted', feedback: 40, wet: 18 }; n.push('Trance: supersaw with dotted delay'); } },
    { words: ['synthwave', 'retrowave', '80s', 'eighties', 'outrun'], base: 'lead', apply: (d, n) => { d.fx.chorus = { rate: 0.5, depth: 8, wet: 45 }; d.fx.reverb = { type: 'kHall', size: 60, wet: 25 }; n.push('Synthwave: chorus and a big hall'); } },
    { words: ['dnb', 'drum and bass', 'neuro', 'neurofunk', 'jungle'], base: 'reese', apply: (d, n) => { wobble(d, '1/4', 20); dirt(d, 'kDiode2', 55, 60); n.push('Drum and bass: moving, driven reese'); } },
    { words: ['hardstyle', 'hardcore', 'rawstyle'], base: 'lead', apply: (d, n) => { dirt(d, 'kHardClip', 70, 80); d.oscA.unison = 7; d.oscA.detune = 0.3; n.push('Hardstyle: hard-clipped, wide lead'); } },
    { words: ['ambient', 'cinematic', 'soundscape'], base: 'pad', apply: (d, n) => { d.ampEnv.attack = 2; d.ampEnv.release = 4; d.fx.reverb = { type: 'kHall', size: 90, wet: 45 }; n.push('Ambient: slow swell and a long hall'); } },
    { words: ['lofi', 'lo-fi', 'chill', 'chillhop'], base: 'keys', apply: (d, n) => { filterOn(d, 1800).cutoff = Math.min(d.filter.cutoff, 1800); d.fx.dist = { mode: 'kTapeSat', drive: 30, wet: 35 }; d.fx.chorus = { rate: 0.3, depth: 9, wet: 40 }; n.push('Lo-fi: dark, tape-saturated and wobbly'); } },
    { words: ['edm', 'big room', 'festival'], base: 'supersaw', apply: (d, n) => { d.fx.comp = { amount: 60 }; d.fx.reverb = { type: 'kHall', size: 65, wet: 25 }; n.push('Big room: loud and wide'); } },
  ];

  function filterOn(d, cutoff) {
    if (!d.filter || d.filter.on === false) d.filter = { type: 'MgL12', cutoff, reso: 10 };
    d.filter.on = true;
    return d.filter;
  }
  function wobble(d, rate, amount) {
    if (!d.filter || d.filter.on === false) d.filter = { type: 'MgL24', cutoff: 500, reso: 25, drive: 25 };
    d.filter.on = true;
    d.filter.cutoff = Math.min(d.filter.cutoff, 700);
    d.lfo = { target: 'cutoff', rate, amount, shape: 'triangle' };
  }
  function dirt(d, mode, drive, wet) { d.fx.dist = { mode, drive, wet }; }
  function pump(d) { d.lfo = { target: 'volume', rate: '1/4', amount: -45, shape: 'ramp' }; }

  function detectBase(t) {
    if (has(t, 'future\\s?bass')) return 'supersaw';
    if (has(t, 'drum\\s?and\\s?bass')) return 'reese';
    if (has(t, 'reese')) return 'reese';
    if (has(t, 'supersaw', 'super saw', 'hoover')) return 'supersaw';
    if (has(t, '808', 'sub ?bass', 'subbass')) return 'sub';
    if (has(t, 'bass', 'bassline', 'wobble', 'growl', 'wub')) return 'bass';
    if (has(t, 'pad', 'pads', 'atmosphere', 'drone', 'texture')) return 'pad';
    if (has(t, 'pluck', 'plucks', 'plucky', 'pizzicato')) return 'pluck';
    if (has(t, 'bell', 'bells', 'chime', 'mallet')) return 'bell';
    if (has(t, 'keys', 'piano', 'rhodes', 'organ', 'ep')) return 'keys';
    if (has(t, 'stab', 'stabs', 'chord', 'chords')) return 'stab';
    if (has(t, 'arp', 'arpeggio', 'sequence')) return 'arp';
    if (has(t, 'lead', 'leads', 'solo', 'melody')) return 'lead';
    for (const g of GENRES) if (has(t, ...g.words.map((w) => w.replace(/ /g, '\\s?')))) return g.base;
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
    const t = ' ' + String(text).toLowerCase().replace(/[^a-z0-9.%/\s-]/g, ' ') + ' ';
    const notes = [];
    const baseName = detectBase(t);
    const d = JSON.parse(JSON.stringify(BASES[baseName]));
    d.fx = d.fx || {};
    notes.push('Sound type: ' + baseName);
    const isBass = ['bass', 'sub', 'reese'].includes(baseName);
    const ensureFilter = (cutoff) => filterOn(d, cutoff);

    // Waveform
    for (const w of ['sine', 'triangle', 'square', 'saw', 'pulse']) {
      const re = w === 'saw' ? 'saw|sawtooth' : w === 'triangle' ? 'triangle|tri' : w;
      if (has(t, re) && !(w === 'saw' && baseName === 'supersaw')) { d.oscA.wave = w; notes.push('Main wave: ' + w); break; }
    }

    // Genre and style
    for (const g of GENRES) if (has(t, ...g.words.map((w) => w.replace(/ /g, '\\s?')))) { g.apply(d, notes); break; }

    // Brightness
    if (d.filter && d.filter.on !== false) {
      if (has(t, 'dark', 'muffled', 'deep', 'underwater', 'mellow')) { d.filter.cutoff *= 0.55; notes.push('Darker: lower cutoff'); }
      if (has(t, 'warm', 'soft', 'smooth', 'round')) { d.filter.cutoff *= 0.75; d.fx.dist = d.fx.dist || { mode: 'kTube', drive: 20, wet: 30 }; notes.push('Warmer: softer top and a little tube'); }
      if (has(t, 'bright', 'airy', 'crisp', 'shiny', 'sharp', 'open')) { d.filter.cutoff *= 1.8; if (d.filterEnv) d.filterEnv.amount += 10; notes.push('Brighter: filter opened up'); }
    } else if (has(t, 'dark', 'muffled', 'mellow')) {
      ensureFilter(1800); notes.push('Added a low-pass filter for a darker tone');
    }
    if (has(t, 'bright', 'airy', 'crisp', 'shiny')) { d.fx.eq = { treble: 3, bass: 0, lowCut: isBass ? 0 : 100 }; }
    if (has(t, 'resonant', 'squelchy', 'acid', 'squelch', '303')) {
      ensureFilter(500);
      d.filter.type = 'MgL24'; d.filter.reso = 70; d.filter.cutoff = Math.min(d.filter.cutoff, 600);
      d.filterEnv = { amount: 50, attack: 0, decay: 0.25, sustain: 0.1, release: 0.2 };
      if (has(t, 'acid', '303')) { d.mono = true; d.legato = true; d.glide = 0.06; d.fx.dist = d.fx.dist || { mode: 'kOverdrive', drive: 40, wet: 50 }; }
      notes.push('Resonant filter with an envelope sweep');
    }

    // Width / detune
    if (has(t, 'wide', 'huge', 'massive', 'big', 'stereo', 'lush', 'thick', 'fat')) {
      d.oscA.unison = Math.max(d.oscA.unison || 1, isBass ? 3 : 7);
      d.oscA.detune = Math.max(d.oscA.detune || 0, 0.25);
      if (!isBass) d.fx.hyper = { wet: Math.max(d.fx.hyper ? d.fx.hyper.wet : 0, 40) };
      notes.push('Wider: more unison voices and detune');
    }
    if (has(t, 'detuned', 'detune')) { d.oscA.unison = Math.max(d.oscA.unison || 1, 5); d.oscA.detune = 0.35; notes.push('Extra detune'); }
    if (has(t, 'thin', 'narrow')) { d.oscA.unison = 1; if (d.oscB) d.oscB.unison = 1; delete d.fx.hyper; notes.push('Narrow: unison off'); }

    // Movement
    if (has(t, 'wobble', 'wobbly', 'wub', 'wubs')) { wobble(d, has(t, 'slow') ? '1/4' : '1/8', 40); notes.push('Wobble: LFO on the filter cutoff'); }
    if (has(t, 'evolving', 'moving', 'movement', 'animated', 'sweeping')) { ensureFilter(2500); d.lfo = { target: 'cutoff', rate: '2 bar', amount: 15, shape: 'triangle' }; notes.push('Slow filter movement'); }
    if (has(t, 'vibrato')) { d.lfo = { target: 'pitch', rate: '1/16', amount: 1.2, shape: 'triangle' }; notes.push('Vibrato'); }
    if (has(t, 'tremolo')) { d.lfo = { target: 'volume', rate: '1/8', amount: -35, shape: 'triangle' }; notes.push('Tremolo'); }
    if (has(t, 'pump', 'pumping', 'sidechain', 'side chain', 'ducking')) { pump(d); notes.push('Sidechain-style volume pump on every beat'); }

    // Dirt and punch
    if (has(t, 'distorted', 'distortion', 'dirty', 'gritty', 'grit', 'aggressive', 'growl', 'angry', 'heavy', 'saturated', 'saturation', 'crunchy', 'drive', 'overdrive')) {
      const hard = has(t, 'aggressive', 'heavy', 'angry', 'growl', 'distorted', 'distortion');
      dirt(d, hard ? 'kOverdrive' : 'kTube', hard ? 65 : 40, hard ? 70 : 50);
      if (has(t, 'growl')) { ensureFilter(900); d.lfo = { target: 'cutoff', rate: '1/16', amount: 30, shape: 'triangle' }; }
      notes.push(hard ? 'Heavy distortion' : 'Light saturation');
    }
    if (has(t, 'bitcrush', 'bitcrushed', 'crushed', 'downsampled')) { dirt(d, 'kDownsample', 40, 45); notes.push('Bit-crushed (downsample)'); }
    if (has(t, 'clean', 'pure')) { delete d.fx.dist; notes.push('Clean: no distortion'); }
    if (has(t, 'punchy', 'punch', 'compressed', 'tight', 'ott', 'loud')) { d.fx.comp = { amount: Math.max(d.fx.comp ? d.fx.comp.amount : 0, 55) }; notes.push('OTT-style punch'); }

    // Effects by name
    if (has(t, 'chorus', 'chorused')) { d.fx.chorus = { rate: 0.6, depth: 7, wet: 45 }; notes.push('Chorus'); }
    if (has(t, 'phaser', 'phased', 'phasing')) { d.fx.phaser = { rate: 0.3, depth: 60, feedback: 55, wet: 45 }; notes.push('Phaser'); }
    if (has(t, 'flanger', 'flanged', 'flanging', 'jet')) { d.fx.flanger = { rate: 0.2, depth: 60, feedback: 60, wet: 40 }; notes.push('Flanger'); }
    if (has(t, 'dimension', 'hyper')) { d.fx.hyper = { wet: 50 }; notes.push('Hyper/Dimension width'); }
    if (has(t, 'reverb', 'reverby', 'hall', 'spacey', 'spacious', 'roomy', 'dreamy', 'ethereal', 'wet')) {
      const big = has(t, 'huge', 'hall', 'spacey', 'ethereal', 'dreamy', 'massive');
      d.fx.reverb = { type: big ? 'kHall' : 'kPlate', size: big ? 75 : 50, wet: big ? 40 : 28 };
      notes.push(big ? 'Big hall reverb' : 'Plate reverb');
    }
    if (has(t, 'dry', 'no reverb')) { delete d.fx.reverb; delete d.fx.delay; notes.push('Dry: no reverb or delay'); }
    if (has(t, 'delay', 'echo', 'echoes')) { d.fx.delay = d.fx.delay || { time: '1/8 dotted', feedback: 40, wet: 25 }; notes.push('Delay'); }
    if (has(t, 'vintage')) { if (d.fx.reverb) d.fx.reverb.type = 'kVintage'; }

    // Layers
    if (has(t, 'sub', 'subby', 'deep', 'weight', 'heavy') && baseName !== 'sub' && !d.sub) { d.sub = { level: 0.65, octave: -1 }; notes.push('Added sub oscillator'); }
    if (has(t, 'no sub')) { delete d.sub; notes.push('Sub oscillator off'); }
    if (has(t, 'noise', 'noisy', 'breath', 'breathy', 'air', 'airy', 'hiss')) { d.noise = { level: 0.25, sample: 'air' }; notes.push('Added a noise layer'); }
    if (has(t, 'single', 'simple', 'one oscillator', 'one osc')) { delete d.oscB; notes.push('Single oscillator'); }
    if (has(t, 'fifth', 'fifths', 'power')) { d.oscB = Object.assign({ wave: d.oscA.wave, level: 0.6 }, d.oscB || {}, { semi: 7, octave: 0 }); notes.push('Osc B a fifth up'); }

    // Envelope words
    const e = d.ampEnv;
    if (has(t, 'slow attack', 'swell', 'swelling', 'fade in', 'soft attack', 'evolving')) { e.attack = Math.max(e.attack, 0.9); notes.push('Slow attack'); }
    if (has(t, 'snappy', 'percussive', 'short', 'staccato', 'tight')) { e.decay = Math.min(e.decay, 0.25); e.sustain = Math.min(e.sustain, 0.1); e.release = Math.min(e.release, 0.12); notes.push('Short, snappy envelope'); }
    if (has(t, 'long', 'sustained', 'held')) { e.release = Math.max(e.release, 1.5); notes.push('Long release'); }

    // Pitch / voicing
    if (has(t, 'high', 'higher')) { d.oscA.octave = (d.oscA.octave || 0) + 1; notes.push('Up one octave'); }
    if (has(t, 'low(?!\\s?pass)', 'lower') && !isBass) { d.oscA.octave = (d.oscA.octave || 0) - 1; notes.push('Down one octave'); }
    if (has(t, 'poly', 'polyphonic', 'chords?')) { d.mono = false; }
    if (has(t, 'mono', 'monophonic', 'legato')) { d.mono = true; d.legato = true; }
    if (has(t, 'glide', 'portamento', 'slide', 'sliding')) { d.mono = true; d.legato = true; d.glide = Math.max(d.glide || 0, 0.08); notes.push('Glide between notes'); }

    // Exact numbers override the words
    const atk = readTime(t, 'attack'); if (atk !== null) { e.attack = atk; notes.push('Attack ' + atk + ' s'); }
    const dec = readTime(t, 'decay'); if (dec !== null) { e.decay = dec; notes.push('Decay ' + dec + ' s'); }
    const rel = readTime(t, 'release'); if (rel !== null) { e.release = rel; notes.push('Release ' + rel + ' s'); }
    const sus = readNumber(t, 'sustain', '%'); if (sus) { e.sustain = clamp(sus.v > 1 ? sus.v / 100 : sus.v, 0, 1); notes.push('Sustain ' + Math.round(e.sustain * 100) + '%'); }
    const cut = readNumber(t, '(?:cutoff|filter)', 'khz|hz|k'); if (cut) {
      const hz = cut.unit === 'khz' || cut.unit === 'k' ? cut.v * 1000 : cut.v;
      ensureFilter(hz); d.filter.cutoff = hz; notes.push('Cutoff ' + Math.round(hz) + ' Hz');
    }
    const res = readNumber(t, '(?:resonance|reso|res)', '%'); if (res) { ensureFilter(2000); d.filter.reso = clamp(res.v, 0, 100); notes.push('Resonance ' + d.filter.reso); }
    const uni = readNumber(t, '(?:unison|voices)', ''); if (uni) { d.oscA.unison = clamp(uni.v, 1, 16); d.oscA.detune = d.oscA.detune || 0.25; notes.push('Unison ' + d.oscA.unison); }
    const oct = readNumber(t, 'octave', ''); if (oct) { d.oscA.octave = clamp(oct.v, -4, 4); notes.push('Octave ' + d.oscA.octave); }
    const gl = readTime(t, '(?:glide|portamento)'); if (gl !== null) { d.glide = gl; d.mono = true; d.legato = true; notes.push('Glide ' + gl + ' s'); }
    const rv = readNumber(t, 'reverb', '%'); if (rv) { d.fx.reverb = Object.assign({ type: 'kPlate', size: 50 }, d.fx.reverb, { wet: clamp(rv.v, 0, 100) }); notes.push('Reverb ' + rv.v + '%'); }
    const dl = t.match(/delay\s*(?:of|at|=|:)?\s*(1\/(?:16|8|4|2))(\s*dotted)?/); if (dl) { d.fx.delay = Object.assign({ feedback: 35, wet: 22 }, d.fx.delay, { time: dl[1] + (dl[2] ? ' dotted' : '') }); notes.push('Delay ' + dl[1] + (dl[2] ? ' dotted' : '')); }

    // Modules mentioned above become "on"; filter env and LFO stay off unless set.
    if (d.filter) d.filter.cutoff = clamp(Math.round(d.filter.cutoff), 20, 20000);
    if (d.filterEnv) d.filterEnv.on = true;
    if (d.lfo) d.lfo.on = true;
    e.sustain = clamp(e.sustain, 0, 1);

    // Name: text in quotes, "called X" / "named X", otherwise the first words.
    const q = String(text).match(/["“']([^"”']{2,40})["”']/) || String(text).match(/\b(?:called|named|name)\s*:?\s+([\w -]{2,40})/i);
    const FILLER = /^(a|an|the|with|and|of|for|in|on|some|very|really|that|is|but|plus|make|me|i|want|like)$/i;
    d.name = (q ? q[1] : String(text).replace(/[^\w\s-]/g, ' ').trim().split(/\s+/).filter((w) => !FILLER.test(w)).slice(0, 4).join(' ')).trim() || 'Text Preset';
    d.name = d.name.replace(/\b\w/g, (c) => c.toUpperCase());
    d.description = String(text).trim().slice(0, 300);
    d.category = baseName;
    if (opts.author) d.author = opts.author;
    return { design: normalizeDesign(d), notes };
  }

  function makePreset(text, opts) {
    const { design, notes } = textToDesign(text, opts);
    return Object.assign({ design, notes }, presetFile(design));
  }

  // A random sound: a random type, then a strong variation of it.
  function randomDesign(seed) {
    const r = rng(seed >>> 0);
    const types = Object.keys(BASES);
    const type = types[Math.floor(r() * types.length)];
    const adjectives = ['Neon', 'Velvet', 'Glass', 'Rust', 'Silk', 'Static', 'Amber', 'Hollow', 'Prism', 'Ember', 'Frost', 'Echo'];
    const nouns = { bass: 'Bass', sub: '808', reese: 'Reese', lead: 'Lead', supersaw: 'Saws', pad: 'Pad', pluck: 'Pluck', keys: 'Keys', bell: 'Bell', stab: 'Stab', arp: 'Arp' };
    const name = adjectives[Math.floor(r() * adjectives.length)] + ' ' + nouns[type];
    const start = textToDesign(type).design;
    const d = varyDesign(start, { seed: Math.floor(r() * 1e9), amount: 0.9, name });
    d.description = 'Random ' + type;
    return { design: d, notes: ['Random ' + type + ' sound. Use "Make variations" for more like it.'] };
  }

  /* ------------------------------------------------- AI (Claude) -> design */

  // What Claude fills in. Kept flat with every field required so it works as a
  // structured-output schema; fromAi() maps it onto a design and clamps it.
  const S = {
    obj: (props) => ({ type: 'object', additionalProperties: false, required: Object.keys(props), properties: props }),
    num: { type: 'number' }, int: { type: 'integer' }, bool: { type: 'boolean' }, str: { type: 'string' },
    en: (values) => ({ type: 'string', enum: values }),
  };
  const AI_OSC = (withEnabled) => S.obj(Object.assign(withEnabled ? { enabled: S.bool } : {}, {
    wave: S.en(WAVES), octave: S.int, semitones: S.int, unison: S.int, detune: S.num, level: S.num,
  }));
  const AI_DESIGN_SCHEMA = S.obj({
    name: S.str,
    category: S.en(['bass', 'sub', 'reese', 'lead', 'supersaw', 'pad', 'pluck', 'keys', 'bell', 'stab', 'arp']),
    mono: S.bool,
    glideMs: S.num,
    oscA: AI_OSC(false),
    oscB: AI_OSC(true),
    sub: S.obj({ enabled: S.bool, level: S.num, octave: S.int }),
    noise: S.obj({ enabled: S.bool, level: S.num }),
    filter: S.obj({ enabled: S.bool, type: S.en(Object.keys(FILTER_TYPES)), cutoffHz: S.num, resonance: S.num, drive: S.num }),
    ampEnv: S.obj({ attackMs: S.num, decayMs: S.num, sustainPercent: S.num, releaseMs: S.num }),
    filterEnv: S.obj({ amount: S.num, attackMs: S.num, decayMs: S.num, sustainPercent: S.num, releaseMs: S.num }),
    lfo: S.obj({ target: S.en(['none'].concat(Object.keys(LFO_TARGETS))), rate: S.en(Object.keys(LFO_DIVISIONS)), shape: S.en(Object.keys(LFO_SHAPES)), amount: S.num }),
    fx: S.obj({
      distortionMode: S.en(Object.keys(DIST_MODES)), distortionDrive: S.num, distortionMix: S.num,
      chorusMix: S.num, phaserMix: S.num, flangerMix: S.num, hyperMix: S.num,
      eqBassDb: S.num, eqTrebleDb: S.num, punch: S.num,
      delayTime: S.en(Object.keys(DELAY_TIMES)), delayFeedback: S.num, delayMix: S.num,
      reverbType: S.en(Object.keys(REVERB_TYPES)), reverbSize: S.num, reverbMix: S.num,
    }),
    notes: { type: 'array', items: S.str },
  });
  const AI_PACK_SCHEMA = S.obj({ presets: { type: 'array', items: AI_DESIGN_SCHEMA } });

  const AI_EXAMPLE = {
    name: 'Grit Reese', category: 'reese', mono: true, glideMs: 0,
    oscA: { wave: 'saw', octave: -1, semitones: 0, unison: 4, detune: 0.2, level: 0.8 },
    oscB: { enabled: true, wave: 'saw', octave: -1, semitones: 0, unison: 4, detune: 0.3, level: 0.7 },
    sub: { enabled: true, level: 0.6, octave: -1 }, noise: { enabled: false, level: 0.2 },
    filter: { enabled: true, type: 'MgL24', cutoffHz: 1400, resonance: 15, drive: 30 },
    ampEnv: { attackMs: 3, decayMs: 1000, sustainPercent: 100, releaseMs: 120 },
    filterEnv: { amount: 0, attackMs: 1, decayMs: 300, sustainPercent: 20, releaseMs: 200 },
    lfo: { target: 'cutoff', rate: '2 bar', shape: 'triangle', amount: 10 },
    fx: {
      distortionMode: 'kOverdrive', distortionDrive: 35, distortionMix: 45, chorusMix: 0, phaserMix: 0, flangerMix: 0, hyperMix: 0,
      eqBassDb: 0, eqTrebleDb: 0, punch: 50, delayTime: '1/8 dotted', delayFeedback: 35, delayMix: 0, reverbType: 'kPlate', reverbSize: 40, reverbMix: 0,
    },
    notes: ['Two detuned saws an octave down give the reese its movement.', 'A slow LFO sweeps the filter; use the Brightness macro to open it further.'],
  };

  function aiInstructions() {
    return [
      'You design presets for the Xfer Serum 2 synthesizer. Turn the sound description into settings for the fixed preset template below.',
      'Aim for a finished, mix-ready, loud and bright sound like a commercial preset, not a dull starting point.',
      'Everything the template does not set stays at Serum 2\'s Init value. Every preset also gets four macros (Brightness, Grit, Space, Width).',
      '',
      'Fields:',
      '- name: a short preset name, 2 to 4 words, no quotes. If the description gives a name, use it.',
      '- category: the closest of bass, sub (808 / sub bass), reese, lead, supersaw, pad, pluck, keys, bell, stab, arp.',
      '- mono: true for basses and most leads; false for pads, chords, keys and plucks. glideMs: portamento time, 0 for none (808s and acid lines use 40-120).',
      '- oscA (always on) and oscB (enabled true/false): wave sine, triangle, saw, square, pulse or narrowpulse. octave -3 to 3 relative to the played note',
      '  (basses usually -1). semitones -12 to 12 (7 = a fifth). unison 1 to 16 voices. detune 0 to 1 (0.1 subtle, 0.25 normal, 0.5 very wide).',
      '  level 0 to 1 is Serum\'s level knob (0.75 default, amplitude is level squared, so 0.5 is already 7 dB quieter; keep layers above 0.45).',
      '- sub: a sine sub oscillator. enabled, level 0 to 1, octave -2 to 0 relative to the played note.',
      '- noise: a noise layer for air or texture. enabled, level 0 to 1 (0.15 to 0.35 is typical).',
      '- filter: one filter on oscA, oscB and noise (the sub bypasses it). enabled, type (MgL12/18/24 = Moog-style low-pass, L12 low-pass,',
      '  H12/H24 high-pass, DirtyMg = saturated Moog, CombP = comb), cutoffHz 20 to 20000, resonance 0 to 100, drive 0 to 100.',
      '  Avoid parking a low-pass cutoff low: for darkness use 800-2000 Hz with a filter envelope rather than 300 Hz.',
      '- ampEnv: the volume envelope. attackMs, decayMs, sustainPercent 0 to 100, releaseMs.',
      '- filterEnv: an envelope on the filter cutoff. amount -100 to 100 (0 is off; plucks 40-60, basses 20-35), attackMs, decayMs, sustainPercent, releaseMs.',
      '- lfo: target none, cutoff (wobble), wtpos (wavetable movement), pitch (vibrato: amount 1-2), volume (tremolo or sidechain pump: negative amount with',
      '  shape ramp at 1/4). rate is tempo-synced. amount -100 to 100.',
      '- fx: distortionMode (kTube, kSoftClip, kHardClip, kDiode1, kDiode2, kOverdrive, kTapeSat, kSoftSat, kAsym, kSinFold, kLinFold, kDownsample, kRectify),',
      '  distortionDrive and distortionMix 0 to 100 (mix 0 is off); chorusMix, phaserMix, flangerMix, hyperMix (stereo width) 0 to 100 (0 is off);',
      '  eqBassDb and eqTrebleDb -12 to 12; punch 0 to 100 (an OTT-style multiband compressor that adds loudness and presence; most modern sounds use 30-60);',
      '  delayTime, delayFeedback 0 to 95, delayMix 0 to 100 (0 is off); reverbType kPlate, kHall or kVintage, reverbSize and reverbMix 0 to 100 (0 is off).',
      '- notes: 2 to 5 short sentences for the musician: what you chose and why, and anything the template cannot do.',
      '',
      'The template cannot do other wavetables, warp modes, more than one LFO or envelope routing, or the Bode, Convolve, Splitter and Utility effects.',
      'When the description asks for one of these, get as close as the fields allow and say in notes what to set in Serum 2 afterwards.',
    ].join('\n');
  }

  function aiSinglePrompt(text) {
    return aiInstructions() + '\n\nReply with only one JSON object in exactly this shape (the values here are just an example):\n'
      + JSON.stringify(AI_EXAMPLE) + '\n\nSound description:\n' + String(text).slice(0, 2000);
  }

  function aiPackPrompt(lines) {
    const list = lines.map((l, i) => (i + 1) + '. ' + String(l).slice(0, 500)).join('\n');
    return aiInstructions() + '\n\nDesign one preset for each numbered description below, in the same order, with distinct names.'
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
    const o = (v) => (v && typeof v === 'object' ? v : null);
    const ms = (v, fallbackSeconds) => num(v, fallbackSeconds * 1000) / 1000;

    if (typeof ai.name === 'string' && ai.name.trim()) d.name = ai.name.trim().slice(0, 40);
    if (BASES[ai.category]) d.category = ai.category;
    if (typeof ai.mono === 'boolean') { d.mono = ai.mono; d.legato = ai.mono; }
    if (ai.glideMs !== undefined) d.glide = ms(ai.glideMs, d.glide);
    const osc = (target, src) => {
      if (!o(src)) return;
      if (typeof src.enabled === 'boolean') target.on = src.enabled;
      for (const k of ['wave', 'octave', 'unison', 'detune', 'level']) if (src[k] !== undefined) target[k] = src[k];
      if (src.semitones !== undefined) target.semi = src.semitones;
    };
    osc(d.oscA, ai.oscA);
    osc(d.oscB, ai.oscB);
    for (const k of ['sub', 'noise']) {
      const src = o(ai[k]);
      if (!src) continue;
      if (typeof src.enabled === 'boolean') d[k].on = src.enabled;
      if (src.level !== undefined) d[k].level = src.level;
      if (k === 'sub' && src.octave !== undefined) d.sub.octave = src.octave;
    }
    const f = o(ai.filter);
    if (f) {
      if (typeof f.enabled === 'boolean') d.filter.on = f.enabled;
      if (f.type !== undefined) d.filter.type = f.type;
      d.filter.cutoff = num(f.cutoffHz, d.filter.cutoff);
      d.filter.reso = num(f.resonance, d.filter.reso);
      d.filter.drive = num(f.drive, d.filter.drive);
    }
    const e = o(ai.ampEnv);
    if (e) {
      d.ampEnv.attack = ms(e.attackMs, d.ampEnv.attack);
      d.ampEnv.decay = ms(e.decayMs, d.ampEnv.decay);
      d.ampEnv.sustain = num(e.sustainPercent, d.ampEnv.sustain * 100) / 100;
      d.ampEnv.release = ms(e.releaseMs, d.ampEnv.release);
    }
    const fe = o(ai.filterEnv);
    if (fe) {
      const amount = num(fe.amount, 0);
      d.filterEnv = {
        on: amount !== 0, amount: amount || d.filterEnv.amount,
        attack: ms(fe.attackMs, d.filterEnv.attack), decay: ms(fe.decayMs, d.filterEnv.decay),
        sustain: num(fe.sustainPercent, d.filterEnv.sustain * 100) / 100, release: ms(fe.releaseMs, d.filterEnv.release),
      };
    }
    const l = o(ai.lfo);
    if (l) {
      const on = l.target !== 'none' && num(l.amount, 0) !== 0;
      d.lfo = { on, target: l.target === 'none' ? d.lfo.target : l.target, rate: l.rate, shape: l.shape, amount: on ? num(l.amount, 0) : d.lfo.amount };
    }
    const x = o(ai.fx);
    if (x) {
      const mix = (key, value, extra) => {
        const v = num(value, -1);
        if (v < 0) return;
        d.fx[key] = Object.assign({}, d.fx[key], extra || {}, { on: v > 0 }, v > 0 ? { wet: v } : {});
      };
      mix('dist', x.distortionMix, { mode: x.distortionMode, drive: num(x.distortionDrive, d.fx.dist.drive) });
      mix('chorus', x.chorusMix);
      mix('phaser', x.phaserMix);
      mix('flanger', x.flangerMix);
      mix('hyper', x.hyperMix);
      const bass = num(x.eqBassDb, 0), treble = num(x.eqTrebleDb, 0);
      d.fx.eq = Object.assign({}, d.fx.eq, { on: bass !== 0 || treble !== 0, bass, treble });
      const punch = num(x.punch, -1);
      if (punch >= 0) d.fx.comp = { on: punch > 0, amount: punch > 0 ? punch : d.fx.comp.amount };
      mix('delay', x.delayMix, { time: x.delayTime, feedback: num(x.delayFeedback, d.fx.delay.feedback) });
      mix('reverb', x.reverbMix, { type: x.reverbType, size: num(x.reverbSize, d.fx.reverb.size) });
    }
    if (Array.isArray(ai.notes)) for (const n of ai.notes.slice(0, 6)) if (typeof n === 'string' && n.trim()) notes.push(n.trim().slice(0, 300));
    const design = normalizeDesign(d);
    design.description = base.description;
    return { design, notes };
  }

  const api = {
    makePreset, textToDesign, normalizeDesign, varyDesign, randomDesign, presetFile, fileNameFor,
    buildPreset, buildState, packPreset, cborEncode, zstdRawFrame, md5Hex, hzToNorm, lfoRate,
    aiSinglePrompt, aiPackPrompt, aiInstructions, fromAi, AI_DESIGN_SCHEMA, AI_PACK_SCHEMA,
    WAVES, CATEGORIES: Object.keys(BASES), MODULE_DEFAULTS, FX_KEYS, MACRO_NAMES,
    FILTER_TYPES, SUB_SHAPES, DIST_MODES, REVERB_TYPES, LFO_DIVISIONS, LFO_TARGETS, LFO_SHAPES, DELAY_TIMES, EQ_LOW_CUTS,
    INIT,
  };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.SerumPresetMaker = api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
