# ME Pro EQ — validation report

What was tested, where, and with what result. **PASS** = executed and passed. **FAIL** = executed and
failed. **NOT TESTED** = not executed; nothing is claimed about it.

Version: 0.1.0, milestone M2. Results are reproduced by the GitHub Actions workflow
`.github/workflows/me-pro-eq.yml` on every push; its uploaded `*-reports` artifacts contain the full test
reports, validator logs, benchmark tables and editor screenshots.

## Environments

| Environment | Details |
|---|---|
| Linux development VM | Ubuntu 24.04, GCC 13.3.0, CMake 3.28.3, Intel Xeon @ 2.10 GHz (4 logical cores), headless (Xvfb for GUI tests) |
| GitHub Actions — Windows | `windows-2022` runner (Windows Server 2022; JUCE reports it as "Windows 10"), Visual Studio 2022 (MSVC, x64, Release) |
| GitHub Actions — Linux | `ubuntu-24.04` runner, GCC, Ninja, Release |
| Dependencies | JUCE 9.0.4 (commit `3fb5cbc0`, verified at configure time), VST3 SDK 3.8.0 (validator), pluginval 1.0.4, Inno Setup 6 |

## Summary

| Check | Linux (local VM) | Windows (GitHub Actions) |
|---|---|---|
| Build: DSP library, VST3, tests, benchmark | PASS | PASS |
| DSP test suite (55 tests, 19,638 checks) | PASS 55/55 | PASS 55/55 |
| Plugin host-level test suite | PASS 20/20 (1,297 checks) | PASS 20/20 (1,297 checks) |
| pluginval 1.0.4, strictness 10, in-process | PASS (SUCCESS, 25 test groups) | PASS (SUCCESS, 25 test groups) |
| Steinberg VST3 validator 3.8.0, extensive (`-e`) | PASS 537/537 | PASS 537/537 |
| Windows installer builds (`ME Pro EQ Setup.exe`) | — | PASS (built; 4,307,056 bytes in run 38713169857) |
| Installer runs, installs to `C:\Program Files\Common Files\VST3`, uninstalls | — | **NOT TESTED** |
| CPU benchmark 1/8/16/32 instances | PASS (measured, below) | PASS (measured, below) |
| **Loading and using ME Pro EQ in FL Studio** | — | **NOT TESTED** |
| Any other DAW (Reaper, Ableton Live, Cubase, …) | **NOT TESTED** | **NOT TESTED** |
| Real audio interface / real-time audio thread of a DAW | **NOT TESTED** | **NOT TESTED** |
| High-DPI rendering on a real Windows display | — | **NOT TESTED** (offscreen 2× rendering is tested) |
| Listening tests | **NOT TESTED** | **NOT TESTED** |

Windows results are from the workflow run on commit `a9591d6` (run 38715334690), the last commit with code
changes in M2.

A previous Windows run (commit `9d7b232`, run 38695277978) reported pluginval as failed. The cause was the
CI script, not the plugin: `pluginval.exe` is a GUI-subsystem program, so PowerShell neither waited for it
nor captured its exit code (the exit code was empty after 1 second). The step now runs under bash, which
waits and returns the real exit code; the next run passed.

## What the automated tests cover

The test programs print every measured value into a report (`reports/dsp_test_report.md`,
`reports/plugin_test_report.md`). Highlights; the full method and figures are in `DSP_Design.md`.

**Frequency response and phase**
- Every realised filter equals its designed transfer function: 7.5e-11 dB magnitude, 2.3e-11 rad phase.
- Engine response at 44.1, 48, 88.2, 96, 176.4, 192 kHz equals the design within 1.9e-12 dB.
- Accuracy against the analog prototypes, per sample rate and frequency region (including the weak spots
  near Nyquist at 44.1/48 kHz: up to 3 dB for steep shelves between fs/8 and fs/4, up to 15.9 dB for bells
  above fs/4).
- Exact values at DC, f0 and Nyquist; Butterworth −3.01 dB points (2.4e-14 dB error); slopes 6 … 96 dB/oct;
  bell bandwidth vs Q; boost/cut mirror symmetry (3.9e-14 dB).
- Shape invariants over 2,500 frequencies for every type, slope, gain, Q, frequency and sample rate
  (monotonic cut filters, no shelf overshoot beyond 0.17 dB, single-peaked bells).

**Stability**
- 714,432 designs: 0 unstable, 0 non-minimum-phase. Extreme impulse responses decay.
- 10 minutes of continuous processing; random parameter storms (20 s); sample-rate and block-size
  changes; NaN/Inf/denormal input; repeated create/destroy; 32 instances processed concurrently on 32
  threads, each matching a single-threaded render.

**Automation and switching** (residual energy above a high-pass, relative to the signal)
- Gain jumps −114.5 dB; band on/off −136.4 dB; type/slope/stereo switching −130.0 dB; bypass −150.0 dB;
  frequency sweeps −125.9 dB. Output is identical for any host block size.

**Dynamic EQ**
- Gain curve, frequency selectivity, attack/release timing, external sidechain, enhancement limits,
  sibilance and drum-resonance scenarios, smooth gain modulation, reported gain = applied gain.

