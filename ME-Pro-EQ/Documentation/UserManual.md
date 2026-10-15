# ME Pro EQ — User Manual

ME Pro EQ is a 16-band equalizer with analog-matched filter curves, dynamic EQ on any bell or shelf band,
per-band stereo routing (Stereo, Left, Right, Mid, Side), a real-time pre/post spectrum analyser, a
zero-latency minimum-phase engine and an optional linear-phase mode.

## The screen

| Area | What it does |
|---|---|
| Header | **UNDO / REDO**, preset browser (**<** name **>** **SAVE**), **A / B** comparison with copy, **AUTO GAIN**, **MONO** (mono-compatibility listening, never rendered), **BYPASS** (host-automatable global bypass). |
| Graph | Frequency response of every band and the combined response; real-time spectrum (input = dark line, output = filled). |
| Band strip | 16 band slots. Coloured = band exists (dim = switched off). Click an empty slot to create a band there. A dot marks bands with dynamics enabled. |
| Band panel | All settings of the selected band, including the dynamic section. |
| Meters | Clip indicator (click to reset), **phase mode** (Min phase / Linear) and linear-phase quality, input and output sample-peak meters (peak hold 1.5 s), **OUT − IN** level difference with **MATCH**, input and output trim (±24 dB), engine status (sample rate, channel layout, latency reported to the host). |

## Working in the graph

| Action | Result |
|---|---|
| Double-click empty area | Create a band (bell; below 25 Hz a high-pass, above 18 kHz a low-pass) |
| Drag a node | Horizontal: frequency. Vertical: gain (for high/low-pass, notch, band-pass and all-pass: Q) |
| Shift + drag | Fine adjustment (10× slower) |
| Alt + drag | Lock to the dominant axis |
| Mouse wheel over a node | Q (Shift = fine) |
| Double-click a node | Switch the band on/off |
| Right-click a node | Band menu: type, slope, stereo mode, dynamics, audition, reset, delete |
| Right-click empty area | Add a band of a chosen type, display range, analyser settings |
| Keyboard (graph focused) | ←/→ frequency (1/24 oct, Shift 1/96), ↑/↓ gain (0.5 dB, Shift 0.1), PageUp/PageDown Q, Space on/off, Delete remove, Tab/Shift+Tab next/previous band, S audition |

The ±6 / ±12 / ±18 / ±30 buttons change the graph's dB range (display only).

Curves show the **exact** response of the filters the audio engine is running at the current sample rate
— not an idealised drawing. When bands use Left/Right/Mid/Side routing, additional combined curves appear
in blue (L), red (R), green (M) and violet (S); the white curve is the response common to both channels.

## Knobs and numeric entry

- Drag up/down (or left/right). **Shift** while dragging = fine.
- **Double-click** a knob to type a value. Units are understood: `1.2k`, `1200 Hz`, `-3.5 dB`, `40 ms`.
- **Ctrl/Cmd/Alt + click** resets to the default value.
- Mouse wheel adjusts; arrow keys step when the knob has keyboard focus.

Parameter values are stored with full floating-point precision in your project.

## Filter types

| Type | Controls | Notes |
|---|---|---|
| Bell | Freq, Gain, Q | Symmetric boost/cut; Q relates to the bandwidth between the half-gain points: 1/Q = 2·sinh(ln2/2 · BW) |
| Low shelf / High shelf | Freq, Gain, Q, Slope 6–48 dB/oct | Freq is the shelf midpoint (half the gain). Q > 0.71 adds a resonant bump (12 dB/oct), Q is limited to 4 |
| High pass / Low pass | Freq, Q, Slope 6–96 dB/oct | Butterworth at Q 0.71: exactly −3.01 dB at Freq, no pass-band ripple or overshoot at any frequency. Other Q values add/remove resonance on the most resonant section (limited to Q 10) |
| Notch | Freq, Q | Infinite depth at Freq |
| Band pass | Freq, Q | 0 dB at Freq |
| Tilt | Freq (pivot), Gain, Slope 6–48 dB/oct | Gain is the total tilt: −Gain/2 at low frequencies, +Gain/2 at high frequencies, 0 dB at the pivot. The node sits at the +Gain/2 high-frequency plateau |
| All pass | Freq, Q | Phase only: −180° at Freq, magnitude flat |

Frequencies above 0.49 × sample rate (e.g. 21.6 kHz at 44.1 kHz) are designed at that limit; the graph shows
the response actually in use. Bells and shelves set above about a quarter of the sample rate (11–12 kHz at
44.1/48 kHz) keep their shape (no overshoot) but are narrower/steeper than an ideal analog curve; running
the project at 88.2/96 kHz reduces this. Measured figures: `Documentation/DSP_Design.md`.

