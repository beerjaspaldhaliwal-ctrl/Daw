// ME Pro EQ — ME Audio
// Analysis suggestions: no false alarms on neutral material, the planted problem is found (and only it), the
// proposed band is sensible, and applying it removes the finding (closed loop).
#include "EngineTestUtils.h"
#include "TestFramework.h"

#include "DSP/ToneAnalysis.h"

using namespace mepro::dsp;
using namespace metest;

namespace
{
    void filterInPlace (std::vector<double>& x, const DesignSpec& spec, double fs)
    {
        SvfCascade c;
        c.setLayout (sectionLayout (spec.type, spec.slopeIndex));
        c.setCoefficients (designCascade (spec, fs));
        c.processMono (x.data(), (int) x.size(), 1, 0);
    }

    std::vector<Suggestion> analyseSignal (const std::vector<double>& mono, double fs)
    {
        ToneAnalysis a;
        a.prepare (fs);
        std::vector<float> st (mono.size() * 2);
        for (size_t i = 0; i < mono.size(); ++i)
            st[2 * i] = st[2 * i + 1] = (float) mono[i];
        for (size_t pos = 0; pos < mono.size(); pos += 1000) // fed in UI-sized chunks
            a.feed (st.data() + 2 * pos, (int) std::min<size_t> (1000, mono.size() - pos), 2);
        return a.analyse();
    }

    int countKind (const std::vector<Suggestion>& v, Suggestion::Kind k)
    {
        int n = 0;
        for (const auto& s : v)
            n += s.kind == k ? 1 : 0;
        return n;
    }

    std::string describe (const std::vector<Suggestion>& v)
    {
        std::string s;
        for (const auto& x : v)
            s += "[" + x.title + "] ";
        return s.empty() ? "(none)" : s;
    }

    const Suggestion* first (const std::vector<Suggestion>& v, Suggestion::Kind k)
    {
        for (const auto& s : v)
            if (s.kind == k)
                return &s;
        return nullptr;
    }
} // namespace

ME_TEST ("Suggestions", NeutralMaterialGivesNoSuggestions)
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (uint64_t seed : { 3u, 4u })
        {
            const auto s = analyseSignal (pinkNoise ((int) (fs * 8), 0.3, seed), fs);
            CHECK_MSG (s.empty(), "fs " << fs << " seed " << seed << ": " << describe (s));
        }
    measure ("pink noise, 8 s, 3 sample rates x 2 seeds: suggestions", 0.0);
}

ME_TEST ("Suggestions", NotEnoughMaterialGivesNothing)
{
    const double fs = 48000.0;
    ToneAnalysis a;
    a.prepare (fs);
    std::vector<float> silence ((size_t) fs * 20, 0.0f);
    a.feed (silence.data(), (int) fs * 10, 2);
    CHECK (! a.hasEnoughMaterial());
    CHECK (a.analyse().empty());
    CHECK (analyseSignal (pinkNoise ((int) fs, 0.3, 1), fs).empty()); // 1 s is not enough
}

ME_TEST ("Suggestions", ResonanceIsFoundAndSized)
{
    const double fs = 48000.0;
    struct Case { double f, g, q; };
    for (const auto c : { Case { 2500.0, 12.0, 8.0 }, Case { 180.0, 10.0, 6.0 }, Case { 6300.0, 9.0, 10.0 } })
    {
        auto x = pinkNoise ((int) fs * 8, 0.3, 7);
        filterInPlace (x, { FilterType::Bell, c.f, c.g, c.q, 1 }, fs);
        const auto s = analyseSignal (x, fs);
        const auto* r = first (s, Suggestion::Kind::Resonance);
        CHECK_MSG (r != nullptr && countKind (s, Suggestion::Kind::Resonance) == 1, "f " << c.f << ": " << describe (s));
        if (r == nullptr)
            continue;
        const double octErr = std::abs (std::log2 (r->band.freqHz / c.f));
        measure ("planted +" + str (c.g) + " dB Q " + str (c.q) + " at " + str (c.f) + " Hz: suggested f / gain / Q / octave error",
                 str (r->band.freqHz) + " / " + str (r->band.gainDb) + " / " + str (r->band.q) + " / " + str (octErr));
        CHECK (octErr < 1.0 / 24.0);
        CHECK (r->band.gainDb <= -0.5 * c.g && r->band.gainDb >= -c.g);
        CHECK (r->band.q >= 0.5 * c.q && r->band.q <= 2.0 * c.q);
        CHECK (r->band.type == FilterType::Bell && ! r->band.dyn.enabled && r->band.stereo == StereoMode::Stereo);
        CHECK_MSG (s.size() == 1, "f " << c.f << ": " << describe (s)); // nothing else is flagged
    }
}

