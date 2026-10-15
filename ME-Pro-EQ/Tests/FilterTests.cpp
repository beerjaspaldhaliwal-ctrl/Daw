// ME Pro EQ — ME Audio
// Filter design + realisation tests: accuracy against the analog prototype, exactness of the realised
// transfer function, stability/realizability over dense parameter grids at every supported sample rate,
// cutoff/slope/bandwidth accuracy, boost/cut symmetry and bit-exact identity.
#include "TestFramework.h"
#include "TestSignals.h"

#include "DSP/FilterDesign.h"
#include "DSP/SvfFilter.h"

#include <algorithm>
#include <map>

using namespace mepro::dsp;
using namespace metest;

namespace
{
struct DevStats
{
    int n = 0, within02 = 0, within05 = 0;
    double maxDev = 0.0;
    std::vector<double> all;
    std::string worst;

    void add (double d, const std::string& where)
    {
        ++n;
        within02 += d <= 0.2;
        within05 += d <= 0.5;
        all.push_back (d);
        if (d > maxDev)
        {
            maxDev = d;
            worst = where;
        }
    }
    double pct (int k) const { return n ? 100.0 * k / n : 0.0; }
    double percentile (double p)
    {
        if (all.empty())
            return 0.0;
        std::sort (all.begin(), all.end());
        return all[(size_t) std::min ((double) all.size() - 1, p / 100.0 * (all.size() - 1))];
    }
};

// Max |digital dB - analog dB| over [10 Hz, min(20 kHz, 0.45 fs)], optionally only where analog > floorDb.
double maxDeviationDb (const DesignSpec& spec, double fs, double floorDb = -1000.0)
{
    const CascadeCoeffs dig = designCascade (spec, fs);
    const AnalogCascade ana = analogPrototype (spec, fs);
    const double fmax = std::min (20000.0, 0.45 * fs);
    double m = 0.0;
    for (double f : geomspace (10.0, fmax, 600))
    {
        const double a = db (std::abs (ana.response (f)));
        if (a < floorDb)
            continue;
        const double d = db (std::abs (dig.response (kTwoPi * f / fs)));
        m = std::max (m, std::abs (a - d));
    }
    return m;
}

std::string where (const DesignSpec& s, double fs)
{
    return "fs=" + str (fs) + " f0=" + str (s.freqHz) + " G=" + str (s.gainDb) + " Q=" + str (s.q)
         + " slope=" + str (kSlopesDbPerOct[(size_t) s.slopeIndex]);
}

bool minimumPhase (const CascadeCoeffs& c)
{
    for (int i = 0; i < c.numSections; ++i)
    {
        const auto& s = c.sections[(size_t) i];
        if (s.clp < 0.0 || s.chp < 0.0 || (s.order == 2 && s.cbp < 0.0))
            return false;
    }
    return true;
}

std::vector<double> impulseResponse (const CascadeCoeffs& c, int len)
{
    SvfCascade f;
    SectionLayout l;
    l.numSections = c.numSections;
    for (int i = 0; i < c.numSections; ++i)
        l.orders[(size_t) i] = c.sections[(size_t) i].order;
    f.setLayout (l);
    f.setCoefficients (c);
    std::vector<double> h ((size_t) len, 0.0);
    h[0] = 1.0;
    f.processMono (h.data(), len, 1, 0);
    return h;
}
} // namespace

