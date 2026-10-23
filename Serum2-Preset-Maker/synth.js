/*
 * Serum 2 Preset Maker - preview synth
 *
 * A Web Audio synth that plays a design so you can hear roughly how the preset
 * will sound before saving it, and that measures its loudness for level
 * matching. It follows Serum's level laws (level knobs and Env 1 sustain are
 * quadratic in amplitude; a plain saw at Init level sits near -17 dBFS RMS),
 * the filter with its envelope, keytracking and LFO, glide and the effects. It
 * is still an approximation: Serum 2's oscillators, filters and effects sound
 * richer.
 */
(function (root) {
  'use strict';

  const midiToHz = (m) => 440 * Math.pow(2, (m - 69) / 12);
  const dbToGain = (db) => Math.pow(10, db / 20);
  const NATIVE = { sine: 'sine', triangle: 'triangle', saw: 'sawtooth', square: 'square' };
  const PULSE_WIDTH = { pulse: 0.25, narrowpulse: 0.125 };
  const SUB_WAVE = { kSine: 'sine', kTriangle: 'triangle', kSaw: 'saw', kSquare: 'square', kRoundRect: 'square', kPulse: 'pulse' };
  const MAX_VOICES = 16;
  const TEMPO = 120;
  // Serum's cutoff knob spans 8 Hz..22.05 kHz: one full knob turn is 11.43 octaves.
  const KNOB_CENTS = 1200 * Math.log2(22050 / 8.19);
  // Scales the oscillators so a saw at Init level renders near Serum's -17.4 dBFS RMS.
  const MASTER = 0.415;
  const BEATS = { '4 bar': 16, '2 bar': 8, '1 bar': 4, '1/2': 2, '1/4': 1, '1/8': 0.5, '1/16': 0.25, '1/32': 0.125 };
  const DELAY_BEATS = { '1/16': 0.25, '1/8': 0.5, '1/8 dotted': 0.75, '1/4': 1, '1/4 dotted': 1.5, '1/2': 2 };

  // Short phrases per sound type: [midi notes, start beat, length in beats].
  const DEMOS = {
    bass: { bpm: 124, notes: [[[36], 0, 0.45], [[36], 0.5, 0.45], [[48], 1, 0.3], [[36], 1.5, 0.45], [[39], 2, 0.45], [[36], 2.5, 0.45], [[43], 3, 0.45], [[41], 3.5, 0.45]] },
    sub: { bpm: 140, notes: [[[36], 0, 1.4], [[36], 1.5, 0.4], [[43], 2, 0.9], [[41], 3, 0.45], [[39], 3.5, 0.45]] },
    pad: { bpm: 80, notes: [[[48, 55, 60, 64], 0, 3.5], [[45, 52, 57, 60], 4, 3.5]] },
    keys: { bpm: 100, notes: [[[48, 55, 60, 64], 0, 1.5], [[45, 52, 57, 60], 2, 1.5], [[41, 48, 53, 57], 4, 1.5], [[43, 50, 55, 59], 6, 1.5]] },
    stab: { bpm: 124, notes: [[[60, 63, 67], 0, 0.25], [[60, 63, 67], 0.75, 0.25], [[58, 62, 65], 1.5, 0.25], [[60, 63, 67], 2.5, 0.25], [[63, 67, 70], 3.25, 0.25]] },
    pluck: { bpm: 124, notes: [60, 64, 67, 72, 67, 64, 62, 67, 71, 74, 71, 67].map((n, i) => [[n], i * 0.5, 0.25]) },
    lead: { bpm: 110, notes: [[[72], 0, 0.9], [[74], 1, 0.4], [[75], 1.5, 0.9], [[79], 2.5, 0.4], [[77], 3, 0.9], [[75], 4, 0.4], [[74], 4.5, 1.4]] },
  };
  const DEMO_FOR = { bass: 'bass', sub: 'sub', reese: 'bass', pad: 'pad', keys: 'keys', bell: 'pluck', stab: 'stab', pluck: 'pluck', arp: 'pluck', lead: 'lead', supersaw: 'lead' };
  const MEASURE_NOTE = { bass: 36, sub: 36, reese: 36 };

  function shaperCurve(mode, drive) {
    const n = 2048, curve = new Float32Array(n), k = 1 + (drive / 100) * 24;
    for (let i = 0; i < n; i++) {
      const x = (i / (n - 1)) * 2 - 1, y = x * k;
      let v;
      switch (mode) {
        case 'kHardClip': v = Math.max(-1, Math.min(1, y)); break;
        case 'kSinFold': case 'kLinFold': v = Math.sin(y * Math.PI / 2); break;
        case 'kRectify': v = Math.abs(Math.tanh(y)) * 2 - 1; break;
        case 'kDownsample': { const steps = Math.max(3, 48 - drive * 0.45); v = Math.round(Math.tanh(y) * steps) / steps; break; }
        case 'kTube': case 'kAsym': case 'kDiode1': case 'kDiode2': v = y >= 0 ? Math.tanh(y) : Math.tanh(y * 0.6) / 0.6 * 0.8; break;
        default: v = Math.tanh(y);
      }
      curve[i] = v;
    }
    return curve;
  }

  class PresetSynth {
    constructor(opts) {
      this.ctx = (opts && opts.context) || null;
      this.offline = !!this.ctx;
      // Offline renders (loudness measurement) use fixed "random" phases so they repeat exactly.
      let seed = 12345;
      this.rand = this.offline ? () => ((seed = (seed * 16807) % 2147483647) / 2147483647) : Math.random;
      this.design = null;
      this.voices = new Map(); // note -> voice
      this.timers = [];
      this.waves = {};
      this.lastNote = null;
      if (this.ctx) this.buildBus();
    }

    // Creates the audio context on first use (browsers only allow sound after a click or key press).
    start() {
      if (!this.ctx) {
        const AC = root.AudioContext || root.webkitAudioContext;
        if (!AC) throw new Error('This browser has no Web Audio support.');
        this.ctx = new AC();
        this.buildBus();
        if (this.design) this.applyFx();
      }
      if (!this.offline && this.ctx.state === 'suspended') this.ctx.resume();
      return this.ctx;
    }

    setDesign(design) {
      this.design = design;
      if (this.ctx && this.bus) this.applyFx();
    }

    // A fixed chain with every effect present; applyFx() sets mixes and switches.
    buildBus() {
      const c = this.ctx;
      const g = (v) => { const n = c.createGain(); n.gain.value = v; return n; };
      const lfo = (hz, depth, target) => { const o = c.createOscillator(); o.frequency.value = hz; const d = g(depth); o.connect(d).connect(target); o.start(); return { osc: o, depth: d }; };
      const stage = (input) => { const dry = g(1), wet = g(0), out = g(1); input.connect(dry).connect(out); return { dry, wet, out }; };
      const b = this.bus = { input: g(1) };

      // Distortion.
      b.dist = stage(b.input);
      b.shaper = c.createWaveShaper(); b.shaper.oversample = '4x';
      b.distTrim = g(1);
      b.input.connect(b.shaper).connect(b.distTrim).connect(b.dist.wet).connect(b.dist.out);

      // Chorus: two modulated delays panned apart.
      b.chorus = stage(b.dist.out);
      b.chorusLfos = [];
      for (const [delay, pan, phase] of [[0.005, -0.8, 0], [0.01, 0.8, 0.5]]) {
        const d = c.createDelay(0.1); d.delayTime.value = delay;
        b.chorusLfos.push(lfo(0.5 + phase * 0.1, 0.003, d.delayTime));
        const p = c.createStereoPanner(); p.pan.value = pan;
        b.dist.out.connect(d).connect(p).connect(b.chorus.wet);
      }
      b.chorus.wet.connect(b.chorus.out);

      // Flanger: a short modulated delay with feedback.
      b.flanger = stage(b.chorus.out);
      b.flDelay = c.createDelay(0.05); b.flDelay.delayTime.value = 0.003;
      b.flFb = g(0.5);
      b.flLfo = lfo(0.2, 0.002, b.flDelay.delayTime);
      b.chorus.out.connect(b.flDelay).connect(b.flanger.wet);
      b.flDelay.connect(b.flFb).connect(b.flDelay);
      b.flanger.wet.connect(b.flanger.out);

      // Phaser: six all-pass stages swept by an LFO, with feedback.
      b.phaser = stage(b.flanger.out);
      b.phStages = [];
      let node = b.flanger.out;
      b.phIn = g(1); node.connect(b.phIn); node = b.phIn;
      b.phLfoGain = g(1200);
      b.phLfo = c.createOscillator(); b.phLfo.frequency.value = 0.3; b.phLfo.connect(b.phLfoGain); b.phLfo.start();
      for (let i = 0; i < 6; i++) {
        const ap = c.createBiquadFilter(); ap.type = 'allpass'; ap.frequency.value = 600; ap.Q.value = 0.7;
        b.phLfoGain.connect(ap.detune);
        node.connect(ap); node = ap; b.phStages.push(ap);
      }
      b.phFb = g(0.5); node.connect(b.phFb).connect(b.phIn);
      node.connect(b.phaser.wet).connect(b.phaser.out);

      // Hyper/Dimension stand-in: more modulated delays.
      b.hyper = stage(b.phaser.out);
      for (const [delay, rate, pan] of [[0.012, 0.6, -1], [0.019, 0.83, 1], [0.015, 0.71, 0]]) {
        const d = c.createDelay(0.05); d.delayTime.value = delay;
        lfo(rate, 0.003, d.delayTime);
        const p = c.createStereoPanner(); p.pan.value = pan;
        b.phaser.out.connect(d).connect(p).connect(b.hyper.wet);
      }
      b.hyper.wet.connect(b.hyper.out);

      // EQ: low cut, bass shelf, treble shelf.
      b.lowCut = c.createBiquadFilter(); b.lowCut.type = 'highpass'; b.lowCut.frequency.value = 10; b.lowCut.Q.value = 0.707;
      b.bass = c.createBiquadFilter(); b.bass.type = 'lowshelf'; b.bass.frequency.value = 150;
      b.treble = c.createBiquadFilter(); b.treble.type = 'highshelf'; b.treble.frequency.value = 5000;
      b.hyper.out.connect(b.lowCut).connect(b.bass).connect(b.treble);

      // Punch: compressor with makeup, mixed in parallel.
      b.comp = stage(b.treble);
      b.compressor = c.createDynamicsCompressor();
      b.compressor.threshold.value = -30; b.compressor.knee.value = 12; b.compressor.ratio.value = 4;
      b.compressor.attack.value = 0.008; b.compressor.release.value = 0.09;
      b.compMakeup = g(2.2);
      b.treble.connect(b.compressor).connect(b.compMakeup).connect(b.comp.wet).connect(b.comp.out);

      // Output gain, then delay and reverb as sends.
      b.gain = g(1);
      b.comp.out.connect(b.gain);
      b.master = g(MASTER);
      b.gain.connect(b.master);
      b.delaySend = g(0);
      b.delayL = c.createDelay(4); b.delayR = c.createDelay(4);
      b.delayFb = g(0.35);
      b.delayTone = c.createBiquadFilter(); b.delayTone.type = 'lowpass'; b.delayTone.frequency.value = 4500;
      const merger = c.createChannelMerger(2), toL = g(1), toR = g(1);
      b.gain.connect(b.delaySend).connect(b.delayTone).connect(b.delayL);
      b.delayL.connect(toL).connect(merger, 0, 0);
      b.delayL.connect(b.delayR);
      b.delayR.connect(toR).connect(merger, 0, 1);
      b.delayR.connect(b.delayFb).connect(b.delayTone);
      merger.connect(b.master);
      b.reverbSend = g(0);
      b.reverb = c.createConvolver();
      b.gain.connect(b.reverbSend).connect(b.reverb).connect(b.master);
      b.reverbKey = null;

      // Safety limiter and an analyser for the scope.
      const limiter = c.createDynamicsCompressor();
      limiter.threshold.value = -1; limiter.knee.value = 0; limiter.ratio.value = 20; limiter.attack.value = 0.001; limiter.release.value = 0.1;
      b.analyser = c.createAnalyser(); b.analyser.fftSize = 2048;
      b.master.connect(limiter).connect(b.analyser).connect(c.destination);
    }

    applyFx() {
      const f = this.design.fx, b = this.bus, c = this.ctx, t = c.currentTime;
      const set = (param, v) => { param.cancelScheduledValues(t); param.setValueAtTime(v, t); };
      const mix = (st, on, wet) => { set(st.wet.gain, on ? wet / 100 : 0); set(st.dry.gain, on ? 1 - wet / 200 : 1); };

      b.shaper.curve = shaperCurve(f.dist.mode, f.dist.drive);
      set(b.distTrim.gain, 1 / (1 + f.dist.drive / 120));
      mix(b.dist, f.dist.on, f.dist.wet * 1.0); set(b.dist.dry.gain, f.dist.on ? 1 - f.dist.wet / 100 : 1);

      for (const l of b.chorusLfos) { set(l.osc.frequency, f.chorus.rate); set(l.depth.gain, f.chorus.depth / 2000); }
      mix(b.chorus, f.chorus.on, f.chorus.wet);

      set(b.flLfo.osc.frequency, f.flanger.rate); set(b.flLfo.depth.gain, 0.0005 + f.flanger.depth / 100 * 0.0025);
      set(b.flFb.gain, f.flanger.feedback / 100 * 0.85);
      mix(b.flanger, f.flanger.on, f.flanger.wet);

      set(b.phLfo.frequency, f.phaser.rate); set(b.phLfoGain.gain, 300 + f.phaser.depth * 24);
      set(b.phFb.gain, f.phaser.feedback / 100 * 0.7);
      mix(b.phaser, f.phaser.on, f.phaser.wet);

      mix(b.hyper, f.hyper.on, f.hyper.wet * 0.9);

      set(b.lowCut.frequency, f.eq.on && f.eq.lowCut ? f.eq.lowCut : 10);
      set(b.bass.gain, f.eq.on && !f.eq.lowCut ? f.eq.bass : 0);
      set(b.treble.gain, f.eq.on ? f.eq.treble : 0);

      set(b.comp.wet.gain, f.comp.on ? f.comp.amount / 100 : 0);
      set(b.comp.dry.gain, f.comp.on ? 1 - f.comp.amount / 100 : 1);

      set(b.gain.gain, dbToGain(this.design.gainDb || 0));

      const beat = 60 / TEMPO, dt = DELAY_BEATS[f.delay.time] * beat;
      set(b.delayL.delayTime, dt); set(b.delayR.delayTime, f.delay.pingpong ? dt : 0.0001);
      set(b.delayFb.gain, f.delay.feedback / 100);
      set(b.delaySend.gain, f.delay.on ? f.delay.wet / 100 : 0);

      set(b.reverbSend.gain, f.reverb.on ? (f.reverb.wet / 100) * 1.3 : 0);
      const key = f.reverb.type + Math.round(f.reverb.size / 10);
      if (f.reverb.on && b.reverbKey !== key) {
        b.reverbKey = key;
        const base = { kPlate: 0.5, kHall: 1.0, kVintage: 0.7 }[f.reverb.type] || 0.6;
        b.reverb.buffer = this.impulse(base + (f.reverb.size / 100) * 3.5, f.reverb.type === 'kVintage' ? 2.5 : 1.6);
      }
    }

    impulse(seconds, darkness) {
      const c = this.ctx, len = Math.max(1, Math.floor(c.sampleRate * seconds)), buf = c.createBuffer(2, len, c.sampleRate);
      for (let ch = 0; ch < 2; ch++) {
        const data = buf.getChannelData(ch);
        let lp = 0;
        for (let i = 0; i < len; i++) {
          lp += ((Math.random() * 2 - 1) - lp) / darkness;
          data[i] = lp * Math.pow(1 - i / len, 3);
        }
      }
      return buf;
    }

    noiseBuffer() {
      if (!this.noise) {
        const c = this.ctx, len = c.sampleRate * 2, buf = c.createBuffer(1, len, c.sampleRate), data = buf.getChannelData(0);
        for (let i = 0; i < len; i++) data[i] = Math.random() * 2 - 1;
        this.noise = buf;
      }
      return this.noise;
    }

    setWave(osc, wave) {
      if (NATIVE[wave]) { osc.type = NATIVE[wave]; return; }
      if (!this.waves[wave]) {
        const n = 64, width = PULSE_WIDTH[wave] || 0.25, real = new Float32Array(n), imag = new Float32Array(n);
        for (let k = 1; k < n; k++) real[k] = (2 / (k * Math.PI)) * Math.sin(k * Math.PI * width);
        this.waves[wave] = this.ctx.createPeriodicWave(real, imag);
      }
      osc.setPeriodicWave(this.waves[wave]);
    }

    // A 0..1 LFO signal in Serum's shapes (triangle 1->0->1, ramp 1->0), restarted at `t`.
    lfoSignal(shape, rate, t) {
      const c = this.ctx, osc = c.createOscillator();
      osc.type = shape === 'ramp' ? 'sawtooth' : 'triangle';
      osc.frequency.value = (TEMPO / 60) / BEATS[rate];
      const scale = c.createGain(); scale.gain.value = shape === 'ramp' ? -0.5 : 0.5;
      const offset = c.createConstantSource(); offset.offset.value = 0.5;
      const sum = c.createGain();
      osc.connect(scale).connect(sum);
      offset.connect(sum);
      osc.start(t); offset.start(t);
      return { out: sum, sources: [osc, offset] };
    }

    noteOn(note, velocity, when) {
      if (!this.design) return null;
      const c = this.start(), d = this.design;
      const t = when === undefined ? c.currentTime + 0.005 : when;
      velocity = velocity === undefined ? 0.85 : velocity;
      if (this.voices.has(note)) this.release(this.voices.get(note), t, 0.01);
      const glideFrom = d.glide > 0 && this.lastNote !== null ? this.lastNote : null;
      if (d.mono) for (const v of [...this.voices.values()]) this.release(v, t, 0.012);
      if (this.voices.size >= MAX_VOICES) this.release(this.voices.values().next().value, t, 0.01);
      this.lastNote = note;

      const sources = [];
      const env = c.createGain(); env.gain.value = 0;
      const vca = c.createGain(); vca.gain.value = 1; // LFO volume target
      env.connect(vca).connect(this.bus.input);

      // Filter path (oscillators and noise); the sub goes straight to the envelope.
      let tone = env;
      let filters = [];
      if (d.filter.on) {
        const fl = d.filter, type = /^H/.test(fl.type) ? 'highpass' : 'lowpass';
        const stages = /24$/.test(fl.type) || fl.type === 'DirtyMg' ? 2 : /18$/.test(fl.type) ? 2 : 1;
        const q = 0.707 + (fl.reso / 100) * (stages > 1 ? 8 : 14);
        let last = env;
        for (let i = 0; i < stages; i++) {
          const bq = c.createBiquadFilter(); bq.type = type; bq.frequency.value = fl.cutoff; bq.Q.value = i === stages - 1 ? q : 0.707;
          bq.detune.value = (note - 60) * fl.keytrack; // keytrack: 100% follows the keyboard
          bq.connect(last); last = bq; filters.push(bq);
        }
        tone = last;
        const drive = fl.drive + (fl.type === 'DirtyMg' ? 40 : 0);
        if (drive > 0) {
          const sh = c.createWaveShaper(); sh.curve = shaperCurve('kSoftClip', drive * 0.4);
          const trim = c.createGain(); trim.gain.value = 1 / (1 + drive / 150);
          sh.connect(trim).connect(tone); tone = sh;
        }
      }
      const cutoffMod = (sourceNode, cents) => { const gn = c.createGain(); gn.gain.value = cents; sourceNode.connect(gn); for (const bq of filters) gn.connect(bq.detune); };

      // Filter envelope (Env 2).
      if (d.filter.on && d.filterEnv.on && d.filterEnv.amount) {
        const fe = d.filterEnv, cs = c.createConstantSource(), shape = c.createGain();
        shape.gain.value = 0;
        cs.connect(shape); cs.start(t); sources.push(cs);
        const aEnd = t + Math.max(0.001, fe.attack);
        shape.gain.setValueAtTime(0, t); shape.gain.linearRampToValueAtTime(1, aEnd);
        shape.gain.setTargetAtTime(fe.sustain, aEnd, Math.max(0.005, fe.decay / 4));
        cutoffMod(shape, fe.amount / 100 * KNOB_CENTS);
      }

      const lfo = d.lfo, lfoOn = lfo.on && lfo.amount && (lfo.target !== 'cutoff' || d.filter.on) && lfo.target !== 'wtpos';
      let lfoSig = null;
      if (lfoOn) { lfoSig = this.lfoSignal(lfo.shape, lfo.rate, t); sources.push(...lfoSig.sources); }
      if (lfoOn && lfo.target === 'cutoff') cutoffMod(lfoSig.out, lfo.amount / 100 * KNOB_CENTS);
      if (lfoOn && lfo.target === 'volume') { const gn = c.createGain(); gn.gain.value = lfo.amount / 100 * 1.6; lfoSig.out.connect(gn).connect(vca.gain); }

      const oscs = [];
      const addOsc = (o) => {
        const n = o.unison, f = midiToHz(note + 12 * o.octave + o.semi + o.fine / 100);
        const each = (o.level * o.level) / Math.sqrt(n);
        for (let i = 0; i < n; i++) {
          const pos = n > 1 ? (i / (n - 1)) * 2 - 1 : 0;
          const osc = c.createOscillator();
          this.setWave(osc, o.wave);
          if (glideFrom !== null) {
            osc.frequency.setValueAtTime(midiToHz(glideFrom + 12 * o.octave + o.semi + o.fine / 100), t);
            osc.frequency.setTargetAtTime(f, t, Math.max(0.005, d.glide / 3));
          } else osc.frequency.value = f;
          osc.detune.value = pos * o.detune * 50;
          const gain = c.createGain(); gain.gain.value = each;
          const pan = c.createStereoPanner(); pan.pan.value = n > 1 ? pos * 0.8 * (o.width / 100) : o.pan / 50;
          osc.connect(gain).connect(pan).connect(tone);
          osc.start(t + this.rand() * 0.002);
          sources.push(osc); oscs.push(osc);
        }
      };
      addOsc(d.oscA);
      if (d.oscB.on) addOsc(d.oscB);
      if (lfoOn && lfo.target === 'pitch') { const gn = c.createGain(); gn.gain.value = lfo.amount / 100 * 2400; lfoSig.out.connect(gn); for (const o of oscs) gn.connect(o.detune); }
      if (d.noise.on) {
        const src = c.createBufferSource(); src.buffer = this.noiseBuffer(); src.loop = true;
        const gn = c.createGain(); gn.gain.value = d.noise.level * d.noise.level * 0.4;
        src.connect(gn).connect(tone);
        src.start(t, this.rand());
        sources.push(src);
      }
      if (d.sub.on) {
        const osc = c.createOscillator();
        this.setWave(osc, SUB_WAVE[d.sub.shape] || 'sine');
        osc.frequency.value = midiToHz(note + 12 * d.sub.octave);
        const gn = c.createGain(); gn.gain.value = d.sub.level * d.sub.level;
        osc.connect(gn).connect(env);
        osc.start(t);
        sources.push(osc);
      }

      const e = d.ampEnv, peak = velocity;
      const attackEnd = t + Math.max(0.002, e.attack);
      env.gain.setValueAtTime(0, t);
      env.gain.linearRampToValueAtTime(peak, attackEnd);
      env.gain.setTargetAtTime(peak * e.sustain * e.sustain, attackEnd + e.hold, Math.max(0.005, e.decay / 4));
      const voice = { note, env, sources, release: e.release, feRelease: d.filterEnv.release };
      this.voices.set(note, voice);
      return voice;
    }

    noteOff(note, when) {
      const v = this.voices.get(note);
      if (v && this.ctx) this.release(v, when === undefined ? this.ctx.currentTime : when, v.release);
    }

    release(v, t, time) {
      if (!v || v.released) return;
      v.released = true;
      if (this.voices.get(v.note) === v) this.voices.delete(v.note);
      const g = v.env.gain;
      if (g.cancelAndHoldAtTime) g.cancelAndHoldAtTime(t);
      else { g.cancelScheduledValues(t); g.setValueAtTime(g.value, t); }
      g.setTargetAtTime(0, t, Math.max(0.003, time / 5));
      const end = t + Math.max(0.02, time) * 1.6 + 0.05;
      for (const s of v.sources) { try { s.stop(end); } catch (err) { /* already stopped */ } }
      v.sources[0].onended = () => v.env.disconnect();
    }

    allOff() {
      for (const id of this.timers) clearTimeout(id);
      this.timers = [];
      if (!this.ctx) return;
      const t = this.ctx.currentTime;
      for (const v of [...this.voices.values()]) this.release(v, t, 0.05);
    }

    // Plays a short phrase that suits the sound type. Returns its length in seconds.
    playDemo() {
      if (!this.design) return 0;
      this.allOff();
      this.start();
      const demo = DEMOS[DEMO_FOR[this.design.category] || 'lead'];
      const beat = 60 / demo.bpm;
      let last = 0;
      for (const [notes, at, len] of demo.notes) {
        this.timers.push(setTimeout(() => { for (const n of notes) this.noteOn(n, 0.8); }, at * beat * 1000));
        this.timers.push(setTimeout(() => { for (const n of notes) this.noteOff(n); }, (at + len) * beat * 1000));
        last = Math.max(last, (at + len) * beat);
      }
      return last + this.design.ampEnv.release;
    }

    // Fills `out` (Float32Array) with the current output waveform, or returns false before audio starts.
    scope(out) {
      if (!this.ctx || !this.bus) return false;
      this.bus.analyser.getFloatTimeDomainData(out);
      return true;
    }
  }

  // Renders one held note offline and returns its loudness in dBFS RMS over the
  // loudest window (150 ms for plucky sounds, 400 ms for sustained ones),
  // ignoring the design's own output gain.
  PresetSynth.measure = async function (design) {
    const OAC = root.OfflineAudioContext || root.webkitOfflineAudioContext;
    if (!OAC) return null;
    const rate = 22050, hold = Math.min(2.5, Math.max(0.8, design.ampEnv.attack + 0.6));
    const ctx = new OAC(2, Math.ceil(rate * (hold + 0.3)), rate);
    const synth = new PresetSynth({ context: ctx });
    synth.setDesign(Object.assign({}, design, { gainDb: 0 }));
    const note = MEASURE_NOTE[design.category] || 60;
    synth.noteOn(note, 0.85, 0.02);
    synth.noteOff(note, 0.02 + hold);
    const buf = await ctx.startRendering();
    const L = buf.getChannelData(0), R = buf.getChannelData(1), win = Math.floor(rate * (design.ampEnv.sustain < 0.3 ? 0.15 : 0.4)), step = Math.floor(rate * 0.025);
    let best = 0;
    for (let s = 0; s + win <= L.length; s += step) {
      let sum = 0;
      for (let i = s; i < s + win; i++) sum += L[i] * L[i] + R[i] * R[i];
      best = Math.max(best, sum / (2 * win));
    }
    return best > 0 ? 10 * Math.log10(best) : -120;
  };

  // Output gain (dB) that brings a design to the target loudness: a plain saw at
  // Serum's Init level (about -17 dBFS RMS in Serum) plus TARGET_DB, which is
  // closer to where finished presets sit.
  let reference = null;
  PresetSynth.TARGET_DB = 4;
  PresetSynth.autoGain = async function (design) {
    if (reference === null) {
      const D = root.SerumPresetMaker;
      reference = await PresetSynth.measure(D ? D.normalizeDesign({ macros: false }) : Object.assign({}, design));
    }
    const level = await PresetSynth.measure(design);
    if (level === null || reference === null) return null;
    return Math.max(-12, Math.min(12, Math.round((reference + PresetSynth.TARGET_DB - level) * 2) / 2));
  };

  root.PresetSynth = PresetSynth;
})(typeof globalThis !== 'undefined' ? globalThis : this);
