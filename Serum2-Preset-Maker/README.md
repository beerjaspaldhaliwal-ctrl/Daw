# Serum 2 Preset Maker

Type a description like `dark wide reese bass with distortion` and get a `.SerumPreset` file for Xfer Serum 2.
Hear it in the browser, fine-tune every setting, make variations, or make a whole pack at once.

## Install on Windows (with FL Studio)

1. On GitHub, open **Actions → Serum 2 Preset Maker**, click the latest green run, and download **Serum-2-Preset-Maker-for-FL-Studio-Setup** under *Artifacts*.
2. Unzip it and run `Serum 2 Preset Maker for FL Studio Setup.exe`. It installs for your Windows user only, so no admin password is needed.
   If Windows SmartScreen says "Windows protected your PC", click **More info → Run anyway** (the installer isn't code-signed).
3. The installer creates the folder your presets are saved to:
   - `Documents\Xfer\Serum 2 Presets\Presets\User\Preset Maker` if Serum 2 is installed, so the presets also show in Serum 2's own browser
   - `Documents\Serum 2 Preset Maker\Presets` otherwise
4. Set up FL Studio once (the installer shows these steps and can open the folder for you):
   - Drag the presets folder from File Explorer onto FL Studio's Browser.
   - In the Browser's options menu, turn on **Show unknown file types**, so FL Studio lists `.SerumPreset` files.
5. Open **Serum 2 Preset Maker** from the Start menu or desktop, make a preset and click **Save to presets folder** (Edge or Chrome; the first time, pick the folder from step 3). Then drag it from FL Studio's Browser onto Serum 2.

The installer doesn't change FL Studio's settings itself. To remove it: **Settings → Apps → Installed apps → Serum 2 Preset Maker → Uninstall**. Your presets folder is kept.

## Use it

1. Open `index.html` in a browser (or use the installed shortcut).
2. Describe the sound and press **Make preset**. Or open the **Pack** tab, write one description per line and press **Make pack**.
3. Press **Play demo**, click the on-screen keys, or play with your computer keyboard (`A`–`;`, `Z`/`X` for octave) to hear a preview.
4. Adjust anything in the editor: oscillators, sub, noise, filter, envelope, voicing and effects. **Undo my changes** goes back to what was generated.
5. **Make variations of this sound** gives 4–8 nearby versions. Play them, untick the ones you don't want, and save the rest together.
6. **Save to presets folder** (Edge or Chrome, installed version) or **Download**. A batch is saved into a subfolder named after it, or downloaded as one `.zip`.

The preview is a small Web Audio synth that follows the same settings. It is an approximation: Serum 2's oscillators, filter and effects sound richer.

### Reading the text

- **Keywords** (instant, offline): sound types (bass, reese, 808, lead, supersaw, pad, pluck, keys, bell, stab, arp), tone words (dark, warm, bright, wide, detuned, dirty, punchy, …) and exact values (`attack 10ms`, `release 1.2s`, `cutoff 800hz`, `resonance 40`, `unison 7`, `octave -1`). Put a name in quotes or write `called My Preset` to name it.
- **Claude AI**: Claude reads the description and picks every setting, so any words work, and explains its choices. Online (claude.ai) it uses your Claude account. The installed version needs an Anthropic API key from console.anthropic.com, which is billed to that account; it calls `claude-opus-5-5` with structured outputs from the browser using the official SDK. If Claude can't answer, the keyword reading is used and the page says why.

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
| Sub, Noise | on/off, level, *sub octave (guess)* | The sub is set an octave down by default, under the oscillators |
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

The test builds presets from sample prompts, from hand-broken designs, from variations and from sample Claude answers. It decodes each file with its own CBOR decoder and Node's zstd, and checks that the hash, the container fields and the full state match. It also checks that the Claude answer schemas follow the structured-output rules. It needs Node 22.15 or newer.

## Files

| File | What it does |
|---|---|
| `index.html` | The page and its styles |
| `app.js` | Page logic: editor, keyboard, batch list, saving, Claude requests |
| `engine.js` | Text and Claude answers to designs, variations, and designs to `.SerumPreset` files |
| `synth.js` | The Web Audio preview synth |
| `config.js` | The presets folder; the Windows installer rewrites it |