## Stereo routing

Each band processes **Stereo** (both channels, linked), **Left**, **Right**, **Mid** ((L+R)/2) or **Side**
((L−R)/2). Mid/Side bands are encoded and decoded around the band only; a band at 0 dB leaves the signal
bit-for-bit untouched. On a mono track Left, Right and Side bands have no effect (the band panel says so).

## Dynamic EQ

Available on bell and shelf bands. The band measures the level **in its own frequency region** (bell:
band-pass at the band's frequency and Q; low shelf: low-pass; high shelf: high-pass at the shelf frequency)
and moves its gain accordingly:

| Control | Range | Meaning |
|---|---|---|
| Threshold | −80…0 dBFS | Level (of the band-limited detector signal) where the dynamic action starts (6 dB soft knee) |
| Ratio | 1:1…20:1 | Gain change per dB over threshold = (1 − 1/ratio) |
| Attack | 0.1…250 ms | Exponential time constant of the detector's rise |
| Release | 5…2000 ms | Exponential time constant of the detector's fall (peak hold with release) |
| Range | −18…+18 dB | Maximum dynamic gain. Negative = reduction (tames a resonance when it gets loud). Positive = enhancement (brings out a region when it is active) |
| Detector | Peak / RMS | Peak follows waveform peaks; RMS averages power over about 2.5 cycles of the band frequency |
| Sidechain | Internal / External | External listens to the plugin's sidechain input instead of the track itself |

The static Gain still applies; the dynamic gain is added to it (the sum is limited to ±30 dB). The live gain
change is drawn in the graph (vertical marker on the node and a second curve) and shown as DYN GAIN in the
band panel — both read the value the audio engine is applying.

Dynamic processing updates the filter every 8 samples at 44.1/48 kHz (0.17 ms) with per-sample
interpolation in between.

**External sidechain in FL Studio** (follows FL Studio's documented sidechain workflow; not yet verified
by us inside FL Studio): route the key track to the mixer track that holds ME Pro EQ (right-click the
routing arrow → "Sidechain to this track"), then in the plugin wrapper's settings (*Processing* tab) map
that sidechain input to the plugin's sidechain bus. If the host sends no sidechain signal while External is selected, the detector hears
silence and the band panel shows a warning.

## Audition (solo) and mono check

- **SOLO** (band panel, band menu, or key **S**): you hear only the region of the selected band after the
  EQ — bell/notch/band-pass: a band-pass at the band frequency and Q; low shelf: low-pass; high shelf:
  high-pass; cut filters and tilt: band-pass (Q 0.71) at the band frequency. Mid/Side/Left/Right bands
  play the routed component on both speakers. An amber banner is shown while auditioning.
- **MONO** in the header plays (L+R)/2 on both channels for a mono-compatibility check.

Both are monitoring functions: they are never saved with the project, they switch off when the editor is
closed (audition), and they are **ignored during offline rendering/export**. In linear-phase mode, audition
follows the (delayed) output.

## Spectrum analyser

Right-click the graph → *Analyzer*:

- input (pre EQ) / output (post EQ) on or off, freeze, peak hold (and reset);
- resolution: FFT 2048 … 32768 points (75 % overlap, Blackman-Harris window);
- smoothing: off (highest bin per pixel, peaks never hidden) or 1/12, 1/6, 1/3 octave;
- decay: 10…120 dB/s;
- display tilt 0 / 3 / 4.5 dB/oct (display only).

Calibration: a steady sine wave of peak amplitude 1.0 reads 0 dBFS (left scale) when it falls on an FFT
bin; between bins it reads up to 0.8 dB lower. Broadband signals read lower per bin as the FFT size grows,
because their energy is spread over more bins — compare levels only at the same analyser settings. The FFT
runs on the user-interface thread, never on the audio thread, and costs nothing while the editor is closed.

## Input / output

Input and output trim: ±24 dB, smoothed. The clip indicator lights when an output sample exceeds 0 dBFS.
Global bypass crossfades (20 ms) to the untouched input; when fully bypassed the output is the input,
bit for bit (in linear-phase mode: the input delayed by the reported latency).

## Phase mode: minimum phase or linear phase

The selector under CLIP in the meter strip chooses how the curve is realised:

- **Min phase** (default): zero latency. Like analog equalizers, frequencies around a boost or cut are
  shifted in phase. Dynamic EQ works only in this mode.
- **Linear**: the same magnitude curve without any phase shift between frequencies — useful on buses,
  parallel or multi-microphone sources where phase alignment matters. The price:
  - **Latency** (reported to the host, which compensates it): Low latency 91 ms, Balanced 176 ms,
    High resolution 347 ms at 48/96/192 kHz (about 9 % more at 44.1/88.2/176.4 kHz: 99 / 192 / 377 ms).
    Not suitable for live input monitoring.
  - **Low-frequency resolution**: the filter has a finite length, so very steep or very narrow settings
    below about 100 Hz are smoothed (measured example: a 48 dB/oct high-pass at 30 Hz plus a bell at 60 Hz,
    Balanced quality, differs by up to 2.4 dB from the designed curve between 20 and 50 Hz, and by less than
    0.1 dB above 50 Hz). The graph draws the response actually running
    as a dashed line; choose High resolution if the dashed line departs from the curve where it matters.
  - **Pre-ringing**: sharp low-frequency features smear energy slightly *before* transients, which can be
    audible on kick drums and bass. If you hear it, use Min phase.
  - **Dynamic EQ is inactive**: dynamic bands apply their static settings.
  - Switching the mode during playback causes a short dropout (the new mode starts from silence).

The phase mode and quality are saved with your project, but are not part of presets, A/B or undo (they
change the plugin's latency) and cannot be automated. CPU use in Linear mode is higher; see
`ValidationReport.md`.

## Auto gain and level matching

Louder usually sounds "better", which makes EQ decisions unreliable. Two tools help you compare at equal
loudness:

- **AUTO GAIN** (header, host-automatable, saved with the project): offsets the output by the estimated
  loudness change of the current EQ curve. The estimate weights the curve like pink noise between 20 Hz
  and 20 kHz. It follows the static curve only; dynamic EQ movements are not compensated. The applied
  value is shown on the button (e.g. *AUTO −1.4 dB*). For material far from pink noise (an isolated 808,
  a hi-hat) the estimate is less accurate — use MATCH instead.
- **OUT − IN** and **MATCH** (meter strip): the difference between output and input level, measured as
  RMS over about the last 3 seconds of audio. Play typical material, then click MATCH: the output trim is
  set so the difference becomes 0 dB. MATCH is one undoable step. When the input is silent the readout
  shows "−" and MATCH is disabled.

## Undo / redo

Every change you make in the ME Pro EQ window can be undone: **UNDO / REDO** in the header, or **Ctrl+Z**,
**Ctrl+Shift+Z** / **Ctrl+Y** while the plugin window has keyboard focus (in some hosts the host's own
shortcut handling takes priority; the buttons always work).

- A whole drag is one step; creating, resetting or deleting a band is one step; loading a preset is one
  step.
- Up to 100 steps are kept. Host automation and changes made through the host's generic controls are not
  recorded.
- Loading a project starts with an empty history. A/B slots each have their own history (see below).

## A/B comparison

**A** and **B** hold two complete settings (everything except global bypass). The first time you click
**B**, it starts as a copy of A; from then on the two are independent. **A > B** (or **B > A**) copies the
active slot into the other one; it is enabled only while the slots differ. Both slots are saved with your
project.

Each slot keeps its own undo history, so UNDO in slot B only steps back through changes made in B. Switching
slots is not itself an undo step and is not possible in the middle of a drag.

## Presets

The preset name in the header opens the browser; **<** and **>** step through the list.

- **Factory presets** (35, read-only, built into the plugin) in six categories: Vocals, Drums, Bass,
  Instruments, Punjabi Production (dhol, dholak, tabla, tumbi, harmonium, bansuri) and Bus Processing.
  They are starting points for typical sources — always adjust frequencies and amounts by ear for your
  material; no preset can suit every recording.
- **User presets**: **SAVE** stores the current settings under a name of your choice (factory names are
  reserved). Files are kept in `Documents\ME Audio\ME Pro EQ\Presets` as `.mepreset` (XML); you can copy
  them between computers. The browser's menu can delete the loaded user preset and open the folder.
- A preset contains every setting except global bypass, including the trims and auto gain.
- A `*` after the name means the settings were changed after loading. Loading a preset is one undoable
  step, so you can always get back to what you had.

## Precision and safety

- All processing is 64-bit floating point internally (the host may run the plugin in 32- or 64-bit).
- Parameter changes are smoothed (20 ms); changing filter type, slope or stereo routing crossfades
  between two filters (15 ms); switching a band on/off fades (10 ms).
- Non-finite input samples (NaN/Inf from a faulty upstream plugin) are replaced by silence before
  filtering.

## Known limitations of this version

- No intelligent analysis suggestions yet (planned).
- ME Pro EQ has **not yet been tested inside FL Studio**. It passes pluginval (strictness 10) and the
  Steinberg VST3 validator, which exercise the same VST3 interface FL Studio uses, but those tools are not
  FL Studio. `Documentation/ValidationReport.md` lists exactly what was and was not tested.
