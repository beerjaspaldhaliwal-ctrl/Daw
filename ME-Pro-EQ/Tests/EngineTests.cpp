// ME Pro EQ — ME Audio
// Engine-level tests: transparency, stereo / mid-side integrity, block-size invariance, sample rates,
// bypass, trims, robustness against invalid input, audition, mono layouts.
#include "EngineTestUtils.h"
#include "TestFramework.h"

#include <chrono>
#include <limits>

using namespace mepro::dsp;
using namespace metest;

ME_TEST ("Engine", NeutralEngineIsBitExact)
{
    for (double fs : allSampleRates())
    {
        EQEngine e;
        e.prepare (fs, 512, 2);
        Stereo s (whiteNoise (20000, 0.7, 1), whiteNoise (20000, 0.7, 2));
        const Stereo ref = s;
        runEngine (e, s, 512);
        CHECK_MSG (s.l == ref.l && s.r == ref.r, "no bands, fs " << fs);
    }

    // All 16 bands used, gain exactly 0 dB, every gain-type and every stereo mode: output must be bit-exact.
    EQEngine e;
    e.prepare (48000.0, 256, 2);
    EngineSettings st;
    const FilterType types[] = { FilterType::Bell, FilterType::LowShelf, FilterType::HighShelf, FilterType::Tilt };
    for (int b = 0; b < kNumBands; ++b)
        st.bands[(size_t) b] = band (types[b % 4], 40.0 * std::pow (1.5, b), 0.0, 0.5 + b, b % 6, (StereoMode) (b % 5));
    e.setSettings (st);
    Stereo s (pinkNoise (48000, 0.8, 3), pinkNoise (48000, 0.8, 4));
    const Stereo ref = s;
    runEngine (e, s, 256);
    CHECK_MSG (s.l == ref.l && s.r == ref.r, "16 neutral bands (all stereo modes) not bit-exact");
}

