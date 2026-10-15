// ME Pro EQ — ME Audio
// Linear-phase mode: FIR symmetry and latency, magnitude accuracy against the minimum-phase design, stereo
// routing, switching, bypass alignment, parameter changes, worker-thread hand-over, offline determinism.
#include "EngineTestUtils.h"
#include "TestFramework.h"

#include "DSP/FFT.h"
#include "DSP/LinearPhase.h"

#include <chrono>
#include <thread>

using namespace mepro::dsp;
using namespace metest;

namespace
{
    LinearPhaseBands bandsOf (std::initializer_list<BandSettings> list)
    {
        LinearPhaseBands b {};
        int i = 0;
        for (const auto& s : list)
            b[(size_t) i++] = s;
        return b;
    }

    // Target: the magnitude of the same digital designs the minimum-phase engine runs (one channel domain).
    double targetDb (const LinearPhaseBands& bands, double fs, double f, StereoMode domain = StereoMode::Stereo)
    {
        double t = 0.0;
        for (const auto& b : bands)
            if (b.active && (b.stereo == StereoMode::Stereo || b.stereo == domain))
                t += designCascade ({ b.type, b.freqHz, b.gainDb, b.q, b.slopeIndex }, fs).magnitudeDb (kTwoPi * f / fs);
        return t;
    }

    // Impulse response of a LinearPhaseEngine for input on channel `in`, output channel `out`.
    std::vector<double> impulseResponse (LinearPhaseEngine& lp, int nch, int in, int out, int length)
    {
        lp.reset();
        std::vector<double> x ((size_t) (length * nch), 0.0), dry = x;
        x[(size_t) in] = 1.0;
        for (int pos = 0; pos < length; pos += 333)
        {
            const int n = std::min (333, length - pos);
            lp.process (x.data() + pos * nch, dry.data() + pos * nch, nch, n);
        }
        std::vector<double> y ((size_t) length);
        for (int i = 0; i < length; ++i)
            y[(size_t) i] = x[(size_t) (i * nch + out)];
        return y;
    }

    // DTFT of `ir` at w with the pure delay `delay` removed (rotating phasor, renormalised every 1024 steps).
    std::complex<double> dtft (const std::vector<double>& ir, double w, int delay)
    {
        const std::complex<double> step = std::polar (1.0, -w);
        std::complex<double> z = 1.0, s = 0.0;
        for (size_t n = 0; n < ir.size(); ++n)
        {
            s += ir[n] * z;
            z *= step;
            if ((n & 1023) == 1023)
                z = std::polar (1.0, -w * (double) (n + 1));
        }
        return s * std::polar (1.0, w * delay);
    }

    struct ErrorBands { double lf = 0, mid = 0, hf = 0; };

    // Max |measured - target| (dB) in 20-50 Hz, 50-200 Hz and 200 Hz-20 kHz, ignoring points where the target
    // is below -60 dB (deep stop bands).
    ErrorBands magnitudeError (const std::vector<double>& ir, int delay, const LinearPhaseBands& bands, double fs,
                               StereoMode domain = StereoMode::Stereo)
    {
        ErrorBands e;
        for (int i = 0; i < 240; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, i / 239.0);
            const double t = targetDb (bands, fs, f, domain);
            if (t < -60.0)
                continue;
            const double err = std::abs (db (std::abs (dtft (ir, kTwoPi * f / fs, delay))) - t);
            (f < 50.0 ? e.lf : (f < 200.0 ? e.mid : e.hf)) = std::max (f < 50.0 ? e.lf : (f < 200.0 ? e.mid : e.hf), err);
        }
        return e;
    }

    const LinearPhaseBands& vocalCurve()
    {
        static const LinearPhaseBands b = bandsOf ({ band (FilterType::HighPass, 80, 0, kButterworthQ, 3),
                                                     bell (250, -3, 1.4), bell (3000, 2.5, 0.9),
                                                     band (FilterType::HighShelf, 11000, 3, kButterworthQ, 1) });
        return b;
    }
} // namespace

