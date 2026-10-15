// ME Pro EQ — ME Audio
// Deterministic test signals and measurement helpers shared by the test suites.
#pragma once

#include "DSP/MathUtils.h"

#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace metest
{
using mepro::dsp::kPi;
using mepro::dsp::kTwoPi;

// Deterministic xorshift RNG (identical sequences on every platform/compiler).
struct Rng
{
    uint64_t s = 0x9E3779B97F4A7C15ull;
    explicit Rng (uint64_t seed = 1) : s (seed * 0x9E3779B97F4A7C15ull + 1) {}
    uint64_t next()
    {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return s;
    }
    double uniform() { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); } // [0,1)
    double bipolar() { return 2.0 * uniform() - 1.0; }
};

inline std::vector<double> sine (double freq, double fs, int n, double amp = 1.0, double phase = 0.0)
{
    std::vector<double> v ((size_t) n);
    for (int i = 0; i < n; ++i)
        v[(size_t) i] = amp * std::sin (kTwoPi * freq * i / fs + phase);
    return v;
}

inline std::vector<double> whiteNoise (int n, double amp, uint64_t seed = 1)
{
    Rng r (seed);
    std::vector<double> v ((size_t) n);
    for (auto& x : v)
        x = amp * r.bipolar();
    return v;
}

// Pink noise (Paul Kellet's refined filter, -3 dB/oct within ~0.05 dB from 10 Hz to Nyquist at 44.1 kHz).
inline std::vector<double> pinkNoise (int n, double amp, uint64_t seed = 2)
{
    Rng r (seed);
    std::vector<double> v ((size_t) n);
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    for (auto& x : v)
    {
        const double w = r.bipolar();
        b0 = 0.99886 * b0 + w * 0.0555179;
        b1 = 0.99332 * b1 + w * 0.0750759;
        b2 = 0.96900 * b2 + w * 0.1538520;
        b3 = 0.86650 * b3 + w * 0.3104856;
        b4 = 0.55000 * b4 + w * 0.5329522;
        b5 = -0.7616 * b5 - w * 0.0168980;
        x = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362) * 0.11 * amp;
        b6 = w * 0.115926;
    }
    return v;
}

inline std::vector<double> logSweep (double f1, double f2, double fs, int n, double amp = 1.0)
{
    std::vector<double> v ((size_t) n);
    const double T = n / fs, L = T / std::log (f2 / f1);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs;
        v[(size_t) i] = amp * std::sin (kTwoPi * f1 * L * (std::exp (t / L) - 1.0));
    }
    return v;
}

// Complex amplitude of a sinusoid at `freq` over [start, start+len) by least squares (exact for an
// integer number of periods; with long windows the leakage error is negligible).
inline std::complex<double> toneAmplitude (const std::vector<double>& x, double freq, double fs, int start, int len)
{
    double sc = 0, ss = 0, cc = 0, xs = 0, xc = 0;
    for (int i = start; i < start + len; ++i)
    {
        const double ph = kTwoPi * freq * i / fs;
        const double s = std::sin (ph), c = std::cos (ph);
        ss += s * s;
        cc += c * c;
        sc += s * c;
        xs += x[(size_t) i] * s;
        xc += x[(size_t) i] * c;
    }
    // Solve [ss sc; sc cc] [a; b] = [xs; xc] for x ~ a sin + b cos
    const double det = ss * cc - sc * sc;
    const double a = (xs * cc - xc * sc) / det;
    const double b = (xc * ss - xs * sc) / det;
    return { a, b }; // amplitude = |a + jb|
}

inline double rms (const std::vector<double>& x, int start = 0, int len = -1)
{
    if (len < 0)
        len = (int) x.size() - start;
    double s = 0;
    for (int i = start; i < start + len; ++i)
        s += x[(size_t) i] * x[(size_t) i];
    return std::sqrt (s / std::max (1, len));
}

inline double peakAbs (const std::vector<double>& x)
{
    double p = 0;
    for (double v : x)
        p = std::max (p, std::abs (v));
    return p;
}

inline bool allFinite (const std::vector<double>& x)
{
    for (double v : x)
        if (! std::isfinite (v))
            return false;
    return true;
}

// DFT of a (decayed) impulse response at angular frequency w.
inline std::complex<double> dftAt (const std::vector<double>& h, double w)
{
    std::complex<double> acc (0.0, 0.0);
    const std::complex<double> rot (std::cos (w), -std::sin (w));
    std::complex<double> ph (1.0, 0.0);
    for (size_t n = 0; n < h.size(); ++n)
    {
        acc += h[n] * ph;
        ph *= rot;
        if ((n & 1023) == 1023) // renormalise the rotating phasor to avoid drift
            ph /= std::abs (ph);
    }
    return acc;
}

inline double db (double x) { return 20.0 * std::log10 (std::max (x, 1e-300)); }

inline std::vector<double> geomspace (double a, double b, int n)
{
    std::vector<double> v ((size_t) n);
    for (int i = 0; i < n; ++i)
        v[(size_t) i] = a * std::pow (b / a, n > 1 ? (double) i / (n - 1) : 0.0);
    return v;
}

inline const std::vector<double>& allSampleRates()
{
    static const std::vector<double> r { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
    return r;
}

} // namespace metest
