// ME Pro EQ — ME Audio
#include "ToneAnalysis.h"
#include "DynamicEQ.h"
#include "MathUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>

namespace mepro::dsp
{

namespace
{
    constexpr double kBandsPerOct = 24.0;
    constexpr double kLowestBandHz = 16.0;

    double percentile (std::vector<double> v, double p)
    {
        if (v.empty())
            return -200.0;
        std::sort (v.begin(), v.end());
        const double pos = clampValue (p, 0.0, 1.0) * (double) (v.size() - 1);
        const auto i = (size_t) pos;
        const double t = pos - (double) i;
        return i + 1 < v.size() ? v[i] + t * (v[i + 1] - v[i]) : v[i];
    }

    double median (std::vector<double> v) { return percentile (std::move (v), 0.5); }

    std::string fmtFreq (double f)
    {
        char b[32];
        if (f < 1000.0)
            std::snprintf (b, sizeof b, "%.0f Hz", f);
        else
            std::snprintf (b, sizeof b, "%.2f kHz", f / 1000.0);
        return b;
    }

    std::string fmt1 (double v)
    {
        char b[32];
        std::snprintf (b, sizeof b, "%.1f", v);
        return b;
    }

    std::string fmt0 (double v)
    {
        char b[32];
        std::snprintf (b, sizeof b, "%.0f", v);
        return b;
    }

    double roundTo (double v, double step) { return std::round (v / step) * step; }

    // Bell Q for a bandwidth (octaves) between the half-gain points: 1/Q = 2 sinh(ln2/2 * BW).
    double qFromBandwidth (double bwOct) { return 1.0 / (2.0 * std::sinh (0.5 * std::log (2.0) * bwOct)); }

    double powToDb (double p) { return 10.0 * std::log10 (std::max (p, 1e-30)); }
    double dbToPow (double d) { return std::pow (10.0, d / 10.0); }

    int rateMultiplier (double fs)
    {
        const int mult = std::max (1, (int) std::lround (fs / 48000.0));
        int p = 1;
        while (p < mult)
            p <<= 1;
        return p;
    }