ME_TEST ("LinearPhase", GeometryAndLatency)
{
    for (double fs : allSampleRates())
        for (int q = 0; q < kNumLinearPhaseQualities; ++q)
        {
            const auto g = linearPhaseGeometry (fs, q);
            CHECK (g.firLength == g.partition * g.numPartitions);
            CHECK (g.latency == g.firLength / 2 + g.partition);
            if (fs == 48000.0 || fs == 96000.0)
                measure ("fs " + str (fs) + " " + kLinearPhaseQualityNames[(size_t) q] + ": FIR taps / latency (samples, ms)",
                         str (g.firLength) + " / " + str (g.latency) + " (" + str (1000.0 * g.latency / fs) + " ms)");
        }
    // Frequency resolution is kept constant across sample rates (taps scale with the rate).
    CHECK (linearPhaseGeometry (96000.0, 1).firLength == 2 * linearPhaseGeometry (48000.0, 1).firLength);
    CHECK (linearPhaseGeometry (192000.0, 1).firLength == 4 * linearPhaseGeometry (48000.0, 1).firLength);
}

ME_TEST ("LinearPhase", ImpulseResponseIsSymmetricAndDelayed)
{
    const double fs = 48000.0;
    LinearPhaseEngine lp (fs, 1, false);
    lp.setBands (vocalCurve(), 2, true);
    const auto& g = lp.getGeometry();
    std::vector<double> fir;
    CHECK (lp.copyLastDesignedFir (0, fir));
    double asym = 0.0, peak = 0.0;
    for (int m = 1; m < g.firLength / 2; ++m)
        asym = std::max (asym, std::abs (fir[(size_t) (g.firLength / 2 + m)] - fir[(size_t) (g.firLength / 2 - m)]));
    for (double v : fir)
        peak = std::max (peak, std::abs (v));
    measure ("max |h[N/2+m] - h[N/2-m]| relative to peak", asym / peak);
    CHECK (asym <= 1e-15 * peak);
    CHECK (fir[0] == 0.0); // periodic Hann: the unpaired first tap is zero, so the FIR is exactly symmetric

    // The engine output is the FIR delayed by the partition size: total latency N/2 + P.
    const auto ir = impulseResponse (lp, 2, 0, 0, g.latency + g.firLength);
    double maxDiff = 0.0;
    for (int n = 0; n < g.firLength; ++n)
        maxDiff = std::max (maxDiff, std::abs (ir[(size_t) (n + g.partition)] - fir[(size_t) n]));
    for (int n = 0; n < g.partition; ++n)
        maxDiff = std::max (maxDiff, std::abs (ir[(size_t) n]));
    measure ("engine impulse response vs designed FIR (delayed by P): max diff", maxDiff);
    CHECK (maxDiff < 1e-6 * peak); // FIR spectra are stored as 32-bit floats

    size_t argmax = 0;
    for (size_t n = 0; n < ir.size(); ++n)
        if (std::abs (ir[n]) > std::abs (ir[argmax]))
            argmax = n;
    measure ("impulse response peak position (samples) / reported latency", str ((int) argmax) + " / " + str (lp.getLatency()));
    CHECK ((int) argmax == lp.getLatency());
}

