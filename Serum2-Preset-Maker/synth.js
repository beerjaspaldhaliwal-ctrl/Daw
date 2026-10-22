/*
 * Serum 2 Preset Maker - preview synth
 *
 * A small Web Audio synth that plays a design so you can hear roughly how the
 * preset will sound before saving it. It follows the same settings (waves,
 * octaves, unison, sub, noise, low-pass filter, amp envelope, effects) but is
 * an approximation: Serum 2's oscillators, filter and effects sound different.
 */
(function (root) {
  'use strict';

  const midiToHz = (m) => 440 * Math.pow(2, (m - 69) / 12);
  const NATIVE = { sine: 'sine', triangle: 'triangle', saw: 'sawtooth', square: 'square' };
  const PULSE_WIDTH = { pulse: 0.25, narrowpulse: 0.125 };
  const MAX_VOICES = 16;

  // Short phrases per sound type: [midi notes, start beat, length in beats].
  const DEMOS = {
    bass: { bpm: 124, notes: [[[36], 0, 0.45], [[36], 0.5, 0.45], [[48], 1, 0.3], [[36], 1.5, 0.45], [[39], 2, 0.45], [[36], 2.5, 0.45], [[43], 3, 0.45], [[41], 3.5, 0.45]] },
    pad: { bpm: 80, notes: [[[48, 55, 60, 64], 0, 3.5], [[45, 52, 57, 60], 4, 3.5]] },
    keys: { bpm: 100, notes: [[[48, 55, 60, 64], 0, 1.5], [[45, 52, 57, 60], 2, 1.5], [[41, 48, 53, 57], 4, 1.5], [[43, 50, 55, 59], 6, 1.5]] },
    stab: { bpm: 124, notes: [[[60, 63, 67], 0, 0.25], [[60, 63, 67], 0.75, 0.25], [[58, 62, 65], 1.5, 0.25], [[60, 63, 67], 2.5, 0.25], [[63, 67, 70], 3.25, 0.25]] },
    pluck: { bpm: 124, notes: [60, 64, 67, 72, 67, 64, 62, 67, 71, 74, 71, 67].map((n, i) => [[n], i * 0.5, 0.25]) },
    lead: { bpm: 110, notes: [[[72], 0, 0.9], [[74], 1, 0.4], [[75], 1.5, 0.9], [[79], 2.5, 0.4], [[77], 3, 0.9], [[75], 4, 0.4], [[74], 4.5, 1.4]] },
  };
  const DEMO_FOR = { bass: 'bass', sub: 'bass', reese: 'bass', pad: 'pad', keys: 'keys', bell: 'pluck', stab: 'stab', pluck: 'pluck', arp: 'pluck', lead: 'lead', supersaw: 'lead' };

  function distortionCurve(drive) {
    const k = 1 + (drive / 100) * 60, n = 2048, curve = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      const x = (i / (n - 1)) * 2 - 1;
      curve[i] = Math.tanh(k * x) / Math.tanh(k);
    }
    return curve;
  }

  class PresetSynth {
    constructor() {
      this.ctx = null;
      this.design = null;
      this.voices = new Map(); // note -> voice
      this.timers = [];
      this.waves = {};
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
      if (this.ctx.state === 'suspended') this.ctx.resume();
      return this.ctx;
    }

    setDesign(design) {
      this.design = design;
      if (this.ctx) this.applyFx();
    }

    buildBus() {
      const c = this.ctx;
      const g = (v) => { const n = c.createGain(); n.gain.value = v; return n; };
      const b = this.bus = { input: g(1) };

      // Distortion: dry/wet around a soft clipper.
      b.distDry = g(1); b.distWet = g(0); b.shaper = c.createWaveShaper(); b.shaper.oversample = '4x';
      b.afterDist = g(1);
      b.input.connect(b.distDry).connect(b.afterDist);
      b.input.connect(b.shaper).connect(b.distWet).connect(b.afterDist);

      // Hyper/Dimension stand-in: two modulated delays panned left and right.
      b.afterHyper = g(1);
      b.hyperWet = g(0);
      b.afterDist.connect(b.afterHyper);
      b.afterDist.connect(b.hyperWet);
      for (const [delay, rate, pan] of [[0.012, 0.6, -1], [0.019, 0.83, 1]]) {
        const d = c.createDelay(0.05); d.delayTime.value = delay;
        const lfo = c.createOscillator(); lfo.frequency.value = rate;
        const depth = g(0.003); lfo.connect(depth).connect(d.delayTime); lfo.start();
        const p = c.createStereoPanner(); p.pan.value = pan;
        b.hyperWet.connect(d).connect(p).connect(b.afterHyper);
      }

      // Compressor: switched in or out.
      b.comp = c.createDynamicsCompressor();
      b.comp.threshold.value = -26; b.comp.ratio.value = 5; b.comp.attack.value = 0.004; b.comp.release.value = 0.12;
      b.compWet = g(0); b.compDry = g(1); b.afterComp = g(1);
      b.afterHyper.connect(b.compDry).connect(b.afterComp);
      b.afterHyper.connect(b.comp).connect(b.compWet).connect(b.afterComp);

      // Delay and reverb as sends.
      b.master = g(0.32);
      b.afterComp.connect(b.master);
      b.delaySend = g(0);
      b.delay = c.createDelay(2); b.delay.delayTime.value = 0.36;
      const fb = g(0.38), tone = c.createBiquadFilter(); tone.type = 'lowpass'; tone.frequency.value = 3500;
      b.afterComp.connect(b.delaySend).connect(b.delay).connect(tone).connect(fb).connect(b.delay);
      tone.connect(b.master);
      b.reverbSend = g(0);
      b.reverb = c.createConvolver();
      b.afterComp.connect(b.reverbSend).connect(b.reverb).connect(b.master);
      b.reverbSize = null;

      // Safety limiter and a meter for the scope.
      const limiter = c.createDynamicsCompressor();
      limiter.threshold.value = -3; limiter.ratio.value = 20; limiter.attack.value = 0.002; limiter.release.value = 0.1;
      b.analyser = c.createAnalyser(); b.analyser.fftSize = 2048;
      b.master.connect(limiter).connect(b.analyser).connect(c.destination);
    }

    applyFx() {
      const f = this.design.fx, b = this.bus, t = this.ctx.currentTime;
      const set = (param, v) => param.setTargetAtTime(v, t, 0.02);
      const distMix = f.dist.on ? f.dist.wet / 100 : 0;
      b.shaper.curve = distortionCurve(f.dist.drive);
      set(b.distWet.gain, distMix * 0.7);
      set(b.distDry.gain, 1 - distMix);
      set(b.hyperWet.gain, f.hyper.on ? (f.hyper.wet / 100) * 0.9 : 0);
      set(b.compWet.gain, f.comp.on ? 1.8 : 0);
      set(b.compDry.gain, f.comp.on ? 0 : 1);
      set(b.delaySend.gain, f.delay.on ? f.delay.wet / 100 : 0);
      set(b.reverbSend.gain, f.reverb.on ? (f.reverb.wet / 100) * 1.4 : 0);
      if (f.reverb.on && b.reverbSize !== Math.round(f.reverb.size / 10)) {
        b.reverbSize = Math.round(f.reverb.size / 10);
        b.reverb.buffer = this.impulse(0.6 + b.reverbSize * 0.45);
      }
    }

    impulse(seconds) {
      const c = this.ctx, len = Math.floor(c.sampleRate * seconds), buf = c.createBuffer(2, len, c.sampleRate);
      for (let ch = 0; ch < 2; ch++) {
        const data = buf.getChannelData(ch);
        for (let i = 0; i < len; i++) data[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, 3);
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

    noteOn(note, velocity) {
      if (!this.design) return;
      const c = this.start(), d = this.design, t = c.currentTime + 0.005;
      velocity = velocity === undefined ? 0.85 : velocity;
      if (this.voices.has(note)) this.release(this.voices.get(note), t, 0.01);
      if (d.mono) for (const v of this.voices.values()) this.release(v, t, 0.015);
      if (this.voices.size >= MAX_VOICES) this.release(this.voices.values().next().value, t, 0.01);

      const env = c.createGain(); env.gain.value = 0;
      env.connect(this.bus.input);
      let tone = env;
      if (d.filter.on) {
        const filt = c.createBiquadFilter();
        filt.type = 'lowpass';
        filt.frequency.value = d.filter.cutoff;
        filt.Q.value = 0.707 + (d.filter.reso / 100) * 14;
        filt.connect(env);
        tone = filt;
      }
      const sources = [];
      const addOsc = (o) => {
        const n = o.unison, f = midiToHz(note + 12 * o.octave);
        const each = (o.level * 0.55) / Math.sqrt(n);
        for (let i = 0; i < n; i++) {
          const pos = n > 1 ? (i / (n - 1)) * 2 - 1 : 0;
          const osc = c.createOscillator();
          this.setWave(osc, o.wave);
          osc.frequency.value = f;
          osc.detune.value = pos * o.detune * 50;
          const gain = c.createGain(); gain.gain.value = each;
          const pan = c.createStereoPanner(); pan.pan.value = n > 1 ? pos * 0.8 : o.pan / 100;
          osc.connect(gain).connect(pan).connect(tone);
          osc.start(t + Math.random() * 0.002);
          sources.push(osc);
        }
      };
      addOsc(d.oscA);
      if (d.oscB.on) addOsc(d.oscB);
      if (d.noise.on) {
        const src = c.createBufferSource(); src.buffer = this.noiseBuffer(); src.loop = true;
        const g = c.createGain(); g.gain.value = d.noise.level * 0.35;
        src.connect(g).connect(tone);
        src.start(t, Math.random());
        sources.push(src);
      }
      if (d.sub.on) {
        const osc = c.createOscillator(); osc.type = 'sine';
        osc.frequency.value = midiToHz(note + 12 * d.sub.octave);
        const g = c.createGain(); g.gain.value = d.sub.level * 0.6;
        osc.connect(g).connect(env); // the sub bypasses the filter, as in the preset
        osc.start(t);
        sources.push(osc);
      }

      const e = d.ampEnv, peak = velocity;
      const attackEnd = t + Math.max(0.002, e.attack);
      env.gain.setValueAtTime(0, t);
      env.gain.linearRampToValueAtTime(peak, attackEnd);
      env.gain.setTargetAtTime(peak * e.sustain, attackEnd + e.hold, Math.max(0.005, e.decay / 4));
      const voice = { note, env, sources, release: e.release };
      this.voices.set(note, voice);
      return voice;
    }

    noteOff(note) {
      const v = this.voices.get(note);
      if (v && this.ctx) this.release(v, this.ctx.currentTime, v.release);
    }

    release(v, t, time) {
      if (!v || v.released) return;
      v.released = true;
      if (this.voices.get(v.note) === v) this.voices.delete(v.note);
      const g = v.env.gain;
      g.cancelScheduledValues(t);
      g.setValueAtTime(g.value, t);
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
      if (!this.ctx) return false;
      this.bus.analyser.getFloatTimeDomainData(out);
      return true;
    }
  }

  root.PresetSynth = PresetSynth;
})(typeof globalThis !== 'undefined' ? globalThis : this);
