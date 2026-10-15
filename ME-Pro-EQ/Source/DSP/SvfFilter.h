// ME Pro EQ — ME Audio
// Real-time cascade of trapezoidal state-variable filter sections (Zavalishin/Simper TPT topology) with
// output mixing. Double precision state and coefficients.
//
// Coefficient changes are applied as linear per-sample ramps of (g, k, m0, m1, m2) between two designs of
// identical layout. The TPT SVF remains stable for any positive g and k, so ramps cannot destabilise it.
// Ramp values are computed from the ramp index (start + delta * j), never accumulated, so the output is
// bit-identical no matter how the host splits the audio into blocks.
#pragma once

#include "DspConfig.h"
#include "FilterDesign.h"

#include <array>
#include <cmath>

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
    #define MEPRO_HAS_SSE2 1
    #include <emmintrin.h>
#else
    #define MEPRO_HAS_SSE2 0
#endif

namespace mepro::dsp
{

struct SvfRuntimeCoeffs
{
    int order = 2;
    double g = 1.0, k = 1.0;             // integrator gain, damping
    double m0 = 1.0, m1 = 0.0, m2 = 0.0; // y = m0*x + m1*bandpass + m2*lowpass
    double a1 = 0.0, a2 = 0.0, a3 = 0.0; // derived

