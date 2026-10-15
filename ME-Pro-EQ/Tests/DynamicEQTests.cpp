// ME Pro EQ — ME Audio
// Dynamic EQ tests with synthetic signals containing known spectral events. All verdicts are quantitative.
#include "EngineTestUtils.h"
#include "TestFramework.h"

#include "DSP/DynamicEQ.h"

using namespace mepro::dsp;
using namespace metest;

namespace
{
BandSettings dynBell (double f, double q, double thr, double ratio, double range, double att, double rel,
                      DetectorMode det = DetectorMode::Peak)
{
    BandSettings b = bell (f, 0.0, q);
    b.dyn.enabled = true;
    b.dyn.thresholdDb = thr;
    b.dyn.ratio = ratio;
    b.dyn.rangeDb = range;
    b.dyn.attackMs = att;
    b.dyn.releaseMs = rel;
    b.dyn.detector = det;
    return b;
}

// Runs the engine with block size = control period and records the band's reported dynamic gain per block.
std::vector<double> runAndTraceGain (EQEngine& e, Stereo& s, int bandIndex, const Stereo* sc = nullptr)
{
    const int block = e.getControlPeriod();
    std::vector<double> trace;
    for (int pos = 0; pos < s.size(); pos += block)
    {
        const int n = std::min (block, s.size() - pos);
        double* ch[2] = { s.l.data() + pos, s.r.data() + pos };
        if (sc)
        {
            const double* scp[2] = { sc->l.data() + pos, sc->r.data() + pos };
            e.process (ch, 2, n, scp, 2);
        }
        else
            e.process (ch, 2, n);
        trace.push_back (e.getBandDynamicGainDb (bandIndex));
    }
    return trace;
}
} // namespace

ME_TEST ("Dynamic", GainComputerCurve)
{
    DynamicSettings s;
    s.thresholdDb = -20;
    s.ratio = 4;
    s.rangeDb = -12;
    CHECK_NEAR (dynamicGainForLevel (-40, s), 0.0, 1e-12);
    CHECK_NEAR (dynamicGainForLevel (-23.0001, s), 0.0, 1e-9);           // below knee
    CHECK_NEAR (dynamicGainForLevel (-20, s), -0.75 * 3.0 * 3.0 / 12.0, 1e-12); // knee midpoint: 0.5625 dB
    CHECK_NEAR (dynamicGainForLevel (-10, s), -7.5, 1e-12);               // 10 dB over * 0.75
    CHECK_NEAR (dynamicGainForLevel (0, s), -12.0, 1e-12);                // limited by range
    s.rangeDb = 6;
    CHECK_NEAR (dynamicGainForLevel (-10, s), 6.0, 1e-12);                // enhancement, bounded
    CHECK_NEAR (dynamicGainForLevel (-16, s), 3.0, 1e-12);
    s.ratio = 1;
    CHECK_NEAR (dynamicGainForLevel (0, s), 0.0, 1e-12);                  // ratio 1 => no action
}

ME_TEST ("Dynamic", ExcessiveToneIsReduced)
{
    // 250 Hz tone over quiet pink noise; the tone steps from -40 dBFS to -10 dBFS (peak) after 1 s.
    const double fs = 48000.0;
    const int n = (int) (fs * 3);
    auto noise = pinkNoise (n, 0.003, 100);
    auto x = noise;
    for (int i = 0; i < n; ++i)
        x[(size_t) i] += (i < fs ? 0.01 : 0.316227766) * std::sin (kTwoPi * 250.0 * i / fs);
    EngineSettings st;
    st.bands[0] = dynBell (250, 2.0, -30, 4, -12, 5, 100);
    EQEngine e;
    e.prepare (fs, 512, 2);
    e.setSettings (st);
    Stereo s (x, x);
    const auto trace = runAndTraceGain (e, s, 0);
    // before the step: no reduction; after: limited by range (-12 dB) since 20 dB over * 0.75 = 15 dB
    const int cp = e.getControlPeriod();
    const double before = trace[(size_t) (int) (0.9 * fs / cp)];
    const double after = trace[(size_t) (int) (2.5 * fs / cp)];
    measure ("reported dynamic gain before step (dB)", before);
    measure ("reported dynamic gain after step (dB)", after);
    CHECK (std::abs (before) < 0.05);
    CHECK_NEAR (after, -12.0, 0.01);
    // Measured tone attenuation (output vs input tone amplitude) agrees with the reported gain.
    const int len = (int) (fs * 0.5);
    const double inAmp = std::abs (toneAmplitude (x, 250, fs, n - len, len));
    const double outAmp = std::abs (toneAmplitude (s.l, 250, fs, n - len, len));
    measure ("measured 250 Hz change after step (dB)", db (outAmp / inAmp));
    CHECK_NEAR (db (outAmp / inAmp), after, 0.1);
}