//==============================================================================
ME_TEST ("Filter", BellAccuracyVsAnalogPrototype)
{
    for (double fs : allSampleRates())
    {
        DevStats lo, mid, hi;
        for (double f0 : geomspace (10.0, std::min (30000.0, 0.49 * fs), 48))
            for (double q : { 0.3, 0.5, 0.7071, 1.0, 2.0, 4.0, 8.0, 16.0, 30.0 })
                for (double g : { -18.0, -12.0, -6.0, -3.0, -1.0, 1.0, 3.0, 6.0, 12.0, 18.0 })
                {
                    DesignSpec s { FilterType::Bell, f0, g, q, 1 };
                    const double d = maxDeviationDb (s, fs);
                    (f0 < fs / 8 ? lo : (f0 < fs / 4 ? mid : hi)).add (d, where (s, fs));
                }
        const std::string tag = "fs " + str (fs) + " ";
        measure (tag + "f0<fs/8: max dev (dB)", lo.maxDev);
        measure (tag + "f0<fs/8: P95 dev (dB)", lo.percentile (95));
        measure (tag + "f0<fs/8: % within 0.2 dB", lo.pct (lo.within02));
        measure (tag + "fs/8<=f0<fs/4: max dev (dB)", mid.maxDev);
        measure (tag + "fs/8<=f0<fs/4: % within 0.5 dB", mid.pct (mid.within05));
        if (hi.n)
            measure (tag + "f0>=fs/4: max dev (dB)", hi.maxDev);

        // Regression guards (|G| <= 18 dB, 0.3 <= Q <= 30). Values from Documentation/DSP_Design.md.
        CHECK_MSG (lo.maxDev < 1.0, tag << "worst " << lo.worst << " dev " << lo.maxDev);
        CHECK_MSG (lo.percentile (95) < 0.2, tag << "P95 " << lo.percentile (95));
        CHECK_MSG (mid.maxDev < 1.0, tag << "worst " << mid.worst << " dev " << mid.maxDev);
    }
}

ME_TEST ("Filter", ShelfAccuracyVsAnalogPrototype)
{
    for (double fs : allSampleRates())
    {
        DevStats lo, mid;
        for (auto type : { FilterType::LowShelf, FilterType::HighShelf })
            for (int slope : { 0, 1, 2, 3, 4, 5 })
                for (double f0 : geomspace (10.0, std::min (30000.0, 0.49 * fs), 40))
                    for (double q : { 0.3, 0.5, 0.7071, 1.0, 1.5 })
                        for (double g : { -24.0, -12.0, -6.0, -2.0, 2.0, 6.0, 12.0, 24.0 })
                        {
                            if (slope == 0 && q != 0.7071)
                                continue; // Q unused for 6 dB/oct
                            DesignSpec s { type, f0, g, q, slope };
                            const double d = maxDeviationDb (s, fs);
                            if (f0 < fs / 8)
                                lo.add (d, where (s, fs));
                            else if (f0 < fs / 4)
                                mid.add (d, where (s, fs));
                        }
        const std::string tag = "fs " + str (fs) + " ";
        measure (tag + "f0<fs/8: max dev (dB)", lo.maxDev);
        measure (tag + "f0<fs/8: P95 dev (dB)", lo.percentile (95));
        measure (tag + "f0<fs/8: % within 0.2 dB", lo.pct (lo.within02));
        measure (tag + "fs/8<=f0<fs/4: max dev (dB)", mid.maxDev);
        measure (tag + "fs/8<=f0<fs/4: P95 dev (dB)", mid.percentile (95));
        CHECK_MSG (lo.percentile (95) < 0.2, tag << "P95 " << lo.percentile (95));
        CHECK_MSG (lo.maxDev < 2.0, tag << "worst " << lo.worst << " dev " << lo.maxDev);
    }
}

