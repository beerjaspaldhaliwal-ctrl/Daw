#!/usr/bin/env python3
"""ME Pro EQ — factory preset generator.

The factory presets are defined in the table below (readable, reviewable) and written as .mepreset XML files
into Presets/Factory/<Category>/. CMake embeds those files into the plugin. Re-run after editing:

    python3 Scripts/generate_factory_presets.py

All presets are original starting points written for ME Pro EQ. They are NOT universal or "perfect"
settings: every source, microphone, arrangement and room is different. Adjust frequencies, gains and
thresholds by ear and with the analyser.
"""
from __future__ import annotations

import os
import sys
from xml.sax.saxutils import quoteattr

TYPES = {"bell": 0, "lowshelf": 1, "highshelf": 2, "highpass": 3, "lowpass": 4, "notch": 5, "bandpass": 6, "tilt": 7, "allpass": 8}
SLOPES = {6: 0, 12: 1, 18: 2, 24: 3, 36: 4, 48: 5, 72: 6, 96: 7}
STEREO = {"stereo": 0, "left": 1, "right": 2, "mid": 3, "side": 4}


def band(kind, freq, gain=0.0, q=None, slope=12, stereo="stereo", dyn=None):
    if q is None:
        q = 0.7071 if kind in ("highpass", "lowpass", "lowshelf", "highshelf", "tilt") else 1.0
    return dict(kind=kind, freq=freq, gain=gain, q=q, slope=slope, stereo=stereo, dyn=dyn)


def dyn(thr, rng, ratio=3.0, att=5.0, rel=120.0, det="peak", sc="internal"):
    return dict(thr=thr, range=rng, ratio=ratio, att=att, rel=rel, det=det, sc=sc)