ME_TEST ("Engine", MidSideIntegrity)
{
    const double fs = 48000.0;
    // 1) Mono-compatible input (L == R) through a heavily boosted Mid band and a boosted Side band:
    //    S must stay exactly zero (L == R at the output) and the Side band must not alter the signal.
    {
        EQEngine e;
        e.prepare (fs, 512, 2);
        EngineSettings st;
        st.bands[0] = bell (1000, 9, 1.0, StereoMode::Mid);
        st.bands[1] = bell (5000, 12, 0.7, StereoMode::Side);
        e.setSettings (st);
        auto x = pinkNoise (48000, 0.5, 9);
        Stereo s (x, x);
        runEngine (e, s, 512);
        CHECK (s.l == s.r);
        // Mid band applied, side untouched: compare with the same band in Stereo mode.
        EQEngine e2;
        e2.prepare (fs, 512, 2);
        EngineSettings st2;
        st2.bands[0] = bell (1000, 9, 1.0, StereoMode::Stereo);
        e2.setSettings (st2);
        Stereo s2 (x, x);
        runEngine (e2, s2, 512);
        double maxDiff = 0.0;
        for (int i = 0; i < s.size(); ++i)
            maxDiff = std::max (maxDiff, std::abs (s.l[(size_t) i] - s2.l[(size_t) i]));
        measure ("mono input: max |Mid-band output - Stereo-band output|", maxDiff);
        CHECK (maxDiff < 1e-12);
    }
    // 2) Pure side input (L == -R) through a Mid band: untouched (bit-exact).
    {
        EQEngine e;
        e.prepare (fs, 512, 2);
        EngineSettings st;
        st.bands[0] = bell (1000, 12, 1.0, StereoMode::Mid);
        e.setSettings (st);
        auto x = pinkNoise (48000, 0.5, 10);
        std::vector<double> nx (x.size());
        for (size_t i = 0; i < x.size(); ++i)
            nx[i] = -x[i];
        Stereo s (x, nx);
        const Stereo ref = s;
        runEngine (e, s, 512);
        double maxDiff = 0.0;
        for (int i = 0; i < s.size(); ++i)
            maxDiff = std::max ({ maxDiff, std::abs (s.l[(size_t) i] - ref.l[(size_t) i]), std::abs (s.r[(size_t) i] - ref.r[(size_t) i]) });
        measure ("side-only input through Mid band: max deviation", maxDiff);
        CHECK (maxDiff == 0.0);
    }
    // 3) Neutral M/S round trip with arbitrary stereo material: bit-exact (identity bands bypass decode).
    {
        EQEngine e;
        e.prepare (fs, 512, 2);
        EngineSettings st;
        st.bands[0] = bell (1000, 0, 1.0, StereoMode::Mid);
        st.bands[1] = bell (300, 0, 3.0, StereoMode::Side);
        e.setSettings (st);
        Stereo s (whiteNoise (30000, 0.9, 11), whiteNoise (30000, 0.9, 12));
        const Stereo ref = s;
        runEngine (e, s, 512);
        CHECK (s.l == ref.l && s.r == ref.r);
    }
    // 4) Non-neutral Mid + Side bands: reconstruction error relative to the exact M/S math.
    {
        EQEngine e;
        e.prepare (fs, 4096, 2);
        EngineSettings st;
        st.bands[0] = bell (2000, 6, 1.0, StereoMode::Mid);
        e.setSettings (st);
        const int n = 48000;
        Stereo s (whiteNoise (n, 0.5, 13), whiteNoise (n, 0.5, 14));
        std::vector<double> mid (n), side (n);
        for (int i = 0; i < n; ++i)
        {
            mid[(size_t) i] = 0.5 * (s.l[(size_t) i] + s.r[(size_t) i]);
            side[(size_t) i] = 0.5 * (s.l[(size_t) i] - s.r[(size_t) i]);
        }
        runEngine (e, s, 4096);
        // Reference: filter mid alone with the same band in a mono engine.
        EQEngine m;
        m.prepare (fs, 4096, 1);
        EngineSettings sm;
        sm.bands[0] = bell (2000, 6, 1.0, StereoMode::Stereo);
        m.setSettings (sm);
        runEngineMono (m, mid, 4096);
        double err = 0.0;
        for (int i = 0; i < n; ++i)
        {
            err = std::max (err, std::abs (s.l[(size_t) i] - (mid[(size_t) i] + side[(size_t) i])));
            err = std::max (err, std::abs (s.r[(size_t) i] - (mid[(size_t) i] - side[(size_t) i])));
        }
        measure ("M/S processing max reconstruction error vs exact math", err);
        CHECK (err < 1e-14);
    }
}

ME_TEST ("Engine", ChannelIsolation)
{
    const double fs = 48000.0;
    EQEngine e;
    e.prepare (fs, 512, 2);
    EngineSettings st;
    st.bands[0] = bell (1000, 12, 1.0, StereoMode::Left);
    st.bands[1] = band (FilterType::HighPass, 500, 0, kButterworthQ, 5, StereoMode::Left);
    e.setSettings (st);
    Stereo s (pinkNoise (48000, 0.5, 20), pinkNoise (48000, 0.5, 21));
    const Stereo ref = s;
    runEngine (e, s, 512);
    CHECK (s.r == ref.r);
    CHECK (s.l != ref.l);

    EQEngine e2;
    e2.prepare (fs, 512, 2);
    EngineSettings st2;
    st2.bands[0] = bell (1000, 12, 1.0, StereoMode::Right);
    e2.setSettings (st2);
    Stereo s2 = ref;
    runEngine (e2, s2, 512);
    CHECK (s2.l == ref.l);
    CHECK (s2.r != ref.r);

    // Stereo band: identical inputs give identical outputs on both channels (balance / polarity preserved).
    EQEngine e3;
    e3.prepare (fs, 512, 2);
    EngineSettings st3;
    st3.bands[0] = bell (3000, -6, 2.0);
    st3.bands[1] = band (FilterType::LowShelf, 100, 4, kButterworthQ, 1);
    e3.setSettings (st3);
    auto x = pinkNoise (48000, 0.5, 22);
    Stereo s3 (x, x);
    runEngine (e3, s3, 512);
    CHECK (s3.l == s3.r);
    // polarity: correlation of output with input at 0 lag is positive for a positive-gain band at DC region
    double c = 0.0;
    for (int i = 0; i < s3.size(); ++i)
        c += s3.l[(size_t) i] * x[(size_t) i];
    CHECK (c > 0.0);
}