ME_TEST ("Filter", CutFilterAccuracyVsAnalogPrototype)
{
    for (double fs : allSampleRates())
    {
        DevStats hp6, lp6, hp24, lp24;
        for (auto type : { FilterType::HighPass, FilterType::LowPass })
            for (int slope = 0; slope < (int) kSlopesDbPerOct.size(); ++slope)
                for (double f0 : geomspace (10.0, std::min (30000.0, 0.45 * fs), 30))
                {
                    if (f0 >= fs / 8)
                        continue;
                    DesignSpec s { type, f0, 0.0, kButterworthQ, slope };
                    const bool hp = type == FilterType::HighPass;
                    (hp ? hp6 : lp6).add (maxDeviationDb (s, fs, -6.0), where (s, fs));   // pass band + cutoff
                    (hp ? hp24 : lp24).add (maxDeviationDb (s, fs, -24.0), where (s, fs)); // + transition
                }
        const std::string tag = "fs " + str (fs) + " ";
        measure (tag + "HP f0<fs/8: max dev where analog > -6 dB (dB)", hp6.maxDev);
        measure (tag + "LP f0<fs/8: max dev where analog > -6 dB (dB)", lp6.maxDev);
        measure (tag + "HP f0<fs/8: max dev where analog > -24 dB (dB)", hp24.maxDev);
        measure (tag + "LP f0<fs/8: max dev where analog > -24 dB (dB)", lp24.maxDev);
        CHECK_MSG (hp6.maxDev < 0.2, tag << "HP worst " << hp6.worst << " " << hp6.maxDev);
        CHECK_MSG (lp6.maxDev < 0.25, tag << "LP worst " << lp6.worst << " " << lp6.maxDev);
        CHECK_MSG (hp24.maxDev < 0.25, tag << "HP worst " << hp24.worst << " " << hp24.maxDev);
        CHECK_MSG (lp24.maxDev < 1.2, tag << "LP worst " << lp24.worst << " " << lp24.maxDev);
    }
}

ME_TEST ("Filter", ExactAtMatchPoints)
{
    // The design is exact (to rounding) at DC, f0 and Nyquist for bells/shelves, and at f0 for cut filters.
    double worst = 0.0;
    for (double fs : allSampleRates())
        for (double f0 : geomspace (10.0, 0.49 * fs, 25))
            for (double q : { 0.3, 1.0, 10.0 })
                for (double g : { -30.0, -6.0, 6.0, 30.0 })
                    for (auto t : { FilterType::Bell, FilterType::LowShelf, FilterType::HighShelf })
                    {
                        DesignSpec s { t, f0, g, std::min (q, t == FilterType::Bell ? 30.0 : kMaxShelfQ), 1 };
                        const auto dig = designCascade (s, fs);
                        const auto ana = analogPrototype (s, fs);
                        const double fd = ana.designFreqHz;
                        const double w0 = kTwoPi * fd / fs;
                        for (auto [w, f] : { std::pair { 1e-9, 1e-9 * fs / kTwoPi }, std::pair { w0, fd }, std::pair { kPi, fs / 2 } })
                        {
                            const double e = std::abs (db (std::abs (dig.response (w))) - db (std::abs (ana.response (f))));
                            worst = std::max (worst, e);
                            CHECK_MSG (e < 1e-6, where (s, fs) << " w=" << w << " err " << e);
                        }
                    }
    measure ("max error at DC / f0 / Nyquist (dB)", worst);

    double worstHp = 0.0;
    for (double fs : allSampleRates())
        for (int slope = 0; slope < 8; ++slope)
            for (double f0 : geomspace (10.0, 0.45 * fs, 20))
                for (auto t : { FilterType::HighPass, FilterType::LowPass })
                {
                    DesignSpec s { t, f0, 0.0, kButterworthQ, slope };
                    const double e = std::abs (designCascade (s, fs).magnitudeDb (kTwoPi * f0 / fs) - db (std::sqrt (0.5)));
                    worstHp = std::max (worstHp, e);
                    CHECK_MSG (e < 1e-6, where (s, fs) << " cutoff err " << e);
                }
    measure ("max Butterworth cutoff error vs -3.0103 dB (dB)", worstHp);
}

