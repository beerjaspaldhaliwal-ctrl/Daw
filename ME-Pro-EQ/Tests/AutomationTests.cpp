// ME Pro EQ — ME Audio
// Automation and stability tests: rapid parameter sweeps, abrupt jumps, structural switches, random
// parameter storms, sample-rate / block-size changes and a long-duration run.
#include "EngineTestUtils.h"
#include "TestFramework.h"

#include <chrono>

using namespace mepro::dsp;
using namespace metest;

namespace
{
// Runs `fn(blockIndex, settings)` before each block to modify settings.
template <typename Fn>
void runAutomated (EQEngine& e, Stereo& s, int block, EngineSettings st, Fn&& fn)
{
    for (int pos = 0, k = 0; pos < s.size(); pos += block, ++k)
    {
        fn (k, st);
        e.setSettings (st);
        const int n = std::min (block, s.size() - pos);
        double* ch[2] = { s.l.data() + pos, s.r.data() + pos };
        e.process (ch, 2, n);
    }
}
} // namespace

ME_TEST ("Automation", ClickMetricSanityCheck)
{
    // Validates the click metric: an *unsmoothed* coefficient jump on a 200 Hz tone must register clearly
    // above the threshold used by the smoothing tests, while a static filter registers ~nothing.
    const double fs = 48000.0;
    const int n = (int) fs * 2;
    auto x = sine (200, fs, n, 0.5);
    auto staticOut = x, jumpOut = x;
    SvfCascade f;
    f.setLayout (sectionLayout (FilterType::Bell, 1));
    f.setCoefficients (designCascade ({ FilterType::Bell, 200, 12, 1, 1 }, fs));
    f.processMono (staticOut.data(), n, 1, 0);
    SvfCascade j;
    j.setLayout (sectionLayout (FilterType::Bell, 1));
    j.setCoefficients (designCascade ({ FilterType::Bell, 200, 0, 1, 1 }, fs));
    for (int pos = 0, k = 0; pos < n; pos += 4800, ++k)
    {
        j.setCoefficients (designCascade ({ FilterType::Bell, 200, (k % 2) ? 12.0 : 0.0, 1, 1 }, fs)); // abrupt
        j.processMono (jumpOut.data() + pos, std::min (4800, n - pos), 1, 0);
    }
    const double a = hfResidualDb (staticOut, fs, 4000, 4800), b = hfResidualDb (jumpOut, fs, 4000, 4800);
    measure ("static filter HF residual (dB)", a);
    measure ("unsmoothed gain jumps 0<->12 dB every 100 ms: HF residual (dB)", b);
    CHECK (a < -200.0);
    CHECK (b > -95.0);
}

ME_TEST ("Automation", GainJumpsAreSmoothed)
{
    const double fs = 48000.0;
    EQEngine e;
    e.prepare (fs, 480, 2);
    Stereo s (sine (200, fs, 96000, 0.5), sine (200, fs, 96000, 0.5));
    EngineSettings st;
    st.bands[0] = bell (200, 0, 1.0);
    runAutomated (e, s, 480, st, [] (int k, EngineSettings& x) { x.bands[0].gainDb = ((k / 10) % 2) ? 12.0 : 0.0; });
    const double hf = hfResidualDb (s.l, fs, 4000, 4800);
    measure ("host gain jumps 0<->12 dB every 100 ms: HF residual >4 kHz (dB)", hf);
    CHECK (hf < -110.0);
}

ME_TEST ("Automation", RapidFrequencySweep)
{
    const double fs = 48000.0;
    EQEngine e;
    e.prepare (fs, 64, 2);
    const int n = (int) fs * 10;
    Stereo s (pinkNoise (n, 0.25, 200), pinkNoise (n, 0.25, 201));
    const double inPk = std::max (peakAbs (s.l), peakAbs (s.r));
    EngineSettings st;
    st.bands[0] = bell (1000, 12, 4.0);
    st.bands[1] = band (FilterType::HighPass, 100, 0, 2.0, 7);
    runAutomated (e, s, 64, st, [&] (int k, EngineSettings& x)
    {
        const double t = k * 64 / fs;
        const double ph = std::fmod (t / 0.2, 2.0);         // 200 ms per direction
        const double u = ph < 1 ? ph : 2 - ph;
        x.bands[0].freqHz = 20.0 * std::pow (1000.0, u);     // 20 Hz .. 20 kHz
        x.bands[1].freqHz = 20.0 * std::pow (50.0, 1 - u);   // 1 kHz .. 20 Hz
    });
    CHECK (allFinite (s.l) && allFinite (s.r));
    const double outPk = std::max (peakAbs (s.l), peakAbs (s.r));
    measure ("10 s of 20 Hz<->20 kHz sweeps (bell +12 dB Q4, HP 96 dB/oct Q2): output/input peak (dB)", db (outPk / inPk));
    CHECK (outPk < inPk * std::pow (10.0, (12.0 + 12.0) / 20.0));
}

