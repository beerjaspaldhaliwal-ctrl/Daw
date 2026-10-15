// ME Pro EQ — ME Audio
#include "FilterDesign.h"
#include "MathUtils.h"

#include <cmath>

namespace mepro::dsp
{

//==============================================================================
// Exact digital responses (evaluated in a form that stays finite at w = 0 and w = pi)

std::complex<double> SectionCoeffs::response (double omega) const noexcept
{
    const double c = std::cos (0.5 * omega);
    const double s = std::sin (0.5 * omega);
    const double cg = c * g;

    if (order == 1)
        return std::complex<double> (clp * cg, chp * s) / std::complex<double> (cg, s);

    const std::complex<double> num (clp * cg * cg - chp * s * s, cbp * s * cg);
    const std::complex<double> den (cg * cg - s * s, k * s * cg);
    return num / den;
}

bool SectionCoeffs::isFiniteAndStable() const noexcept
{
    const bool finite = std::isfinite (g) && std::isfinite (k) && std::isfinite (clp)
                     && std::isfinite (cbp) && std::isfinite (chp);
    if (! finite || ! (g > 0.0))
        return false;
    return order == 1 || k > 0.0;
}

std::complex<double> CascadeCoeffs::response (double omega) const noexcept
{
    std::complex<double> h (1.0, 0.0);
    for (int i = 0; i < numSections; ++i)
        h *= sections[(size_t) i].response (omega);
    return h;
}

double CascadeCoeffs::magnitudeDb (double omega) const noexcept
{
    return gainToDb (std::abs (response (omega)));
}

std::complex<double> AnalogSection::response (double W) const noexcept
{
    const std::complex<double> s (0.0, W);
    if (order == 1)
        return scale * (n0 + n1 * s) / (d0 + d1 * s);
    return scale * (n0 + n1 * s + n2 * s * s) / (d0 + d1 * s + d2 * s * s);
}

double AnalogSection::magnitudeSquared (double W) const noexcept
{
    const double sc2 = scale * scale;
    if (order == 1)
        return sc2 * (n0 * n0 + n1 * n1 * W * W) / (d0 * d0 + d1 * d1 * W * W);

    const double W2 = W * W;
    const double nr = n0 - n2 * W2, ni = n1 * W;
    const double dr = d0 - d2 * W2, di = d1 * W;
    return sc2 * (nr * nr + ni * ni) / (dr * dr + di * di);
}

std::complex<double> AnalogCascade::response (double freqHz) const noexcept
{
    std::complex<double> h (1.0, 0.0);
    const double W = freqHz / designFreqHz;
    for (int i = 0; i < numSections; ++i)
        h *= sections[(size_t) i].response (W);
    return h;
}

//==============================================================================
int butterworthQs (int order, std::array<double, kMaxSections>& qs) noexcept
{
    const int pairs = order / 2;
    for (int i = 0; i < pairs; ++i)
        qs[(size_t) i] = 1.0 / (2.0 * std::sin ((2.0 * (i + 1) - 1.0) * kPi / (2.0 * order)));
    return pairs;
}

SectionLayout sectionLayout (FilterType type, int slopeIndex) noexcept
{
    SectionLayout l;
    auto addOrderN = [&l] (int n)
    {
        for (int i = 0; i < n / 2; ++i)
            l.orders[(size_t) l.numSections++] = 2;
        if (n % 2)
            l.orders[(size_t) l.numSections++] = 1;
    };

    switch (type)
    {
        case FilterType::HighPass:
        case FilterType::LowPass:
            addOrderN (filterOrderForSlopeIndex (slopeIndex));
            break;
        case FilterType::LowShelf:
        case FilterType::HighShelf:
        case FilterType::Tilt:
            addOrderN (filterOrderForSlopeIndex (std::min (slopeIndex, kMaxShelfSlopeIndex)));
            break;
        default:
            l.orders[0] = 2;
            l.numSections = 1;
            break;
    }
    return l;
}

double effectiveDesignFrequency (double freqHz, double sampleRate) noexcept
{
    const double hi = kMaxDesignFreqRatio * sampleRate;
    if (! std::isfinite (freqHz))
        freqHz = 1000.0;
    return clampValue (freqHz, std::min (1.0, hi), hi);
}

//==============================================================================
namespace
{
    // A = 10^(dB/40) (the square root of the linear gain), via exp: noticeably cheaper than pow.
    inline double dbToA (double gainDb) noexcept { return std::exp (gainDb * (2.302585092994045684 / 40.0)); }