ME_TEST ("Engine", BlockSizeInvariance)
{
    const double fs = 44100.0;
    EngineSettings st;
    st.bands[0] = band (FilterType::HighPass, 35, 0, kButterworthQ, 4);
    st.bands[1] = bell (250, -4, 1.2, StereoMode::Mid);
    st.bands[2] = bell (3200, 3, 0.8, StereoMode::Side);
    st.bands[3] = band (FilterType::HighShelf, 9000, 2.5, kButterworthQ, 1);
    st.bands[4] = bell (180, 0, 2.0);
    st.bands[4].dyn.enabled = true;
    st.bands[4].dyn.thresholdDb = -30;
    st.bands[4].dyn.rangeDb = -9;
    st.bands[4].dyn.attackMs = 2;
    st.bands[5] = band (FilterType::LowPass, 16000, 0, 1.2, 2, StereoMode::Left);
    st.inputTrimDb = -1.5;
    st.outputTrimDb = 2.0;

    const Stereo src (pinkNoise (60000, 0.6, 30), pinkNoise (60000, 0.6, 31));
    Stereo ref;
    bool first = true;
    for (int block : { 4096, 1, 3, 16, 17, 64, 333, 1024 })
    {
        EQEngine e;
        e.prepare (fs, 4096, 2);
        e.setSettings (st);
        Stereo s = src;
        runEngine (e, s, block);
        if (first)
        {
            ref = s;
            first = false;
            continue;
        }
        CHECK_MSG (s.l == ref.l && s.r == ref.r, "block size " << block << " output differs from block size 4096");
    }
}

ME_TEST ("Engine", ResponseAtAllSampleRates)
{
    for (double fs : allSampleRates())
    {
        EngineSettings st;
        st.bands[0] = bell (1000, 6, 1.0);
        st.bands[1] = band (FilterType::HighPass, 40, 0, kButterworthQ, 3);
        st.bands[2] = band (FilterType::HighShelf, 8000, -3, kButterworthQ, 1);
        st.bands[3] = band (FilterType::LowPass, 0.45 * fs > 30000 ? 30000 : 0.4 * fs, 0, kButterworthQ, 5);
        EQEngine e;
        e.prepare (fs, 1024, 2);
        e.setSettings (st);
        double worst = 0.0;
        for (double f : { 60.0, 200.0, 1000.0, 3000.0, 10000.0, 18000.0 })
        {
            const int n = (int) (fs * 1.5);
            Stereo s (sine (f, fs, n, 0.25), sine (f, fs, n, 0.25));
            runEngine (e, s, 1024);
            const int len = (int) (fs * 0.5);
            const double got = db (std::abs (toneAmplitude (s.l, f, fs, n - len, len)) / 0.25);
            const double want = designedGainDb (st, fs, f);
            worst = std::max (worst, std::abs (got - want));
            e.reset();
        }
        measure ("fs " + str (fs) + ": max |measured - designed| (dB)", worst);
        CHECK (worst < 0.001);
    }
}