ME_TEST ("Filter", RealisedFilterEqualsDesignedTransferFunction)
{
    // Process an impulse through the real-time SVF cascade and compare its spectrum with the design's
    // closed-form transfer function. Validates the processing kernel, not just the math.
    double worstMag = 0.0, worstPh = 0.0;
    struct Case { FilterType t; double f, g, q; int slope; };
    const Case cases[] = {
        { FilterType::Bell, 1000, 9, 1.4, 1 },       { FilterType::Bell, 60, -12, 8, 1 },
        { FilterType::Bell, 12000, 6, 0.7, 1 },      { FilterType::LowShelf, 120, 6, 0.7071, 1 },
        { FilterType::HighShelf, 9000, -4, 1.0, 3 }, { FilterType::LowShelf, 300, -8, 0.7071, 0 },
        { FilterType::HighPass, 80, 0, 0.7071, 3 },  { FilterType::HighPass, 30, 0, 0.7071, 7 },
        { FilterType::LowPass, 8000, 0, 0.7071, 5 }, { FilterType::LowPass, 15000, 0, 2.0, 1 },
        { FilterType::Notch, 3000, 0, 4, 1 },        { FilterType::BandPass, 700, 0, 2, 1 },
        { FilterType::Tilt, 1000, 6, 0.7071, 0 },    { FilterType::AllPass, 2000, 0, 1, 1 },
        { FilterType::HighPass, 40, 0, 4.0, 1 },     { FilterType::LowShelf, 40, 12, 0.7071, 5 },
    };
    for (double fs : { 44100.0, 96000.0, 192000.0 })
        for (const auto& c : cases)
        {
            DesignSpec s { c.t, c.f, c.g, c.q, c.slope };
            const auto d = designCascade (s, fs);
            const auto h = impulseResponse (d, (int) (fs * 3.0));
            for (double f : geomspace (20.0, 0.45 * fs, 40))
            {
                const double w = kTwoPi * f / fs;
                const auto H = d.response (w);
                const auto M = dftAt (h, w);
                if (std::abs (H) < 1e-6)
                    continue; // inside a notch / deep stop band: relative comparison meaningless
                const double em = std::abs (db (std::abs (M)) - db (std::abs (H)));
                const double ep = std::abs (std::arg (M / H));
                worstMag = std::max (worstMag, em);
                worstPh = std::max (worstPh, ep);
                CHECK_MSG (em < 1e-6 && ep < 1e-6, where (s, fs) << " f=" << f << " magErr " << em << " phErr " << ep);
            }
        }
    measure ("max |measured - designed| magnitude (dB)", worstMag);
    measure ("max |measured - designed| phase (rad)", worstPh);
}

ME_TEST ("Filter", StabilityRealizabilityMinimumPhaseGrid)
{
    int designs = 0, bad = 0, nonMinPhase = 0;
    for (double fs : allSampleRates())
        for (int ti = 0; ti < (int) FilterType::NumTypes; ++ti)
            for (int slope = 0; slope < 8; ++slope)
            {
                const auto t = (FilterType) ti;
                if (! typeUsesSlope (t) && slope != 1)
                    continue;
                for (double f0 : geomspace (10.0, 30000.0, 61))
                    for (double q : { 0.1, 0.2, 0.5, 0.7071, 1.0, 3.0, 10.0, 30.0 })
                        for (double g : { -30.0, -15.0, -3.0, -0.1, 0.0, 0.1, 3.0, 15.0, 30.0 })
                        {
                            if (! typeUsesGain (t) && g != 0.0)
                                continue;
                            DesignSpec s { t, f0, g, q, slope };
                            const auto c = designCascade (s, fs);
                            ++designs;
                            bool ok = c.numSections == sectionLayout (t, slope).numSections;
                            for (int i = 0; i < c.numSections; ++i)
                                ok = ok && c.sections[(size_t) i].isFiniteAndStable();
                            if (! ok)
                            {
                                ++bad;
                                CHECK_MSG (false, "invalid design " << filterTypeName (t) << " " << where (s, fs));
                            }
                            if (t != FilterType::AllPass && ! minimumPhase (c))
                            {
                                ++nonMinPhase;
                                CHECK_MSG (false, "not minimum phase " << filterTypeName (t) << " " << where (s, fs));
                            }
                            // Magnitude response must be finite everywhere (sampled).
                            for (double w : { 1e-6, 0.01, 0.1, 0.5, 1.0, 2.0, 3.0, kPi })
                                if (! std::isfinite (std::abs (c.response (w))))
                                {
                                    ++bad;
                                    CHECK_MSG (false, "non-finite response " << where (s, fs));
                                }
                        }
            }
    measure ("designs evaluated", (double) designs);
    measure ("invalid/unstable designs", (double) bad);
    measure ("non-minimum-phase designs", (double) nonMinPhase);
}