    // Map the analog prototype's slope/resonance control to per-section Q values (highest-Q section first).
    // At the default Q (1/sqrt2) the result is an exact Butterworth distribution; other Q values scale the
    // most resonant section ("resonance"), limited to [0.1, maxQ].
    int resonantSectionQs (int order, double userQ, double maxQ, std::array<double, kMaxSections>& qs) noexcept
    {
        const int pairs = butterworthQs (order, qs);
        if (pairs > 0)
        {
            const double ratio = userQ / kButterworthQ;
            const double cap = std::max (maxQ, qs[0]);
            qs[0] = clampValue (qs[0] * ratio, 0.1, cap);
        }
        return pairs;
    }

    AnalogSection lowShelf2 (double gainDb, double q)
    {
        const double A = dbToA (gainDb), sA = std::sqrt (A);
        AnalogSection s;
        s.n0 = A * A; s.n1 = A * sA / q; s.n2 = A;
        s.d0 = 1.0;   s.d1 = sA / q;     s.d2 = A;
        return s;
    }

    AnalogSection highShelf2 (double gainDb, double q)
    {
        const double A = dbToA (gainDb), sA = std::sqrt (A);
        AnalogSection s;
        s.n0 = A; s.n1 = A * sA / q; s.n2 = A * A;
        s.d0 = A; s.d1 = sA / q;     s.d2 = 1.0;
        return s;
    }

    AnalogSection lowShelf1 (double gainDb)
    {
        // (s + A) / (s + 1/A): DC gain A^2, HF gain 1, |H(j1)| = A (half the dB gain at f0)
        const double A = dbToA (gainDb);
        AnalogSection s;
        s.order = 1;
        s.n0 = A; s.n1 = 1.0; s.d0 = 1.0 / A; s.d1 = 1.0;
        return s;
    }

    AnalogSection highShelf1 (double gainDb)
    {
        // A^2 (s + 1/A) / (s + A): DC gain 1, HF gain A^2, |H(j1)| = A
        const double A = dbToA (gainDb);
        AnalogSection s;
        s.order = 1;
        s.n0 = A; s.n1 = A * A; s.d0 = A; s.d1 = 1.0;
        return s;
    }

    void addShelfChain (AnalogCascade& c, bool high, int order, double gainDb, double userQ)
    {
        std::array<double, kMaxSections> qs {};
        const int pairs = resonantSectionQs (order, userQ, kMaxShelfQ, qs);
        const double perSecondOrder = gainDb * 2.0 / order;
        for (int i = 0; i < pairs; ++i)
            c.sections[(size_t) c.numSections++] = high ? highShelf2 (perSecondOrder, qs[(size_t) i])
                                                        : lowShelf2 (perSecondOrder, qs[(size_t) i]);
        if (order % 2)
            c.sections[(size_t) c.numSections++] = high ? highShelf1 (gainDb / order) : lowShelf1 (gainDb / order);
    }

    double sq (double x) noexcept { return x * x; }


    // Robust 2nd-order impulse-invariant pole mapping helpers ------------------------------------------
    constexpr double kAngleSoftStart = 0.9 * kPi;
    constexpr double kAngleMax = 0.995 * kPi;

    double compressPoleAngle (double theta) noexcept
    {
        if (theta <= kAngleSoftStart)
            return theta;
        const double span = kAngleMax - kAngleSoftStart;
        return kAngleSoftStart + span * std::tanh ((theta - kAngleSoftStart) / span);
    }

    // Three-point (DC / wm / Nyquist) numerator match for given poles. Returns false if the target was not
    // exactly realizable (the band-pass mixing gain had to be clamped).
    bool matchNumerator (SectionCoeffs& s, double tDc, double tNy, double wm, double tM) noexcept
    {
        s.clp = std::sqrt (std::max (tDc, 0.0));
        s.chp = std::sqrt (std::max (tNy, 0.0));
        const double um = std::tan (0.5 * wm) / s.g;
        const double vm = um * um;
        const double dn = sq (1.0 - vm) + s.k * s.k * vm;
        const double cbp2 = (tM * dn - s.clp * s.clp - s.chp * s.chp * vm * vm) / vm + 2.0 * s.clp * s.chp;
        s.cbp = std::sqrt (std::max (cbp2, 0.0));
        return cbp2 >= 0.0;
    }

