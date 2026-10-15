// ME Pro EQ — ME Audio
// Host-visible parameter definitions. Parameter IDs are part of the saved-project format and of host
// automation data: they must never be renamed or re-used. New parameters may be added (with defaults).
#pragma once

#include "DSP/EQEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace mepro::params
{

inline constexpr int kParameterVersion = 1;

namespace id
{
    inline constexpr const char* inputTrim = "inGain";
    inline constexpr const char* outputTrim = "outGain";
    inline constexpr const char* bypass = "bypass";
    inline constexpr const char* autoGain = "autoGain";
} // namespace id

enum class BandParam : int
{
    Used = 0,
    On,
    Type,
    Freq,
    Gain,
    Q,
    Slope,
    Stereo,
    DynOn,
    Threshold,
    Ratio,
    Attack,
    Release,
    Range,
    Detector,
    Sidechain,
    Count
};

inline constexpr int kNumBandParams = (int) BandParam::Count;

const char* bandParamSuffix (BandParam p) noexcept;
juce::String bandParamId (int band, BandParam p);   // band is 0-based; IDs are "b1_freq" ... "b16_sc"
juce::String bandParamName (int band, BandParam p); // host-visible name, e.g. "Band 3 Frequency"

float defaultFrequencyForBand (int band) noexcept;

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Text formatting shared by the parameters and the editor.
juce::String formatFrequency (double hz, bool withUnit = true);
juce::String formatGain (double db, bool withUnit = true);
juce::String formatQ (double q);
juce::String formatMs (double ms);
juce::String formatRatio (double r);
double parseFrequency (const juce::String& text); // accepts "1k", "1.2 kHz", "440 Hz", "440"

// Cached raw-value pointers for lock-free reads on the audio thread.
class ParameterReader
{
public:
    void init (juce::AudioProcessorValueTreeState& state);
    void read (dsp::EngineSettings& out) const noexcept;
    bool bandUsed (int band) const noexcept { return band_[(size_t) band][(size_t) BandParam::Used]->load() > 0.5f; }

private:
    std::atomic<float>* inTrim = nullptr;
    std::atomic<float>* outTrim = nullptr;
    std::atomic<float>* bypassRaw = nullptr;
    std::atomic<float>* autoGainRaw = nullptr;
    std::array<std::array<std::atomic<float>*, kNumBandParams>, dsp::kNumBands> band_ {};
};

} // namespace mepro::params