ME_TEST ("Filter", ImpulseResponsesDecay)
{
    // Time-domain stability: impulse responses of extreme designs must decay (no growth, no limit cycles).
    int n = 0;
    for (double fs : { 44100.0, 192000.0 })
        for (auto t : { FilterType::Bell, FilterType::HighPass, FilterType::LowPass, FilterType::LowShelf, FilterType::Notch })
            for (double f0 : { 10.0, 100.0, 5000.0, 0.49 * fs })
                for (double q : { 0.1, 30.0 })
                {
                    DesignSpec s { t, f0, t == FilterType::Bell || t == FilterType::LowShelf ? 30.0 : 0.0, q, 7 };
                    const auto h = impulseResponse (designCascade (s, fs), (int) fs * 4);
                    double early = 0, late = 0;
                    for (size_t i = 0; i < h.size(); ++i)
                        (i < h.size() / 2 ? early : late) += h[i] * h[i];
                    CHECK_MSG (allFinite (h), where (s, fs));
                    CHECK_MSG (late <= early * 1e-3 + 1e-20, where (s, fs) << " late/early energy " << late / std::max (early, 1e-300));
                    ++n;
                }
    measure ("extreme impulse responses checked", (double) n);
}

ME_TEST ("Filter", ButterworthSlopes)
{
    // Slope measured between 1.5 and 2.5 octaves into the stop band of 1 kHz filters @ 96 kHz, compared with
    // the exact analog Butterworth magnitude over the same octave (which approaches N*6 dB/oct asymptotically).
    const double fs = 96000.0;
    const double k15 = std::pow (2.0, 1.5), k25 = std::pow (2.0, 2.5);
    for (int slope = 0; slope < 8; ++slope)
    {
        const int order = filterOrderForSlopeIndex (slope);
        const double nominal = kSlopesDbPerOct[(size_t) slope];
        auto bw = [order] (double x) { return -10.0 * std::log10 (1.0 + std::pow (x, 2.0 * order)); };
        const double expected = bw (k25) - bw (k15); // analog attenuation increase per octave (negative)
        DesignSpec hp { FilterType::HighPass, 1000.0, 0, kButterworthQ, slope };
        DesignSpec lp { FilterType::LowPass, 1000.0, 0, kButterworthQ, slope };
        const auto ch = designCascade (hp, fs), cl = designCascade (lp, fs);
        const double hpSlope = ch.magnitudeDb (kTwoPi * 1000.0 / k25 / fs) - ch.magnitudeDb (kTwoPi * 1000.0 / k15 / fs);
        const double lpSlope = cl.magnitudeDb (kTwoPi * 1000.0 * k25 / fs) - cl.magnitudeDb (kTwoPi * 1000.0 * k15 / fs);
        measure ("HP " + str (nominal) + " dB/oct: measured slope 1.5-2.5 oct (dB/oct)", -hpSlope);
        measure ("LP " + str (nominal) + " dB/oct: measured slope 1.5-2.5 oct (dB/oct)", -lpSlope);
        CHECK_NEAR (hpSlope, expected, 0.01 * nominal);
        CHECK_NEAR (lpSlope, expected, 0.02 * nominal);
        CHECK_NEAR (-expected, nominal, 0.06 * nominal); // the analog reference itself is ~N*6 dB/oct here
    }
}

