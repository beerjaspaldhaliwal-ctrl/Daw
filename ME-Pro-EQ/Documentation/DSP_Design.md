# ME Pro EQ — DSP design

This document describes how ME Pro EQ processes audio and how accurate it is. Every number in it comes
from the automated test suite (`meproeq_dsp_tests`, report: `reports/dsp_test_report.md` in the build
folder). When a number here and the report disagree, the report is the truth and this document is out of
date.

Version described: 0.1.0 (milestone M2). Engine mode: minimum phase, zero latency. A linear-phase mode is
planned for M3 and is **not** implemented yet.

## 1. Signal flow

```
input ─► sanitize (NaN/Inf → 0) ─► input meter ─► [dry copy] ─► input trim ─► pre-EQ analyser tap
      ─► band 1 … band 16 (serial) ─► band audition (monitoring) ─► output trim + auto gain
      ─► bypass crossfade with dry ─► mono fold (monitoring) ─► non-finite guard ─► output meter
      ─► level RMS ─► post-EQ analyser tap ─► output
```

- All processing is 64-bit floating point. The host may call the plugin with 32-bit or 64-bit buffers;
  both entry points run the same double-precision engine (test `Engine.FloatAndDoubleEntryPointsAgree`:
  max difference 0 after float rounding).
- Flush-to-zero / denormals-are-zero is enabled for the duration of `processBlock`
  (`Engine.DenormalsDoNotSlowProcessing`: 4 s of 16-band stereo audio takes 0.0179 s with normal input,
  0.0178 s with subnormal input, 0.0189 s with 1e-300 input).
- Nothing in `processBlock` allocates memory, takes a lock, performs I/O or waits on another thread.
  Buffers are sized in `prepareToPlay`; the analyser uses a wait-free single-producer/single-consumer FIFO.

## 2. Filter design

Each band is a cascade of first- and second-order sections ("the analog prototype") designed in the
normalised s-plane, then mapped to digital sections.

### 2.1 Analog prototypes

| Type | Prototype |
|---|---|
| Bell | (s² + (A/Q) s + 1) / (s² + s/(A Q) + 1), A = 10^(G/40) — constant-Q, boost/cut symmetric |
| Low / high shelf | Cascade of second-order shelves with Butterworth-distributed Q (plus one first-order shelf for odd orders), the gain split evenly over the order. The user Q scales the most resonant section (limit Q ≤ 4) |
| High / low pass | Butterworth cascade of order N = slope / 6 (6 … 96 dB/oct, orders 1 … 16). The user Q scales the most resonant section (limit Q ≤ 10) |
| Notch | (s² + 1) / (s² + s/Q + 1) |
| Band pass | (s/Q) / (s² + s/Q + 1), 0 dB at f0 |
| Tilt | Shelf cascade with gain G and an overall −G/2 offset: −G/2 below the pivot, +G/2 above |
| All pass | (s² − s/Q + 1) / (s² + s/Q + 1) |

The Q of a bell relates to the bandwidth BW (octaves) between the half-gain (dB) points by
1/Q = 2 sinh(ln 2 / 2 · BW) (`Filter.BellBandwidthMatchesQ`: max error 0.046 octave for f0 ≤ 4 kHz).

### 2.2 Analog → digital mapping

Two mappings are used, chosen per filter type and frequency.

**Analog-matched design (bells, shelves, tilt, notch, band pass below 0.25 fs).** After M. Vicanek,
"Matched Second Order Digital Filters" (2016):

- The lower-frequency singularity pair of each section is mapped exactly with the impulse-invariant
  (matched-Z) map z = e^(sT). Pole angles above 0.9π are softly compressed to stay below 0.995π.
- The numerator is then chosen so that the digital magnitude equals the analog magnitude **exactly** at DC,
  at f0 and at Nyquist (`Filter.ExactAtMatchPoints`: max error 7.5e-14 dB).
- Cuts are designed as the exact inverse of the corresponding boost. This keeps every design stable,
  realisable and minimum-phase, and makes boost and cut exact mirror images
  (`Filter.BoostCutMirrorSymmetry`: max |dB(+G) + dB(−G)| = 3.9e-14).