    void hann (std::vector<double>& w, int n, double& norm)
    {
        w.resize ((size_t) n);
        double sum2 = 0.0;
        for (int i = 0; i < n; ++i)
        {
            w[(size_t) i] = 0.5 - 0.5 * std::cos (kTwoPi * i / n);
            sum2 += w[(size_t) i] * w[(size_t) i];
        }
        norm = 1.0 / ((double) n * sum2); // one-sided bin power (mean square) = 2 |X|^2 * norm
    }
} // namespace

//==============================================================================
void ToneAnalysis::prepare (double sampleRate)
{
    fs = sampleRate > 0.0 ? sampleRate : 48000.0;
    const int mult = rateMultiplier (fs);
    longSize = 16384 * mult;
    shortSize = 2048 * mult;
    longFft = std::make_unique<RealFFT> (longSize);
    shortFft = std::make_unique<RealFFT> (shortSize);
    hann (longWindow, longSize, longNorm);
    hann (shortWindow, shortSize, shortNorm);
    longFrameBuf.assign ((size_t) longSize, 0.0);
    shortFrameBuf.assign ((size_t) shortSize, 0.0);
    longSpec.assign ((size_t) (longSize / 2 + 1), {});
    shortSpec.assign ((size_t) (shortSize / 2 + 1), {});
    longHist.assign ((size_t) longSize, 0.0f);
    shortHist.assign ((size_t) shortSize, 0.0f);

    const double top = std::min (20000.0, 0.45 * fs);
    numBands = (int) std::floor (kBandsPerOct * std::log2 (top / kLowestBandHz)) + 1;
    bandPowerSum.assign ((size_t) numBands, 0.0);
    scratch.assign ((size_t) numBands, 0.0);
    maxLongFrames = (int) std::ceil (kMaxSeconds * fs / (longSize / 4));
    bandSeries.clear();
    bandSeries.reserve ((size_t) maxLongFrames * (size_t) numBands);

    shortBandFreq.clear();
    for (int i = 0; i < kShortBands; ++i)
        shortBandFreq.push_back (1000.0 * std::pow (2.0, i / 6.0));
    maxShortFrames = (int) std::ceil (kMaxSeconds * fs / (shortSize / 4));
    shortSeries.clear();
    shortSeries.reserve ((size_t) maxShortFrames * kShortBands);
    reset();
}

void ToneAnalysis::reset()
{
    std::fill (bandPowerSum.begin(), bandPowerSum.end(), 0.0);
    std::fill (longHist.begin(), longHist.end(), 0.0f);
    std::fill (shortHist.begin(), shortHist.end(), 0.0f);
    bandSeries.clear();
    shortSeries.clear();
    activeLongFrames = activeShortFrames = 0;
    longPos = shortPos = longSinceHop = shortSinceHop = longFill = shortFill = 0;
}

double ToneAnalysis::bandFreq (int j) const noexcept
{
    return kLowestBandHz * std::pow (2.0, j / kBandsPerOct);
}

int ToneAnalysis::bandIndex (double f) const noexcept
{
    if (f <= kLowestBandHz)
        return 0;
    return clampValue ((int) std::lround (kBandsPerOct * std::log2 (f / kLowestBandHz)), 0, numBands - 1);
}

double ToneAnalysis::getActiveSeconds() const noexcept
{
    return activeLongFrames * (longSize / 4) / fs;
}

double ToneAnalysis::bandLevelDb (double freqHz) const
{
    if (activeLongFrames == 0)
        return -200.0;
    return powToDb (bandPowerSum[(size_t) bandIndex (freqHz)] / activeLongFrames);
}

void ToneAnalysis::feed (const float* x, int n, int nch)
{
    if (longFft == nullptr)
        prepare (fs);
    nch = std::max (1, nch);
    for (int i = 0; i < n; ++i)
    {
        const float m = nch >= 2 ? 0.5f * (x[i * nch] + x[i * nch + 1]) : x[i * nch];
        longHist[(size_t) longPos] = m;
        longPos = longPos + 1 == longSize ? 0 : longPos + 1;
        longFill = std::min (longFill + 1, longSize);
        if (++longSinceHop >= longSize / 4 && longFill == longSize)
        {
            longSinceHop = 0;
            longFrame();
        }
        shortHist[(size_t) shortPos] = m;
        shortPos = shortPos + 1 == shortSize ? 0 : shortPos + 1;
        shortFill = std::min (shortFill + 1, shortSize);
        if (++shortSinceHop >= shortSize / 4 && shortFill == shortSize)
        {
            shortSinceHop = 0;
            shortFrame();
        }
    }
}

void ToneAnalysis::longFrame()
{
    if (activeLongFrames >= maxLongFrames)
        return;
    const int N = longSize;
    for (int i = 0; i < N; ++i)
        longFrameBuf[(size_t) i] = longHist[(size_t) ((longPos + i) % N)] * longWindow[(size_t) i];
    longFft->forward (longFrameBuf.data(), longSpec.data());

    double total = 0.0;
    std::fill (scratch.begin(), scratch.end(), 0.0);
    const double df = fs / N;
    for (int k = 1; k < N / 2; ++k)
    {
        const double p = 2.0 * std::norm (longSpec[(size_t) k]) * longNorm;
        total += p;
        // Distribute the bin's power over the 1/24-octave bands its interval [f - df/2, f + df/2] overlaps
        // (low-frequency bands are narrower than a bin).
        const double lo = (k - 0.5) * df, hi = (k + 0.5) * df;
        if (hi <= kLowestBandHz * std::pow (2.0, -0.5 / kBandsPerOct))
            continue;
        const double jlo = kBandsPerOct * std::log2 (std::max (lo, 1e-9) / kLowestBandHz) + 0.5;
        const double jhi = kBandsPerOct * std::log2 (hi / kLowestBandHz) + 0.5;
        const int j0 = std::max (0, (int) std::floor (jlo)), j1 = std::min (numBands - 1, (int) std::floor (jhi));
        if (j0 > numBands - 1)
            break;
        for (int j = j0; j <= j1; ++j)
        {
            const double bandLo = kLowestBandHz * std::pow (2.0, (j - 0.5) / kBandsPerOct);
            const double bandHi = kLowestBandHz * std::pow (2.0, (j + 0.5) / kBandsPerOct);
            const double overlap = std::min (hi, bandHi) - std::max (lo, bandLo);
            if (overlap > 0.0)
                scratch[(size_t) j] += p * overlap / df;
        }
    }
    if (powToDb (total) < kSilenceDb)
        return;
    for (int j = 0; j < numBands; ++j)
    {
        bandPowerSum[(size_t) j] += scratch[(size_t) j];
        bandSeries.push_back ((float) powToDb (scratch[(size_t) j]));
    }
    ++activeLongFrames;
}

void ToneAnalysis::shortFrame()
{
    if (activeShortFrames >= maxShortFrames)
        return;
    const int N = shortSize;
    for (int i = 0; i < N; ++i)
        shortFrameBuf[(size_t) i] = shortHist[(size_t) ((shortPos + i) % N)] * shortWindow[(size_t) i];
    shortFft->forward (shortFrameBuf.data(), shortSpec.data());
    double total = 0.0;
    double band[kShortBands] {};
    const double df = fs / N;
    for (int k = 1; k < N / 2; ++k)
    {
        const double p = 2.0 * std::norm (shortSpec[(size_t) k]) * shortNorm;
        total += p;
        const double f = k * df;
        const int i = (int) std::lround (6.0 * std::log2 (f / 1000.0));
        if (i >= 0 && i < kShortBands)
            band[i] += p;
    }
    if (powToDb (total) < kSilenceDb)
        return;
    for (double b : band)
        shortSeries.push_back ((float) powToDb (b));
    ++activeShortFrames;
}

//==============================================================================
std::vector<Suggestion> ToneAnalysis::analyse() const
{
    std::vector<Suggestion> out;
    if (! hasEnoughMaterial() || numBands < 48)
        return out;

    const int nb = numBands;
    std::vector<double> L ((size_t) nb);
    for (int j = 0; j < nb; ++j)
        L[(size_t) j] = powToDb (bandPowerSum[(size_t) j] / activeLongFrames);

    auto smooth = [&] (double widthOct)
    {
        std::vector<double> s ((size_t) nb);
        const int h = std::max (1, (int) std::lround (widthOct * kBandsPerOct / 2.0));
        for (int j = 0; j < nb; ++j)
        {
            double sum = 0.0;
            int cnt = 0;
            for (int i = std::max (0, j - h); i <= std::min (nb - 1, j + h); ++i, ++cnt)
                sum += dbToPow (L[(size_t) i]);
            s[(size_t) j] = powToDb (sum / cnt);
        }
        return s;
    };
    const auto S3 = smooth (1.0 / 3.0);
    const double maxF = 0.45 * fs;
    auto inRange = [&] (int j, double lo, double hi) { const double f = bandFreq (j); return f >= lo && f <= std::min (hi, maxF); };

    // Trend: robust straight-line fit of S3 against log2(f) over 100 Hz - 8 kHz. A region under test is left
    // out of its own trend (a hump must not pull the reference line towards itself).
    struct Line { double a = 0.0, b = 0.0; double at (double f) const { return a + b * std::log2 (f / 1000.0); } };
    auto fitTrend = [&] (double excludeLo, double excludeHi)
    {
        std::vector<int> idx;
        for (int j = 0; j < nb; ++j)
            if (inRange (j, 100.0, 8000.0) && ! (bandFreq (j) >= excludeLo && bandFreq (j) <= excludeHi))
                idx.push_back (j);
        Line line;
        auto fit = [&] (const std::vector<int>& use)
        {
            double sx = 0, sy = 0, sxx = 0, sxy = 0;
            for (int j : use)
            {
                const double x = std::log2 (bandFreq (j) / 1000.0), y = S3[(size_t) j];
                sx += x; sy += y; sxx += x * x; sxy += x * y;
            }
            const double n = (double) use.size(), d = n * sxx - sx * sx;
            line.b = d != 0.0 ? (n * sxy - sx * sy) / d : 0.0;
            line.a = n > 0.0 ? (sy - line.b * sx) / n : 0.0;
        };
        fit (idx);
        for (double limit : { 6.0, 3.0 }) // drop outliers, refit
        {
            std::vector<int> keep;
            for (int j : idx)
                if (std::abs (S3[(size_t) j] - line.at (bandFreq (j))) <= limit)
                    keep.push_back (j);
            if (keep.size() > idx.size() / 2)
                fit (keep);
        }
        return line;
    };

    double Lref = -300.0;
    for (int j = 0; j < nb; ++j)
        if (inRange (j, 40.0, 2000.0))
            Lref = std::max (Lref, S3[(size_t) j]);

    auto regionExcess = [&] (double lo, double hi, double& mean, double& peak, double& centroid)
    {
        // "mean" is the median excess over the region: a broad hump raises it, a single narrow resonance
        // (reported separately) does not.
        const auto line = fitTrend (lo / 1.2, hi * 1.2);
        std::vector<double> ex;
        double wsum = 0.0, wlog = 0.0;
        peak = -100.0;
        for (int j = 0; j < nb; ++j)
            if (inRange (j, lo, hi))
            {
                const double e = S3[(size_t) j] - line.at (bandFreq (j));
                ex.push_back (e);
                peak = std::max (peak, e);
                const double w = std::max (e, 0.0);
                wsum += w;
                wlog += w * std::log2 (bandFreq (j));
            }
        mean = median (ex);
        centroid = wsum > 0.0 ? std::pow (2.0, wlog / wsum) : std::sqrt (lo * hi);
        return ! ex.empty();
    };

    auto bell = [] (double f, double g, double q)
    {
        BandSettings b;
        b.active = true;
        b.type = FilterType::Bell;
        b.freqHz = roundTo (f, f < 1000.0 ? 1.0 : 10.0);
        b.gainDb = roundTo (g, 0.5);
        b.q = roundTo (q, 0.1);
        b.stereo = StereoMode::Stereo;
        return b;
    };

    //== High-pass ==============================================================================================
    {
        int jLow = -1;
        for (int j = 0; j < nb && jLow < 0; ++j)
            if (bandFreq (j) >= 25.0 && S3[(size_t) j] >= Lref - 15.0)
                jLow = j;
        if (jLow >= 0)
        {
            const double fLow = bandFreq (jLow);
            double sum = 0.0;
            int n = 0;
            for (int j = 0; j < nb; ++j)
                if (bandFreq (j) <= fLow / 2.0)
                {
                    sum += dbToPow (L[(size_t) j]);
                    ++n;
                }
            const double lSub = n > 0 ? powToDb (sum / n) : -300.0;
            if (fLow >= 50.0 && lSub >= Lref - 45.0)
            {
                const double fHp = roundTo (clampValue (0.6 * fLow, 20.0, 120.0), 5.0);
                Suggestion s;
                s.kind = Suggestion::Kind::HighPass;
                s.confidence = Suggestion::Confidence::Medium;
                s.band.active = true;
                s.band.type = FilterType::HighPass;
                s.band.freqHz = fHp;
                s.band.q = kButterworthQ;
                s.band.slopeIndex = 2; // 18 dB/oct
                s.title = "Low-frequency rumble below " + fmtFreq (fHp);
                s.reason = "The main content starts around " + fmtFreq (fLow) + ", yet below " + fmtFreq (fLow / 2.0)
                         + " there is energy only " + fmt0 (Lref - lSub) + " dB below the strongest region (rumble, handling "
                           "or stand noise, room hum). An 18 dB/oct high-pass at " + fmtFreq (fHp)
                         + " removes it and frees headroom. Skip this if the sound should carry deep sub-bass.";
                out.push_back (s);
            }
        }
    }

    //== Resonances =============================================================================================
    {
        struct Cand { int j; double excess, q, persistence, freq; };
        std::vector<Cand> cands;
        const int frames = activeLongFrames;
        for (int j = 12; j < nb - 12; ++j)
        {
            if (! inRange (j, 80.0, 12000.0))
                continue;
            const double v = L[(size_t) j];
            if (! (v >= L[(size_t) (j - 1)] && v >= L[(size_t) (j + 1)] && v > L[(size_t) (j - 2)] && v > L[(size_t) (j + 2)]))
                continue;
            std::vector<double> around;
            for (int i = j - 12; i <= j + 12; ++i)
                if (std::abs (i - j) > 4)
                    around.push_back (L[(size_t) i]);
            const double base = median (around);
            const double excess = v - base;
            if (excess < 6.0)
                continue;
            // Width between the points where the level has fallen by half the excess (dB).
            const double half = v - 0.5 * excess;
            double left = -1.0, right = -1.0;
            for (int i = j - 1; i >= j - 12 && left < 0.0; --i)
                if (L[(size_t) i] <= half)
                    left = i + (half - L[(size_t) i]) / std::max (1e-9, L[(size_t) (i + 1)] - L[(size_t) i]);
            for (int i = j + 1; i <= j + 12 && right < 0.0; ++i)
                if (L[(size_t) i] <= half)
                    right = i - (half - L[(size_t) i]) / std::max (1e-9, L[(size_t) (i - 1)] - L[(size_t) i]);
            if (left < 0.0 || right < 0.0)
                continue; // too broad for a resonance
            const double bw = (right - left) / kBandsPerOct;
            // Persistence: share of frames in which the peak stands out by at least half its average excess.
            int count = 0;
            for (int t = 0; t < frames; ++t)
            {
                const float* row = bandSeries.data() + (size_t) t * (size_t) nb;
                std::vector<double> ar;
                for (int i = j - 12; i <= j + 12; ++i)
                    if (std::abs (i - j) > 4)
                        ar.push_back (row[i]);
                if (row[j] - median (ar) >= 0.5 * excess)
                    ++count;
            }
            const double persistence = frames > 0 ? (double) count / frames : 0.0;
            if (persistence < 0.6)
                continue;
            // Sub-band peak position (parabola through the three highest bands).
            const double a = L[(size_t) (j - 1)], b = L[(size_t) j], c = L[(size_t) (j + 1)];
            const double den = a - 2.0 * b + c;
            const double off = den < 0.0 ? clampValue (0.5 * (a - c) / den, -0.5, 0.5) : 0.0;
            cands.push_back ({ j, excess, clampValue (qFromBandwidth (bw), 2.0, 16.0), persistence,
                               kLowestBandHz * std::pow (2.0, (j + off) / kBandsPerOct) });
        }
        std::sort (cands.begin(), cands.end(), [] (const Cand& x, const Cand& y) { return x.excess > y.excess; });
        std::vector<int> chosen;
        for (const auto& c : cands)
        {
            if (chosen.size() >= 3)
                break;
            bool farEnough = true;
            for (int j : chosen)
                farEnough = farEnough && std::abs (j - c.j) >= 8;
            if (! farEnough)
                continue;
            chosen.push_back (c.j);
            Suggestion s;
            s.kind = Suggestion::Kind::Resonance;
            s.confidence = c.persistence >= 0.85 && c.excess >= 9.0 ? Suggestion::Confidence::High : Suggestion::Confidence::Medium;
            s.band = bell (c.freq, -clampValue (0.7 * c.excess, 2.0, 10.0), c.q);
            s.title = "Resonance at " + fmtFreq (s.band.freqHz);
            s.reason = "A narrow peak " + fmt1 (c.excess) + " dB above its surroundings stands out in " + fmt0 (100.0 * c.persistence)
                     + " % of the analysed time. A cut of " + fmt1 (-s.band.gainDb) + " dB with Q " + fmt1 (s.band.q)
                     + " tames it. If it is a note or a tone you want to keep, skip this.";
            out.push_back (s);
        }
    }

    //== Low-mid build-up =======================================================================================
    {
        double mean = 0, peak = 0, centroid = 0;
        if (regionExcess (150.0, 500.0, mean, peak, centroid) && mean >= 2.5 && peak >= 3.5)
        {
            Suggestion s;
            s.kind = Suggestion::Kind::LowMid;
            s.confidence = Suggestion::Confidence::Medium;
            s.band = bell (centroid, -clampValue (0.7 * mean, 1.5, 4.0), 1.0);
            s.title = "Low-mid build-up around " + fmtFreq (s.band.freqHz);
            s.reason = "Between 150 and 500 Hz the level is typically " + fmt1 (mean) + " dB (up to " + fmt1 (peak)
                     + " dB) above the overall tonal trend of this material - often heard as muddy or boxy. A broad cut of "
                     + fmt1 (-s.band.gainDb) + " dB clears it.";
            out.push_back (s);
        }
    }

    //== Harshness (static or dynamic) ==========================================================================
    auto shortRegionLevels = [&] (double lo, double hi)
    {
        std::vector<double> levels;
        levels.reserve ((size_t) activeShortFrames);
        for (int t = 0; t < activeShortFrames; ++t)
        {
            const float* row = shortSeries.data() + (size_t) t * kShortBands;
            double p = 0.0;
            for (int i = 0; i < kShortBands; ++i)
                if (shortBandFreq[(size_t) i] >= lo && shortBandFreq[(size_t) i] <= std::min (hi, maxF))
                    p += dbToPow (row[i]);
            levels.push_back (powToDb (p));
        }
        return levels;
    };
    {
        double mean = 0, peak = 0, centroid = 0;
        if (regionExcess (2000.0, 5000.0, mean, peak, centroid) && mean >= 2.5 && peak >= 3.5)
        {
            Suggestion s;
            s.kind = Suggestion::Kind::Harshness;
            s.confidence = Suggestion::Confidence::Medium;
            const auto lv = shortRegionLevels (centroid / std::sqrt (2.0), centroid * std::sqrt (2.0));
            const double p10 = percentile (lv, 0.1), p90 = percentile (lv, 0.9);
            if (p90 - p10 >= 6.0)
            {
                s.band = bell (centroid, 0.0, 1.2);
                s.band.dyn.enabled = true;
                s.band.dyn.thresholdDb = roundTo (clampValue (0.5 * (p10 + p90), kMinThresholdDb, kMaxThresholdDb), 1.0);
                s.band.dyn.ratio = 3.0;
                s.band.dyn.rangeDb = -roundTo (clampValue (0.7 * mean + 1.0, 2.0, 6.0), 0.5);
                s.band.dyn.attackMs = 3.0;
                s.band.dyn.releaseMs = 120.0;
                s.band.dyn.detector = DetectorMode::Rms;
                s.title = "Harshness around " + fmtFreq (s.band.freqHz) + " (dynamic)";
                s.reason = "The 2-5 kHz region is typically " + fmt1 (mean) + " dB above the tonal trend, and its level swings by "
                         + fmt0 (p90 - p10) + " dB between quieter and louder passages. A dynamic band reduces it by up to "
                         + fmt1 (-s.band.dyn.rangeDb) + " dB only in the loud passages (threshold " + fmt0 (s.band.dyn.thresholdDb)
                         + " dBFS RMS, halfway between them).";
            }
            else
            {
                s.band = bell (centroid, -clampValue (0.6 * mean, 1.5, 4.0), 1.2);
                s.title = "Harshness around " + fmtFreq (s.band.freqHz);
                s.reason = "The 2-5 kHz region is typically " + fmt1 (mean) + " dB (up to " + fmt1 (peak)
                         + " dB) above the tonal trend of this material - often heard as harsh or edgy. A gentle cut of "
                         + fmt1 (-s.band.gainDb) + " dB smooths it.";
            }
            out.push_back (s);
        }
    }

    //== Sibilance / sharp bursts ===============================================================================
    if (activeShortFrames >= 50 && maxF > 7000.0)
    {
        const auto region = shortRegionLevels (5000.0, 10000.0);
        const double p50 = percentile (region, 0.5);
        std::vector<bool> burst (region.size());
        int bursts = 0;
        for (size_t t = 0; t < region.size(); ++t)
        {
            burst[t] = region[t] >= p50 + 8.0;
            bursts += burst[t] ? 1 : 0;
        }
        const double frac = region.empty() ? 0.0 : (double) bursts / (double) region.size();
        if (frac >= 0.02 && frac <= 0.30)
        {
            // Centroid of what the bursts add (burst average minus non-burst average, per 1/6-octave band).
            double wsum = 0.0, wlog = 0.0;
            for (int i = 0; i < kShortBands; ++i)
            {
                const double f = shortBandFreq[(size_t) i];
                if (f < 4000.0 || f > std::min (12000.0, maxF))
                    continue;
                double pb = 0.0, pn = 0.0;
                for (size_t t = 0; t < region.size(); ++t)
                    (burst[t] ? pb : pn) += dbToPow (shortSeries[t * kShortBands + (size_t) i]);
                const double add = pb / std::max (1, bursts) - pn / std::max<size_t> (1, region.size() - (size_t) bursts);
                if (add > 0.0)
                {
                    wsum += add;
                    wlog += add * std::log2 (f);
                }
            }
            if (wsum > 0.0)
            {
                const double fc = std::pow (2.0, wlog / wsum);
                // Levels in the detector's own region (about +-1/3 octave for Q 3).
                const auto lv = shortRegionLevels (fc * std::pow (2.0, -1.0 / 3.0), fc * std::pow (2.0, 1.0 / 3.0));
                const double m50 = percentile (lv, 0.5);
                double burstMean = 0.0;
                int nbur = 0;
                for (size_t t = 0; t < lv.size(); ++t)
                    if (burst[t])
                    {
                        burstMean += lv[t];
                        ++nbur;
                    }
                burstMean = nbur > 0 ? burstMean / nbur : m50;
                Suggestion s;
                s.kind = Suggestion::Kind::Sibilance;
                s.confidence = Suggestion::Confidence::Medium;
                s.band = bell (fc, 0.0, 3.0);
                s.band.dyn.enabled = true;
                s.band.dyn.thresholdDb = roundTo (clampValue (m50 + 0.5 * (burstMean - m50), kMinThresholdDb, kMaxThresholdDb), 1.0);
                s.band.dyn.ratio = 4.0;
                s.band.dyn.rangeDb = -6.0;
                s.band.dyn.attackMs = 1.0;
                s.band.dyn.releaseMs = 60.0;
                s.band.dyn.detector = DetectorMode::Rms;
                s.title = "Sibilance / sharp bursts around " + fmtFreq (s.band.freqHz);
                s.reason = "Short bursts " + fmt0 (burstMean - m50) + " dB above the usual level occur in " + fmt0 (100.0 * frac)
                         + " % of the time between 5 and 10 kHz (typical of \"s\" sounds, hi-hats or pick attacks). A dynamic band "
                           "reduces them by up to 6 dB only while they happen (threshold " + fmt0 (s.band.dyn.thresholdDb)
                         + " dBFS RMS, halfway between the usual and the burst level).";
                out.push_back (s);
            }
        }
    }

    //== Air ====================================================================================================
    if (maxF > 12000.0)
    {
        double mean = 0, peak = 0, centroid = 0;
        double presence = -300.0;
        for (int j = 0; j < nb; ++j)
            if (inRange (j, 2000.0, 8000.0))
                presence = std::max (presence, S3[(size_t) j]);
        if (regionExcess (9000.0, 16000.0, mean, peak, centroid) && mean <= -8.0 && presence >= Lref - 25.0)
        {
            Suggestion s;
            s.kind = Suggestion::Kind::Air;
            s.confidence = Suggestion::Confidence::Low;
            s.band.active = true;
            s.band.type = FilterType::HighShelf;
            s.band.freqHz = 10000.0;
            s.band.gainDb = 2.0;
            s.band.q = kButterworthQ;
            s.band.slopeIndex = 1;
            s.title = "Little air above 9 kHz";
            s.reason = "Above 9 kHz the level falls " + fmt0 (-mean) + " dB below the trend of the rest of the spectrum. If the sound "
                       "should be more open, a gentle +2 dB high shelf at 10 kHz adds air. Many sources are naturally darker - "
                       "trust your ears here.";
            out.push_back (s);
        }
    }
    return out;
}

} // namespace mepro::dsp