ME_TEST ("LinearPhase", MagnitudeMatchesMinimumPhaseDesign)
{
    struct Curve { const char* name; LinearPhaseBands bands; };
    const std::vector<Curve> curves {
        { "vocal (HP 80/18, bells, HS)", vocalCurve() },
        { "bass (HP 30 Hz 48 dB/oct, bell 60 Hz +4 Q2, bell 400 Hz -3)",
          bandsOf ({ band (FilterType::HighPass, 30, 0, kButterworthQ, 5), bell (60, 4, 2), bell (400, -3, 1) }) },
        { "narrow bell 1 kHz -9 dB Q8", bandsOf ({ bell (1000, -9, 8) }) },
        { "broad tone: LS 200 -4, HS 6k +5, tilt", bandsOf ({ band (FilterType::LowShelf, 200, -4, kButterworthQ, 1),
                                                              band (FilterType::HighShelf, 6000, 5, kButterworthQ, 1) }) },
    };
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int q = 0; q < kNumLinearPhaseQualities; ++q)
        {
            if (fs == 96000.0 && q != 1)
                continue; // resolution in Hz is the same as at 48 kHz; one quality is enough here
            LinearPhaseEngine lp (fs, q, false);
            for (const auto& c : curves)
            {
                lp.setBands (c.bands, 2, true);
                const auto ir = impulseResponse (lp, 2, 0, 0, lp.getLatency() + lp.getGeometry().firLength);
                const auto e = magnitudeError (ir, lp.getLatency(), c.bands, fs);
                measure ("fs " + str (fs) + " " + kLinearPhaseQualityNames[(size_t) q] + ", " + c.name
                             + ": max error 20-50 Hz / 50-200 Hz / 0.2-20 kHz (dB)",
                         str (e.lf) + " / " + str (e.mid) + " / " + str (e.hf));
                // Above 200 Hz every quality is accurate; low-frequency resolution grows with the FIR length.
                CHECK_MSG (e.hf < 0.05, c.name << " fs " << fs << " q " << q << " hf " << e.hf);
                if (q == 2)
                    CHECK_MSG (e.mid < 0.1, c.name << " fs " << fs << " mid " << e.mid);
            }
        }
}

ME_TEST ("LinearPhase", PhaseIsExactlyLinear)
{
    const double fs = 48000.0;
    LinearPhaseEngine lp (fs, 1, false);
    lp.setBands (vocalCurve(), 2, true);
    const int L = lp.getLatency();
    const auto ir = impulseResponse (lp, 2, 0, 0, L + lp.getGeometry().firLength);
    double worst = 0.0;
    for (int i = 0; i < 200; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, i / 199.0);
        if (targetDb (vocalCurve(), fs, f) < -40.0)
            continue;
        worst = std::max (worst, std::abs (std::arg (dtft (ir, kTwoPi * f / fs, L)))); // pure delay removed
    }
    measure ("phase deviation from a pure delay of the reported latency, where |H| > -40 dB (rad)", worst);
    CHECK (worst < 1e-6);
}