ME_TEST ("Engine", BypassAndTrims)
{
    const double fs = 48000.0;
    EQEngine e;
    e.prepare (fs, 512, 2);
    EngineSettings st;
    st.bands[0] = bell (1000, 12, 1.0);
    st.outputTrimDb = 6.0;
    st.bypass = true;
    e.setSettings (st);
    e.reset(); // start fully bypassed
    Stereo s (whiteNoise (30000, 0.5, 40), whiteNoise (30000, 0.5, 41));
    const Stereo ref = s;
    runEngine (e, s, 512);
    CHECK_MSG (s.l == ref.l && s.r == ref.r, "bypassed output must be bit-exact input");

    // Trim accuracy: +6 dB output trim, -3 dB input trim, no bands.
    EQEngine t;
    t.prepare (fs, 512, 2);
    EngineSettings ts;
    ts.inputTrimDb = -3.0;
    ts.outputTrimDb = 6.0;
    t.setSettings (ts);
    t.reset();
    Stereo x (sine (1000, fs, 48000, 0.1), sine (1000, fs, 48000, 0.1));
    runEngine (t, x, 512);
    const double g = db (std::abs (toneAmplitude (x.l, 1000, fs, 24000, 24000)) / 0.1);
    measure ("trim -3 dB in / +6 dB out: measured gain (dB)", g);
    CHECK_NEAR (g, 3.0, 1e-9);

    // Bypass toggle is click-free: HF residual on a 200 Hz tone while toggling every 100 ms.
    EQEngine b;
    b.prepare (fs, 480, 2);
    EngineSettings bs;
    bs.bands[0] = bell (200, 12, 1.0);
    Stereo y (sine (200, fs, 96000, 0.5), sine (200, fs, 96000, 0.5));
    for (int pos = 0, k = 0; pos < y.size(); pos += 480, ++k)
    {
        bs.bypass = (k / 10) % 2 == 1;
        b.setSettings (bs);
        double* ch[2] = { y.l.data() + pos, y.r.data() + pos };
        b.process (ch, 2, std::min (480, y.size() - pos));
    }
    const double hf = hfResidualDb (y.l, fs, 4000, 4800);
    measure ("bypass toggling every 100 ms on 200 Hz tone: HF residual >4 kHz (dB rel. signal)", hf);
    CHECK (hf < -80.0);
}

ME_TEST ("Engine", InvalidInputIsContained)
{
    const double fs = 48000.0;
    EQEngine e;
    e.prepare (fs, 256, 2);
    EngineSettings st;
    st.bands[0] = bell (100, 18, 10.0);
    st.bands[1] = band (FilterType::HighPass, 20, 0, 5.0, 7);
    e.setSettings (st);
    Stereo s (whiteNoise (20000, 0.5, 50), whiteNoise (20000, 0.5, 51));
    s.l[5000] = std::numeric_limits<double>::quiet_NaN();
    s.r[7000] = std::numeric_limits<double>::infinity();
    s.l[9000] = -std::numeric_limits<double>::infinity();
    runEngine (e, s, 256);
    CHECK (allFinite (s.l) && allFinite (s.r));
    CHECK (e.getNonFiniteEvents() == 0); // sanitised at the input, never reached the filters

    // Huge but finite input: filters stay finite.
    Stereo h (whiteNoise (20000, 1e12, 52), whiteNoise (20000, 1e12, 53));
    runEngine (e, h, 256);
    CHECK (allFinite (h.l) && allFinite (h.r));
}

ME_TEST ("Engine", DenormalsDoNotSlowProcessing)
{
    const double fs = 48000.0;
    EngineSettings st;
    for (int b = 0; b < kNumBands; ++b)
        st.bands[(size_t) b] = bell (30.0 * std::pow (1.55, b), (b % 2 ? 6 : -6), 2.0);
    auto timeIt = [&] (double amp)
    {
        EQEngine e;
        e.prepare (fs, 512, 2);
        e.setSettings (st);
        Stereo s (whiteNoise (fs * 4, amp, 60), whiteNoise (fs * 4, amp, 61));
        // let the bands fade in with normal signal first
        Stereo warm (whiteNoise (4800, 0.1, 62), whiteNoise (4800, 0.1, 63));
        runEngine (e, warm, 512);
        const auto a = std::chrono::steady_clock::now();
        runEngine (e, s, 512);
        const double t = std::chrono::duration<double> (std::chrono::steady_clock::now() - a).count();
        CHECK (allFinite (s.l));
        return t;
    };
    const double normal = timeIt (0.1);
    const double denorm = timeIt (1e-310); // subnormal input
    const double tiny = timeIt (1e-300);
    measure ("4 s stereo, 16 bands: normal input (s)", normal);
    measure ("4 s stereo, 16 bands: subnormal input (s)", denorm);
    measure ("4 s stereo, 16 bands: 1e-300 input (s)", tiny);
    CHECK (denorm < normal * 2.0 + 0.05);
    CHECK (tiny < normal * 2.0 + 0.05);
}