**Stereo**
- Mid/Side reconstruction exact (error 0), channel isolation for Left/Right/Mid/Side bands, mono-bus
  behaviour.

**Host level (real AudioProcessor)**
- 260 parameters with stable IDs, all automatable with readable text; bus layouts (mono, stereo,
  sidechain); float and double rendering; state save/load exact and byte-identical on re-save; older and
  corrupt states; monitoring functions (audition, mono) ignored during offline rendering.
- Presets: all 35 factory presets parse, reference only existing parameters with in-range values, load as
  one undo step and render finite audio (highest output peak −8.2 dBFS for a −7.7 dBFS-peak pink-noise
  input); user preset save/load/delete in a temporary folder; corrupt preset files ignored; preset name and
  modified flag saved with the project.
- Undo/redo: one step per edit or drag, host automation not recorded, 100-step limit, history cleared on
  project load; Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y in the editor.
- A/B: independent slots with separate undo histories, saved with the project, copy, switching refused
  mid-drag.
- Auto gain at plugin level (+9 dB high shelf: +4.5 dB without, −0.7 dB with auto gain on unfiltered
  pink noise); MATCH (+3.53 dB difference → +0.04 dB).
- Editor opens, renders at 1220×760, 980×620, 1800×1100 and at 2× scale, and drives edits.

## Validators

**pluginval 1.0.4**, `--strictness-level 10 --validate-in-process`: SUCCESS on Linux (local and CI) and
Windows (CI). All 25 test groups ran, among them: open plugin cold/warm, editor, open editor whilst
processing, audio processing, plugin state and state restoration, automation and editor automation,
parameter thread safety, background-thread state, bus layouts (enable all / disable non-main / restore
default) and fuzzed parameters.

**Steinberg VST3 validator** (VST3 SDK 3.8.0, extensive mode): 537 tests passed, 0 failed on Linux and
Windows. An earlier run reported a failure (the default program had no name); fixed by naming it
"Default".

## CPU and memory

All instances processed sequentially on one thread (as on one DAW audio thread), 48 kHz, 256-sample
blocks, 10 s of stereo noise through the 32-bit host entry point. Load = processing time / audio time.
These are virtual machines shared with other workloads: worst-block figures are noisy and should be read
as indications, not guarantees. No measurement on a desktop PC has been made yet.

| Configuration | Instances | Linux VM, Xeon @ 2.1 GHz | Windows CI, AMD EPYC 9V45 (run 38715334690) | Windows CI, AMD EPYC 7763 (run 38713169857) |
|---|---|---|---|---|
| Typical vocal: 6 static bands | 1 | 0.24 % | 0.21 % | 0.32 % |
| | 8 | 1.95 % | 1.54 % | 2.61 % |
| | 16 | 3.74 % | 3.14 % | 5.27 % |
| | 32 | 8.29 % | 6.94 % | 10.70 % |
| 8 bands, 2 dynamic, analyser on | 1 | 0.56 % | 0.40 % | 0.69 % |
| | 8 | 4.63 % | 3.25 % | 5.58 % |
| | 16 | 9.82 % | 6.72 % | 11.34 % |
| | 32 | 19.40 % | 13.88 % | 22.71 % |
| Worst case: 16 bands, 96 dB/oct cuts, 8 dynamic, M/S, analyser on | 1 | 1.91 % | 1.32 % | 2.24 % |
| | 8 | 17.78 % | 11.21 % | 18.07 % |
| | 16 | 37.17 % | 22.14 % | 36.20 % |
| | 32 | 68.66 % | 45.55 % | 72.59 % |

GitHub assigns different processors to its runners; the two Windows columns show how much the same build
varies with the hardware. Memory: about 0.8 MB (Linux) / 1.6 MB (Windows) resident per instance
(32-instance worst-case run).
Latency: 0 samples in every configuration.

Observation: 32 worst-case instances need 46–73 % of one core on these machines, and individual
blocks occasionally took longer than the 5.3 ms block period on the Linux VM (worst block 15 ms,
P99 5.6 ms). Such a session would need the DAW to spread instances over several cores.

## Not tested — and why

- **FL Studio.** No FL Studio installation was available in the build environment (Linux VM and
  GitHub-hosted Windows runners without a DAW). The plugin passes the two VST3 conformance tools above,
  which exercise the same VST3 interfaces FL Studio uses, but that is not the same as testing in FL Studio.
  Recommended manual checks: plugin scan; insert on a mixer track; automation recording and playback;
  project save/reload; sidechain routing; rendering (export) with audition/mono active (they must be
  ignored); undo shortcuts inside FL Studio; window resizing and high-DPI scaling.
- **Installer execution.** The installer is built automatically but has not been run on a Windows machine.
- **Listening.** No listening tests have been carried out.

## CI runs

| Commit | Run | Windows | Linux |
|---|---|---|---|
| `9d7b232` (M1) | 38695277978 | FAIL (pluginval step: CI script error, see above) | PASS |
| `5602645` (M2 part 1) | 38713169857 | PASS | PASS |
| `a9591d6` (M2 part 2) | 38715334690 | PASS | PASS |