ME_TEST ("Automation", ToneUnderFrequencySweepHasNoClicks)
{
    // A 150 Hz tone; a +9 dB bell sweeps 2 kHz <-> 16 kHz (never near the tone). The tone must pass with no
    // broadband artefacts.
    const double fs = 48000.0;
    EQEngine e;
    e.prepare (fs, 128, 2);
    const int n = (int) fs * 4;
    Stereo s (sine (150, fs, n, 0.5), sine (150, fs, n, 0.5));
    EngineSettings st;
    st.bands[0] = bell (4000, 9, 2.0);
    runAutomated (e, s, 128, st, [&] (int k, EngineSettings& x)
    {
        const double t = k * 128 / fs;
        x.bands[0].freqHz = 2000.0 * std::pow (8.0, 0.5 + 0.5 * std::sin (kTwoPi * t * 2.0));
    });
    const double hf = hfResidualDb (s.l, fs, 1000, 4800);
    measure ("150 Hz tone, bell swept 2-16 kHz at 2 Hz: residual >1 kHz (dB)", hf);
    CHECK (hf < -100.0);
}

ME_TEST ("Automation", EnableAndTypeSwitchingIsClickFree)
{
    const double fs = 48000.0;
    const int n = (int) fs * 4;
    // Enable/disable a +12 dB bell every 50 ms.
    {
        EQEngine e;
        e.prepare (fs, 240, 2);
        Stereo s (sine (200, fs, n, 0.5), sine (200, fs, n, 0.5));
        EngineSettings st;
        st.bands[0] = bell (200, 12, 1.0);
        runAutomated (e, s, 240, st, [] (int k, EngineSettings& x) { x.bands[0].active = ((k / 10) % 2) == 0; });
        const double hf = hfResidualDb (s.l, fs, 4000, 4800);
        measure ("band on/off every 50 ms: HF residual >4 kHz (dB)", hf);
        CHECK (hf < -80.0);
    }
    // Cycle filter type / slope / stereo mode every 50 ms (structural crossfades).
    {
        EQEngine e;
        e.prepare (fs, 240, 2);
        Stereo s (sine (200, fs, n, 0.5), sine (200, fs, n, 0.5));
        const double inPk = peakAbs (s.l);
        EngineSettings st;
        st.bands[0] = bell (300, 6, 1.0);
        runAutomated (e, s, 240, st, [] (int k, EngineSettings& x)
        {
            const int m = (k / 10) % 6;
            const FilterType ts[] = { FilterType::Bell, FilterType::LowShelf, FilterType::HighPass, FilterType::Notch, FilterType::Tilt, FilterType::AllPass };
            x.bands[0].type = ts[m];
            x.bands[0].slopeIndex = (k / 10) % 8;
            x.bands[0].stereo = (StereoMode) ((k / 20) % 5);
        });
        CHECK (allFinite (s.l) && allFinite (s.r));
        const double hf = hfResidualDb (s.l, fs, 4000, 4800);
        measure ("type/slope/stereo switching every 50 ms: HF residual >4 kHz (dB)", hf);
        measure ("type switching: output/input peak (dB)", db (std::max (peakAbs (s.l), peakAbs (s.r)) / inPk));
        CHECK (hf < -60.0);
    }
}