    void updateDerived() noexcept
    {
        if (order == 2)
        {
            a1 = 1.0 / (1.0 + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        else
        {
            a1 = g / (1.0 + g);
            a2 = a3 = 0.0;
        }
    }

    static SvfRuntimeCoeffs fromSection (const SectionCoeffs& s) noexcept
    {
        SvfRuntimeCoeffs r;
        r.order = s.order;
        r.g = s.g;
        r.k = s.order == 2 ? s.k : 0.0;
        r.m0 = s.chp;
        r.m1 = s.order == 2 ? (s.cbp - s.chp * s.k) : 0.0;
        r.m2 = s.clp - s.chp;
        r.updateDerived();
        return r;
    }
};

class SvfCascade
{
public:
    void setLayout (const SectionLayout& l) noexcept
    {
        layout = l;
        for (int i = 0; i < layout.numSections; ++i)
        {
            coeffs[(size_t) i].order = layout.orders[(size_t) i];
            coeffs[(size_t) i].updateDerived();
        }
        reset();
    }

    const SectionLayout& getLayout() const noexcept { return layout; }

    void reset() noexcept
    {
        for (auto& s : state)
            s = {};
    }

    void setCoefficients (const CascadeCoeffs& c) noexcept
    {
        for (int i = 0; i < layout.numSections; ++i)
            coeffs[(size_t) i] = SvfRuntimeCoeffs::fromSection (c.sections[(size_t) i]);
        rampLength = rampPos = 0;
    }

    // Ramp from the current coefficients to `c` over `numSamples` samples (sample j of the ramp uses
    // start + (j + 1) * delta; the final sample uses the exact target).
    void rampTo (const CascadeCoeffs& c, int numSamples) noexcept
    {
        if (numSamples <= 1)
        {
            setCoefficients (c);
            return;
        }
        const double inv = 1.0 / numSamples;
        for (int i = 0; i < layout.numSections; ++i)
        {
            const auto t = SvfRuntimeCoeffs::fromSection (c.sections[(size_t) i]);
            const auto& cur = coeffs[(size_t) i];
            start[(size_t) i] = cur;
            target[(size_t) i] = t;
            auto& d = delta[(size_t) i];
            d.order = t.order;
            d.g = (t.g - cur.g) * inv;
            d.k = (t.k - cur.k) * inv;
            d.m0 = (t.m0 - cur.m0) * inv;
            d.m1 = (t.m1 - cur.m1) * inv;
            d.m2 = (t.m2 - cur.m2) * inv;
        }
        rampLength = numSamples;
        rampPos = 0;
    }

    bool isRamping() const noexcept { return rampPos < rampLength; }

    // Process a single channel (strided) in place, then advance the coefficient ramp by n samples.
    void processMono (double* x, int n, int stride, int channel = 0) noexcept
    {
        processChannelNoAdvance (x, n, stride, channel);
        advance (n);
    }

    // Process interleaved stereo (same coefficients for both channels), then advance by n samples.
    void processStereo (double* x, int n) noexcept
    {
#if MEPRO_HAS_SSE2
        for (int s = 0; s < layout.numSections; ++s)
        {
            if (isRamping())
                sectionRampStereo (s, x, n);
            else
                sectionStaticStereo (s, x, n);
        }
#else
        processChannelNoAdvance (x, n, 2, 0);
        processChannelNoAdvance (x + 1, n, 2, 1);
#endif
        advance (n);
    }

    bool stateIsFinite() const noexcept
    {
        for (const auto& s : state)
            if (! std::isfinite (s.s1) || ! std::isfinite (s.s2))
                return false;
        return true;
    }

    const SvfRuntimeCoeffs& currentCoeffs (int section) const noexcept { return coeffs[(size_t) section]; }

private:
    struct State
    {
        double s1 = 0.0, s2 = 0.0;
    };

    State& st (int section, int channel) noexcept { return state[(size_t) (section * kMaxChannels + channel)]; }

    // Coefficients at ramp sample index j (0-based within the current chunk).
    void rampCoeffsAt (int s, int rampIndex, SvfRuntimeCoeffs& c) const noexcept
    {
        if (rampIndex >= rampLength)
        {
            c = target[(size_t) s];
            return;
        }
        const auto& a = start[(size_t) s];
        const auto& d = delta[(size_t) s];
        const double t = (double) rampIndex;
        c.order = a.order;
        c.g = a.g + d.g * t;
        c.k = a.k + d.k * t;
        c.m0 = a.m0 + d.m0 * t;
        c.m1 = a.m1 + d.m1 * t;
        c.m2 = a.m2 + d.m2 * t;
    }

    void advance (int n) noexcept
    {
        if (! isRamping())
            return;
        rampPos += n;
        if (rampPos >= rampLength)
        {
            rampPos = rampLength = 0;
            for (int i = 0; i < layout.numSections; ++i)
                coeffs[(size_t) i] = target[(size_t) i];
        }
        else
        {
            for (int i = 0; i < layout.numSections; ++i)
            {
                rampCoeffsAt (i, rampPos, coeffs[(size_t) i]);
                coeffs[(size_t) i].updateDerived();
            }
        }
    }

    void processChannelNoAdvance (double* x, int n, int stride, int ch) noexcept
    {
        for (int s = 0; s < layout.numSections; ++s)
        {
            if (isRamping())
                sectionRamp (s, x, n, stride, ch);
            else
                sectionStatic (s, x, n, stride, ch);
        }
    }

    void sectionStatic (int s, double* x, int n, int stride, int ch) noexcept
    {
        const auto& c = coeffs[(size_t) s];
        auto& z = st (s, ch);
        double s1 = z.s1, s2 = z.s2;
        if (c.order == 2)
        {
            const double a1 = c.a1, a2 = c.a2, a3 = c.a3, m0 = c.m0, m1 = c.m1, m2 = c.m2;
            for (int i = 0; i < n; ++i)
            {
                double& io = x[i * stride];
                const double v0 = io;
                const double v3 = v0 - s2;
                const double v1 = a1 * s1 + a2 * v3;
                const double v2 = s2 + a2 * s1 + a3 * v3;
                s1 = 2.0 * v1 - s1;
                s2 = 2.0 * v2 - s2;
                io = m0 * v0 + m1 * v1 + m2 * v2;
            }
        }
        else
        {
            const double G = c.a1, m0 = c.m0, m2 = c.m2;
            for (int i = 0; i < n; ++i)
            {
                double& io = x[i * stride];
                const double v0 = io;
                const double v = (v0 - s1) * G;
                const double lp = v + s1;
                s1 = lp + v;
                io = m0 * v0 + m2 * lp;
            }
        }
        z.s1 = s1;
        z.s2 = s2;
    }

    void sectionRamp (int s, double* x, int n, int stride, int ch) noexcept
    {
        auto& z = st (s, ch);
        double s1 = z.s1, s2 = z.s2;
        SvfRuntimeCoeffs c;
        for (int i = 0; i < n; ++i)
        {
            rampCoeffsAt (s, rampPos + i + 1, c);
            double& io = x[i * stride];
            const double v0 = io;
            if (c.order == 2)
            {
                const double a1 = 1.0 / (1.0 + c.g * (c.g + c.k));
                const double a2 = c.g * a1, a3 = c.g * a2;
                const double v3 = v0 - s2;
                const double v1 = a1 * s1 + a2 * v3;
                const double v2 = s2 + a2 * s1 + a3 * v3;
                s1 = 2.0 * v1 - s1;
                s2 = 2.0 * v2 - s2;
                io = c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
            }
            else
            {
                const double G = c.g / (1.0 + c.g);
                const double v = (v0 - s1) * G;
                const double lp = v + s1;
                s1 = lp + v;
                io = c.m0 * v0 + c.m2 * lp;
            }
        }
        z.s1 = s1;
        z.s2 = s2;
    }

#if MEPRO_HAS_SSE2
    void sectionStaticStereo (int s, double* x, int n) noexcept
    {
        const auto& c = coeffs[(size_t) s];
        auto& zL = st (s, 0);
        auto& zR = st (s, 1);
        __m128d s1 = _mm_set_pd (zR.s1, zL.s1);
        __m128d s2 = _mm_set_pd (zR.s2, zL.s2);
        const __m128d two = _mm_set1_pd (2.0);
        if (c.order == 2)
        {
            const __m128d a1 = _mm_set1_pd (c.a1), a2 = _mm_set1_pd (c.a2), a3 = _mm_set1_pd (c.a3);
            const __m128d m0 = _mm_set1_pd (c.m0), m1 = _mm_set1_pd (c.m1), m2 = _mm_set1_pd (c.m2);
            for (int i = 0; i < n; ++i)
            {
                const __m128d v0 = _mm_loadu_pd (x + 2 * i);
                const __m128d v3 = _mm_sub_pd (v0, s2);
                const __m128d v1 = _mm_add_pd (_mm_mul_pd (a1, s1), _mm_mul_pd (a2, v3));
                const __m128d v2 = _mm_add_pd (_mm_add_pd (s2, _mm_mul_pd (a2, s1)), _mm_mul_pd (a3, v3));
                s1 = _mm_sub_pd (_mm_mul_pd (two, v1), s1);
                s2 = _mm_sub_pd (_mm_mul_pd (two, v2), s2);
                _mm_storeu_pd (x + 2 * i, _mm_add_pd (_mm_add_pd (_mm_mul_pd (m0, v0), _mm_mul_pd (m1, v1)), _mm_mul_pd (m2, v2)));
            }
        }
        else
        {
            const __m128d G = _mm_set1_pd (c.a1), m0 = _mm_set1_pd (c.m0), m2 = _mm_set1_pd (c.m2);
            for (int i = 0; i < n; ++i)
            {
                const __m128d v0 = _mm_loadu_pd (x + 2 * i);
                const __m128d v = _mm_mul_pd (_mm_sub_pd (v0, s1), G);
                const __m128d lp = _mm_add_pd (v, s1);
                s1 = _mm_add_pd (lp, v);
                _mm_storeu_pd (x + 2 * i, _mm_add_pd (_mm_mul_pd (m0, v0), _mm_mul_pd (m2, lp)));
            }
        }
        double t[2];
        _mm_storeu_pd (t, s1);
        zL.s1 = t[0];
        zR.s1 = t[1];
        _mm_storeu_pd (t, s2);
        zL.s2 = t[0];
        zR.s2 = t[1];
    }

    void sectionRampStereo (int s, double* x, int n) noexcept
    {
        auto& zL = st (s, 0);
        auto& zR = st (s, 1);
        __m128d s1 = _mm_set_pd (zR.s1, zL.s1);
        __m128d s2 = _mm_set_pd (zR.s2, zL.s2);
        const __m128d two = _mm_set1_pd (2.0);
        SvfRuntimeCoeffs c;
        for (int i = 0; i < n; ++i)
        {
            rampCoeffsAt (s, rampPos + i + 1, c);
            const __m128d v0 = _mm_loadu_pd (x + 2 * i);
            if (c.order == 2)
            {
                const double sa1 = 1.0 / (1.0 + c.g * (c.g + c.k));
                const double sa2 = c.g * sa1;
                const __m128d a1 = _mm_set1_pd (sa1), a2 = _mm_set1_pd (sa2), a3 = _mm_set1_pd (c.g * sa2);
                const __m128d v3 = _mm_sub_pd (v0, s2);
                const __m128d v1 = _mm_add_pd (_mm_mul_pd (a1, s1), _mm_mul_pd (a2, v3));
                const __m128d v2 = _mm_add_pd (_mm_add_pd (s2, _mm_mul_pd (a2, s1)), _mm_mul_pd (a3, v3));
                s1 = _mm_sub_pd (_mm_mul_pd (two, v1), s1);
                s2 = _mm_sub_pd (_mm_mul_pd (two, v2), s2);
                const __m128d y = _mm_add_pd (_mm_add_pd (_mm_mul_pd (_mm_set1_pd (c.m0), v0), _mm_mul_pd (_mm_set1_pd (c.m1), v1)),
                                              _mm_mul_pd (_mm_set1_pd (c.m2), v2));
                _mm_storeu_pd (x + 2 * i, y);
            }
            else
            {
                const __m128d G = _mm_set1_pd (c.g / (1.0 + c.g));
                const __m128d v = _mm_mul_pd (_mm_sub_pd (v0, s1), G);
                const __m128d lp = _mm_add_pd (v, s1);
                s1 = _mm_add_pd (lp, v);
                _mm_storeu_pd (x + 2 * i, _mm_add_pd (_mm_mul_pd (_mm_set1_pd (c.m0), v0), _mm_mul_pd (_mm_set1_pd (c.m2), lp)));
            }
        }
        double t[2];
        _mm_storeu_pd (t, s1);
        zL.s1 = t[0];
        zR.s1 = t[1];
        _mm_storeu_pd (t, s2);
        zL.s2 = t[0];
        zR.s2 = t[1];
    }
#endif

    SectionLayout layout;
    int rampLength = 0, rampPos = 0;
    std::array<SvfRuntimeCoeffs, kMaxSections> coeffs {};
    std::array<SvfRuntimeCoeffs, kMaxSections> start {};
    std::array<SvfRuntimeCoeffs, kMaxSections> target {};
    std::array<SvfRuntimeCoeffs, kMaxSections> delta {};
    std::array<State, kMaxSections * kMaxChannels> state {};
};

} // namespace mepro::dsp
