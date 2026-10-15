# Third-party software used by ME Pro EQ

ME Pro EQ's own source code (everything under `Source/`, `Tests/`, `Benchmarks/`, `Scripts/`, `Installer/`)
is original work by ME Audio. All DSP algorithms are original implementations of published methods
(see `Documentation/DSP_Design.md` for references). No proprietary code, presets or interface designs from
other products are included.

## JUCE 9.0.4

- Website: https://juce.com — Source: https://github.com/juce-framework/JUCE (tag `9.0.4`, commit
  `3fb5cbc0340758596dac39f7c8de47d562b4896e`, verified at configure time)
- Licence: dual — **GNU AGPLv3** or the **commercial JUCE 9 End User Licence Agreement**
  (https://juce.com/legal/juce-9-licence/).

**Important for distribution.** Binaries built from this repository link JUCE. You may distribute them either

1. under the terms of the AGPLv3 (which requires making the complete corresponding source code of
   ME Pro EQ available to recipients under the AGPLv3), **or**
2. under a JUCE commercial licence (JUCE offers several tiers; at the time of writing the JUCE 9 EULA
   includes a free "Starter" tier below a revenue threshold — check the current EULA for the exact terms).

ME Audio must choose and obtain the appropriate licence **before** distributing ME Pro EQ commercially as
closed source. This document is not legal advice.

JUCE itself vendors further components (inventory: `JUCE.spdx.json` in the JUCE source tree). Those linked
into the VST3 include:

| Component | Version | Licence |
|---|---|---|
| Steinberg VST3 SDK (headers / wrapper sources) | 3.8.0 | MIT |
| FreeType (Linux builds only, system library) | system | FreeType / GPLv2 |
| zlib (inside JUCE core) | 1.3.2 | zlib |
| libpng / jpeglib (inside JUCE graphics, image I/O) | 1.6.58 / 10.0 | libpng-2.0 / IJG |
| HarfBuzz (inside JUCE graphics) | 14.2.1 | MIT |

## Steinberg VST3 SDK 3.8.0

MIT License — Copyright (c) 2025, Steinberg Media Technologies GmbH. "VST" is a registered trademark of
Steinberg Media Technologies GmbH. The full licence text is in `LICENSES/VST3_SDK_MIT.txt`.

## Tools used for validation only (not distributed)

- pluginval 1.0.4 (Tracktion, GPLv3) — plugin validation.
- Steinberg VST3 validator (VST3 SDK 3.8.0, MIT) — conformance tests.
- Inno Setup 6 (Jordan Russell / Martijn Laan, Inno Setup licence) — builds the Windows installer.