ME_TEST ("LinearPhase", StereoRoutingMatchesMinimumPhaseDomains)
{
    // Per channel domain the linear-phase magnitude equals the minimum-phase design's magnitude: Left/Right
    // bands act on L/R only, Mid/Side bands on M/S only.
    const double fs = 48000.0;
    LinearPhaseEngine lp (fs, 1, false);

    const auto lrBands = bandsOf ({ bell (1000, 6, 1, StereoMode::Left), bell (4000, -5, 2, StereoMode::Right), bell (300, 2, 1) });
    lp.setBands (lrBands, 2, true);
    CHECK (lp.getLastDesignForm() == 1);
    const int L = lp.getLatency(), len = L + lp.getGeometry().firLength;
    {
        const auto ll = impulseResponse (lp, 2, 0, 0, len), lr = impulseResponse (lp, 2, 0, 1, len);
        const auto rr = impulseResponse (lp, 2, 1, 1, len);
        const auto eL = magnitudeError (ll, L, lrBands, fs, StereoMode::Left);
        const auto eR = magnitudeError (rr, L, lrBands, fs, StereoMode::Right);
        double leak = 0.0;
        for (double v : lr)
            leak = std::max (leak, std::abs (v));
        measure ("L/R bands: max error L / R above 200 Hz (dB)", str (eL.hf) + " / " + str (eR.hf));
        measure ("L/R bands: max L->R leakage sample", leak);
        CHECK (eL.hf < 0.05 && eR.hf < 0.05);
        CHECK (leak == 0.0);
    }

    const auto msBands = bandsOf ({ bell (2000, -6, 1.5, StereoMode::Mid), band (FilterType::LowShelf, 150, -8, kButterworthQ, 1, StereoMode::Side),
                                    bell (500, 3, 1) });
    lp.setBands (msBands, 2, true);
    CHECK (lp.getLastDesignForm() == 2);
    {
        // Mid input (L = R = impulse) and Side input (L = -R).
        auto domainIr = [&] (double sign)
        {
            lp.reset();
            std::vector<double> x ((size_t) (2 * len), 0.0), dry = x;
            x[0] = 1.0;
            x[1] = sign;
            lp.process (x.data(), dry.data(), 2, len);
            std::vector<double> y ((size_t) len);
            for (int i = 0; i < len; ++i)
                y[(size_t) i] = 0.5 * (x[(size_t) (2 * i)] + sign * x[(size_t) (2 * i + 1)]);
            return y;
        };
        const auto eM = magnitudeError (domainIr (1.0), L, msBands, fs, StereoMode::Mid);
        const auto eS = magnitudeError (domainIr (-1.0), L, msBands, fs, StereoMode::Side);
        measure ("M/S bands: max error M / S above 200 Hz (dB)", str (eM.hf) + " / " + str (eS.hf));
        CHECK (eM.hf < 0.05 && eS.hf < 0.05);

        // A pure Mid signal must not produce any Side output (and vice versa).
        lp.reset();
        auto x = whiteNoise (len, 0.5, 3);
        std::vector<double> st ((size_t) (2 * len)), dry = st;
        for (int i = 0; i < len; ++i)
            st[(size_t) (2 * i)] = st[(size_t) (2 * i + 1)] = x[(size_t) i];
        lp.process (st.data(), dry.data(), 2, len);
        double side = 0.0;
        for (int i = 0; i < len; ++i)
            side = std::max (side, std::abs (st[(size_t) (2 * i)] - st[(size_t) (2 * i + 1)]));
        measure ("M/S bands, mono-compatible input: max |L - R| at the output", side);
        CHECK (side < 1e-12);
    }

    // Mono bus: Stereo and Mid bands apply, Left/Right/Side bands are ignored (as in minimum-phase mode).
    const auto mixed = bandsOf ({ bell (1000, 6, 1, StereoMode::Mid), bell (3000, 9, 1, StereoMode::Left), bell (5000, 9, 1, StereoMode::Side),
                                  bell (200, -4, 1) });
    lp.setBands (mixed, 1, true);
    CHECK (lp.getLastDesignForm() == 0);
    {
        const auto ir = impulseResponse (lp, 1, 0, 0, len);
        auto monoBands = mixed;
        monoBands[0].stereo = StereoMode::Stereo;
        monoBands[1].active = monoBands[2].active = false;
        const auto e = magnitudeError (ir, L, monoBands, fs);
        measure ("mono bus: max error above 200 Hz vs Stereo+Mid bands only (dB)", e.hf);
        CHECK (e.hf < 0.05);
    }
}

ME_TEST ("LinearPhase", RealisedResponseIsPublished)
{
    const double fs = 48000.0;
    LinearPhaseEngine lp (fs, 0, false);
    lp.setBands (vocalCurve(), 2, true);
    LinearPhaseEngine::RealisedResponse r;
    CHECK (lp.getRealisedResponse (r));
    CHECK (r.nameA == 'C' && r.domainB.empty());
    const auto ir = impulseResponse (lp, 2, 0, 0, lp.getLatency() + lp.getGeometry().firLength);
    double worst = 0.0;
    for (double f : { 30.0, 100.0, 1000.0, 10000.0 })
    {
        const size_t k = (size_t) std::lround (f / r.binHz);
        const double w = kTwoPi * (double) k * r.binHz / fs;
        worst = std::max (worst, std::abs (db (std::abs (dtft (ir, w, 0))) - (double) r.domainA[k]));
    }
    measure ("published realised response vs measured engine response (dB)", worst);
    CHECK (worst < 1e-3);
}

namespace
{
    std::unique_ptr<LinearPhaseEngine> makeLp (double fs, int q, bool worker = false)
    {
        return std::make_unique<LinearPhaseEngine> (fs, q, worker);
    }
} // namespace