- All quantities are computed directly in the state-variable-filter parameter domain with `expm1`-based
  formulas, which stay well conditioned for 10 Hz at 192 kHz
  (`Filter.LowFrequencyNumericalPrecision`: 9.3e-6 dB deviation from the analog prototype).

Compared with the bilinear transform, this avoids the "cramping" of bells and shelves towards Nyquist.

**Prewarped bilinear design (high pass, low pass, all pass at all frequencies; every type above 0.40 fs).**
s = j·tan(ω/2)/tan(ω0/2). The bilinear map sends the entire analog frequency axis monotonically onto
0 … Nyquist, so every shape property of the prototype survives:

- Butterworth cut filters have an exact −3.01 dB point at the set frequency
  (max error 2.4e-14 dB) and are strictly monotonic with no overshoot at every frequency and sample rate
  (`Filter.ResponseShapeInvariants`: max non-monotonic step 2.5e-14 dB).
- The all pass reaches −180° exactly at f0.

The price is the bilinear frequency warp. For cut filters up to fs/16 the pass band deviates at most
0.17 dB from the analog prototype (`Filter.CutFilterAccuracyVsAnalogPrototype`). In the stop band the
digital low pass attenuates **more** than the analog prototype because of its zero at Nyquist (up to
13 dB more at the −24 dB point for 44.1 kHz); this is reported, not treated as a defect.

Earlier development versions used magnitude matching for cut filters too. That produced pass-band bumps of
several dB for steep, high cut-offs; the bilinear design replaced it.

**Blend region (0.25 fs … 0.40 fs, bells/shelves/tilt/notch/band pass).** Very close to Nyquist the
analog-matched design cannot follow analog prototypes whose transition extends beyond Nyquist, and it
overshot by up to 22 dB in development measurements. Between 0.25 fs and 0.40 fs the section coefficients
are therefore blended from the analog-matched to the bilinear design with a smoothstep weight. Second-order
sections are blended before the boost/cut inversion, first-order sections geometrically (log domain);
both keep boost/cut mirror symmetry exact. Notch zeros are re-pinned exactly at f0 after blending.

**Design frequency limit.** Frequencies above 0.49 fs are designed at 0.49 fs (e.g. 21.6 kHz at 44.1 kHz).
The graph shows the response actually used.

### 2.3 Realisation

