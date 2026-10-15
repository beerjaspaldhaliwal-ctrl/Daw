# ME Pro EQ — validation report

What was tested, where, and with what result. **PASS** = executed and passed. **FAIL** = executed and
failed. **NOT TESTED** = not executed; nothing is claimed about it.

Version: 0.1.0, milestone M3. Results are reproduced by the GitHub Actions workflow
`.github/workflows/me-pro-eq.yml` on every push; its uploaded `*-reports` artifacts contain the full test
reports, validator logs, benchmark tables and editor screenshots.

## Environments

| Environment | Details |
|---|---|
| Linux development VM | Ubuntu 24.04, GCC 13.3.0, CMake 3.28.3, Intel Xeon @ 2.10 GHz (4 logical cores), headless (Xvfb for GUI tests) |
| GitHub Actions — Windows | `windows-2022` runner (Windows Server 2022; JUCE reports it as "Windows 10"), Visual Studio 2022 (MSVC, x64, Release); the processor model varies between runs |
| GitHub Actions — Linux | `ubuntu-24.04` runner, GCC, Ninja, Release |
| Dependencies | JUCE 9.0.4 (commit `3fb5cbc0`, verified at configure time), VST3 SDK 3.8.0 (validator), pluginval 1.0.4, Inno Setup 6 |

## Summary

| Check | Linux (local VM, latest code) | Windows (GitHub Actions) ¹ |
|---|---|---|
| Build: DSP library, VST3, tests, benchmark | PASS | PASS |
| DSP test suite | PASS 75/75 (19,793 checks) | PASS 66/66 (19,742 checks) |
| Plugin host-level test suite | PASS 23/23 (1,329 checks) | PASS 22/22 (1,315 checks) |
| ThreadSanitizer run of the linear-phase tests (3 runs) | PASS (no reports) | — |
| pluginval 1.0.4, strictness 10, in-process | PASS (SUCCESS, 25 test groups) | PASS (SUCCESS, 25 test groups) |
| Steinberg VST3 validator 3.8.0, extensive (`-e`) | PASS 537/537 | PASS 537/537 |
| Windows installer builds (`ME Pro EQ Setup.exe`) | — | PASS (built) |
| Installer runs, installs to `C:\Program Files\Common Files\VST3`, uninstalls | — | **NOT TESTED** |
| CPU benchmark 1/8/16/32 instances (minimum and linear phase) | PASS (measured, below) | PASS (measured, below) |
| **Loading and using ME Pro EQ in FL Studio** | — | **NOT TESTED** |
| Any other DAW (Reaper, Ableton Live, Cubase, …) | **NOT TESTED** | **NOT TESTED** |
| Real audio interface / real-time audio thread of a DAW | **NOT TESTED** | **NOT TESTED** |
| High-DPI rendering on a real Windows display | — | **NOT TESTED** (offscreen 2× rendering is tested) |
| Listening tests | **NOT TESTED** | **NOT TESTED** |

¹ Windows column: workflow run 38721829366 on commit `888d52d` (linear-phase mode). The analysis-suggestion
tests (9 DSP tests, 1 plugin test) were added after that commit; their Windows run is listed under "CI runs"
below once it has completed.

## What the automated tests cover

The test programs print every measured value into a report (`reports/dsp_test_report.md`,
`reports/plugin_test_report.md`). Highlights; the full method and figures are in `DSP_Design.md`.

**Frequency response and phase (minimum phase)**
- Every realised filter equals its designed transfer function: 7.5e-11 dB magnitude, 2.3e-11 rad phase.
- Engine response at 44.1, 48, 88.2, 96, 176.4, 192 kHz equals the design within 1.9e-12 dB.
- Accuracy against the analog prototypes, per sample rate and frequency region (including the weak spots
  near Nyquist at 44.1/48 kHz: up to 3 dB for steep shelves between fs/8 and fs/4, up to 15.9 dB for bells
  above fs/4).
- Exact values at DC, f0 and Nyquist; Butterworth −3.01 dB points (2.4e-14 dB error); slopes 6 … 96 dB/oct;
  bell bandwidth vs Q; boost/cut mirror symmetry (3.9e-14 dB).