ME_TEST ("LinearPhase", EngineReportsLatencyAndSwitchesWithoutClicks)
{
    const double fs = 48000.0;
    EQEngine e;
    e.prepare (fs, 512, 2);
    EngineSettings st;
    st.bands[0] = bell (3000, 6, 1);
    e.setSettings (st);
    e.installLinearPhaseNow (makeLp (fs, 0));
    CHECK (e.getLatencySamples() == 0); // minimum phase requested

    // 200 Hz tone; the phase mode toggles every 0.25 s.
    Stereo s (sine (200, fs, (int) fs * 2, 0.5), sine (200, fs, (int) fs * 2, 0.5));
    std::vector<int> latencies;
    for (int pos = 0; pos < s.size(); pos += 480)
    {
        if (pos % 12000 == 0)
        {
            st.phaseMode = (pos / 12000) % 2 == 1 ? PhaseMode::Linear : PhaseMode::Minimum;
            e.setSettings (st);
        }
        double* ch[2] = { s.l.data() + pos, s.r.data() + pos };
        e.process (ch, 2, std::min (480, s.size() - pos));
        latencies.push_back (e.getLatencySamples());
    }
    bool sawLinear = false, sawMinimum = false;
    for (int l : latencies)
    {
        sawLinear = sawLinear || l == linearPhaseGeometry (fs, 0).latency;
        sawMinimum = sawMinimum || l == 0;
    }
    CHECK (sawLinear && sawMinimum);
    const double hf = hfResidualDb (s.l, fs, 4000, 4800);
    measure ("phase mode toggled every 250 ms on a 200 Hz tone: HF residual >4 kHz (dB)", hf);
    CHECK (hf < -110.0);
}

ME_TEST ("LinearPhase", BypassIsTimeAligned)
{
    const double fs = 48000.0;
    EQEngine e;
    e.prepare (fs, 512, 2);
    EngineSettings st;
    st.bands[0] = bell (3000, 6, 1);
    st.phaseMode = PhaseMode::Linear;
    e.setSettings (st);
    e.installLinearPhaseNow (makeLp (fs, 0));
    CHECK (e.isUsingLinearPhase());
    const int L = e.getLatencySamples();

    // Fully bypassed in linear-phase mode: output = input delayed by the reported latency, bit for bit.
    st.bypass = true;
    e.setSettings (st);
    e.reset();
    auto x = whiteNoise (L * 3, 0.5, 9);
    Stereo s (x, x);
    runEngine (e, s, 256);
    bool exact = true;
    for (int i = L; i < (int) x.size(); ++i)
        exact = exact && s.l[(size_t) i] == x[(size_t) (i - L)];
    CHECK (exact);

    // Bypass toggling on a tone: both paths are aligned, so the crossfade adds no clicks.
    st.bypass = false;
    e.setSettings (st);
    e.reset();
    Stereo t (sine (200, fs, (int) fs * 2, 0.5), sine (200, fs, (int) fs * 2, 0.5));
    for (int pos = 0; pos < t.size(); pos += 480)
    {
        if (pos % 4800 == 0)
        {
            st.bypass = ! st.bypass;
            e.setSettings (st);
        }
        double* ch[2] = { t.l.data() + pos, t.r.data() + pos };
        e.process (ch, 2, std::min (480, t.size() - pos));
    }
    const double hf = hfResidualDb (t.l, fs, 4000, L + 4800);
    measure ("linear phase, bypass toggled every 100 ms on a 200 Hz tone: HF residual >4 kHz (dB)", hf);
    CHECK (hf < -100.0);
}

ME_TEST ("LinearPhase", ParameterChangesAreSmooth)
{
    // Offline (synchronous) designs: a new FIR per change, crossfaded over two partitions.
    const double fs = 48000.0;
    EQEngine e;
    e.prepare (fs, 512, 2);
    e.setNonRealtime (true);
    EngineSettings st;
    st.bands[0] = bell (3000, 0, 1);
    st.phaseMode = PhaseMode::Linear;
    e.setSettings (st);
    e.installLinearPhaseNow (makeLp (fs, 0));
    Stereo s (sine (200, fs, (int) fs * 2, 0.5), sine (200, fs, (int) fs * 2, 0.5));
    for (int pos = 0; pos < s.size(); pos += 480)
    {
        if (pos % 4800 == 0)
        {
            st.bands[0].gainDb = st.bands[0].gainDb == 0.0 ? 12.0 : 0.0; // jumps on a band 4 octaves above the tone
            st.bands[0].freqHz = 800.0;
            e.setSettings (st);
        }
        double* ch[2] = { s.l.data() + pos, s.r.data() + pos };
        e.process (ch, 2, std::min (480, s.size() - pos));
    }
    const double hf = hfResidualDb (s.l, fs, 4000, 20000);
    measure ("linear phase, gain jumps 0<->12 dB every 100 ms (bell 800 Hz, 200 Hz tone): HF residual >4 kHz (dB)", hf);
    CHECK (hf < -80.0);
}

