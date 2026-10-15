// ME Pro EQ — ME Audio
// Global DSP constants and enumerations shared by the engine, the plugin layer and the tests.
// This header (and everything in Source/DSP) is framework-independent C++20: no JUCE dependency.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace mepro::dsp
{

inline constexpr int kNumBands = 16;
inline constexpr int kMaxSections = 8;      // 96 dB/oct Butterworth = order 16 = 8 second-order sections
inline constexpr int kMaxChannels = 2;

inline constexpr double kMinFreqHz = 10.0;
inline constexpr double kMaxFreqHz = 30000.0;
inline constexpr double kMinGainDb = -30.0;
inline constexpr double kMaxGainDb = 30.0;
inline constexpr double kMinQ = 0.1;
inline constexpr double kMaxQ = 30.0;

// Filters are designed at min(f, kMaxDesignFreqRatio * fs). Above this ratio the matched second-order
// design is no longer guaranteed to be realizable (measured: Documentation/DSP_Design.md).
inline constexpr double kMaxDesignFreqRatio = 0.49;

// Resonance limit for cut filters (HP/LP) — protects monitoring from extreme resonant peaks.
inline constexpr double kMaxCutFilterQ = 10.0;
// Shelf resonance limit — beyond this the "shelf" becomes a resonant peak/dip pair and the analog match degrades.
inline constexpr double kMaxShelfQ = 4.0;

inline constexpr double kButterworthQ = 0.70710678118654752440;

enum class FilterType : int
{
    Bell = 0,
    LowShelf,
    HighShelf,
    HighPass,
    LowPass,
    Notch,
    BandPass,
    Tilt,
    AllPass,
    NumTypes
};

enum class StereoMode : int
{
    Stereo = 0, // both channels, identical coefficients, linked dynamics
    Left,
    Right,
    Mid,
    Side,
    NumModes
};

enum class DetectorMode : int
{
    Peak = 0,
    Rms,
    NumModes
};

enum class SidechainSource : int
{
    Internal = 0,
    External,
    NumSources
};

// Slope options (dB/octave). Index is what is stored in the parameter.
inline constexpr std::array<int, 8> kSlopesDbPerOct { 6, 12, 18, 24, 36, 48, 72, 96 };
inline constexpr int kDefaultSlopeIndex = 1; // 12 dB/oct

// Shelf/tilt slopes are limited to 48 dB/oct (order 8 = 4 sections).
inline constexpr int kMaxShelfSlopeIndex = 5;

inline constexpr int filterOrderForSlopeIndex (int slopeIndex) noexcept
{
    if (slopeIndex < 0) slopeIndex = 0;
    if (slopeIndex >= (int) kSlopesDbPerOct.size()) slopeIndex = (int) kSlopesDbPerOct.size() - 1;
    return kSlopesDbPerOct[(std::size_t) slopeIndex] / 6;
}

inline constexpr bool typeUsesGain (FilterType t) noexcept
{
    return t == FilterType::Bell || t == FilterType::LowShelf || t == FilterType::HighShelf || t == FilterType::Tilt;
}

inline constexpr bool typeUsesSlope (FilterType t) noexcept
{
    return t == FilterType::HighPass || t == FilterType::LowPass || t == FilterType::LowShelf
        || t == FilterType::HighShelf || t == FilterType::Tilt;
}

inline constexpr bool typeUsesQ (FilterType t, int slopeIndex) noexcept
{
    // First-order (6 dB/oct) shelves/tilts/cut filters have no resonance parameter.
    if (typeUsesSlope (t) && filterOrderForSlopeIndex (slopeIndex) == 1)
        return false;
    return true;
}

inline constexpr bool typeSupportsDynamics (FilterType t) noexcept
{
    return t == FilterType::Bell || t == FilterType::LowShelf || t == FilterType::HighShelf;
}

inline constexpr const char* filterTypeName (FilterType t) noexcept
{
    switch (t)
    {
        case FilterType::Bell:      return "Bell";
        case FilterType::LowShelf:  return "Low Shelf";
        case FilterType::HighShelf: return "High Shelf";
        case FilterType::HighPass:  return "High Pass";
        case FilterType::LowPass:   return "Low Pass";
        case FilterType::Notch:     return "Notch";
        case FilterType::BandPass:  return "Band Pass";
        case FilterType::Tilt:      return "Tilt";
        case FilterType::AllPass:   return "All Pass";
        case FilterType::NumTypes:  break;
    }
    return "?";
}

inline constexpr const char* stereoModeName (StereoMode m) noexcept
{
    switch (m)
    {
        case StereoMode::Stereo: return "Stereo";
        case StereoMode::Left:   return "Left";
        case StereoMode::Right:  return "Right";
        case StereoMode::Mid:    return "Mid";
        case StereoMode::Side:   return "Side";
        case StereoMode::NumModes: break;
    }
    return "?";
}

} // namespace mepro::dsp