ME_TEST ("Filter", BellBandwidthMatchesQ)
{
    // Half-gain (G/2 dB) bandwidth in octaves vs the analog relation 1/Q = 2 sinh(ln2/2 * BW).
    double worst = 0.0;
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (double f0 : { 100.0, 1000.0, 4000.0 })
            for (double q : { 0.5, 1.0, 2.0, 5.0, 10.0 })
                for (double g : { -12.0, 12.0 })
                {
                    DesignSpec s { FilterType::Bell, f0, g, q, 1 };
                    const auto c = designCascade (s, fs);
                    auto edge = [&] (double fa, double fb)
                    {
                        // bisection for |H| = G/2 dB on a monotonic flank
                        for (int it = 0; it < 80; ++it)
                        {
                            const double fm = std::sqrt (fa * fb);
                            const double m = c.magnitudeDb (kTwoPi * fm / fs);
                            const bool beyond = g > 0 ? (m > g / 2) : (m < g / 2);
                            (beyond ? fb : fa) = fm;
                        }
                        return std::sqrt (fa * fb);
                    };
                    const double lo = edge (1.0, f0);
                    const double hi = edge (std::min (0.499 * fs, f0 * 64), f0);
                    const double bw = std::log2 (hi / lo);
                    const double expected = 2.0 / std::log (2.0) * std::asinh (1.0 / (2.0 * q));
                    const double e = std::abs (bw - expected);
                    worst = std::max (worst, e);
                    CHECK_MSG (e < 0.02 * expected + 0.005, "fs " << fs << " f0 " << f0 << " Q " << q << " G " << g << " bw " << bw << " expected " << expected);
                }
    measure ("max bandwidth error (octaves), f0 <= 4 kHz", worst);
}

ME_TEST ("Filter", BoostCutMirrorSymmetry)
{
    double worst = 0.0;
    for (double fs : allSampleRates())
        for (auto t : { FilterType::Bell, FilterType::LowShelf, FilterType::HighShelf, FilterType::Tilt })
            for (double f0 : geomspace (20.0, 0.49 * fs, 15))
                for (double g : { 0.5, 6.0, 30.0 })
                    for (int slope : { 0, 1, 3, 5 })
                    {
                        DesignSpec a { t, f0, g, 1.0, slope }, b { t, f0, -g, 1.0, slope };
                        const auto ca = designCascade (a, fs), cb = designCascade (b, fs);
                        for (double w : { 0.001, 0.05, 0.3, 1.0, 2.0, 3.1 })
                            worst = std::max (worst, std::abs (ca.magnitudeDb (w) + cb.magnitudeDb (w)));
                    }
    measure ("max |dB(+G) + dB(-G)|", worst);
    CHECK (worst < 1e-8);
}

ME_TEST ("Filter", NotchAllPassAndTilt)
{
    const double fs = 48000.0;
    for (double f0 : { 50.0, 1000.0, 15000.0 })
    {
        DesignSpec n { FilterType::Notch, f0, 0, 5.0, 1 };
        const auto c = designCascade (n, fs);
        CHECK_MSG (std::abs (c.response (kTwoPi * f0 / fs)) < 1e-9, "notch depth at f0 " << f0);
        CHECK_NEAR (c.magnitudeDb (1e-12), 0.0, 1e-9);
        DesignSpec ap { FilterType::AllPass, f0, 0, 0.7, 1 };
        const auto a = designCascade (ap, fs);
        double dev = 0;
        for (double w : geomspace (1e-4, kPi, 200))
            dev = std::max (dev, std::abs (a.magnitudeDb (w)));
        CHECK_MSG (dev < 1e-9, "allpass flatness " << dev);
        CHECK_NEAR (std::abs (std::arg (a.response (kTwoPi * f0 / fs))), kPi, 1e-9); // -180 deg exactly at f0
    }
    // Tilt: 0 dB at the pivot, -G/2 / +G/2 at the extremes (first-order: slowly approached).
    for (int slope : { 0, 1, 3 })
    {
        DesignSpec t { FilterType::Tilt, 1000.0, 8.0, kButterworthQ, slope };
        const auto c = designCascade (t, 192000.0);
        CHECK_NEAR (c.magnitudeDb (kTwoPi * 1000.0 / 192000.0), 0.0, 1e-6);
        CHECK_NEAR (c.magnitudeDb (1e-7), -4.0, 1e-3);
        measure ("tilt +8 dB slope " + str (kSlopesDbPerOct[(size_t) slope]) + ": gain at 20 Hz", c.magnitudeDb (kTwoPi * 20.0 / 192000.0), "dB");
        measure ("tilt +8 dB slope " + str (kSlopesDbPerOct[(size_t) slope]) + ": gain at 20 kHz", c.magnitudeDb (kTwoPi * 20000.0 / 192000.0), "dB");
    }
}

