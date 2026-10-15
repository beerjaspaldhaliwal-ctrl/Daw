// ME Pro EQ — ME Audio
// Filter design: analog prototypes -> "analog-matched" digital sections realised as TPT state-variable filters.
//
// Method (see Documentation/DSP_Design.md for derivation and measured accuracy):
//   * Every band is a cascade of first/second-order analog prototype sections (the "documented design").
//   * Poles of the lower-frequency singularity pair are mapped exactly with the matched-Z (impulse
//     invariant) mapping z = exp(sT) — no bilinear frequency warping/cramping near Nyquist.
//   * The remaining numerator is chosen so the digital magnitude equals the analog magnitude exactly at
//     DC, at the design frequency f0 and at Nyquist (three-point magnitude match, after M. Vicanek,
//     "Matched Second Order Digital Filters", 2016). For cuts (or high shelves) the section is designed
//     as the exact inverse of the corresponding boost, which keeps every design realizable, stable and
//     minimum-phase and makes boost/cut responses exact mirror images in dB.
//   * All quantities are computed in the SVF parameter domain (g, k, mixing gains) using expm1-based
//     formulas, which stay well-conditioned down to 10 Hz at 192 kHz.
#pragma once

#include "DspConfig.h"

#include <array>
#include <complex>

namespace mepro::dsp
{

// One section of a cascade, realised as a trapezoidal-integrator state-variable filter with output mixing.
//   second order:  H(w) = (clp - chp*u^2 + j*cbp*u) / (1 - u^2 + j*k*u),   u = tan(w/2) / g
//   first order:   H(w) = (clp + j*chp*u) / (1 + j*u)
// clp = DC gain, chp = Nyquist gain. These expressions are the *exact* digital transfer functions.
struct SectionCoeffs
{
    int order = 2;
    double g = 1.0;
    double k = 1.0;
    double clp = 1.0;
    double cbp = 1.0;
    double chp = 1.0;

    std::complex<double> response (double omega) const noexcept;
    bool isFiniteAndStable() const noexcept;
};

struct CascadeCoeffs
{
    int numSections = 0;
    std::array<SectionCoeffs, kMaxSections> sections {};
    bool identity = true; // response is exactly unity; the processor passes audio through bit-exactly

    std::complex<double> response (double omega) const noexcept;
    double magnitudeDb (double omega) const noexcept;
};

struct DesignSpec
{
    FilterType type = FilterType::Bell;
    double freqHz = 1000.0;
    double gainDb = 0.0;
    double q = 1.0;
    int slopeIndex = kDefaultSlopeIndex;
};

// Analog prototype section in normalised s (s = j f / f0):
//   order 2: (n0 + n1 s + n2 s^2) / (d0 + d1 s + d2 s^2)       order 1: (n0 + n1 s) / (d0 + d1 s)
struct AnalogSection
{
    enum class Kind { General, LowPass, HighPass, BandPass, Notch, AllPass };
    int order = 2;
    Kind kind = Kind::General;
    double n0 = 1, n1 = 0, n2 = 0, d0 = 1, d1 = 0, d2 = 0;
    double scale = 1.0; // extra scalar gain (used for the tilt's -G/2 offset)

    std::complex<double> response (double normalisedFreq) const noexcept;
    double magnitudeSquared (double normalisedFreq) const noexcept;
};

struct AnalogCascade
{
    int numSections = 0;
    std::array<AnalogSection, kMaxSections> sections {};
    double designFreqHz = 1000.0;

    std::complex<double> response (double freqHz) const noexcept;
};

// Number of sections and the order of each, for a given type/slope. Two specs with identical layout can be
// interpolated coefficient-by-coefficient; different layouts require a crossfade.
struct SectionLayout
{
    int numSections = 0;
    std::array<int, kMaxSections> orders {};
    bool operator== (const SectionLayout&) const = default;
};

SectionLayout sectionLayout (FilterType type, int slopeIndex) noexcept;

double effectiveDesignFrequency (double freqHz, double sampleRate) noexcept;

AnalogCascade analogPrototype (const DesignSpec& spec, double sampleRate) noexcept;
CascadeCoeffs designCascade (const DesignSpec& spec, double sampleRate) noexcept;

// Convenience: exact analog (s-domain) response of the documented design at freqHz.
std::complex<double> analogPrototypeResponse (const DesignSpec& spec, double sampleRate, double freqHz) noexcept;

// Section-level building blocks (exposed for the dynamic-EQ detector and the tests).
SectionCoeffs designSection (const AnalogSection& section, double w0) noexcept;
SectionCoeffs invertSection (const SectionCoeffs& s) noexcept;
void polesToSvf (double poleW, double zeta, double& g, double& k) noexcept;

// Butterworth section Q values for an order-N filter (N/2 values, highest Q first).
int butterworthQs (int order, std::array<double, kMaxSections>& qs) noexcept;

} // namespace mepro::dsp