ME_TEST ("Dynamic", DetectorIsFrequencySelective)
{
    const double fs = 48000.0;
    const int n = (int) (fs * 2);
    EngineSettings st;
    st.bands[0] = dynBell (3000, 2.0, -30, 10, -12, 3, 80);

    // Loud out-of-band 200 Hz tone (-6 dBFS) must not trigger the 3 kHz band.
    {
        EQEngine e;
        e.prepare (fs, 512, 2);
        e.setSettings (st);
        auto x = sine (200, fs, n, 0.5);
        Stereo s (x, x);
        const auto tr = runAndTraceGain (e, s, 0);
        double worst = 0.0;
        for (double g : tr)
            worst = std::min (worst, g);
        measure ("200 Hz @ -6 dBFS into 3 kHz dynamic band: max reduction (dB)", worst);
        CHECK (worst > -0.5);
    }
    // In-band 3 kHz tone of the same level triggers full range, and a simultaneous 200 Hz component is
    // left essentially untouched.
    {
        EQEngine e;
        e.prepare (fs, 512, 2);
        e.setSettings (st);
        auto x = sine (3000, fs, n, 0.5);
        auto lo = sine (200, fs, n, 0.1);
        for (int i = 0; i < n; ++i)
            x[(size_t) i] += lo[(size_t) i];
        Stereo s (x, x);
        const auto tr = runAndTraceGain (e, s, 0);
        measure ("3 kHz @ -6 dBFS: dynamic gain (dB)", tr.back());
        CHECK_NEAR (tr.back(), -12.0, 0.01);
        const int len = (int) (fs * 0.5);
        const double c200 = db (std::abs (toneAmplitude (s.l, 200, fs, n - len, len)) / 0.1);
        const double c3k = db (std::abs (toneAmplitude (s.l, 3000, fs, n - len, len)) / 0.5);
        measure ("collateral change at 200 Hz while 3 kHz is reduced 12 dB (dB)", c200);
        measure ("3 kHz component change (dB)", c3k);
        CHECK (std::abs (c200) < 0.1);
        CHECK_NEAR (c3k, -12.0, 0.1);
    }
}

