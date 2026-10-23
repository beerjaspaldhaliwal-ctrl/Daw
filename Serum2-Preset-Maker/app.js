/*
 * Serum 2 Preset Maker - page logic
 *
 * Text (keywords or Claude) -> design -> editor + preview synth -> .SerumPreset files.
 */
(function () {
  'use strict';
  const $ = (id) => document.getElementById(id);
  const E = window.SerumPresetMaker;
  const CFG = window.PRESET_MAKER_CONFIG || {};
  const Synth = window.PresetSynth;
  const synth = new Synth();

  const store = {
    get(k) { try { return localStorage.getItem('s2pm:' + k); } catch (e) { return null; } },
    set(k, v) { try { if (v === null) localStorage.removeItem('s2pm:' + k); else localStorage.setItem('s2pm:' + k, v); } catch (e) { /* storage off */ } },
  };

  // items: the presets made in this batch; sel: the one in the editor.
  const state = { items: [], sel: 0, batchName: '', busy: null };
  const current = () => state.items[state.sel];

  /* ---------------------------------------------------------------- helpers */

  const WAVE_LABELS = { sine: 'Sine', triangle: 'Triangle', saw: 'Saw', square: 'Square', pulse: 'Pulse', narrowpulse: 'Narrow pulse' };
  const pct = (v) => Math.round(v * 100) + '%';
  const pct100 = (v) => Math.round(v) + '%';
  const fmtTime = (s) => (s < 1 ? (s < 0.01 ? (s * 1000).toFixed(1) : Math.round(s * 1000)) + ' ms' : (+s.toFixed(2)) + ' s');
  const fmtHz = (hz) => (hz >= 1000 ? (+(hz / 1000).toFixed(hz >= 10000 ? 1 : 2)) + ' kHz' : Math.round(hz) + ' Hz');
  const signed = (v) => (v > 0 ? '+' : '') + v;
  const fmtDb = (v) => (v > 0 ? '+' : '') + (+v.toFixed(1)) + ' dB';
  const entries = (o) => Object.entries(o).map(([v, t]) => [v, t]);

  function getPath(obj, path) { return path.split('.').reduce((o, k) => o[k], obj); }
  function setPath(obj, path, v) {
    const keys = path.split('.'), last = keys.pop();
    keys.reduce((o, k) => o[k], obj)[last] = v;
  }
  function status(id, text, isError) {
    const n = $(id);
    n.textContent = text || '';
    n.className = 'status' + (isError ? ' err' : '');
  }
  function el(tag, attrs, ...children) {
    const n = document.createElement(tag);
    for (const [k, v] of Object.entries(attrs || {})) {
      if (k === 'class') n.className = v;
      else if (k.startsWith('on')) n.addEventListener(k.slice(2), v);
      else if (v !== false && v !== null && v !== undefined) n.setAttribute(k, v === true ? '' : v);
    }
    for (const c of children) if (c !== null && c !== undefined) n.append(c);
    return n;
  }
  function uniqueNames(items) {
    const seen = new Map();
    for (const it of items) {
      const base = it.design.name;
      const n = seen.get(base.toLowerCase()) || 0;
      seen.set(base.toLowerCase(), n + 1);
      if (n) it.design.name = base + ' ' + (n + 1);
    }
  }
  function newItem(design, notes) {
    return { design, notes: notes || [], original: JSON.parse(JSON.stringify(design)), include: true, levelSig: null };
  }

  /* ------------------------------------------------------- editor controls */

  // Log-scaled sliders run 0..1000.
  const logScale = (min, max) => ({
    to: (v) => Math.round((Math.log(Math.max(v, min) / min) / Math.log(max / min)) * 1000),
    from: (x) => min * Math.pow(max / min, x / 1000),
  });
  const steps = (lo, hi) => Array.from({ length: hi - lo + 1 }, (_, i) => [String(lo + i), signed(lo + i)]);
  const range = (k, label, min, max, step, fmt) => ({ k, label, type: 'range', min, max, step, fmt });
  const logRange = (k, label, min, max, fmt) => ({ k, label, type: 'log', scale: logScale(min, max), fmt });
  const choice = (k, label, options, cast) => ({ k, label, type: 'select', options, cast });
  const onOff = (k, label) => choice(k, label, [['true', 'On'], ['false', 'Off']], (v) => v === 'true');

  const oscControls = [
    choice('wave', 'Wave', entries(WAVE_LABELS)),
    choice('octave', 'Octave', steps(-3, 3), Number),
    choice('semi', 'Semitone', steps(-12, 12), Number),
    range('unison', 'Unison', 1, 16, 1, String),
    range('detune', 'Detune', 0, 1, 0.01, (v) => v.toFixed(2)),
    range('width', 'Width', 0, 100, 1, pct100),
    range('level', 'Level', 0, 1, 0.01, pct),
  ];
  const envControls = (withAmount) => (withAmount ? [range('amount', 'Amount', -100, 100, 1, (v) => signed(Math.round(v)))] : []).concat([
    logRange('attack', 'Attack', 0.0005, 10, fmtTime), logRange('decay', 'Decay', 0.01, 20, fmtTime),
    range('sustain', 'Sustain', 0, 1, 0.01, pct), logRange('release', 'Release', 0.005, 20, fmtTime),
  ]);
  const GROUPS = [
    { id: 'g-sources', title: 'Sound sources', modules: [
      { path: 'oscA', title: 'Osc A', controls: oscControls },
      { path: 'oscB', title: 'Osc B', toggle: true, controls: oscControls },
      { path: 'sub', title: 'Sub', toggle: true, controls: [choice('shape', 'Shape', entries(E.SUB_SHAPES)), choice('octave', 'Octave', steps(-2, 1), Number), range('level', 'Level', 0, 1, 0.01, pct)] },
      { path: 'noise', title: 'Noise', toggle: true, controls: [range('level', 'Level', 0, 1, 0.01, pct)] },
    ] },
    { id: 'g-shape', title: 'Filter, envelopes and movement', modules: [
      { path: 'filter', title: 'Filter', toggle: true, controls: [
        choice('type', 'Type', entries(E.FILTER_TYPES)), logRange('cutoff', 'Cutoff', 20, 20000, fmtHz),
        range('reso', 'Resonance', 0, 100, 1, pct100), range('drive', 'Drive', 0, 100, 1, pct100), range('keytrack', 'Keytrack', 0, 100, 1, pct100)] },
      { path: 'filterEnv', title: 'Filter env (Env 2)', toggle: true, env: 'fe', controls: envControls(true) },
      { path: 'ampEnv', title: 'Volume env (Env 1)', env: 'amp', controls: envControls(false) },
      { path: 'lfo', title: 'LFO 1', toggle: true, controls: [
        choice('target', 'Target', entries(E.LFO_TARGETS)), choice('rate', 'Rate', Object.keys(E.LFO_DIVISIONS).map((r) => [r, r])),
        choice('shape', 'Shape', entries(E.LFO_SHAPES)), range('amount', 'Amount', -100, 100, 0.5, (v) => signed(+v.toFixed(1)))] },
      { path: '', title: 'Voicing', controls: [
        choice('mono', 'Mode', [['true', 'Mono'], ['false', 'Poly']], (v) => v === 'true'), onOff('legato', 'Legato'),
        range('glide', 'Glide', 0, 1, 0.005, (v) => (v ? fmtTime(v) : 'Off')), range('polyCount', 'Voices', 1, 32, 1, String)] },
      { path: '', title: 'Macros', togglePath: 'macros', note: 'Macro 1 Brightness (filter), 2 Grit (distortion), 3 Space (reverb + delay), 4 Width (Hyper). Turn them in Serum.', controls: [] },
    ] },
    { id: 'g-fx', title: 'Effects (in rack order)', modules: [
      { path: 'fx.dist', title: 'Distortion', toggle: true, controls: [choice('mode', 'Mode', entries(E.DIST_MODES)), range('drive', 'Drive', 0, 100, 1, pct100), range('wet', 'Mix', 0, 100, 1, pct100)] },
      { path: 'fx.chorus', title: 'Chorus', toggle: true, controls: [logRange('rate', 'Rate', 0.05, 8, (v) => v.toFixed(2) + ' Hz'), range('depth', 'Depth', 0, 26, 0.5, (v) => v + ' ms'), range('wet', 'Mix', 0, 100, 1, pct100)] },
      { path: 'fx.flanger', title: 'Flanger', toggle: true, controls: [logRange('rate', 'Rate', 0.02, 8, (v) => v.toFixed(2) + ' Hz'), range('depth', 'Depth', 0, 100, 1, pct100), range('feedback', 'Feedback', 0, 100, 1, pct100), range('wet', 'Mix', 0, 100, 1, pct100)] },
      { path: 'fx.phaser', title: 'Phaser', toggle: true, controls: [logRange('rate', 'Rate', 0.02, 8, (v) => v.toFixed(2) + ' Hz'), range('depth', 'Depth', 0, 100, 1, pct100), range('feedback', 'Feedback', 0, 100, 1, pct100), range('wet', 'Mix', 0, 100, 1, pct100)] },
      { path: 'fx.hyper', title: 'Hyper/Dimension', toggle: true, controls: [range('wet', 'Mix', 0, 100, 1, pct100), range('detune', 'Detune', 0, 100, 1, pct100), range('size', 'Dimension', 0, 100, 1, pct100)] },
      { path: 'fx.eq', title: 'EQ', toggle: true, controls: [choice('lowCut', 'Low cut', entries(E.EQ_LOW_CUTS), Number), range('bass', 'Bass', -12, 12, 0.5, fmtDb), range('treble', 'Treble', -12, 12, 0.5, fmtDb)] },
      { path: 'fx.comp', title: 'Punch (OTT-style)', toggle: true, controls: [range('amount', 'Amount', 0, 100, 1, pct100)] },
      { path: 'fx.delay', title: 'Delay', toggle: true, controls: [choice('time', 'Time', entries(E.DELAY_TIMES)), range('feedback', 'Feedback', 0, 95, 1, pct100), onOff('pingpong', 'Ping-pong'), range('wet', 'Mix', 0, 100, 1, pct100)] },
      { path: 'fx.reverb', title: 'Reverb', toggle: true, controls: [choice('type', 'Type', entries(E.REVERB_TYPES)), range('size', 'Size', 0, 100, 1, pct100), range('wet', 'Mix', 0, 100, 1, pct100)] },
    ] },
  ];
  const MODULES = GROUPS.flatMap((g) => g.modules);
  const fullPath = (m, k) => (m.path ? m.path + '.' + k : k);
  const toggleOf = (m) => (m.togglePath ? m.togglePath : m.toggle ? m.path + '.on' : null);
  const ctlId = (p) => 'c-' + p.replace(/\./g, '-');
  const modId = (m) => 'm-' + (m.path || m.title).replace(/[^a-z]/gi, '');

  function buildEditor() {
    const host = $('modules');
    host.replaceChildren();
    for (const g of GROUPS) {
      const grid = el('div', { class: 'modules' });
      host.append(el('h3', { class: 'group-title', id: g.id }, g.title), grid);
      for (const m of g.modules) {
        const card = el('div', { class: 'mod', id: modId(m) });
        const head = el('div', { class: 'mod-head' });
        const tp = toggleOf(m);
        if (tp) {
          const id = ctlId(tp);
          const box = el('input', { type: 'checkbox', id, onchange: (e) => change(tp, e.target.checked) });
          head.append(el('label', { class: 'mod-toggle', for: id }, box, el('span', {}, m.title)));
        } else {
          head.append(el('span', { class: 'mod-title' }, m.title));
        }
        card.append(head);
        if (m.env) card.append(el('div', { class: 'env-draw', id: 'env-' + m.env }));
        if (m.note) card.append(el('p', { class: 'mod-note' }, m.note));
        for (const c of m.controls) {
          const p = fullPath(m, c.k), id = ctlId(p);
          const row = el('div', { class: 'ctl' });
          row.append(el('label', { for: id }, c.label));
          let input;
          if (c.type === 'select') {
            input = el('select', { id, onchange: (e) => change(p, c.cast ? c.cast(e.target.value) : e.target.value) },
              ...c.options.map(([v, t]) => el('option', { value: v }, t)));
            row.append(input);
          } else {
            const isLog = c.type === 'log';
            input = el('input', {
              type: 'range', id, min: isLog ? 0 : c.min, max: isLog ? 1000 : c.max, step: isLog ? 1 : c.step,
              oninput: (e) => change(p, isLog ? c.scale.from(+e.target.value) : +e.target.value),
            });
            row.append(input, el('output', { for: id, id: id + '-v' }));
          }
          card.append(row);
        }
        grid.append(card);
      }
    }
  }

  function refreshEditor() {
    const it = current();
    if (!it) return;
    const d = it.design;
    if (document.activeElement !== $('name')) $('name').value = d.name;
    $('r-tags').replaceChildren(...d.tags.map((t) => el('span', { class: 'tag' }, t)));
    for (const m of MODULES) {
      const card = $(modId(m)), tp = toggleOf(m);
      if (tp) {
        const on = !!getPath(d, tp);
        $(ctlId(tp)).checked = on;
        card.classList.toggle('off', !on);
      }
      for (const c of m.controls) {
        const p = fullPath(m, c.k), id = ctlId(p), v = getPath(d, p);
        const input = $(id);
        if (c.type === 'select') input.value = String(v);
        else input.value = c.type === 'log' ? c.scale.to(v) : v;
        const out = $(id + '-v');
        if (out) out.textContent = c.fmt(v);
        const row = input.closest('.ctl');
        if (p === 'polyCount') row.hidden = d.mono;
        if (p === 'legato') row.hidden = !d.mono;
        if (/^osc[AB]\.width$/.test(p)) row.hidden = getPath(d, p.replace('width', 'unison')) < 2;
        if (p === 'fx.eq.bass') row.hidden = !!d.fx.eq.lowCut;
      }
    }
    $('m-filterEnv').classList.toggle('off', !d.filterEnv.on || !d.filter.on);
    if (d.lfo.target === 'cutoff' && !d.filter.on) $('m-lfo').classList.add('off');
    drawEnvelope('env-amp', d.ampEnv);
    drawEnvelope('env-fe', d.filterEnv);
    $('notes').replaceChildren(...(it.notes.length ? it.notes : ['Edited by hand.']).map((n) => el('li', {}, n)));
    $('undo').disabled = JSON.stringify(it.original) === JSON.stringify(d);
    refreshOutput();
    synth.setDesign(d);
  }

  function drawEnvelope(id, e) {
    // Square-root time scale so short and long envelopes both stay readable.
    const W = 220, H = 52, total = Math.sqrt(e.attack) + Math.sqrt(e.decay) + 0.6 + Math.sqrt(e.release);
    const x = (s) => (s / total) * W;
    const a = x(Math.sqrt(e.attack)), dd = a + x(Math.sqrt(e.decay)), hold = dd + x(0.6), r = hold + x(Math.sqrt(e.release));
    const sy = 4 + (1 - e.sustain) * (H - 8);
    const pts = [[0, H - 4], [a, 4], [dd, sy], [hold, sy], [r, H - 4]].map((p) => p.map((n) => n.toFixed(1)).join(',')).join(' ');
    $(id).innerHTML = '<svg viewBox="0 0 ' + W + ' ' + H + '" role="img" aria-label="Envelope shape"><polyline points="' + pts + '" /></svg>';
  }

  function change(path, value) {
    const it = current();
    if (!it) return;
    setPath(it.design, path, value);
    it.design = E.normalizeDesign(it.design);
    refreshEditor();
    renderCards();
    saveSession();
    scheduleLevel();
  }

  /* ------------------------------------------------------- level matching */

  const autoLevel = () => $('autoLevel').checked;
  const levelSig = (d) => JSON.stringify(Object.assign({}, d, { gainDb: 0, name: '', description: '' }));
  let levelTimer = null;

  function refreshOutput() {
    const d = current().design;
    $('gainDb').value = d.gainDb;
    $('gainDb').disabled = autoLevel();
    $('gainDb-v').textContent = fmtDb(d.gainDb);
    $('level-note').textContent = autoLevel()
      ? (current().levelSig === levelSig(d) ? 'Matched to a finished-preset level.' : 'Measuring…')
      : 'Set by hand.';
  }

  // Measures an item and sets its output gain (only when level matching is on).
  async function matchLevel(it) {
    if (!autoLevel() || !Synth.autoGain) return;
    const sig = levelSig(it.design);
    if (it.levelSig === sig) return;
    try {
      const g = await Synth.autoGain(it.design);
      if (g === null || levelSig(it.design) !== sig) return;
      it.design.gainDb = g;
      it.levelSig = sig;
    } catch (e) { /* no offline audio: keep the current gain */ }
  }
  function scheduleLevel() {
    clearTimeout(levelTimer);
    if (!autoLevel()) return;
    levelTimer = setTimeout(async () => {
      const it = current();
      await matchLevel(it);
      if (it === current()) { refreshOutput(); synth.setDesign(it.design); saveSession(); }
    }, 250);
  }
  async function matchAll(items) {
    for (const it of items) await matchLevel(it);
  }

  /* --------------------------------------------------------------- the list */

  function summary(d) {
    const parts = [WAVE_LABELS[d.oscA.wave] + (d.oscA.unison > 1 ? ' ×' + d.oscA.unison : '')];
    if (d.oscB.on) parts[0] += ' + ' + WAVE_LABELS[d.oscB.wave];
    if (d.sub.on) parts.push('sub');
    if (d.filter.on) parts.push(E.FILTER_TYPES[d.filter.type] + ' ' + fmtHz(d.filter.cutoff));
    if (d.lfo.on) parts.push('LFO ' + d.lfo.rate);
    const fx = E.FX_KEYS.filter((k) => d.fx[k].on).map((k) => ({ dist: 'dist', chorus: 'chorus', flanger: 'flanger', phaser: 'phaser', hyper: 'width', eq: 'EQ', comp: 'punch', delay: 'delay', reverb: 'reverb' })[k]);
    if (fx.length) parts.push(fx.join(', '));
    return parts.join(' · ');
  }

  function renderCards() {
    const show = state.items.length > 1;
    $('listPanel').hidden = !show;
    if (!show) return;
    $('batchName').value = $('batchName').value || state.batchName;
    $('list-count').textContent = state.items.filter((i) => i.include).length + ' of ' + state.items.length + ' selected';
    $('cards').replaceChildren(...state.items.map((it, i) => {
      const box = el('input', { type: 'checkbox', id: 'inc-' + i, 'aria-label': 'Include ' + it.design.name, onchange: (e) => { it.include = e.target.checked; renderCards(); saveSession(); } });
      box.checked = it.include;
      return el('div', { class: 'card' + (i === state.sel ? ' sel' : '') },
        el('div', { class: 'card-top' }, box, el('strong', {}, it.design.name)),
        el('div', { class: 'card-sum' }, summary(it.design)),
        el('div', { class: 'card-actions' },
          el('button', { type: 'button', class: 'small', onclick: () => { select(i); synth.allOff(); synth.setDesign(it.design); synth.playDemo(); startScope(); } }, 'Play'),
          i === state.sel ? el('span', { class: 'editing' }, 'In the editor') : el('button', { type: 'button', class: 'small', onclick: () => select(i) }, 'Edit')));
    }));
  }

  function select(i) {
    state.sel = i;
    refreshEditor();
    renderCards();
    saveSession();
    scheduleLevel();
  }

  function setItems(items, batchName) {
    uniqueNames(items);
    state.items = items;
    state.sel = 0;
    state.batchName = batchName || '';
    $('batchName').value = batchName || '';
    status('status', '');
    status('listStatus', '');
    refreshEditor();
    renderCards();
    saveSession();
    scheduleLevel();
    if (items.length > 1) matchAll(items.slice(1)).then(saveSession);
  }

  function saveSession() {
    store.set('session', JSON.stringify({ v: 3, items: state.items, sel: state.sel, batchName: state.batchName }));
  }
  function loadSession() {
    try {
      const s = JSON.parse(store.get('session') || 'null');
      if (!s || s.v !== 3 || !Array.isArray(s.items) || !s.items.length) return false;
      state.items = s.items.map((it) => ({
        design: E.normalizeDesign(it.design), notes: Array.isArray(it.notes) ? it.notes.map(String) : [],
        original: E.normalizeDesign(it.original || it.design), include: it.include !== false, levelSig: it.levelSig || null,
      }));
      state.sel = Math.min(Math.max(0, s.sel | 0), state.items.length - 1);
      state.batchName = String(s.batchName || '');
      $('batchName').value = state.batchName;
      return true;
    } catch (e) { return false; }
  }

  /* ------------------------------------------------------------------- Claude */

  const SDK_URL = 'https://cdn.jsdelivr.net/npm/@anthropic-ai/sdk@0.132.0/+esm';
  const MODEL = 'claude-opus-5-5';
  let sampleCap = null;   // Claude through the viewer's account (online version)
  let apiKey = store.get('apiKey') || '';

  function aiAvailable() { return !!sampleCap || !!apiKey; }

  // Asks Claude and returns parsed JSON. `kind` is 'single' or 'pack'.
  async function askClaude(kind, input, signal) {
    if (sampleCap) {
      const prompt = kind === 'pack' ? E.aiPackPrompt(input) : E.aiSinglePrompt(input);
      return sampleCap.json(prompt, { signal, modelTier: 'default' });
    }
    const { default: Anthropic } = await import(SDK_URL);
    const client = new Anthropic({ apiKey, dangerouslyAllowBrowser: true });
    const user = kind === 'pack'
      ? 'Design one preset for each numbered description, in the same order, with distinct names.\n\n' + input.map((l, i) => (i + 1) + '. ' + l).join('\n')
      : 'Sound description:\n' + input;
    try {
      const res = await client.beta.messages.create({
        model: MODEL,
        max_tokens: 16000,
        betas: ['server-side-fallback-2026-07-01'],
        fallbacks: 'default',
        system: E.aiInstructions(),
        output_config: { effort: 'medium', format: { type: 'json_schema', schema: kind === 'pack' ? E.AI_PACK_SCHEMA : E.AI_DESIGN_SCHEMA } },
        messages: [{ role: 'user', content: user }],
      }, { signal });
      if (res.stop_reason === 'refusal') throw { code: 'refused' };
      if (res.stop_reason === 'max_tokens') throw { code: 'too_long' };
      const text = res.content.filter((b) => b.type === 'text').map((b) => b.text).join('');
      return JSON.parse(text);
    } catch (e) {
      if (e && e.code) throw e;
      if (e instanceof Anthropic.APIUserAbortError) throw { code: 'cancelled' };
      if (e instanceof Anthropic.AuthenticationError) throw { code: 'bad_key' };
      if (e instanceof Anthropic.PermissionDeniedError) throw { code: 'bad_key' };
      if (e instanceof Anthropic.RateLimitError) throw { code: 'rate_limited' };
      if (e instanceof Anthropic.APIConnectionError) throw { code: 'offline' };
      if (e instanceof Anthropic.APIError) throw { code: 'api', message: e.message };
      if (e instanceof SyntaxError) throw { code: 'invalid_json' };
      throw { code: 'api', message: String(e && e.message || e) };
    }
  }

  function aiErrorText(e) {
    const code = e && e.code;
    return ({
      cancelled: 'Stopped.',
      not_granted: 'Claude wasn\'t allowed for this page, so the keyword reading is used.',
      sampling_disabled: 'Claude isn\'t available on this account, so the keyword reading is used.',
      rate_limited: 'Claude is busy or your usage limit is reached. The keyword reading is used; try again later.',
      bad_key: 'The API key was rejected. Check it at console.anthropic.com. The keyword reading is used for now.',
      offline: 'Couldn\'t reach Claude. Check your internet connection. The keyword reading is used for now.',
      refused: 'Claude declined this description. The keyword reading is used.',
      invalid_json: 'Claude\'s answer couldn\'t be read. The keyword reading is used; try again.',
      too_long: 'Claude\'s answer was cut short. Try fewer descriptions at once.',
    })[code] || ('Claude couldn\'t answer (' + (e && (e.message || e.code) || 'unknown error') + '). The keyword reading is used.');
  }

  /* ------------------------------------------------------------ generating */

  const useAi = () => $('mode-ai').checked && aiAvailable();
  const genOpts = () => ({ author: $('author').value.trim() || undefined });

  function beginBusy(label) {
    const ctl = new AbortController();
    state.busy = ctl;
    $('make').disabled = true;
    $('stop').hidden = false;
    status('genStatus', label);
    return ctl;
  }
  function endBusy() {
    state.busy = null;
    $('make').disabled = false;
    $('stop').hidden = true;
  }

  async function makeOne() {
    const text = $('prompt').value.trim();
    if (!text) return status('genStatus', 'Type a description first, for example "warm pad with slow attack".', true);
    const opts = genOpts();
    if (!useAi()) {
      const r = E.textToDesign(text, opts);
      setItems([newItem(r.design, r.notes)]);
      return status('genStatus', 'Made "' + r.design.name + '" from keywords.');
    }
    const ctl = beginBusy('Claude is designing the sound…');
    try {
      const ai = await askClaude('single', text, ctl.signal);
      const r = E.fromAi(ai, text, opts);
      if (opts.author) r.design.author = opts.author;
      setItems([newItem(r.design, r.notes)]);
      status('genStatus', 'Claude designed "' + r.design.name + '".');
    } catch (e) {
      const r = E.textToDesign(text, opts);
      setItems([newItem(r.design, r.notes)]);
      status('genStatus', aiErrorText(e), e && e.code !== 'cancelled');
    } finally { endBusy(); }
  }

  async function makePack() {
    const lines = $('packList').value.split('\n').map((l) => l.trim()).filter(Boolean).slice(0, 16);
    if (!lines.length) return status('genStatus', 'Write one description per line first.', true);
    const opts = genOpts();
    const name = $('packName').value.trim() || 'Preset Pack';
    const keyword = (l) => { const r = E.textToDesign(l, opts); return newItem(r.design, r.notes); };
    if (!useAi()) {
      setItems(lines.map(keyword), name);
      return status('genStatus', 'Made ' + lines.length + ' presets from keywords.');
    }
    const ctl = beginBusy('Claude is designing ' + lines.length + ' sounds…');
    try {
      const ai = await askClaude('pack', lines, ctl.signal);
      const list = ai && Array.isArray(ai.presets) ? ai.presets : [];
      setItems(lines.map((l, i) => {
        if (!list[i]) return keyword(l);
        const r = E.fromAi(list[i], l, opts);
        if (opts.author) r.design.author = opts.author;
        return newItem(r.design, r.notes);
      }), name);
      status('genStatus', 'Claude designed ' + lines.length + ' presets.' + (list.length < lines.length ? ' Some used the keyword reading.' : ''));
    } catch (e) {
      setItems(lines.map(keyword), name);
      status('genStatus', aiErrorText(e), e && e.code !== 'cancelled');
    } finally { endBusy(); }
  }

  function makeVariations() {
    const it = current();
    if (!it) return;
    const count = +$('varCount').value;
    const base = it.design;
    const seed = Date.now() & 0xffffff;
    const first = JSON.parse(JSON.stringify(base));
    first.name = base.name + ' 1';
    const items = [newItem(first, it.notes)];
    for (let i = 2; i <= count; i++) {
      const v = E.varyDesign(base, { seed: seed + i * 7919, amount: +$('varAmount').value, name: base.name + ' ' + i });
      items.push(newItem(v, ['Variation ' + i + ' of "' + base.name + '": nearby wave, detune, filter, movement, envelope and effect settings.']));
    }
    setItems(items, base.name + ' variations');
    status('status', 'Made ' + (count - 1) + ' variations. Play them below, untick any you don\'t want, then save.');
  }

  function surprise() {
    const r = E.randomDesign((Date.now() ^ Math.floor(Math.random() * 1e9)) >>> 0);
    if ($('author').value.trim()) r.design.author = $('author').value.trim();
    setItems([newItem(r.design, r.notes)]);
    status('genStatus', 'Made a random ' + r.design.category + ': "' + r.design.name + '".');
    try { synth.playDemo(); startScope(); } catch (e) { /* audio not allowed yet */ }
  }

  /* -------------------------------------------------------------- saving */

  const canPickFolder = location.protocol === 'file:' && typeof window.showDirectoryPicker === 'function';
  let dirHandle = null;
  let downloadsCap = null;

  function idb(mode, fn) {
    return new Promise((resolve, reject) => {
      const open = indexedDB.open('serum2-preset-maker', 1);
      open.onupgradeneeded = () => open.result.createObjectStore('kv');
      open.onerror = () => reject(open.error);
      open.onsuccess = () => {
        const tx = open.result.transaction('kv', mode), req = fn(tx.objectStore('kv'));
        tx.oncomplete = () => resolve(req.result);
        tx.onerror = () => reject(tx.error);
      };
    });
  }
  const loadHandle = () => idb('readonly', (s) => s.get('dir')).catch(() => null);
  const storeHandle = (h) => idb('readwrite', (s) => s.put(h, 'dir')).catch(() => {});

  function showFolder() {
    $('folder').hidden = false;
    $('change').hidden = !dirHandle;
    $('folder-text').textContent = dirHandle
      ? (dirHandle.name ? 'Saving to the "' + dirHandle.name + '" folder.' : 'Saving to the folder you chose.')
      : CFG.presetDir ? 'The first time you save, pick this folder: ' + CFG.presetDir : 'The first time you save, pick the folder you added to FL Studio\'s Browser.';
  }

  async function folder(pickNew) {
    let dir = pickNew ? null : dirHandle || await loadHandle();
    if (dir) {
      let perm = await dir.queryPermission({ mode: 'readwrite' });
      if (perm !== 'granted') perm = await dir.requestPermission({ mode: 'readwrite' });
      if (perm !== 'granted') dir = null;
    }
    if (!dir) {
      dir = await window.showDirectoryPicker({ id: 'serum-presets', mode: 'readwrite', startIn: 'documents' });
      await storeHandle(dir);
    }
    dirHandle = dir;
    showFolder();
    return dir;
  }

  async function freeName(dir, name) {
    const base = name.replace(/\.SerumPreset$/, '');
    for (let i = 1; i < 1000; i++) {
      const candidate = i === 1 ? name : base + ' (' + i + ').SerumPreset';
      try { await dir.getFileHandle(candidate); } catch (e) { return candidate; }
    }
    return base + ' ' + Date.now() + '.SerumPreset';
  }

  async function writeFiles(dir, files) {
    const names = [];
    for (const f of files) {
      const name = await freeName(dir, f.fileName);
      const w = await (await dir.getFileHandle(name, { create: true })).createWritable();
      await w.write(f.bytes);
      await w.close();
      names.push(name);
    }
    return names;
  }

  // Minimal stored (uncompressed) zip with any number of files.
  const CRC = (() => { const t = new Uint32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; } return t; })();
  function crc32(b) { let c = 0xffffffff; for (let i = 0; i < b.length; i++) c = CRC[(c ^ b[i]) & 255] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; }
  function zip(files) {
    const parts = [], central = [];
    let offset = 0;
    for (const f of files) {
      const nm = new TextEncoder().encode(f.path), crc = crc32(f.bytes), n = f.bytes.length;
      const lh = new DataView(new ArrayBuffer(30));
      lh.setUint32(0, 0x04034b50, true); lh.setUint16(4, 20, true); lh.setUint16(6, 0x0800, true);
      lh.setUint32(14, crc, true); lh.setUint32(18, n, true); lh.setUint32(22, n, true); lh.setUint16(26, nm.length, true);
      const cd = new DataView(new ArrayBuffer(46));
      cd.setUint32(0, 0x02014b50, true); cd.setUint16(4, 20, true); cd.setUint16(6, 20, true); cd.setUint16(8, 0x0800, true);
      cd.setUint32(16, crc, true); cd.setUint32(20, n, true); cd.setUint32(24, n, true); cd.setUint16(28, nm.length, true);
      cd.setUint32(42, offset, true);
      parts.push(lh, nm, f.bytes);
      central.push(cd, nm);
      offset += 30 + nm.length + n;
    }
    const cdLen = central.reduce((s, p) => s + p.byteLength, 0);
    const end = new DataView(new ArrayBuffer(22));
    end.setUint32(0, 0x06054b50, true); end.setUint16(8, files.length, true); end.setUint16(10, files.length, true);
    end.setUint32(12, cdLen, true); end.setUint32(16, offset, true);
    return new Blob([...parts, ...central, end]);
  }

  function browserDownload(name, data) {
    const url = URL.createObjectURL(data instanceof Blob ? data : new Blob([data], { type: 'application/octet-stream' }));
    const a = el('a', { href: url, download: name });
    document.body.append(a); a.click(); a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 2000);
  }

  async function offer(name, data, statusId) {
    if (downloadsCap) {
      try {
        await downloadsCap.save({ filename: name, data });
        return true;
      } catch (e) {
        status(statusId, e && e.code === 'declined' ? 'Download cancelled.' : 'This view can\'t save files. Open the installed version instead.', true);
        return false;
      }
    }
    browserDownload(name, data);
    return true;
  }

  const safeFolder = (s) => String(s || '').replace(/[\\/:*?"<>|]/g, '').trim().slice(0, 60);

  async function currentFile() {
    await matchLevel(current());
    refreshOutput();
    return E.presetFile(current().design);
  }

  async function saveCurrent(pickNew) {
    try {
      const f = await currentFile();
      const names = await writeFiles(await folder(pickNew), [f]);
      status('status', 'Saved "' + names[0] + '". Drag it from FL Studio\'s Browser onto Serum 2.');
    } catch (e) {
      status('status', e && e.name === 'AbortError' ? 'No folder chosen, so nothing was saved.' : 'Couldn\'t save to the folder (' + (e && e.message || e) + '). Use Download instead.', true);
    }
  }

  async function downloadCurrent() {
    const f = await currentFile();
    if (downloadsCap) {
      if (await offer(f.fileName.replace(/\.SerumPreset$/, '') + '.zip', zip([{ path: f.fileName, bytes: f.bytes }]), 'status')) status('status', 'Saved. Unzip it and drag the .SerumPreset file into Serum 2.');
      return;
    }
    browserDownload(f.fileName, f.bytes);
    status('status', 'Downloaded ' + f.fileName + '. Drag it into Serum 2.');
  }

  async function chosenFiles() {
    const items = state.items.filter((i) => i.include);
    await matchAll(items);
    return items.map((i) => E.presetFile(i.design));
  }

  async function saveAll() {
    const files = await chosenFiles();
    if (!files.length) return status('listStatus', 'Tick at least one preset to save.', true);
    const sub = safeFolder($('batchName').value) || 'Serum 2 Preset Maker';
    try {
      const root = await folder(false);
      const dir = await root.getDirectoryHandle(sub, { create: true });
      await writeFiles(dir, files);
      status('listStatus', 'Saved ' + files.length + ' presets in the "' + sub + '" folder. FL Studio\'s Browser shows it as a subfolder.');
    } catch (e) {
      status('listStatus', e && e.name === 'AbortError' ? 'No folder chosen, so nothing was saved.' : 'Couldn\'t save to the folder (' + (e && e.message || e) + '). Use Download instead.', true);
    }
  }

  async function downloadAll() {
    const files = await chosenFiles();
    if (!files.length) return status('listStatus', 'Tick at least one preset to download.', true);
    const sub = safeFolder($('batchName').value) || 'Serum 2 Presets';
    const blob = zip(files.map((f) => ({ path: sub + '/' + f.fileName, bytes: f.bytes })));
    if (await offer(sub + '.zip', blob, 'listStatus')) status('listStatus', 'Saved ' + sub + '.zip with ' + files.length + ' presets. Unzip it into your presets folder.');
  }

  /* ------------------------------------------------------------- keyboard */

  const KEYMAP = { a: 0, w: 1, s: 2, e: 3, d: 4, f: 5, t: 6, g: 7, y: 8, h: 9, u: 10, j: 11, k: 12, o: 13, l: 14, p: 15, ';': 16 };
  let baseNote = 48;
  const held = new Map();

  function buildKeys() {
    const wrap = $('keys');
    wrap.replaceChildren();
    const whites = [];
    for (let i = 0; i < 24; i++) {
      const note = baseNote + i, black = [1, 3, 6, 8, 10].includes(i % 12);
      const key = el('button', { type: 'button', class: 'key ' + (black ? 'black' : 'white'), 'data-note': note, tabindex: '-1', 'aria-label': 'Note ' + note });
      if (!black) whites.push(key);
      else key.style.left = 'calc(' + (whites.length * (100 / 14)) + '% - ' + (100 / 14 * 0.3) + '%)';
      key.addEventListener('pointerdown', (e) => { e.preventDefault(); key.setPointerCapture(e.pointerId); press(note); });
      key.addEventListener('pointerup', () => releaseKey(note));
      key.addEventListener('pointercancel', () => releaseKey(note));
      wrap.append(key);
    }
    for (const w of whites) w.style.width = (100 / 14) + '%';
    $('octave-label').textContent = 'C' + (baseNote / 12 - 1) + '–B' + (baseNote / 12);
  }
  function keyEl(note) { return document.querySelector('.key[data-note="' + note + '"]'); }
  function press(note) {
    try { synth.noteOn(note); } catch (e) { return status('status', e.message, true); }
    const k = keyEl(note); if (k) k.classList.add('down');
    startScope();
  }
  function releaseKey(note) {
    synth.noteOff(note);
    const k = keyEl(note); if (k) k.classList.remove('down');
  }

  document.addEventListener('keydown', (e) => {
    if (e.repeat || e.metaKey || e.ctrlKey || e.altKey) return;
    if (e.target.closest('input, textarea, select')) return;
    const k = e.key.toLowerCase();
    if (k === 'z' || k === 'x') { baseNote = Math.min(84, Math.max(24, baseNote + (k === 'z' ? -12 : 12))); buildKeys(); return; }
    if (KEYMAP[k] === undefined) return;
    const note = baseNote + KEYMAP[k];
    held.set(k, note);
    press(note);
  });
  document.addEventListener('keyup', (e) => {
    const note = held.get(e.key.toLowerCase());
    if (note === undefined) return;
    held.delete(e.key.toLowerCase());
    releaseKey(note);
  });
  window.addEventListener('blur', () => { held.clear(); synth.allOff(); });

  /* ---------------------------------------------------------------- scope */

  let scopeRunning = false;
  const scopeBuf = new Float32Array(2048);
  function startScope() {
    if (scopeRunning || window.matchMedia('(prefers-reduced-motion: reduce)').matches) return;
    scopeRunning = true;
    const canvas = $('scope'), g = canvas.getContext('2d');
    let quiet = 0;
    const frame = () => {
      const w = canvas.width = canvas.clientWidth * devicePixelRatio, h = canvas.height = canvas.clientHeight * devicePixelRatio;
      g.clearRect(0, 0, w, h);
      if (!synth.scope(scopeBuf)) { scopeRunning = false; return; }
      let peak = 0;
      g.beginPath();
      for (let i = 0; i < scopeBuf.length; i += 2) {
        const x = (i / scopeBuf.length) * w, y = h / 2 - scopeBuf[i] * h * 0.9;
        peak = Math.max(peak, Math.abs(scopeBuf[i]));
        if (i) g.lineTo(x, y); else g.moveTo(x, y);
      }
      g.strokeStyle = getComputedStyle(canvas).color;
      g.lineWidth = 1.5 * devicePixelRatio;
      g.stroke();
      quiet = peak < 0.002 ? quiet + 1 : 0;
      if (quiet > 90) { scopeRunning = false; g.clearRect(0, 0, w, h); return; }
      requestAnimationFrame(frame);
    };
    requestAnimationFrame(frame);
  }

  /* ------------------------------------------------------------- wiring */

  function setTab(which) {
    const pack = which === 'pack';
    $('tab-one-btn').setAttribute('aria-selected', String(!pack));
    $('tab-pack-btn').setAttribute('aria-selected', String(pack));
    $('tab-one').hidden = pack;
    $('tab-pack').hidden = !pack;
    $('make').textContent = pack ? 'Make pack' : 'Make preset';
    store.set('tab', which);
  }

  function refreshAiUi() {
    const ai = $('mode-ai').checked;
    $('ai-key').hidden = !ai || !!sampleCap || location.protocol !== 'file:';
    let note = '';
    if (ai && sampleCap) note = 'Uses your Claude account. You\'ll be asked to allow it the first time.';
    else if (ai && location.protocol === 'file:') note = apiKey ? 'Uses your Anthropic API key (billed to that account). Needs an internet connection.' : 'Add an Anthropic API key to use Claude. Until then the keyword reading is used.';
    else if (ai) note = 'Claude isn\'t available in this view, so the keyword reading is used.';
    $('ai-note').textContent = note;
    store.set('mode', ai ? 'ai' : 'keywords');
  }

  function init() {
    buildEditor();
    buildKeys();
    $('tab-one-btn').addEventListener('click', () => setTab('one'));
    $('tab-pack-btn').addEventListener('click', () => setTab('pack'));
    setTab(store.get('tab') === 'pack' ? 'pack' : 'one');

    $('describe').addEventListener('submit', (e) => { e.preventDefault(); if ($('tab-pack').hidden) makeOne(); else makePack(); });
    $('stop').addEventListener('click', () => state.busy && state.busy.abort());
    $('surprise').addEventListener('click', surprise);
    for (const c of document.querySelectorAll('.chip')) c.addEventListener('click', () => { $('prompt').value = c.textContent; setTab('one'); makeOne(); });

    if (store.get('mode') === 'ai') $('mode-ai').checked = true;
    for (const r of document.querySelectorAll('input[name="mode"]')) r.addEventListener('change', refreshAiUi);
    $('apiKey').value = apiKey;
    $('rememberKey').checked = !!store.get('apiKey');
    const keyChanged = () => {
      apiKey = $('apiKey').value.trim();
      store.set('apiKey', $('rememberKey').checked && apiKey ? apiKey : null);
      refreshAiUi();
    };
    $('apiKey').addEventListener('input', keyChanged);
    $('rememberKey').addEventListener('change', keyChanged);

    $('autoLevel').checked = store.get('autoLevel') !== 'off';
    $('autoLevel').addEventListener('change', () => {
      store.set('autoLevel', autoLevel() ? null : 'off');
      if (autoLevel()) { current().levelSig = null; scheduleLevel(); }
      refreshOutput();
    });
    $('gainDb').addEventListener('input', (e) => { if (!autoLevel()) change('gainDb', +e.target.value); });

    $('name').addEventListener('input', (e) => {
      // Kept as typed (spaces included); the file name is cleaned up when saving.
      current().design.name = e.target.value.slice(0, 60) || 'Untitled';
      renderCards();
      saveSession();
    });
    $('name').addEventListener('change', () => change('name', current().design.name));
    $('undo').addEventListener('click', () => { const it = current(); it.design = JSON.parse(JSON.stringify(it.original)); it.levelSig = null; refreshEditor(); renderCards(); saveSession(); scheduleLevel(); });
    $('vary').addEventListener('click', makeVariations);
    $('play').addEventListener('click', () => {
      try { synth.playDemo(); startScope(); } catch (e) { status('status', e.message, true); }
    });
    $('silence').addEventListener('click', () => synth.allOff());
    $('oct-down').addEventListener('click', () => { baseNote = Math.max(24, baseNote - 12); buildKeys(); });
    $('oct-up').addEventListener('click', () => { baseNote = Math.min(84, baseNote + 12); buildKeys(); });

    $('dl').addEventListener('click', downloadCurrent);
    $('dl-all').addEventListener('click', downloadAll);
    $('batchName').addEventListener('input', (e) => { state.batchName = e.target.value; saveSession(); });
    $('all-none').addEventListener('click', () => {
      const all = state.items.every((i) => i.include);
      for (const i of state.items) i.include = !all;
      renderCards();
      saveSession();
    });
    if (canPickFolder) {
      for (const id of ['save', 'save-all']) $(id).hidden = false;
      $('dl').className = 'secondary';
      $('dl').textContent = 'Download';
      $('dl-all').className = 'secondary';
      loadHandle().then((h) => { dirHandle = h || null; showFolder(); });
      $('save').addEventListener('click', () => saveCurrent(false));
      $('change').addEventListener('click', () => saveCurrent(true));
      $('save-all').addEventListener('click', saveAll);
    }

    if (!loadSession()) {
      const r = E.textToDesign($('prompt').value, genOpts());
      state.items = [newItem(r.design, r.notes)];
    }
    refreshEditor();
    renderCards();
    refreshAiUi();
    scheduleLevel();

    // Optional runtime capabilities of the online version.
    const use = window.claude && window.claude.use ? (name) => window.claude.use(name).catch(() => null) : () => Promise.resolve(null);
    use('downloads').then((d) => {
      downloadsCap = d;
      if (d) { $('dl').textContent = 'Download (.zip)'; }
    });
    use('sample').then((s) => {
      sampleCap = s;
      $('mode-ai-label').hidden = !s && location.protocol !== 'file:';
      refreshAiUi();
    });
    $('mode-ai-label').hidden = location.protocol !== 'file:' && !(window.claude && window.claude.use);
  }

  init();
})();