ME_TEST ("Suggestions", AppliedResonanceCutRemovesTheFinding)
{
    const double fs = 48000.0;
    auto x = pinkNoise ((int) fs * 8, 0.3, 9);
    filterInPlace (x, { FilterType::Bell, 2500.0, 12.0, 8.0, 1 }, fs);
    const auto before = analyseSignal (x, fs);
    const auto* r = first (before, Suggestion::Kind::Resonance);
    CHECK (r != nullptr);
    if (r == nullptr)
        return;
    // Apply the suggested band exactly as the editor would (through the engine).
    EQEngine e;
    e.prepare (fs, 512, 2);
    EngineSettings st;
    st.bands[0] = r->band;
    e.setSettings (st);
    e.reset();
    Stereo s (x, x);
    runEngine (e, s, 512);
    const auto after = analyseSignal (s.l, fs);
    measure ("after applying the suggestion: remaining suggestions", (double) after.size());
    CHECK_MSG (countKind (after, Suggestion::Kind::Resonance) == 0, describe (after));
}

ME_TEST ("Suggestions", LowMidBuildUp)
{
    const double fs = 48000.0;
    auto x = pinkNoise ((int) fs * 8, 0.3, 11);
    filterInPlace (x, { FilterType::Bell, 300.0, 5.0, 1.0, 1 }, fs);
    const auto s = analyseSignal (x, fs);
    const auto* m = first (s, Suggestion::Kind::LowMid);
    CHECK_MSG (m != nullptr, describe (s));
    if (m != nullptr)
    {
        measure ("planted +5 dB Q1 at 300 Hz: suggested f / gain / Q", str (m->band.freqHz) + " / " + str (m->band.gainDb) + " / " + str (m->band.q));
        CHECK (std::abs (std::log2 (m->band.freqHz / 300.0)) < 1.0 / 3.0);
        CHECK (m->band.gainDb <= -1.5 && m->band.gainDb >= -4.0);
    }
    CHECK_MSG (countKind (s, Suggestion::Kind::Resonance) == 0, describe (s)); // a broad hump is not a resonance
}

ME_TEST ("Suggestions", RumbleUnderHighPassedSource)
{
    const double fs = 48000.0;
    // A "vocal-like" source: content from about 120 Hz up ...
    auto x = pinkNoise ((int) fs * 8, 0.3, 13);
    filterInPlace (x, { FilterType::HighPass, 120.0, 0.0, kButterworthQ, 3 }, fs);
    // ... plus low-frequency rumble 25 dB down.
    auto rumble = pinkNoise ((int) fs * 8, 0.3 * 0.056, 14);
    filterInPlace (rumble, { FilterType::LowPass, 40.0, 0.0, kButterworthQ, 3 }, fs);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] += rumble[i];
    const auto s = analyseSignal (x, fs);
    const auto* h = first (s, Suggestion::Kind::HighPass);
    CHECK_MSG (h != nullptr, describe (s));
    if (h != nullptr)
    {
        // Rule: 0.6 x the frequency where the content comes within 15 dB of its strongest region (about 78 Hz for
        // a 24 dB/oct high-pass at 120 Hz) - low enough to keep the content, high enough to remove the rumble.
        measure ("source from 120 Hz + rumble: suggested high-pass (Hz)", h->band.freqHz);
        CHECK (h->band.freqHz >= 35.0 && h->band.freqHz <= 75.0);
        CHECK (h->band.type == FilterType::HighPass && h->band.slopeIndex == 2);
    }
    // The same source without rumble needs no high-pass.
    auto clean = pinkNoise ((int) fs * 8, 0.3, 13);
    filterInPlace (clean, { FilterType::HighPass, 120.0, 0.0, kButterworthQ, 7 }, fs);
    const auto c = analyseSignal (clean, fs);
    CHECK_MSG (countKind (c, Suggestion::Kind::HighPass) == 0, describe (c));
}