ME_TEST ("Dynamic", AttackAndReleaseTiming)
{
    // Detector envelope timing for a 1 kHz tone stepping from -80 dBFS to -6 dBFS and back.
    const double fs = 48000.0;
    for (double att : { 1.0, 10.0, 50.0 })
        for (double rel : { 50.0, 300.0 })
        {
            EngineSettings st;
            st.bands[0] = dynBell (1000, 1.0, -60, 2, -6, att, rel);
            EQEngine e;
            e.prepare (fs, 512, 2);
            e.setSettings (st);
            const int stepOn = (int) fs, stepOff = (int) (fs * 3), n = (int) (fs * 6);
            std::vector<double> x ((size_t) n);
            for (int i = 0; i < n; ++i)
                x[(size_t) i] = (i >= stepOn && i < stepOff ? 0.5 : 0.0001) * std::sin (kTwoPi * 1000.0 * i / fs);
            Stereo s (x, x);
            const int cp = e.getControlPeriod();
            std::vector<double> env;
            for (int pos = 0; pos < n; pos += cp)
            {
                double* ch[2] = { s.l.data() + pos, s.r.data() + pos };
                e.process (ch, 2, cp);
                env.push_back (std::pow (10.0, e.getBandDetectorLevelDb (0) / 20.0));
            }
            const double hi = env[(size_t) ((stepOff - 1) / cp)];
            const double lo = env[(size_t) ((stepOn - 1) / cp)];
            auto firstCross = [&] (int from, double level, bool rising)
            {
                for (size_t k = (size_t) (from / cp); k < env.size(); ++k)
                    if (rising ? env[k] >= level : env[k] <= level)
                        return (double) (k + 1) * cp / fs - (double) from / fs;
                return 1e9;
            };
            const double tAtt = firstCross (stepOn, lo + (hi - lo) * (1.0 - std::exp (-1.0)), true) * 1000.0;
            const double tRel = firstCross (stepOff, lo + (hi - lo) * std::exp (-1.0), false) * 1000.0;
            // Expected: attack = one-pole with the attack time constant. Release = peak-hold release (stage 1)
            // followed by the attack smoother (stage 2): two cascaded one-poles; solve for the 1/e point.
            const double ta = att, tr = rel;
            auto twoPole = [&] (double t) { return std::abs (tr - ta) < 1e-9 ? (1 + t / ta) * std::exp (-t / ta)
                                                                               : (tr * std::exp (-t / tr) - ta * std::exp (-t / ta)) / (tr - ta); };
            double lo2 = 0, hi2 = 10 * (ta + tr);
            for (int it = 0; it < 100; ++it)
            {
                const double mid = 0.5 * (lo2 + hi2);
                (twoPole (mid) > std::exp (-1.0) ? lo2 : hi2) = mid;
            }
            const double expRel = 0.5 * (lo2 + hi2);
            measure ("attack " + str (att) + " ms: measured 63% rise time (ms)", tAtt);
            measure ("release " + str (rel) + " ms (attack " + str (att) + "): measured 1/e fall time (ms), expected " + str (expRel), tRel);
            // Tolerance: detector band-pass group delay (~0.3 ms @ 1 kHz, Q=1), first-peak latency (0.25 ms),
            // peak-hold ripple and measurement quantisation (the level is reported once per fine control
            // period: 8 samples = 0.17 ms at 48 kHz).
            CHECK_MSG (std::abs (tAtt - att) < 0.05 * att + 0.9, "attack " << att << " measured " << tAtt);
            CHECK_MSG (std::abs (tRel - expRel) < 0.05 * expRel + 1.0, "release " << rel << " measured " << tRel << " expected " << expRel);
        }
}

ME_TEST ("Dynamic", EnhancementIsBounded)
{
    const double fs = 48000.0;
    EngineSettings st;
    st.bands[0] = dynBell (5000, 1.5, -40, 20, +6, 1, 50);
    EQEngine e;
    e.prepare (fs, 512, 2);
    e.setSettings (st);
    const int n = (int) fs;
    auto x = sine (5000, fs, n, 0.5);
    Stereo s (x, x);
    const auto tr = runAndTraceGain (e, s, 0);
    double mx = -100;
    for (double g : tr)
        mx = std::max (mx, g);
    measure ("enhancement +6 dB range, signal 34 dB over threshold: max gain (dB)", mx);
    CHECK (mx <= 6.0 + 1e-9);
    CHECK_NEAR (tr.back(), 6.0, 1e-6);
    CHECK (allFinite (s.l));
    const int len = n / 4;
    CHECK_NEAR (db (std::abs (toneAmplitude (s.l, 5000, fs, n - len, len)) / 0.5), 6.0, 0.05);
}

ME_TEST ("Dynamic", ExternalSidechain)
{
    const double fs = 48000.0;
    const int n = (int) (fs * 3);
    EngineSettings st;
    st.bands[0] = dynBell (1000, 1.0, -30, 10, -9, 2, 50);
    st.bands[0].dyn.source = SidechainSource::External;
    EQEngine e;
    e.prepare (fs, 512, 2);
    e.setSettings (st);
    auto main = sine (1000, fs, n, 0.3); // loud main signal, above threshold
    std::vector<double> key ((size_t) n, 0.0);
    for (int i = (int) fs; i < 2 * fs; ++i)
        key[(size_t) i] = 0.5 * std::sin (kTwoPi * 1000.0 * i / fs); // sidechain burst in the middle second
    Stereo s (main, main), sc (key, key);
    const auto tr = runAndTraceGain (e, s, 0, &sc);
    const int cp = e.getControlPeriod();
    const double g0 = tr[(size_t) (int) (0.9 * fs / cp)], g1 = tr[(size_t) (int) (1.9 * fs / cp)], g2 = tr[(size_t) (int) (2.9 * fs / cp)];
    measure ("ext. sidechain silent, main loud: gain (dB)", g0);
    measure ("ext. sidechain burst: gain (dB)", g1);
    measure ("after burst: gain (dB)", g2);
    CHECK (std::abs (g0) < 0.01);
    CHECK_NEAR (g1, -9.0, 0.01);
    CHECK (std::abs (g2) < 0.05);
}