ME_TEST ("Automation", RandomParameterStorm)
{
    // Every block, every parameter of every band is randomised (including extremes). Must stay finite and
    // bounded. 20 s at 44.1 kHz, block 32.
    const double fs = 44100.0;
    EQEngine e;
    e.prepare (fs, 32, 2);
    const int n = (int) fs * 20;
    Stereo s (pinkNoise (n, 0.25, 300), pinkNoise (n, 0.25, 301));
    const double inPk = std::max (peakAbs (s.l), peakAbs (s.r));
    Rng r (99);
    EngineSettings st;
    runAutomated (e, s, 32, st, [&] (int, EngineSettings& x)
    {
        for (auto& b : x.bands)
        {
            if (r.uniform() < 0.3)
            {
                b.active = r.uniform() < 0.7;
                b.type = (FilterType) (int) (r.uniform() * (int) FilterType::NumTypes);
                b.slopeIndex = (int) (r.uniform() * 8);
                b.stereo = (StereoMode) (int) (r.uniform() * 5);
            }
            b.freqHz = 10.0 * std::pow (3000.0, r.uniform());
            b.gainDb = -30 + 60 * r.uniform();
            b.q = 0.1 * std::pow (300.0, r.uniform());
            b.dyn.enabled = r.uniform() < 0.3;
            b.dyn.thresholdDb = -80 * r.uniform();
            b.dyn.rangeDb = -18 + 36 * r.uniform();
            b.dyn.attackMs = 0.1 + 250 * r.uniform();
            b.dyn.releaseMs = 5 + 2000 * r.uniform();
        }
        x.inputTrimDb = -24 + 48 * r.uniform();
        x.outputTrimDb = -24 + 48 * r.uniform();
    });
    CHECK (allFinite (s.l) && allFinite (s.r));
    CHECK (e.getNonFiniteEvents() == 0);
    const double outPk = std::max (peakAbs (s.l), peakAbs (s.r));
    measure ("random parameter storm 20 s: output/input peak (dB)", db (outPk / inPk));
}

ME_TEST ("Automation", SampleRateAndBlockSizeChanges)
{
    EngineSettings st;
    st.bands[0] = bell (1000, 6, 1.0);
    st.bands[1] = band (FilterType::HighPass, 30, 0, kButterworthQ, 3);
    EQEngine e;
    double worst = 0.0;
    for (int round = 0; round < 3; ++round)
        for (double fs : allSampleRates())
            for (int block : { 1, 32, 480, 2048, 8192 })
            {
                e.prepare (fs, block, 2);
                e.setSettings (st);
                const int n = (int) (fs * 0.6);
                Stereo s (sine (1000, fs, n, 0.25), sine (1000, fs, n, 0.25));
                runEngine (e, s, block == 8192 ? 3000 : block); // also exceed the prepared block size
                CHECK (allFinite (s.l));
                const int len = (int) (fs * 0.2);
                const double got = db (std::abs (toneAmplitude (s.l, 1000, fs, n - len, len)) / 0.25);
                worst = std::max (worst, std::abs (got - designedGainDb (st, fs, 1000)));
            }
    measure ("re-prepare at all rates/block sizes: max response error at 1 kHz (dB)", worst);
    CHECK (worst < 0.001);
}

ME_TEST ("Automation", LongDurationStability)
{
    // 10 minutes of stereo audio at 48 kHz through a busy configuration with slow automation.
    const double fs = 48000.0;
    EQEngine e;
    e.prepare (fs, 512, 2);
    EngineSettings st;
    st.bands[0] = band (FilterType::HighPass, 30, 0, kButterworthQ, 3);
    st.bands[1] = bell (90, 3, 1.5);
    st.bands[2] = bell (300, -2, 2.0, StereoMode::Mid);
    st.bands[3] = bell (2500, 0, 3.0);
    st.bands[3].dyn.enabled = true;
    st.bands[3].dyn.thresholdDb = -24;
    st.bands[3].dyn.rangeDb = -6;
    st.bands[4] = band (FilterType::HighShelf, 10000, 2, kButterworthQ, 1, StereoMode::Side);
    st.bands[5] = band (FilterType::LowPass, 18000, 0, kButterworthQ, 2);
    const int block = 512;
    const int chunk = (int) fs * 10;
    Stereo src (pinkNoise (chunk, 0.3, 400), pinkNoise (chunk, 0.3, 401));
    double maxPk = 0.0;
    bool finite = true;
    const auto t0 = std::chrono::steady_clock::now();
    for (int rep = 0; rep < 60; ++rep)
    {
        Stereo s = src;
        runAutomated (e, s, block, st, [&] (int k, EngineSettings& x)
        {
            const double t = rep * 10.0 + k * block / fs;
            x.bands[1].gainDb = 3.0 * std::sin (kTwoPi * t / 7.0);
            x.bands[2].freqHz = 300.0 * std::pow (2.0, std::sin (kTwoPi * t / 13.0));
        });
        finite = finite && allFinite (s.l) && allFinite (s.r);
        maxPk = std::max ({ maxPk, peakAbs (s.l), peakAbs (s.r) });
    }
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    measure ("10 min stereo @ 48 kHz processed in (s)", secs);
    measure ("realtime factor (audio time / processing time)", 600.0 / secs);
    measure ("max output peak", maxPk);
    CHECK (finite);
    CHECK (maxPk < 4.0);
    CHECK (e.getNonFiniteEvents() == 0);
}