- Shape invariants over 2,500 frequencies for every type, slope, gain, Q, frequency and sample rate
  (monotonic cut filters, no shelf overshoot beyond 0.17 dB, single-peaked bells).

**Linear phase**
- FIR exactly symmetric (5e-17 of the peak); impulse response peaks exactly at the reported latency; phase is
  a pure delay (≤ 9.3e-7 rad where |H| > −40 dB).
- Magnitude vs the minimum-phase design at 44.1/48/96 kHz and every quality: ≤ 0.04 dB above 200 Hz for all
  test curves; low-frequency deviations as listed in `DSP_Design.md` section 8 (e.g. 2.4 dB between 20 and
  50 Hz for a 48 dB/oct high-pass at 30 Hz with Balanced quality).
- Stereo routing per channel domain (≤ 0.0005 dB), no channel leakage, mono-compatible input stays mono.
- Phase-mode switching −129 dB, bypass toggling −179 dB, parameter jumps −144 dB HF residual.
- Worker-thread hand-over under per-block parameter changes; offline renders bit-identical (engine and
  plugin level). The hand-over test found a real data race during development (an FFT scratch buffer shared
  by the design thread and the audio thread); it was fixed, and 30 repeated runs plus ThreadSanitizer runs
  of the suite are clean.

**Analysis suggestions**
- No suggestions for pink noise (3 sample rates × 2 seeds) or for silence / too little material.
- Planted resonances found at the exact frequency with Q within 25 %; applying the suggestion and analysing
  again removes the finding; low-mid hump, rumble (and no high-pass without rumble), static and dynamic
  harshness, sibilance bursts (the suggested dynamic band reduces 6 dB during bursts, < 0.02 dB between),
  missing air (low confidence).
- Editor level: analysing never changes a parameter or the undo history; APPLY creates exactly the
  suggested band as one undo step; applying twice does nothing.

**Stability**
- 714,432 designs: 0 unstable, 0 non-minimum-phase. Extreme impulse responses decay.
- 10 minutes of continuous processing; random parameter storms (20 s); sample-rate and block-size
  changes; NaN/Inf/denormal input; repeated create/destroy; 32 instances processed concurrently on 32
  threads, each matching a single-threaded render.

**Automation and switching (minimum phase)** (residual energy above a high-pass, relative to the signal)
- Gain jumps −114.5 dB; band on/off −136.4 dB; type/slope/stereo switching −130.0 dB; bypass −150.0 dB;
  frequency sweeps −125.9 dB. Output is identical for any host block size.

**Dynamic EQ**
- Gain curve, frequency selectivity, attack/release timing, external sidechain, enhancement limits,
  sibilance and drum-resonance scenarios, smooth gain modulation, reported gain = applied gain.

**Stereo**
- Mid/Side reconstruction exact (error 0), channel isolation for Left/Right/Mid/Side bands, mono-bus
  behaviour.

**Host level (real AudioProcessor)**
- 262 parameters with stable IDs and readable text; all automatable except phase mode and linear-phase
  quality (they change the latency); bus layouts (mono, stereo, sidechain); float and double rendering;
  state save/load exact and byte-identical on re-save; older and corrupt states; monitoring functions
  (audition, mono) ignored during offline rendering.
- Linear phase: latency reported after switching (message thread) and after state restore + prepare;
  impulse at exactly the reported latency; quality change; phase mode untouched by presets and A/B.
- Presets: all 35 factory presets parse, reference only existing parameters with in-range values, load as
  one undo step and render finite audio; user preset save/load/delete in a temporary folder; corrupt preset
  files ignored; preset name and modified flag saved with the project.
- Undo/redo, A/B with separate histories, auto gain, MATCH, editor rendering at 1220×760, 980×620,
  1800×1100 and 2× scale.

## Validators

**pluginval 1.0.4**, `--strictness-level 10 --validate-in-process`: SUCCESS on Linux (local and CI) and
Windows (CI). All 25 test groups ran, among them: open plugin cold/warm, editor, open editor whilst
processing, audio processing, plugin state and state restoration, automation and editor automation,
parameter thread safety, background-thread state, bus layouts and fuzzed parameters.

**Steinberg VST3 validator** (VST3 SDK 3.8.0, extensive mode): 537 tests passed, 0 failed on Linux and
Windows.