ME_TEST ("Dynamic", ReportedGainMatchesAppliedGain)
{
    // For a steady tone at f0 the measured change must equal static gain + reported dynamic gain.
    const double fs = 44100.0;
    for (double level : { 0.05, 0.1, 0.2, 0.4 })
        for (double staticGain : { -3.0, 0.0, 4.0 })
        {
            EngineSettings st;
            st.bands[0] = dynBell (800, 1.0, -26, 3, -10, 5, 100);
            st.bands[0].gainDb = staticGain;
            EQEngine e;
            e.prepare (fs, 441, 2);
            e.setSettings (st);
            const int n = (int) (fs * 2);
            auto x = sine (800, fs, n, level);
            Stereo s (x, x);
            const auto tr = runAndTraceGain (e, s, 0);
            const int len = (int) (fs * 0.5);
            const double measured = db (std::abs (toneAmplitude (s.l, 800, fs, n - len, len)) / level);
            const double reported = staticGain + tr.back();
            CHECK_MSG (std::abs (measured - reported) < 0.05,
                       "level " << level << " static " << staticGain << " measured " << measured << " reported " << reported);
        }
}

ME_TEST ("Dynamic", SibilantBurstsAndDrumResonance)
{
    const double fs = 48000.0;
    const int n = (int) (fs * 4);
    // (a) Intermittent 6 kHz "sibilance" (band-limited noise bursts, 60 ms every 400 ms) over pink noise.
    {
        Rng r (7);
        auto base = pinkNoise (n, 0.03, 110);
        std::vector<double> burst ((size_t) n, 0.0);
        SvfCascade bp;
        bp.setLayout (sectionLayout (FilterType::BandPass, 1));
        bp.setCoefficients (designCascade ({ FilterType::BandPass, 6000, 0, 3.0, 1 }, fs));
        for (int i = 0; i < n; ++i)
            burst[(size_t) i] = ((i % 19200) < 2880) ? 0.9 * r.bipolar() : 0.0;
        bp.processMono (burst.data(), n, 1, 0);
        std::vector<double> x ((size_t) n);
        for (int i = 0; i < n; ++i)
            x[(size_t) i] = base[(size_t) i] + burst[(size_t) i];
        EngineSettings st;
        st.bands[0] = dynBell (6500, 1.5, -32, 6, -10, 1, 60);
        EQEngine e;
        e.prepare (fs, 512, 2);
        e.setSettings (st);
        Stereo s (x, x);
        const auto tr = runAndTraceGain (e, s, 0);
        const int cp = e.getControlPeriod();
        double inBurst = 0, between = 0;
        int ni = 0, nb = 0;
        for (size_t k = 0; k < tr.size(); ++k)
        {
            const int i = (int) k * cp;
            if (i < fs)
                continue;
            const int ph = i % 19200;
            if (ph > 1000 && ph < 2880)
            {
                inBurst += tr[k];
                ++ni;
            }
            else if (ph > 2880 + 9600 && ph < 19000)
            {
                between += tr[k];
                ++nb;
            }
        }
        inBurst /= std::max (1, ni);
        between /= std::max (1, nb);
        measure ("sibilance: mean dynamic gain during bursts (dB)", inBurst);
        measure ("sibilance: mean dynamic gain between bursts (dB)", between);
        CHECK (inBurst < -6.0);
        CHECK (between > -1.0);
    }
    // (b) A 180 Hz resonance excited by drum hits every 500 ms: attenuated on the hits only.
    {
        std::vector<double> x ((size_t) n, 0.0);
        for (int i = 0; i < n; ++i)
        {
            const int t = i % 24000;
            const double env = std::exp (-t / (0.08 * fs));
            x[(size_t) i] = 0.6 * env * std::sin (kTwoPi * 180.0 * t / fs) + 0.02 * std::sin (kTwoPi * 2500.0 * i / fs);
        }
        EngineSettings st;
        st.bands[0] = dynBell (180, 4, -24, 8, -8, 2, 120);
        EQEngine e;
        e.prepare (fs, 512, 2);
        e.setSettings (st);
        Stereo s (x, x);
        const auto tr = runAndTraceGain (e, s, 0);
        const int cp = e.getControlPeriod();
        const double onHit = tr[(size_t) ((24000 * 4 + 1500) / cp)];
        const double tail = tr[(size_t) ((24000 * 4 + 22000) / cp)];
        measure ("drum resonance: gain 30 ms after a hit (dB)", onHit);
        measure ("drum resonance: gain 460 ms after a hit (dB)", tail);
        CHECK (onHit < -5.0);
        CHECK (tail > -1.0);
        // The unrelated 2.5 kHz component is not modulated: compare its amplitude in a window near a hit
        // and far from it.
        const double a = std::abs (toneAmplitude (s.l, 2500, fs, 24000 * 4 + 480, 4800));
        const double b = std::abs (toneAmplitude (s.l, 2500, fs, 24000 * 4 + 18000, 4800));
        measure ("2.5 kHz component modulation between hit and tail (dB)", db (a / b));
        CHECK (std::abs (db (a / b)) < 0.2);
    }
}