Every section runs as a topology-preserving-transform (trapezoidal) state-variable filter
(A. Simper, "Linear trapezoidal integrated SVF", Cytomic 2013; V. Zavalishin, "The Art of VA Filter
Design") with output mixing y = m0·x + m1·band + m2·low. The digital transfer function of a section is,
exactly,

```
H(ω) = (c_lp − c_hp·u² + j·c_bp·u) / (1 − u² + j·k·u),   u = tan(ω/2) / g
```

so the curve drawn in the editor is the response of the filter that is running, not an idealised drawing
(`Filter.RealisedFilterEqualsDesignedTransferFunction`: measured vs. designed magnitude 7.5e-11 dB, phase
2.3e-11 rad; `Engine.ResponseAtAllSampleRates`: ≤ 1.9e-12 dB at 44.1 … 192 kHz).

The SVF structure stays well behaved under fast coefficient modulation, which matters for automation and
dynamic EQ. Stereo pairs are processed with SSE2 (two channels per instruction).

`Filter.StabilityRealizabilityMinimumPhaseGrid` evaluates 714,432 designs (all types, slopes, gains, Q
values, frequencies 10 Hz … 30 kHz, all supported sample rates): 0 invalid or unstable, 0 non-minimum-phase.

### 2.4 Measured accuracy against the analog prototype

Maximum deviation in dB over 10 Hz … 0.49 fs, from `Filter.BellAccuracyVsAnalogPrototype` /
`Filter.ShelfAccuracyVsAnalogPrototype` (all gains, Q values and slopes in the test grid).

| Sample rate | Bell f0 < fs/8 (max / P95) | Bell fs/8 … fs/4 | Bell ≥ fs/4 | Shelf f0 < fs/8 (max / P95) | Shelf fs/8 … fs/4 (max / P95) |
|---|---|---|---|---|---|
| 44.1 kHz | 0.83 / 0.14 | 0.89 | **15.9** | 1.54 / 0.13 | **3.06 / 1.41** |
| 48 kHz | 0.82 / 0.13 | 0.89 | **15.7** | 1.51 / 0.13 | **3.04 / 1.38** |
| 96 kHz | 0.37 / 0.11 | 0.53 | 0.71 | 1.32 / 0.10 | 1.99 / 0.69 |
| 192 kHz | 0.12 / 0.04 | 0.24 | — | 0.67 / 0.04 | 0.17 / 0.08 |

How to read this honestly:

- For the typical case — bands below fs/8 (5.5 kHz at 44.1 kHz) — 95 % of all measured points are within
  0.14 dB of the analog prototype; the largest deviations are for extreme settings (very narrow, very
  large gains, steep shelves).
- Steep shelves (36/48 dB/oct) between fs/8 and fs/4 at 44.1/48 kHz deviate by up to about 3 dB from the
  analog prototype. The shapes remain monotonic without overshoot (see 2.5), but they are not analog-exact.
- Bells above fs/4 at 44.1/48 kHz (above 11–12 kHz) deviate strongly from the analog prototype because the
  prototype's skirts extend beyond Nyquist and the design blends to the bilinear shape. They remain
  single-peaked with ≤ 0.06 dB overshoot. At 96 kHz the same bells deviate by at most 0.71 dB (88.2 kHz:
  2.2 dB).
- ME Pro EQ does **not** oversample. These figures are the accuracy of the shipped design, not a claim of
  parity with any other product.

### 2.5 Shape guarantees

`Filter.ResponseShapeInvariants` scans 2,500 log-spaced frequencies for every type, slope, gain
(±6, ±24 dB), Q and 40 frequencies per sample rate. Measured worst cases:

| Type | Below fs/4: non-monotonic step / overshoot (dB) | fs/4 and above |
|---|---|---|
| High / low pass | 2.4e-14 / 1.5e-14 | 2.5e-14 / 1.4e-14 |
| Shelves, tilt (Q 0.71) | 0.00018 / 0.046 | 0.00066 / 0.17 |
| Bell | 0.0031 / 0.040 | 0.015 / 0.053 |
| Band pass | 0.0036 / 0.052 | 0.015 / 0.059 |
| Notch | 0 / 0 | 0 / 0 |

Test tolerances: cut filters 1e-6 dB; others 0.01 / 0.075 dB below fs/4 and 0.1 / 0.25 dB above.

## 3. Parameter changes, fades and crossfades

- Control grid: static bands update their coefficients every 16 samples at 44.1/48 kHz (scaled with the
  sample rate); dynamic bands every 8 samples. Between updates each SVF coefficient ramps linearly per
  sample. Ramps are index-based, so the output does not depend on the host block size
  (`Engine.BlockSizeInvariance`).
- Static parameters are smoothed by a one-pole (20 ms) in log-frequency, dB and log-Q.
- Type, slope and stereo-mode changes crossfade between two filter instances (15 ms, raised cosine).
  Band on/off fades 10 ms, global bypass crossfades 20 ms, all with raised-cosine shapes.
- After a reset (start of playback or an offline render) the first control tick jumps straight to the
  target settings, so a render is fully processed from sample 0
  (`Engine.RenderStartIsFullyProcessed`: 5.6e-16 max difference from the ideal filter).

Measured click/zipper residuals (energy above a high-pass, relative to the signal):

| Scenario | Residual |
|---|---|
| Unsmoothed reference: gain jumps 0 ↔ 12 dB every 100 ms | −68.0 dB |
| Same jumps through ME Pro EQ | −114.5 dB |
| Band on/off every 50 ms | −136.4 dB |
| Type/slope/stereo switching every 50 ms | −130.0 dB |
| Bypass toggling every 100 ms | −150.0 dB |
| 150 Hz tone while a bell sweeps 2–16 kHz at 2 Hz | −125.9 dB |

## 4. Stereo routing

Each band processes Stereo (both channels, identical filter), Left, Right, Mid or Side. Mid/Side bands
encode M = (L+R)/2, S = (L−R)/2 around the band and decode afterwards; a band at 0 dB is skipped, so the
signal stays bit-exact (`Engine.MidSideIntegrity`: reconstruction error 0; `Filter.IdentityBandIsBitExact`).

## 5. Dynamic EQ

Available on bell and shelf bands.

- **Detector filter** (band-matched): bell → band pass at f0 and Q; low shelf → 2nd-order low pass at f0;
  high shelf → 2nd-order high pass at f0. Input: the band's own routed signal, or the external sidechain.
  `Dynamic.DetectorIsFrequencySelective`: a −6 dBFS 200 Hz tone into a 3 kHz dynamic band causes 0 dB
  reduction; with 3 kHz reduced by 12 dB the 200 Hz component changes by 0.018 dB.
- **Level**: Peak (two-stage smooth decoupled detector: peak hold with release, then attack smoothing) or
  RMS (window ≈ 2.5 periods of f0).
- **Gain computer**: 6 dB soft knee, gain = over · (1 − 1/ratio), limited to Range (−18 … +18 dB); the
  total band gain (static + dynamic) is limited to ±30 dB.
- **Timing** (`Dynamic.AttackAndReleaseTiming`): attack 1 / 10 / 50 ms → measured 63 % rise 1.83 / 10.67 /
  50.67 ms; release 50 ms (attack 1 ms) → 51.5 ms (expected 51.0 ms from the two-stage model).
- **Modulation smoothness** (`Dynamic.GainModulationIsSmooth`): HF residual of the production control grid
  −88.8 dB vs. −88.4 dB for an ideal per-sample redesign.

The gain shown in the editor is read back from the engine (`Dynamic.ReportedGainMatchesAppliedGain`).

## 6. Auto gain and level comparison

Auto gain (off by default) estimates the static loudness change of the complete curve as the
pink-noise-weighted power gain over 48 log-spaced points between 20 Hz and 20 kHz, per channel domain, and
applies the negative of it at the output stage (smoothed with the output trim). Dynamic gain changes are
not compensated. `Engine.AutoGainMatchesPinkNoiseLoudness` (band-limited pink noise):

| Curve | Level change, auto gain off | with auto gain on |
|---|---|---|
| Bell +6 dB, 1 kHz, Q 1 | +1.44 dB | +0.02 dB |
| Low shelf −6 dB 200 Hz + high shelf +4 dB 8 kHz | −0.42 dB | −0.06 dB |
| Side-only bell +9 dB 3 kHz | +1.83 dB | +0.02 dB |

Material that is not pink-like (e.g. a bass-heavy 808 or a bright hi-hat) will be compensated less
accurately; that is why the applied value is always shown.

Level comparison publishes input and output RMS (exponential, 3 s time constant). The editor shows
OUT − IN and MATCH sets the output trim to cancel the measured difference (undoable).

## 7. Spectrum analyser

- Own radix-2 real FFT (`Analyzer.FFTMatchesNaiveDFT`: 5e-13 relative error), 2048 … 32768 points, 75 %
  overlap, 4-term Blackman-Harris window.
- Calibration: a full-scale sine on a bin centre reads 0 dBFS; between bins it reads −0.83 dB (window
  scalloping, measured).
- Smoothing: off (maximum bin per pixel), or 1/12, 1/6, 1/3 octave (box average via prefix sums, then a
  triangular second pass); release ballistics in dB/s; peak hold; freeze.
- The audio thread only pushes samples into a wait-free FIFO, and only while an editor shows the analyser.
  FFTs run on the UI thread.

## 8. Known limitations (M2)

- No linear-phase mode yet (M3). The engine is minimum phase with 0 samples latency.
- No oversampling; accuracy near Nyquist at 44.1/48 kHz is as measured in section 2.4.
- Dynamic EQ is available on bell and shelf bands only (not on cut filters, notch, band pass, tilt or
  all pass).
- Intelligent analysis suggestions are not implemented yet (M3).

## References

- M. Vicanek, "Matched Second Order Digital Filters", 2016.
- A. Simper, "Linear Trapezoidal Integrated State Variable Filter With Low Noise Optimisation", Cytomic
  technical paper, 2013.
- V. Zavalishin, "The Art of VA Filter Design", rev. 2.1.2, Native Instruments, 2020.
- F. J. Harris, "On the use of windows for harmonic analysis with the discrete Fourier transform",
  Proc. IEEE 66(1), 1978.
- R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients" (bandwidth/Q relation
  only).