ME_TEST ("LinearPhase", WorkerThreadHandsOverDesigns)
{
    const double fs = 48000.0;
    EQEngine e;
    e.prepare (fs, 256, 2);
    EngineSettings st;
    st.bands[0] = bell (750, 0.5, 1);
    st.phaseMode = PhaseMode::Linear;
    e.setSettings (st);
    e.installLinearPhaseNow (makeLp (fs, 0, true));
    const auto* lp = e.getLinearPhaseForUi();
    CHECK (lp != nullptr);

    // Stress: change the gain on every block while the worker designs in the background (real time).
    const unsigned before = lp->getDesignCount();
    Stereo s (whiteNoise (48000, 0.3, 4), whiteNoise (48000, 0.3, 5));
    for (int pos = 0; pos < s.size(); pos += 256)
    {
        st.bands[0].gainDb = 6.0 * std::sin (pos * 1e-3);
        e.setSettings (st);
        double* ch[2] = { s.l.data() + pos, s.r.data() + pos };
        e.process (ch, 2, std::min (256, s.size() - pos));
        if (pos % 4096 == 0)
            std::this_thread::sleep_for (std::chrono::milliseconds (2));
    }
    CHECK (allFinite (s.l) && allFinite (s.r));

    // A final change is designed by the worker and reaches the output.
    st.bands[0].gainDb = 12.0;
    e.setSettings (st);
    const int L = e.getLatencySamples();
    bool arrived = false;
    std::vector<double> tone = sine (750, fs, 2048, 0.25); // exactly 32 cycles: seamless when repeated
    double level = 0.0;
    for (int attempt = 0; attempt < 400 && ! arrived; ++attempt)
    {
        Stereo t (tone, tone);
        runEngine (e, t, 256);
        level = 0.0;
        for (int i = 1024; i < 2048; ++i)
            level = std::max (level, std::abs (t.l[(size_t) i]));
        arrived = std::abs (db (level / 0.25) - 12.0) < 0.05;
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
    measure ("worker designs during 1 s of per-block changes", (double) (lp->getDesignCount() - before));
    measure ("final 750 Hz gain after hand-over (dB)", db (level / 0.25));
    CHECK (lp->getDesignCount() > before);
    CHECK_MSG (arrived, "level " << db (level / 0.25));
    CHECK (L == linearPhaseGeometry (fs, 0).latency);
}

ME_TEST ("LinearPhase", OfflineRenderIsDeterministic)
{
    const double fs = 44100.0;
    auto render = [fs]
    {
        EQEngine e;
        e.prepare (fs, 512, 2);
        e.setNonRealtime (true);
        EngineSettings st;
        st.bands[0] = bell (2000, 3, 1);
        st.bands[1] = band (FilterType::HighPass, 60, 0, kButterworthQ, 3);
        st.phaseMode = PhaseMode::Linear;
        e.setSettings (st);
        e.installLinearPhaseNow (makeLp (fs, 0, true)); // worker present, but offline designs are synchronous
        Stereo s (whiteNoise (44100, 0.3, 8), whiteNoise (44100, 0.3, 9));
        for (int pos = 0; pos < s.size(); pos += 512)
        {
            st.bands[0].gainDb = 3.0 + 6.0 * std::sin (pos * 2e-4);
            e.setSettings (st);
            double* ch[2] = { s.l.data() + pos, s.r.data() + pos };
            e.process (ch, 2, std::min (512, s.size() - pos));
        }
        return s;
    };
    const auto a = render(), b = render();
    CHECK (a.l == b.l && a.r == b.r);
}