    SectionCoeffs designFirstOrder (const AnalogSection& a, double w0) noexcept
    {
        SectionCoeffs s;
        s.order = 1;
        s.k = 0.0;
        s.cbp = 0.0;
        const double t0 = std::tan (0.5 * w0);
        const double T0 = a.magnitudeSquared (0.0);
        const double Tm = a.magnitudeSquared (1.0);
        const double Tn = a.magnitudeSquared (kPi / w0);
        s.clp = std::sqrt (T0);
        s.chp = std::sqrt (Tn);

        const double span = Tn - T0;
        double u0sq = 1.0;
        if (std::abs (span) > 1e-12 * std::max (T0, Tn))
        {
            u0sq = (Tm - T0) / (Tn - Tm);
            if (! (u0sq > 0.0) || ! std::isfinite (u0sq))
                u0sq = 1.0; // unreachable for monotonic first-order prototypes below 0.49 fs; keep it safe
        }
        s.g = t0 / std::sqrt (u0sq);
        return s;
    }
} // namespace

//==============================================================================
void polesToSvf (double W, double zeta, double& g, double& k) noexcept
{
    // Digital poles z = exp(s T) of the analog pair s = W(-zeta +/- sqrt(zeta^2 - 1)), W in rad/sample.
    // g^2 = (1 + a1 + a2) / (1 - a1 + a2),  k = 2 (1 - a2) / ((1 - a1 + a2) g), all via expm1 to avoid
    // cancellation for poles close to z = 1.
    const double sig = zeta * W;
    double onePlus, oneMinus;

    if (zeta < 1.0)
    {
        const double theta = compressPoleAngle (W * std::sqrt (1.0 - zeta * zeta));
        const double r = std::exp (-sig);
        const double em = std::expm1 (-sig);
        const double s2 = sq (std::sin (0.5 * theta));
        onePlus = em * em + 4.0 * r * s2;
        oneMinus = sq (1.0 + r) - 4.0 * r * s2;
    }
    else
    {
        const double q = W * std::sqrt (zeta * zeta - 1.0);
        onePlus = std::expm1 (-sig + q) * std::expm1 (-sig - q);
        oneMinus = (1.0 + std::exp (-sig + q)) * (1.0 + std::exp (-sig - q));
    }

    const double oneMinusA2 = -std::expm1 (-2.0 * sig);
    g = std::sqrt (onePlus / oneMinus);
    k = 2.0 * oneMinusA2 / (oneMinus * g);
}

SectionCoeffs invertSection (const SectionCoeffs& s) noexcept
{
    SectionCoeffs r;
    r.order = s.order;
    if (s.order == 1)
    {
        const double wn = s.clp / s.chp;
        r.g = s.g * wn;
        r.k = 0.0;
        r.clp = 1.0 / s.clp;
        r.chp = 1.0 / s.chp;
        r.cbp = 0.0;
        return r;
    }
    const double wn = std::sqrt (s.clp / s.chp);
    r.g = s.g * wn;
    r.k = s.cbp / (s.chp * wn);
    r.clp = 1.0 / s.clp;
    r.cbp = s.k / (s.chp * wn);
    r.chp = 1.0 / s.chp;
    return r;
}

SectionCoeffs designSection (const AnalogSection& a, double w0) noexcept
{
    SectionCoeffs s;

    if (a.order == 1)
    {
        AnalogSection unscaled = a;
        unscaled.scale = 1.0;
        s = designFirstOrder (unscaled, w0);
    }
    else
    {
        AnalogSection p = a;
        bool invert = false;

        if (a.kind == AnalogSection::Kind::General)
        {
            // Map the lower-frequency (or, at equal frequency, the narrower) singularity pair exactly as poles.
            const double wp = std::sqrt (a.d0 / a.d2), zp = a.d1 / (2.0 * std::sqrt (a.d0 * a.d2));
            const double wz = std::sqrt (a.n0 / a.n2), zz = a.n1 / (2.0 * std::sqrt (a.n0 * a.n2));
            invert = (wz < wp * (1.0 - 1e-12)) || (std::abs (wz - wp) <= 1e-12 * wp && zz < zp);
            if (invert)
            {
                std::swap (p.n0, p.d0);
                std::swap (p.n1, p.d1);
                std::swap (p.n2, p.d2);
            }
        }

        const double wp = std::sqrt (p.d0 / p.d2);
        const double zeta = p.d1 / (2.0 * std::sqrt (p.d0 * p.d2));
        if (p.kind == AnalogSection::Kind::AllPass)
        {
            // An all-pass has no magnitude to match; what matters is its phase. Bilinear poles prewarped at
            // f0 put the -180 degree point exactly at f0 at every sample rate.
            s.g = std::tan (0.5 * wp * w0);
            s.k = 2.0 * zeta;
        }
        else
        {
            polesToSvf (wp * w0, zeta, s.g, s.k);
        }

        const double Wn = kPi / w0;
        const AnalogSection unscaled = [&p] { auto q = p; q.scale = 1.0; return q; }();

        switch (p.kind)
        {
            case AnalogSection::Kind::General:
            case AnalogSection::Kind::LowPass:
                matchNumerator (s, unscaled.magnitudeSquared (0.0), unscaled.magnitudeSquared (Wn),
                                w0, unscaled.magnitudeSquared (1.0));
                break;

            case AnalogSection::Kind::HighPass:
            {
                // Double zero at DC; single gain matched at f0 => exact cutoff gain.
                const double um = std::tan (0.5 * w0) / s.g, vm = um * um;
                const double dn = sq (1.0 - vm) + s.k * s.k * vm;
                s.clp = 0.0;
                s.cbp = 0.0;
                s.chp = std::sqrt (unscaled.magnitudeSquared (1.0) * dn) / vm;
                break;
            }

            case AnalogSection::Kind::BandPass:
            {
                s.clp = 0.0;
                s.chp = std::sqrt (unscaled.magnitudeSquared (Wn));
                const double um = std::tan (0.5 * w0) / s.g, vm = um * um;
                const double dn = sq (1.0 - vm) + s.k * s.k * vm;
                const double cbp2 = (unscaled.magnitudeSquared (1.0) * dn - s.chp * s.chp * vm * vm) / vm;
                s.cbp = std::sqrt (std::max (cbp2, 0.0));
                break;
            }

            case AnalogSection::Kind::Notch:
            {
                // Zeros exactly on the unit circle at w0, unity DC gain.
                const double uz = std::tan (0.5 * w0) / s.g;
                s.clp = 1.0;
                s.cbp = 0.0;
                s.chp = 1.0 / (uz * uz);
                break;
            }

            case AnalogSection::Kind::AllPass:
                s.clp = 1.0;
                s.cbp = -s.k;
                s.chp = 1.0;
                break;
        }

        if (invert)
            s = invertSection (s);
    }

    if (a.scale != 1.0)
    {
        s.clp *= a.scale;
        s.cbp *= a.scale;
        s.chp *= a.scale;
    }
    return s;
}

//==============================================================================
AnalogCascade analogPrototype (const DesignSpec& spec, double sampleRate) noexcept
{
    AnalogCascade c;
    c.designFreqHz = effectiveDesignFrequency (spec.freqHz, sampleRate);
    const double gainDb = clampValue (std::isfinite (spec.gainDb) ? spec.gainDb : 0.0, kMinGainDb, kMaxGainDb);
    const double q = clampValue (std::isfinite (spec.q) ? spec.q : 1.0, kMinQ, kMaxQ);

    switch (spec.type)
    {
        case FilterType::Bell:
        {
            const double A = dbToA (gainDb);
            AnalogSection s;
            s.n0 = 1.0; s.n1 = A / q;         s.n2 = 1.0;
            s.d0 = 1.0; s.d1 = 1.0 / (A * q); s.d2 = 1.0;
            c.sections[0] = s;
            c.numSections = 1;
            break;
        }
        case FilterType::Notch:
        {
            AnalogSection s;
            s.kind = AnalogSection::Kind::Notch;
            s.n0 = 1.0; s.n1 = 0.0;     s.n2 = 1.0;
            s.d0 = 1.0; s.d1 = 1.0 / q; s.d2 = 1.0;
            c.sections[0] = s;
            c.numSections = 1;
            break;
        }
        case FilterType::BandPass:
        {
            AnalogSection s;
            s.kind = AnalogSection::Kind::BandPass;
            s.n0 = 0.0; s.n1 = 1.0 / q; s.n2 = 0.0;
            s.d0 = 1.0; s.d1 = 1.0 / q; s.d2 = 1.0;
            c.sections[0] = s;
            c.numSections = 1;
            break;
        }
        case FilterType::AllPass:
        {
            AnalogSection s;
            s.kind = AnalogSection::Kind::AllPass;
            s.n0 = 1.0; s.n1 = -1.0 / q; s.n2 = 1.0;
            s.d0 = 1.0; s.d1 = 1.0 / q;  s.d2 = 1.0;
            c.sections[0] = s;
            c.numSections = 1;
            break;
        }
        case FilterType::HighPass:
        case FilterType::LowPass:
        {
            const bool hp = spec.type == FilterType::HighPass;
            const int order = filterOrderForSlopeIndex (spec.slopeIndex);
            std::array<double, kMaxSections> qs {};
            const int pairs = resonantSectionQs (order, q, kMaxCutFilterQ, qs);
            for (int i = 0; i < pairs; ++i)
            {
                AnalogSection s;
                s.kind = hp ? AnalogSection::Kind::HighPass : AnalogSection::Kind::LowPass;
                s.n0 = hp ? 0.0 : 1.0; s.n1 = 0.0; s.n2 = hp ? 1.0 : 0.0;
                s.d0 = 1.0; s.d1 = 1.0 / qs[(size_t) i]; s.d2 = 1.0;
                c.sections[(size_t) c.numSections++] = s;
            }
            if (order % 2)
            {
                AnalogSection s;
                s.order = 1;
                s.n0 = hp ? 0.0 : 1.0; s.n1 = hp ? 1.0 : 0.0;
                s.d0 = 1.0; s.d1 = 1.0;
                c.sections[(size_t) c.numSections++] = s;
            }
            break;
        }
        case FilterType::LowShelf:
        case FilterType::HighShelf:
        case FilterType::Tilt:
        {
            const int order = filterOrderForSlopeIndex (std::min (spec.slopeIndex, kMaxShelfSlopeIndex));
            addShelfChain (c, spec.type != FilterType::LowShelf, order, gainDb, q);
            if (spec.type == FilterType::Tilt)
                c.sections[0].scale = dbToA (-gainDb); // -G/2 dB: LF = -G/2, HF = +G/2
            break;
        }
        default:
            break;
    }
    return c;
}

CascadeCoeffs designCascade (const DesignSpec& spec, double sampleRate) noexcept
{
    const AnalogCascade ac = analogPrototype (spec, sampleRate);
    const double w0 = kTwoPi * ac.designFreqHz / sampleRate;

    CascadeCoeffs cc;
    cc.numSections = ac.numSections;
    bool ok = true;
    for (int i = 0; i < ac.numSections; ++i)
    {
        cc.sections[(size_t) i] = designSection (ac.sections[(size_t) i], w0);
        ok = ok && cc.sections[(size_t) i].isFiniteAndStable();
    }

    cc.identity = typeUsesGain (spec.type) && std::abs (spec.gainDb) < 1e-9;

    if (cc.identity)
    {
        // Force the mixing gains to their exact identity values so that y = x holds bit-exactly
        // (m0 = chp = 1, m1 = cbp - chp*k = 0, m2 = clp - chp = 0) while the poles keep the values the
        // design converges to as the gain approaches 0 dB (continuity for smoothing).
        for (int i = 0; i < cc.numSections; ++i)
        {
            auto& s = cc.sections[(size_t) i];
            s.clp = 1.0;
            s.chp = 1.0;
            s.cbp = s.order == 2 ? s.k : 0.0;
        }
    }

    if (! ok)
    {
        // Defensive: never hand an invalid filter to the audio path. Fall back to an identity cascade with
        // the same layout (finite, stable). Exercised only by the tests' corrupted-input cases.
        for (int i = 0; i < cc.numSections; ++i)
        {
            auto& s = cc.sections[(size_t) i];
            s.g = std::tan (0.5 * w0);
            s.k = 1.0 / kButterworthQ;
            s.clp = 1.0;
            s.chp = 1.0;
            s.cbp = s.order == 2 ? s.k : 0.0;
        }
        cc.identity = true;
    }
    return cc;
}

std::complex<double> analogPrototypeResponse (const DesignSpec& spec, double sampleRate, double freqHz) noexcept
{
    return analogPrototype (spec, sampleRate).response (freqHz);
}

} // namespace mepro::dsp
