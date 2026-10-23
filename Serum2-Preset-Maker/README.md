# Serum 2 Preset Maker

Type a description like `dark wide reese bass with distortion` and get a finished `.SerumPreset` file for Xfer Serum 2.
Hear it in the browser, fine-tune every setting, make variations, or make a whole pack at once. Every preset is
level-matched and comes with four working macros.

## Install on Windows (with FL Studio)

1. On GitHub, open **Actions → Serum 2 Preset Maker**, click the latest green run, and download **Serum-2-Preset-Maker-for-FL-Studio-Setup** under *Artifacts*.
2. Unzip it and run `Serum 2 Preset Maker for FL Studio Setup.exe`. It installs for your Windows user only, so no admin password is needed.
   If Windows SmartScreen says "Windows protected your PC", click **More info → Run anyway** (the installer isn't code-signed).
   Installing over an older version upgrades it and keeps your presets.
3. The installer creates the folder your presets are saved to:
   - `Documents\Xfer\Serum 2 Presets\Presets\User\Preset Maker` if Serum 2 is installed, so the presets also show in Serum 2's own browser
   - `Documents\Serum 2 Preset Maker\Presets` otherwise
4. Set up FL Studio once (the installer shows these steps and can open the folder for you):
   - Drag the presets folder from File Explorer onto FL Studio's Browser.
   - In the Browser's options menu, turn on **Show unknown file types**, so FL Studio lists `.SerumPreset` files.
5. Open **Serum 2 Preset Maker** from the Start menu or desktop. It opens in its own window (Microsoft Edge's app mode).
   Make a preset and click **Save to presets folder** (the first time, pick the folder from step 3), then drag it from FL Studio's Browser onto Serum 2.

The installer doesn't change FL Studio's settings itself. To remove it: **Settings → Apps → Installed apps → Serum 2 Preset Maker → Uninstall**. Your presets folder is kept.

## Use it

1. Describe the sound and press **Make preset**, or press **Surprise me** for a random one. Or open the **Pack** tab, write one description per line and press **Make pack**.
2. Press **Play demo**, click the on-screen keys, or play with your computer keyboard (`A`–`;`, `Z`/`X` for octave) to hear a preview.
3. Adjust anything in the editor: oscillators, sub, noise, filter, the filter and volume envelopes, LFO 1, voicing, macros and nine effects. **Undo my changes** goes back to what was generated.
4. **Make variations of this sound** gives 4–8 nearby versions. Play them, untick the ones you don't want, and save the rest together.
5. **Save to presets folder** (installed version) or **Download**. A batch is saved into a subfolder named after it, or downloaded as one `.zip`.

In Serum, turn **Macro 1 Brightness** (filter), **Macro 2 Grit** (distortion), **Macro 3 Space** (reverb and delay) and **Macro 4 Width** (Hyper/Dimension).

The preview is a Web Audio synth that follows the same settings and Serum's level laws. It is an approximation: Serum 2's oscillators, filters and effects sound richer.

### Loudness and brightness

Earlier versions parked the filter low (a "dark" bass ended up around 400 Hz) and kept layers quiet, so presets sounded dull and dropped in volume compared with commercial presets. Now:

- Sounds are voiced bright: filters sit higher and are shaped by Env 2 (plucks, basses, keys) instead of being parked low; oscillator levels stay near Serum's default.
- **Level-match** (on by default) renders each preset in the preview synth and sets an output gain so it lands about 4 dB above a plain saw at Serum's Init level, which is where finished presets sit. Extra gain is added with a clean gain stage at the end of the effects (a compressor at 1:1 with a 0 dB threshold, using its makeup gain); sounds that are already loud get lower oscillator levels instead.
- Most sound types use the OTT-style **Punch** compressor, which adds loudness and presence the way commercial presets do.

### Reading the text

- **Keywords** (instant, offline): sound types (bass, reese, 808, lead, supersaw, pad, pluck, keys, bell, stab, arp), genres (dubstep, future bass, trap, house, techno, trance, synthwave, drum and bass, hardstyle, ambient, lofi, EDM), tone words (dark, warm, bright, acid, wide, detuned), movement (wobble, evolving, vibrato, tremolo, sidechain pump, glide), grit (dirty, distorted, bitcrushed, punchy), effects by name (chorus, phaser, flanger, reverb, delay) and exact values (`attack 10ms`, `release 1.2s`, `cutoff 800hz`, `resonance 40`, `unison 7`, `octave -1`, `glide 80ms`, `reverb 30%`, `delay 1/8 dotted`). Put a name in quotes or write `called My Preset` to name it.
- **Claude AI**: Claude reads the description and picks every setting, so any words work, and explains its choices. Online (claude.ai) it uses your Claude account. The installed version needs an Anthropic API key from console.anthropic.com, which is billed to that account; it calls `claude-opus-5-5` with structured outputs from the browser using the official SDK. If Claude can't answer, the keyword reading is used and the page says why.

## How it works

Serum 2's format isn't documented. The layout comes from community reverse engineering ([serum-preset-packager](https://github.com/KennethWussmann/serum-preset-packager), [0xdevalias's notes](https://gist.github.com/0xdevalias/5a06349b376d01b2a76ad27a86b08c1b)):

```
"XferJson\0" + u32le(len(json)) + u32le(0) + metadata JSON
u32le(len(cbor)) + u32le(2) + zstd frame containing the CBOR-encoded synth state
```

- `metadata.hash` is the MD5 of the zstd frame. This was checked against presets saved by Serum 2.
- The state starts from Serum 2's own Init patch (decoded from a file saved by Serum 2.0.19). The engine's CBOR encoder re-encodes that patch byte-for-byte identical to what Serum wrote, and writes every map with its keys in byte order, as Serum does.
- The zstd frame uses uncompressed ("raw") blocks. That is valid zstd and keeps the tool free of dependencies; a preset is about 10 KB.

### Sources for parameter names and units

- [serum2vital](https://github.com/btesser/serum2vital): Serum 2 effect type ids 0–15 and module keys, effect parameter names and units from a survey of 715 factory presets, filter type names, modulation source ids, level and master laws, and single-change presets saved by Serum 2 (synced delay times, LFO rates, reverb types, chorus, compressor).
- [kasperikaivola/serum-preset-packager](https://github.com/kasperikaivola/serum-preset-packager): the parameter ids Serum 2 uses in modulation routes (for example filter cutoff = 3, effect mix = 1), checked there against Serum 2's own Serum 1 import.
- A commercial Serum 2 preset (in the serum-preset-packager tests): the shape of the mod matrix, the triangle LFO curve and the wavetable position range.

## What is and isn't set

| Section | Set | Notes |
|---|---|---|
| Osc A / B | on/off, wavetable position, octave, semitone, fine, pan, unison, detune, width, level | Uses the factory `Analog/Basic Shapes` table; the waveform positions are approximate |
| Sub, Noise | on/off, level, sub octave and shape | |
| Filter 1 | on, type (MG Low 12/18/24, Low 12, High 12/24, Dirty MG, Comb +), cutoff, resonance, drive, keytrack | A, B, C and noise go through it; the sub stays direct |
| Env 1 / Env 2 | attack, hold, decay, sustain, release | Env 2 is routed to the filter cutoff |
| LFO 1 | tempo-synced rate (4 bars to 1/32), triangle or ramp shape, amount | Routed to cutoff, wavetable position, pitch or volume |
| Global | mono, legato, glide, polyphony | |
| Macros 1–4 | names and routes | Brightness, Grit, Space, Width; effects a macro needs are added at 0% mix |
| FX rack | Distortion (13 modes), Chorus, Flanger, Phaser, Hyper/Dimension, EQ, Compressor (Punch and the output stage), Delay (synced, dotted, ping-pong), Reverb (Plate, Hall, Vintage) | Not yet: Bode, Convolve, Utility, Splitters |
| Not set | other wavetables, warp modes, more than one LFO or envelope route | Add these in Serum after loading |

## Test

```
node test/verify.js
```

The test builds presets from sample prompts (sound types and genres), a design with every module on, hand-broken designs, variations, random sounds and sample Claude answers. It decodes each file with its own CBOR decoder and Node's zstd, and checks that the hash, the container fields, key order and the full state match. It also compares against values Serum 2 saved itself (synced delay times and LFO rates), checks the mod matrix and macro routes, and checks that the Claude answer schemas follow the structured-output rules. It needs Node 22.15 or newer.

## Files

| File | What it does |
|---|---|
| `index.html` | The page and its styles |
| `app.js` | Page logic: editor, keyboard, level matching, batch list, saving, Claude requests |
| `engine.js` | Text and Claude answers to designs, variations, random sounds, and designs to `.SerumPreset` files |
| `synth.js` | The Web Audio preview synth and loudness measurement |
| `config.js` | The presets folder; the Windows installer rewrites it |
| `Installer/` | The Inno Setup script, app icon and FL Studio setup steps |