PRESETS = [
    # --------------------------------------------------------------------------------------------- Vocals
    ("Vocals", "Male Lead Vocal",
     "Rumble filter, low-mid box reduction, presence lift, air shelf and a dynamic de-esser around 6.5 kHz.",
     [band("highpass", 80, slope=18), band("bell", 220, -2.5, 1.2), band("bell", 3200, 2.0, 0.9),
      band("highshelf", 11000, 2.5), band("bell", 6500, 0.0, 3.0, dyn=dyn(-30, -5, 3, 1, 80))]),
    ("Vocals", "Female Lead Vocal",
     "Higher high-pass, gentle 350 Hz cleanup, presence, airy top and a dynamic de-esser near 7.5 kHz.",
     [band("highpass", 110, slope=18), band("bell", 350, -2.0, 1.4), band("bell", 2800, 1.5, 1.0),
      band("highshelf", 12000, 3.0), band("bell", 7500, 0.0, 3.0, dyn=dyn(-30, -5, 3, 1, 80))]),
    ("Vocals", "Background Vocal",
     "Thins and tucks backing vocals behind the lead: steep high-pass, low shelf cut, softened presence.",
     [band("highpass", 150, slope=24), band("lowshelf", 250, -3.0), band("bell", 3000, -1.5, 1.0),
      band("highshelf", 10000, 1.5), band("lowpass", 16000, slope=12)]),
    ("Vocals", "Vocal Clarity",
     "Removes mud and adds articulation for intelligibility in a dense mix.",
     [band("highpass", 90, slope=18), band("bell", 300, -2.0, 1.5), band("bell", 4000, 2.5, 1.2),
      band("highshelf", 12000, 2.0)]),
    ("Vocals", "Vocal Low-Mid Cleanup",
     "A dynamic band reduces 250 Hz only when the voice gets boomy, plus a gentle static 500 Hz cut.",
     [band("highpass", 80, slope=18), band("bell", 250, 0.0, 1.5, dyn=dyn(-28, -6, 3, 8, 150)),
      band("bell", 500, -1.5, 2.0)]),
    ("Vocals", "Vocal Harshness Control",
     "Two dynamic bands catch harsh upper mids (3.2 kHz) and sibilance (6.8 kHz) only when they jump out.",
     [band("highpass", 80, slope=18), band("bell", 3200, 0.0, 2.5, dyn=dyn(-30, -5, 3, 3, 120)),
      band("bell", 6800, 0.0, 3.0, dyn=dyn(-32, -6, 4, 1, 80))]),
    # --------------------------------------------------------------------------------------------- Drums
    ("Drums", "Kick Cleanup",
     "Subsonic filter, boxiness cut around 350 Hz and a gentle top roll-off.",
     [band("highpass", 30, slope=24), band("bell", 350, -4.0, 1.4), band("lowpass", 12000, slope=12)]),
    ("Drums", "Kick Punch",
     "Low-end weight at 60 Hz, box cut and beater click at 3.5 kHz.",
     [band("highpass", 28, slope=24), band("bell", 60, 3.0, 1.2), band("bell", 350, -3.0, 1.5),
      band("bell", 3500, 3.0, 1.5)]),
    ("Drums", "Snare Presence",
     "Body at 200 Hz, ring reduction near 900 Hz, crack at 5 kHz and a little air.",
     [band("highpass", 80, slope=18), band("bell", 200, 2.0, 1.2), band("bell", 900, -2.0, 2.0),
      band("bell", 5000, 2.5, 1.0), band("highshelf", 10000, 1.5)]),
    ("Drums", "Clap Shaping",
     "Removes low thump, focuses the 1.2 kHz body, softens harshness and adds sparkle.",
     [band("highpass", 150, slope=18), band("bell", 1200, 2.0, 1.0), band("bell", 3000, -1.5, 2.0),
      band("highshelf", 9000, 2.0)]),
    ("Drums", "Hi-Hat Cleanup",
     "Steep high-pass to remove bleed, tames clangy upper mids, light air.",
     [band("highpass", 400, slope=24), band("bell", 2500, -2.0, 1.5), band("highshelf", 12000, 1.5)]),
    ("Drums", "Drum Bus Tonal Balance",
     "Broad strokes for a drum group: weight, mid scoop, presence and air.",
     [band("highpass", 25, slope=12), band("lowshelf", 80, 1.5), band("bell", 400, -1.5, 1.0),
      band("bell", 4000, 1.0, 0.8), band("highshelf", 12000, 1.5)]),
    # --------------------------------------------------------------------------------------------- Bass
    ("Bass", "808 Cleanup",
     "Subsonic filter, mud cut at 200 Hz and a steep low-pass to remove distortion fizz.",
     [band("highpass", 25, slope=24), band("bell", 200, -3.0, 1.4), band("lowpass", 8000, slope=24)]),
    ("Bass", "Sub Bass Control",
     "A dynamic low shelf holds back sub energy only when it builds up.",
     [band("highpass", 22, slope=24), band("lowshelf", 80, 0.0, dyn=dyn(-20, -4, 3, 20, 250)),
      band("bell", 250, -2.0, 1.0)]),
    ("Bass", "Bass Definition",
     "Fundamental support, mud cut, growl at 800 Hz and string definition at 2.5 kHz.",
     [band("highpass", 35, slope=18), band("bell", 90, 1.5, 1.0), band("bell", 300, -2.5, 1.4),
      band("bell", 800, 2.0, 1.2), band("bell", 2500, 1.5, 1.0)]),
    ("Bass", "Kick and Bass Separation",
     "Insert on the bass. Route the kick to ME Pro EQ's sidechain input: the 60 Hz band dips the bass only "
     "while the kick hits (external sidechain).",
     [band("highpass", 35, slope=18), band("bell", 60, 0.0, 2.0, dyn=dyn(-24, -5, 4, 2, 90, sc="external"))]),
    # --------------------------------------------------------------------------------------------- Instruments
    ("Instruments", "Warm Rhodes",
     "Warmth, a slightly reduced honk, softer tine bark and a darker top.",
     [band("highpass", 60, slope=12), band("lowshelf", 200, 1.5), band("bell", 700, -1.5, 1.0),
      band("bell", 3000, -1.0, 1.5), band("highshelf", 10000, -1.5)]),
    ("Instruments", "Acoustic Piano",
     "Low-mid cleanup with a gentle presence and air lift.",
     [band("highpass", 40, slope=12), band("bell", 250, -1.5, 1.0), band("bell", 3500, 1.5, 0.8),
      band("highshelf", 10000, 1.5)]),
    ("Instruments", "Acoustic Guitar",
     "Boom reduction, mid declutter, sparkle and air.",
     [band("highpass", 90, slope=18), band("bell", 200, -2.5, 1.2), band("bell", 1000, -1.0, 1.5),
      band("bell", 5000, 2.0, 1.0), band("highshelf", 12000, 1.0)]),
    ("Instruments", "Electric Guitar",
     "Cleans the low end, adds mid focus, removes fizzy harshness and rolls off the top.",
     [band("highpass", 90, slope=18), band("bell", 300, -1.5, 1.0), band("bell", 1500, 1.5, 1.0),
      band("bell", 2500, -2.0, 3.0), band("lowpass", 11000, slope=12)]),
    ("Instruments", "Synth Pluck",
     "Clears space underneath, emphasises the attack and adds air.",
     [band("highpass", 120, slope=18), band("bell", 400, -2.0, 1.0), band("bell", 3000, 2.0, 1.0),
      band("highshelf", 10000, 2.0)]),
    ("Instruments", "Lead Synthesizer",
     "Body at 800 Hz, a dynamic band that tames 3 kHz peaks, and top-end shine.",
     [band("highpass", 150, slope=18), band("bell", 800, 1.0, 1.0), band("bell", 3000, 0.0, 2.0, dyn=dyn(-24, -4, 3, 5, 120)),
      band("highshelf", 10000, 1.5)]),
    ("Instruments", "Atmospheric Pad",
     "Gets the pad out of the low end, softens the mids and adds width with a Side-only presence lift.",
     [band("highpass", 200, slope=12), band("lowshelf", 400, -2.0), band("bell", 2000, -1.5, 0.7),
      band("bell", 5000, 2.0, 0.8, stereo="side"), band("highshelf", 9000, 2.0)]),
    ("Instruments", "Strings",
     "Low-mid cleanup, softened rosin edge and a sheen on top.",
     [band("highpass", 60, slope=12), band("bell", 300, -1.5, 1.0), band("bell", 2500, -1.0, 2.0),
      band("highshelf", 9000, 1.5)]),
    # --------------------------------------------------------------------------------------------- Punjabi
    ("Punjabi Production", "Dhol",
     "Dagga body around 100 Hz, boxiness cut, tilli slap at 2.5 kHz and air.",
     [band("highpass", 40, slope=18), band("bell", 100, 2.0, 1.0), band("bell", 400, -3.0, 1.5),
      band("bell", 2500, 2.5, 1.2), band("highshelf", 9000, 1.0)]),
    ("Punjabi Production", "Tabla",
     "Bayan weight around 120 Hz, mid cleanup and dayan ring at 3.5 kHz.",
     [band("highpass", 50, slope=12), band("bell", 120, 1.5, 1.0), band("bell", 600, -2.0, 1.5),
      band("bell", 3500, 2.0, 1.0), band("highshelf", 10000, 1.0)]),
    ("Punjabi Production", "Dholak",
     "Body, boxy-mid reduction and a defined slap.",
     [band("highpass", 60, slope=18), band("bell", 150, 2.0, 1.2), band("bell", 500, -2.5, 1.5),
      band("bell", 3000, 2.0, 1.0)]),
    ("Punjabi Production", "Tumbi",
     "Steep low cut, nasal-mid cleanup, pluck emphasis and a dynamic band for the harsh string zing.",
     [band("highpass", 250, slope=24), band("bell", 600, -2.0, 1.2), band("bell", 2800, 2.0, 1.5),
      band("bell", 4500, 0.0, 2.5, dyn=dyn(-26, -4, 3, 2, 100)), band("highshelf", 10000, 1.0)]),
    ("Punjabi Production", "Bansuri",
     "Removes handling noise, adds breath and air, with a dynamic band against piercing overblown notes.",
     [band("highpass", 180, slope=18), band("bell", 500, -1.5, 1.0), band("bell", 2000, 1.5, 1.0),
      band("bell", 3500, 0.0, 2.0, dyn=dyn(-24, -4, 3, 3, 120)), band("highshelf", 10000, 2.0)]),
    ("Punjabi Production", "Harmonium",
     "Reedy low-mid cleanup, honk reduction and a little presence.",
     [band("highpass", 100, slope=18), band("bell", 350, -2.0, 1.2), band("bell", 1200, -1.5, 2.0),
      band("bell", 3000, 1.5, 1.0), band("highshelf", 10000, 1.0)]),
    # --------------------------------------------------------------------------------------------- Bus
    ("Bus Processing", "Vocal Bus",
     "Gentle cleanup and air for grouped vocals, with light dynamic sibilance control.",
     [band("highpass", 70, slope=12), band("bell", 300, -1.0, 1.0), band("highshelf", 10000, 1.5),
      band("bell", 6500, 0.0, 3.0, dyn=dyn(-30, -3, 3, 2, 100))]),
    ("Bus Processing", "Drum Bus",
     "Glue-friendly tone with a dynamic low shelf that reins in boom on big hits.",
     [band("highpass", 25, slope=12), band("lowshelf", 60, 1.0), band("bell", 300, -1.0, 1.0),
      band("highshelf", 10000, 1.0), band("lowshelf", 100, 0.0, dyn=dyn(-18, -3, 3, 10, 150))]),
    ("Bus Processing", "Instrument Bus",
     "Makes room for the vocal: low-mid and presence dips on the music bus.",
     [band("highpass", 40, slope=12), band("bell", 250, -1.5, 0.9), band("bell", 2500, -1.0, 1.0),
      band("highshelf", 10000, 1.0)]),
    ("Bus Processing", "Gentle Mix Bus Tone",
     "Very broad, low-order moves: subsonic filter, slight warmth, small mud dip and a 6 dB/oct air shelf.",
     [band("highpass", 20, slope=12), band("lowshelf", 80, 0.8, slope=6), band("bell", 300, -0.5, 0.8),
      band("highshelf", 12000, 1.0, slope=6)]),
    ("Bus Processing", "Mastering Corrective EQ Starting Point",
     "Subsonic filter, tiny low-mid dip, dynamic harshness control, mono-izing low Side cut and a breath of air.",
     [band("highpass", 18, slope=24), band("bell", 250, -0.8, 1.0), band("bell", 3000, 0.0, 1.5, dyn=dyn(-20, -2, 2, 10, 200)),
      band("lowshelf", 120, -3.0, stereo="side"), band("highshelf", 15000, 0.5)]),
]


