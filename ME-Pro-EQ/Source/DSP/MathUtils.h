// ME Pro EQ — ME Audio
// Small numeric helpers used across the DSP core.
#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace mepro::dsp
{

inline constexpr double kPi = std::numbers::pi_v<double>;
inline constexpr double kTwoPi = 2.0 * std::numbers::pi_v<double>;

inline double dbToGain (double db) noexcept { return std::pow (10.0, db * 0.05); }
inline double dbToPower (double db) noexcept { return std::pow (10.0, db * 0.1); }

inline double gainToDb (double g, double floorDb = -300.0) noexcept
{
    return g > 0.0 ? std::max (20.0 * std::log10 (g), floorDb) : floorDb;
}

inline double powerToDb (double p, double floorDb = -300.0) noexcept
{
    return p > 0.0 ? std::max (10.0 * std::log10 (p), floorDb) : floorDb;
}

template <typename T>
constexpr T clampValue (T v, T lo, T hi) noexcept
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// Coefficient of a one-pole smoother reaching 1 - 1/e after `timeSeconds` when updated at `updateRateHz`.
inline double onePoleCoefficient (double timeSeconds, double updateRateHz) noexcept
{
    if (timeSeconds <= 0.0 || updateRateHz <= 0.0)
        return 0.0;
    return std::exp (-1.0 / (timeSeconds * updateRateHz));
}

inline bool isFiniteValue (double x) noexcept { return std::isfinite (x); }

// Exact (bitwise) equality for change detection of cached values; avoids -Wfloat-equal noise while making the
// intent explicit: any change, however small, counts.
inline bool sameValue (double a, double b) noexcept
{
    return ! (a < b) && ! (b < a);
}

} // namespace mepro::dsp
