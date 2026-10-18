# Serum 2 Preset Maker

Type a description like `dark wide reese bass with distortion` and get a `.SerumPreset` file for Xfer Serum 2.

## Use it

1. Open `index.html` in a browser (double-click it; no install, no internet needed).
2. Describe the sound, press **Make preset**, then **Download .SerumPreset**.
3. Drag the file onto Serum 2, or copy it into your Serum 2 presets folder
   (Windows: `Documents\Xfer\Serum 2 Presets\Presets\User`).

The **Words it understands** section on the page lists the keywords: sound types (bass, reese, 808, lead, supersaw, pad, pluck, keys, bell, stab, arp), tone words (dark, warm, bright, wide, detuned, dirty, punchy, …) and exact values (`attack 10ms`, `release 1.2s`, `cutoff 800hz`, `resonance 40`, `unison 7`, `octave -1`). Put a name in quotes or write `called My Preset` to name it.

## How it works

Serum 2's format isn't documented. The layout comes from community reverse engineering ([serum-preset-packager](https://github.com/KennethWussmann/serum-preset-packager), [0xdevalias's notes](https://gist.github.com/0xdevalias/5a06349b376d01b2a76ad27a86b08c1b)):

```
"XferJson\0" + u32le(len(json)) + u32le(0) + metadata JSON
u32le(len(cbor)) + u32le(2) + zstd frame containing the CBOR-encoded synth state
```

- `metadata.hash` is the MD5 of the zstd frame. This was checked against presets saved by Serum 2.
- The state starts from Serum 2's own Init patch (decoded from a file saved by Serum 2.0.19). The engine's CBOR encoder re-encodes that patch byte-for-byte identical to what Serum wrote.
- The zstd frame uses uncompressed ("raw") blocks. That is valid zstd and keeps the tool free of dependencies; a preset is about 9 KB.

## What is and isn't set

Parameter names and units come from real Serum 2 presets, except where marked *guess*.

| Section | Set | Notes |
|---|---|---|
| Osc A / B | on/off, wavetable position, octave, pan, unison, detune, level | Uses the factory `Analog/Basic Shapes` table; the waveform positions are approximate |
| Sub, Noise | on/off, level | |
| Filter 1 | on, cutoff, resonance | Default type (MG Low 12). Cutoff is stored 0–1 on a log scale, so the Hz values are approximate |
| Env 1 (amp) | attack, release, *decay, sustain, hold (guess)* | |
| Global | mono, polyphony | |
| FX | Hyper/Dimension, Distortion, multiband Compressor | Reverb and delay sit behind an *experimental* checkbox: their FX type IDs and parameter names are guesses |
| Modulation | none yet | The mod source IDs aren't known yet |

If a setting doesn't come through in Serum, open an issue with the prompt you used. A preset saved from Serum with that setting changed shows its real name and range.

## Test

```
node test/verify.js
```

The test builds presets from sample prompts, decodes each file with its own CBOR decoder and Node's zstd, and checks that the hash, the container fields and the full state match. It needs Node 22.15 or newer.