def param_lines(bands):
    out = []
    for i, b in enumerate(bands):
        n = i + 1
        p = lambda suffix, v: out.append('  <PARAM id="b%d_%s" value="%s"/>' % (n, suffix, ("%.6g" % v)))
        p("used", 1)
        p("on", 1)
        p("type", TYPES[b["kind"]])
        p("freq", b["freq"])
        p("gain", b["gain"])
        p("q", b["q"])
        p("slope", SLOPES[b["slope"]])
        p("stereo", STEREO[b["stereo"]])
        d = b["dyn"]
        if d:
            p("dyn", 1)
            p("thr", d["thr"])
            p("ratio", d["ratio"])
            p("att", d["att"])
            p("rel", d["rel"])
            p("range", d["range"])
            p("det", 0 if d["det"] == "peak" else 1)
            p("sc", 0 if d["sc"] == "internal" else 1)
    return out


def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Presets", "Factory")
    root = os.path.normpath(root)
    written = 0
    for category, name, desc, bands in PRESETS:
        assert len(bands) <= 16
        for b in bands:
            assert 10 <= b["freq"] <= 30000 and -30 <= b["gain"] <= 30 and 0.1 <= b["q"] <= 30, (name, b)
        d = os.path.join(root, category)
        os.makedirs(d, exist_ok=True)
        lines = ['<?xml version="1.0" encoding="UTF-8"?>',
                 '<MEProEQPreset name=%s category=%s author="ME Audio" version="1" description=%s>'
                 % (quoteattr(name), quoteattr(category), quoteattr(desc))]
        lines += param_lines(bands)
        lines.append("</MEProEQPreset>")
        with open(os.path.join(d, name + ".mepreset"), "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(lines) + "\n")
        written += 1
    print("wrote %d factory presets to %s" % (written, root))
    return 0


if __name__ == "__main__":
    sys.exit(main())