ME_TEST ("Engine", MonoBusBehaviour)
{
    const double fs = 48000.0;
    EQEngine e;
    e.prepare (fs, 512, 1);
    EngineSettings st;
    st.bands[0] = bell (1000, 6, 1.0, StereoMode::Stereo);
    st.bands[1] = bell (3000, 6, 1.0, StereoMode::Mid);
    st.bands[2] = bell (500, 12, 1.0, StereoMode::Left);  // no meaning on a mono bus: inactive
    st.bands[3] = bell (500, 12, 1.0, StereoMode::Side);  // side of a mono signal is zero: inactive
    e.setSettings (st);
    const int n = 72000;
    auto x = sine (500, fs, n, 0.2);
    runEngineMono (e, x, 512);
    EngineSettings ref;
    ref.bands[0] = st.bands[0];
    ref.bands[1] = st.bands[1];
    const double got = db (std::abs (toneAmplitude (x, 500, fs, n / 2, n / 2)) / 0.2);
    const double want = designedGainDb (ref, fs, 500);
    measure ("mono bus: 500 Hz gain (Left/Side bands must be inactive) (dB)", got);
    CHECK_NEAR (got, want, 0.001);
}

ME_TEST ("Engine", BandAuditionAndMonoFold)
{
    const double fs = 48000.0;
    EngineSettings st;
    st.bands[0] = bell (2000, 6, 4.0);
    st.auditionBand = 0;
    EQEngine e;
    e.prepare (fs, 512, 2);
    e.setSettings (st);
    const int n = 48000;
    // level at f0 is preserved (band-pass with 0 dB peak applied after the EQ)
    Stereo a (sine (2000, fs, n, 0.1), sine (2000, fs, n, 0.1));
    runEngine (e, a, 512);
    const double atF0 = db (std::abs (toneAmplitude (a.l, 2000, fs, n / 2, n / 2)) / 0.1);
    measure ("audition: level at f0 relative to input (expect EQ gain +6 dB)", atF0);
    CHECK_NEAR (atF0, 6.0, 0.01);
    e.reset();
    Stereo b (sine (250, fs, n, 0.1), sine (250, fs, n, 0.1));
    runEngine (e, b, 512);
    const double off = db (std::abs (toneAmplitude (b.l, 250, fs, n / 2, n / 2)) / 0.1);
    measure ("audition: 3 octaves below f0 (dB)", off);
    CHECK (off < -20.0);

    // Mono fold: both outputs equal (L+R)/2.
    EngineSettings m;
    m.monoAudition = true;
    EQEngine f;
    f.prepare (fs, 512, 2);
    f.setSettings (m);
    Stereo s (whiteNoise (10000, 0.3, 70), whiteNoise (10000, 0.3, 71));
    const Stereo ref = s;
    runEngine (f, s, 512);
    for (int i = 0; i < s.size(); ++i)
    {
        if (s.l[(size_t) i] != 0.5 * (ref.l[(size_t) i] + ref.r[(size_t) i]) || s.l[(size_t) i] != s.r[(size_t) i])
        {
            CHECK_MSG (false, "mono fold mismatch at " << i);
            break;
        }
    }
}

ME_TEST ("Engine", ClipIndicatorAndMeters)
{
    EQEngine e;
    e.prepare (48000.0, 512, 2);
    EngineSettings st;
    st.outputTrimDb = 12.0;
    e.setSettings (st);
    e.reset();
    Stereo s (sine (100, 48000, 4800, 0.5), sine (100, 48000, 4800, 0.1));
    runEngine (e, s, 512);
    CHECK (e.hasClipped());
    const float inL = e.takeInputPeak (0), outL = e.takeOutputPeak (0);
    CHECK_NEAR (inL, 0.5, 1e-3);
    CHECK_NEAR (outL, 0.5 * std::pow (10.0, 12.0 / 20.0), 2e-3);
    e.resetClip();
    CHECK (! e.hasClipped());
}