ME_TEST ("Filter", HigherOrderShelvesMonotonic)
{
    const double fs = 96000.0;
    for (int slope = 0; slope <= kMaxShelfSlopeIndex; ++slope)
        for (double g : { -12.0, 12.0 })
            for (auto t : { FilterType::LowShelf, FilterType::HighShelf })
            {
                DesignSpec s { t, 1000.0, g, kButterworthQ, slope };
                const auto c = designCascade (s, fs);
                double prev = c.magnitudeDb (kTwoPi * 5.0 / fs);
                bool mono = true;
                for (double f : geomspace (5.0, 40000.0, 800))
                {
                    const double m = c.magnitudeDb (kTwoPi * f / fs);
                    const bool rising = (t == FilterType::HighShelf) == (g > 0);
                    if (rising ? (m < prev - 1e-6) : (m > prev + 1e-6)) // rounding-level ripple allowed
                        mono = false;
                    prev = m;
                }
                CHECK_MSG (mono, filterTypeName (t) << " slope " << kSlopesDbPerOct[(size_t) slope] << " G " << g);
                CHECK_NEAR (c.magnitudeDb (kTwoPi * 1000.0 / fs), g / 2, 1e-6);
            }
}

ME_TEST ("Filter", LowFrequencyNumericalPrecision)
{
    // 10 Hz at 192 kHz is the most ill-conditioned case; the expm1-based design must remain exact.
    double worst = 0.0;
    for (auto t : { FilterType::Bell, FilterType::LowShelf, FilterType::HighShelf, FilterType::HighPass, FilterType::LowPass })
        for (double q : { 0.5, 0.7071, 5.0, 30.0 })
        {
            DesignSpec s { t, 10.0, t == FilterType::HighPass || t == FilterType::LowPass ? 0.0 : 12.0,
                           t == FilterType::HighPass || t == FilterType::LowPass ? std::min (q, 10.0) : q, 3 };
            const auto dig = designCascade (s, 192000.0);
            const auto ana = analogPrototype (s, 192000.0);
            for (double f : geomspace (1.0, 2000.0, 300))
            {
                const double a = db (std::abs (ana.response (f)));
                if (a < -60)
                    continue;
                worst = std::max (worst, std::abs (a - db (std::abs (dig.response (kTwoPi * f / 192000.0)))));
            }
        }
    measure ("10 Hz @ 192 kHz: max deviation from analog (dB)", worst);
    CHECK (worst < 0.01);
}

ME_TEST ("Filter", IdentityBandIsBitExact)
{
    for (auto t : { FilterType::Bell, FilterType::LowShelf, FilterType::HighShelf, FilterType::Tilt })
        for (int slope : { 0, 1, 2, 5 })
        {
            DesignSpec s { t, 1234.0, 0.0, 2.0, slope };
            const auto c = designCascade (s, 48000.0);
            CHECK (c.identity);
            auto x = whiteNoise (4096, 0.5, 7);
            auto y = x;
            SvfCascade f;
            f.setLayout (sectionLayout (t, slope));
            f.setCoefficients (c);
            f.processMono (y.data(), (int) y.size(), 1, 0);
            CHECK_MSG (x == y, filterTypeName (t) << " slope " << slope << " not bit-exact");
        }
}