ME_TEST ("Dynamic", FastSettingsRemainStable)
{
    const double fs = 96000.0;
    EngineSettings st;
    st.bands[0] = dynBell (120, 8, -40, 20, -18, 0.1, 5);
    st.bands[1] = dynBell (8000, 0.5, -50, 20, +18, 0.1, 5, DetectorMode::Rms);
    st.bands[2] = band (FilterType::LowShelf, 80, 0, kButterworthQ, 1);
    st.bands[2].dyn = st.bands[0].dyn;
    st.bands[3] = band (FilterType::HighShelf, 10000, 0, kButterworthQ, 3);
    st.bands[3].dyn = st.bands[1].dyn;
    EQEngine e;
    e.prepare (fs, 512, 2);
    e.setSettings (st);
    Stereo s (pinkNoise ((int) fs * 5, 0.5, 120), pinkNoise ((int) fs * 5, 0.5, 121));
    const double inPk = std::max (peakAbs (s.l), peakAbs (s.r));
    runEngine (e, s, 512);
    CHECK (allFinite (s.l) && allFinite (s.r));
    const double outPk = std::max (peakAbs (s.l), peakAbs (s.r));
    measure ("extreme dynamic settings: output peak / input peak (dB)", db (outPk / inPk));
    // Bounded by the +18 dB (bell) and +18 dB (shelf) maximum enhancement plus overshoot.
    CHECK (outPk < inPk * std::pow (10.0, 40.0 / 20.0));
}

ME_TEST ("Dynamic", GainModulationIsSmooth)
{
    // A 100 Hz tone that alternates between two levels every 250 ms drives a dynamic band at 100 Hz.
    // The gain modulation must not create broadband clicks: measure energy above 3 kHz.
    const double fs = 48000.0;
    const int n = (int) (fs * 4);
    std::vector<double> x ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double env = ((i / 12000) % 2) ? 0.5 : 0.05;
        x[(size_t) i] = env * std::sin (kTwoPi * 100.0 * i / fs);
    }
    // Smooth the level steps of the *test signal* itself so any HF content comes from the EQ.
    {
        double g = 0.05;
        const double c = std::exp (-1.0 / (0.005 * fs));
        for (int i = 0; i < n; ++i)
        {
            const double t = ((i / 12000) % 2) ? 0.5 : 0.05;
            g = t + (g - t) * c;
            x[(size_t) i] = g * std::sin (kTwoPi * 100.0 * i / fs);
        }
    }
    EngineSettings st;
    st.bands[0] = dynBell (100, 2, -26, 4, -12, 5, 80);
    auto runWith = [&] (int finePeriod)
    {
        EQEngine e;
        e.setControlPeriodOverride (finePeriod);
        e.prepare (fs, 512, 2);
        e.setSettings (st);
        Stereo s (x, x);
        runEngine (e, s, 512);
        return hfResidualDb (s.l, fs, 3000, 4800);
    };
    const double inHf = hfResidualDb (x, fs, 3000, 4800);
    const double ideal = runWith (1);  // coefficients redesigned every sample (reference)
    const double actual = runWith (0); // production control grid
    measure ("input HF residual >3 kHz (dB)", inHf);
    measure ("per-sample redesign (ideal) HF residual >3 kHz (dB)", ideal);
    measure ("production control grid HF residual >3 kHz (dB)", actual);
    CHECK (actual < ideal + 3.0);
    CHECK (actual < -85.0);
}