ME_TEST ("Engine", MultipleInstancesAreIndependent)
{
    const double fs = 48000.0;
    EngineSettings a, b;
    a.bands[0] = bell (500, 9, 2.0);
    b.bands[0] = band (FilterType::HighPass, 2000, 0, kButterworthQ, 5);
    const Stereo sa (pinkNoise (30000, 0.4, 80), pinkNoise (30000, 0.4, 81));
    const Stereo sb (whiteNoise (30000, 0.4, 82), whiteNoise (30000, 0.4, 83));

    // Reference: each instance alone
    EQEngine ea, eb;
    ea.prepare (fs, 256, 2);
    eb.prepare (fs, 256, 2);
    ea.setSettings (a);
    eb.setSettings (b);
    Stereo ra = sa, rb = sb;
    runEngine (ea, ra, 256);
    runEngine (eb, rb, 256);

    // Interleaved: 32 instances processed block-by-block in turn (as a host would)
    std::vector<EQEngine> many (32);
    std::vector<Stereo> bufs;
    for (int i = 0; i < 32; ++i)
    {
        many[(size_t) i].prepare (fs, 256, 2);
        many[(size_t) i].setSettings (i % 2 ? b : a);
        bufs.push_back (i % 2 ? sb : sa);
    }
    for (int pos = 0; pos < sa.size(); pos += 256)
        for (int i = 0; i < 32; ++i)
        {
            double* ch[2] = { bufs[(size_t) i].l.data() + pos, bufs[(size_t) i].r.data() + pos };
            many[(size_t) i].process (ch, 2, std::min (256, sa.size() - pos));
        }
    bool ok = true;
    for (int i = 0; i < 32; ++i)
        ok = ok && (bufs[(size_t) i].l == (i % 2 ? rb.l : ra.l)) && (bufs[(size_t) i].r == (i % 2 ? rb.r : ra.r));
    CHECK (ok);
}

ME_TEST ("Engine", FloatAndDoubleEntryPointsAgree)
{
    const double fs = 48000.0;
    EngineSettings st;
    st.bands[0] = bell (700, 5, 1.0);
    st.bands[1] = band (FilterType::LowShelf, 90, -3, kButterworthQ, 1);
    EQEngine ed, ef;
    ed.prepare (fs, 512, 2);
    ef.prepare (fs, 512, 2);
    ed.setSettings (st);
    ef.setSettings (st);
    const int n = 20000;
    auto x = pinkNoise (n, 0.5, 90);
    std::vector<float> fl (x.begin(), x.end()), fr (x.begin(), x.end());
    std::vector<double> dl (fl.begin(), fl.end()), dr (fr.begin(), fr.end());
    for (int pos = 0; pos < n; pos += 512)
    {
        const int m = std::min (512, n - pos);
        float* fc[2] = { fl.data() + pos, fr.data() + pos };
        double* dc[2] = { dl.data() + pos, dr.data() + pos };
        ef.process (fc, 2, m);
        ed.process (dc, 2, m);
    }
    double maxDiff = 0.0;
    for (int i = 0; i < n; ++i)
        maxDiff = std::max (maxDiff, std::abs ((double) fl[(size_t) i] - (double) (float) dl[(size_t) i]));
    measure ("float path vs double path (after float rounding) max diff", maxDiff);
    CHECK (maxDiff == 0.0);
}

ME_TEST ("Engine", RenderStartIsFullyProcessed)
{
    // After prepare()/reset() (transport start, offline render) active bands apply from the very first
    // sample: the output must equal the band filter run from zero state, sample for sample.
    const double fs = 44100.0;
    EngineSettings st;
    st.bands[0] = bell (800, 9, 2.0);
    st.bands[1] = band (FilterType::HighPass, 60, 0, kButterworthQ, 3);
    EQEngine e;
    e.prepare (fs, 256, 2);
    e.setSettings (st);
    e.reset();
    auto x = whiteNoise (8192, 0.5, 500);
    Stereo s (x, x);
    runEngine (e, s, 256);

    auto ref = x;
    for (int b = 0; b < 2; ++b)
    {
        const auto& bs = st.bands[(size_t) b];
        SvfCascade f;
        f.setLayout (sectionLayout (bs.type, bs.slopeIndex));
        f.setCoefficients (designCascade ({ bs.type, bs.freqHz, bs.gainDb, bs.q, bs.slopeIndex }, fs));
        f.processMono (ref.data(), (int) ref.size(), 1, 0);
    }
    double maxDiff = 0.0;
    for (size_t i = 0; i < ref.size(); ++i)
        maxDiff = std::max (maxDiff, std::abs (s.l[i] - ref[i]));
    measure ("first 8192 samples after reset vs ideal filter from zero state: max diff", maxDiff);
    CHECK (maxDiff < 1e-15);
}