## CPU and memory

All instances processed sequentially on one thread (as on one DAW audio thread), 48 kHz, 256-sample
blocks, 10 s of stereo noise through the 32-bit host entry point. Load = processing time / audio time
(100 % = one core). These are shared virtual machines: read the numbers as indications. No measurement on a
desktop PC has been made yet.

| Configuration | Instances | Linux VM, Xeon @ 2.1 GHz (GCC) | Windows CI, AMD EPYC 7763 (MSVC, run 38721829366) |
|---|---|---|---|
| Typical vocal: 6 static bands | 1 / 8 / 16 / 32 | 0.30 / 1.84 / 3.79 / 8.75 % | 0.30 / 2.44 / 4.95 / 10.21 % |
| 8 bands, 2 dynamic, analyser on | 1 / 8 / 16 / 32 | 0.50 / 4.41 / 8.82 / 17.83 % | 0.67 / 5.39 / 10.92 / 21.79 % |
| Worst case: 16 bands, 96 dB/oct cuts, 8 dynamic, M/S, analyser | 1 / 8 / 16 / 32 | 1.89 / 15.78 / 33.27 / 64.38 % | 2.23 / 17.86 / 35.77 / 71.74 % |
| Typical vocal, **linear phase** Balanced (16k taps) | 1 / 8 / 16 / 32 | 0.94 / 11.58 / 21.38 / 44.76 % | 1.47 / 12.24 / 27.04 / 55.88 % |
| Typical vocal, **linear phase** High resolution (32k taps) | 1 / 8 / 16 / 32 | 1.62 / 17.18 / 36.84 / 79.89 % | 2.47 / 20.65 / 46.00 / 117.03 % |
| L/R + M/S routing (full 2×2 FIR), **linear phase** High resolution | 1 / 8 / 16 / 32 | 3.12 / 29.56 / 59.04 / 109.06 % | 4.43 / 37.06 / 88.08 / 189.19 % |

Latency: 0 samples in minimum phase; 8,448 (Balanced) / 16,640 (High resolution) samples in linear phase at
48 kHz.

Memory per instance: about 0.8 MB (Linux) / 1.6 MB (Windows) resident in minimum phase. The linear-phase
engine allocates 6.5 MB (Balanced) / 12.9 MB (High resolution) at 48 kHz (×2 at 96 kHz, ×4 at 192 kHz);
Windows showed 7.2 / 12.7 MB resident per instance.

Observations:
- 32 worst-case minimum-phase instances need 64–72 % of one core on these machines.
- Linear phase is several times heavier, and noticeably heavier with MSVC than with GCC (the convolution
  inner loop is not vectorised as well by MSVC): 32 High-resolution instances exceed one core on the Windows
  runner. Optimising this loop is planned for M4. Until then, prefer Balanced quality when many instances run
  in linear phase.

## Not tested — and why

- **FL Studio.** No FL Studio installation was available in the build environment (Linux VM and
  GitHub-hosted Windows runners without a DAW). The plugin passes the two VST3 conformance tools above,
  which exercise the same VST3 interfaces FL Studio uses, but that is not the same as testing in FL Studio.
  Recommended manual checks: plugin scan; insert on a mixer track; automation recording and playback;
  project save/reload; sidechain routing; rendering (export) with audition/mono active (they must be
  ignored); linear-phase latency compensation (PDC) and switching during playback; undo shortcuts inside
  FL Studio; window resizing and high-DPI scaling.
- **Installer execution.** The installer is built automatically but has not been run on a Windows machine.
- **Listening.** No listening tests have been carried out.

## CI runs

| Commit | Run | Windows | Linux |
|---|---|---|---|
| `9d7b232` (M1) | 38695277978 | FAIL (pluginval step: CI script error — PowerShell did not wait for the GUI-subsystem pluginval.exe; fixed by running it under bash) | PASS |
| `5602645` (M2 part 1) | 38713169857 | PASS | PASS |
| `a9591d6` (M2 part 2) | 38715334690 | PASS | PASS |
| `f6e5ee3` (M2 docs) | 38716349089 | PASS | PASS |
| `888d52d` (M3 linear phase) | 38721829366 | PASS | PASS |
