// ME Pro EQ — ME Audio
// Frequency-selective level detector and gain computer for dynamic EQ bands.
//
// Detector path:  band-limited sidechain -> rectifier (peak |x|, or mean-square x^2 over ~2.5 periods of
//                 f0) -> "smooth decoupled" peak detector (Giannoulis, Massberg & Reiss, JAES 2012):
//                     stage 1: y1 = max(in, release-smoothed y1)   (peak hold with exponential release)
//                     stage 2: env = attack-smoothed y1            (exponential attack)
//                 -> level in dBFS. A level step therefore produces a one-pole attack response with the
//                 attack time constant, independent of the waveform inside the band.
// Gain computer:  over = level - threshold; soft knee (6 dB); amount = over * (1 - 1/ratio);
//                 dynamic gain = sign(range) * min(amount, |range|)
//   range < 0 -> dynamic reduction (frequency-selective compression above threshold)
//   range > 0 -> dynamic enhancement (frequency-selective upward expansion above threshold)
#pragma once

#include "DspConfig.h"
#include "FilterDesign.h"
#include "MathUtils.h"
#include "SvfFilter.h"

#include <algorithm>
#include <cmath>

namespace mepro::dsp
{

struct DynamicSettings
{
    bool enabled = false;
    double thresholdDb = -24.0;
    double ratio = 2.0;
    double attackMs = 10.0;
    double releaseMs = 150.0;
    double rangeDb = -6.0;
    DetectorMode detector = DetectorMode::Peak;
    SidechainSource source = SidechainSource::Internal;
};

inline constexpr double kDynKneeDb = 6.0;
inline constexpr double kMinThresholdDb = -80.0, kMaxThresholdDb = 0.0;
inline constexpr double kMinRatio = 1.0, kMaxRatio = 20.0;
inline constexpr double kMinAttackMs = 0.1, kMaxAttackMs = 250.0;
inline constexpr double kMinReleaseMs = 5.0, kMaxReleaseMs = 2000.0;
inline constexpr double kMinRangeDb = -18.0, kMaxRangeDb = 18.0;

// Static gain-computer curve. Returns the dynamic gain change in dB for a detector level in dBFS.
inline double dynamicGainForLevel (double levelDb, const DynamicSettings& s) noexcept
{
    const double over = levelDb - s.thresholdDb;
    const double halfKnee = 0.5 * kDynKneeDb;
    double x;
    if (over <= -halfKnee)
        x = 0.0;
    else if (over < halfKnee)
        x = (over + halfKnee) * (over + halfKnee) / (2.0 * kDynKneeDb);
    else
        x = over;

    const double ratio = clampValue (s.ratio, kMinRatio, kMaxRatio);
    const double amount = x * (1.0 - 1.0 / ratio);
    const double range = clampValue (s.rangeDb, kMinRangeDb, kMaxRangeDb);
    return range < 0.0 ? -std::min (amount, -range) : std::min (amount, range);
}

// Detector filter matched to the band shape: bell -> constant-peak band-pass (0 dB at f0),
// low shelf -> 2nd-order low-pass at f0, high shelf -> 2nd-order high-pass at f0.
inline CascadeCoeffs designDetectorFilter (FilterType bandType, double freqHz, double q, double sampleRate) noexcept
{
    DesignSpec spec;
    spec.freqHz = freqHz;
    spec.slopeIndex = 1; // 12 dB/oct
    spec.gainDb = 0.0;
    switch (bandType)
    {
        case FilterType::LowShelf:  spec.type = FilterType::LowPass;  spec.q = kButterworthQ; break;
        case FilterType::HighShelf: spec.type = FilterType::HighPass; spec.q = kButterworthQ; break;
        default:                    spec.type = FilterType::BandPass; spec.q = clampValue (q, 0.3, kMaxQ); break;
    }
    return designCascade (spec, sampleRate);
}

class DynamicDetector
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        SectionLayout l;
        l.numSections = 1;
        l.orders[0] = 2;
        filter.setLayout (l);
        reset();
        updateTiming();
    }

    void reset() noexcept
    {
        filter.reset();
        env = 0.0;
        held = 0.0;
        msAvg = 0.0;
    }

    void setFilter (FilterType bandType, double freqHz, double q) noexcept
    {
        if (bandType == lastType && freqHz == lastFreq && q == lastQ)
            return;
        lastType = bandType;
        lastFreq = freqHz;
        lastQ = q;
        filter.setCoefficients (designDetectorFilter (bandType, freqHz, q, sampleRate));
        // Frequency-adaptive mean-square window for the RMS detector: ~2.5 periods of f0, 1..50 ms.
        const double rmsTime = clampValue (2.5 / std::max (freqHz, 1.0), 0.001, 0.05);
        rmsCoef = onePoleCoefficient (rmsTime, sampleRate);
    }

    void setTiming (double attackMs, double releaseMs, DetectorMode newMode) noexcept
    {
        attackMs = clampValue (attackMs, kMinAttackMs, kMaxAttackMs);
        releaseMs = clampValue (releaseMs, kMinReleaseMs, kMaxReleaseMs);
        if (attackMs == attMs && releaseMs == relMs && newMode == mode)
            return;
        if (newMode != mode)
            env = held = msAvg = 0.0;
        attMs = attackMs;
        relMs = releaseMs;
        mode = newMode;
        updateTiming();
    }

    // Feed one channel (strided). `scratch` must hold n doubles.
    void processMono (const double* x, int n, int stride, double* scratch) noexcept
    {
        for (int i = 0; i < n; ++i)
            scratch[i] = x[i * stride];
        filter.processMono (scratch, n, 1, 0);
        for (int i = 0; i < n; ++i)
            follow (mode == DetectorMode::Peak ? std::abs (scratch[i]) : scratch[i] * scratch[i]);
    }

    // Feed interleaved stereo, linked: peak = max(|L|,|R|), RMS = mean of L^2 and R^2. scratch: 2n doubles.
    void processStereoLinked (const double* x, int n, double* scratch) noexcept
    {
        for (int i = 0; i < 2 * n; ++i)
            scratch[i] = x[i];
        filter.processStereo (scratch, n);
        for (int i = 0; i < n; ++i)
        {
            const double l = scratch[2 * i], r = scratch[2 * i + 1];
            follow (mode == DetectorMode::Peak ? std::max (std::abs (l), std::abs (r)) : 0.5 * (l * l + r * r));
        }
    }

    // Feed a derived mono signal (e.g. mid or side) that the caller has already computed into `x`.
    void processDerived (double* xInPlace, int n) noexcept
    {
        filter.processMono (xInPlace, n, 1, 0);
        for (int i = 0; i < n; ++i)
            follow (mode == DetectorMode::Peak ? std::abs (xInPlace[i]) : xInPlace[i] * xInPlace[i]);
    }

    double levelDb() const noexcept
    {
        return mode == DetectorMode::Peak ? gainToDb (env, -200.0) : powerToDb (env, -200.0);
    }

    double envelope() const noexcept { return env; }

private:
    void updateTiming() noexcept
    {
        attCoef = onePoleCoefficient (attMs * 0.001, sampleRate);
        relCoef = onePoleCoefficient (relMs * 0.001, sampleRate);
    }

    inline void follow (double in) noexcept
    {
        if (mode == DetectorMode::Rms)
        {
            msAvg = in + rmsCoef * (msAvg - in);
            in = msAvg;
        }
        held = std::max (in, in + relCoef * (held - in)); // stage 1: peak hold with release
        env = held + attCoef * (env - held);              // stage 2: attack smoothing
        if (! (env < 1e30) || ! (held < 1e30)) // NaN/Inf guard (comparison is false for NaN)
            env = held = 0.0;
    }

    SvfCascade filter;
    double sampleRate = 48000.0;
    double env = 0.0, held = 0.0, msAvg = 0.0;
    double attMs = 10.0, relMs = 150.0;
    double attCoef = 0.0, relCoef = 0.0, rmsCoef = 0.0;
    DetectorMode mode = DetectorMode::Peak;
    FilterType lastType = FilterType::NumTypes;
    double lastFreq = -1.0, lastQ = -1.0;
};

} // namespace mepro::dsp