ME_TEST ("Suggestions", SibilanceBurstsGetADynamicBand)
{
    const double fs = 48000.0;
    const int n = (int) fs * 10;
    auto x = pinkNoise (n, 0.2, 15);
    auto hiss = whiteNoise (n, 0.5, 16);
    filterInPlace (hiss, { FilterType::BandPass, 7000.0, 0.0, 2.0, 1 }, fs);
    // 80 ms bursts every 700 ms with 5 ms ramps.
    for (int i = 0; i < n; ++i)
    {
        const int ph = i % (int) (0.7 * fs);
        const double on = ph < (int) (0.08 * fs) ? std::min ({ 1.0, ph / (0.005 * fs), ((0.08 * fs) - ph) / (0.005 * fs) }) : 0.0;
        x[(size_t) i] += on * hiss[(size_t) i];
    }
    const auto s = analyseSignal (x, fs);
    const auto* d = first (s, Suggestion::Kind::Sibilance);
    CHECK_MSG (d != nullptr, describe (s));
    if (d != nullptr)
    {
        measure ("bursts at 7 kHz: suggested f / Q / threshold (dBFS RMS) / range",
                 str (d->band.freqHz) + " / " + str (d->band.q) + " / " + str (d->band.dyn.thresholdDb) + " / " + str (d->band.dyn.rangeDb));
        CHECK (std::abs (std::log2 (d->band.freqHz / 7000.0)) < 0.5);
        CHECK (d->band.dyn.enabled && d->band.gainDb == 0.0 && d->band.dyn.detector == DetectorMode::Rms);
        // The threshold must sit between the steady level and the burst level of the detector's region:
        // the band acts on the bursts and leaves the rest alone (verified through the engine).
        EQEngine e;
        e.prepare (fs, 512, 2);
        EngineSettings st;
        st.bands[0] = d->band;
        e.setSettings (st);
        e.reset();
        Stereo y (x, x);
        double minGain = 0.0, gainBetween = 0.0;
        for (int pos = 0; pos < n; pos += 480)
        {
            double* ch[2] = { y.l.data() + pos, y.r.data() + pos };
            e.process (ch, 2, std::min (480, n - pos));
            const double g = e.getBandDynamicGainDb (0);
            const int ph = pos % (int) (0.7 * fs);
            if (pos > (int) fs)
            {
                if (ph > (int) (0.3 * fs) && ph < (int) (0.65 * fs))
                    gainBetween = std::min (gainBetween, g);
                minGain = std::min (minGain, g);
            }
        }
        measure ("dynamic gain during bursts (min) / between bursts (min), dB", str (minGain) + " / " + str (gainBetween));
        CHECK (minGain < -2.0);
        CHECK (gainBetween > -0.5);
    }
}

ME_TEST ("Suggestions", HarshnessStaticOrDynamic)
{
    const double fs = 48000.0;
    // Constant presence boost -> static cut.
    {
        auto x = pinkNoise ((int) fs * 8, 0.3, 17);
        filterInPlace (x, { FilterType::Bell, 3200.0, 5.0, 0.9, 1 }, fs);
        const auto s = analyseSignal (x, fs);
        const auto* h = first (s, Suggestion::Kind::Harshness);
        CHECK_MSG (h != nullptr, describe (s));
        if (h != nullptr)
        {
            measure ("constant +5 dB at 3.2 kHz: suggested f / gain / dynamic", str (h->band.freqHz) + " / " + str (h->band.gainDb) + " / "
                                                                                    + str ((int) h->band.dyn.enabled));
            CHECK (! h->band.dyn.enabled && h->band.gainDb < 0.0);
            CHECK (std::abs (std::log2 (h->band.freqHz / 3200.0)) < 0.5);
        }
    }
    // The same region only loud at times (2 s loud / 2 s normal) -> dynamic band.
    {
        const int n = (int) fs * 12;
        auto x = pinkNoise (n, 0.3, 18);
        auto edge = x;
        filterInPlace (edge, { FilterType::BandPass, 3200.0, 0.0, 0.9, 1 }, fs);
        for (int i = 0; i < n; ++i)
            if ((i / (int) (2 * fs)) % 2 == 1)
                x[(size_t) i] += 2.5 * edge[(size_t) i];
        const auto s = analyseSignal (x, fs);
        const auto* h = first (s, Suggestion::Kind::Harshness);
        CHECK_MSG (h != nullptr, describe (s));
        if (h != nullptr)
        {
            measure ("3.2 kHz loud half of the time: suggested threshold / range / dynamic",
                     str (h->band.dyn.thresholdDb) + " / " + str (h->band.dyn.rangeDb) + " / " + str ((int) h->band.dyn.enabled));
            CHECK (h->band.dyn.enabled && h->band.gainDb == 0.0);
        }
    }
}

ME_TEST ("Suggestions", MissingAirIsALowConfidenceSuggestion)
{
    const double fs = 48000.0;
    auto x = pinkNoise ((int) fs * 8, 0.3, 19);
    filterInPlace (x, { FilterType::LowPass, 6000.0, 0.0, kButterworthQ, 3 }, fs);
    const auto s = analyseSignal (x, fs);
    const auto* a = first (s, Suggestion::Kind::Air);
    CHECK_MSG (a != nullptr, describe (s));
    if (a != nullptr)
    {
        CHECK (a->confidence == Suggestion::Confidence::Low);
        CHECK (a->band.type == FilterType::HighShelf && a->band.gainDb > 0.0 && a->band.gainDb <= 3.0);
    }
    // Every suggestion explains itself.
    for (const auto& x2 : s)
        CHECK (! x2.title.empty() && x2.reason.size() > 40);
}
