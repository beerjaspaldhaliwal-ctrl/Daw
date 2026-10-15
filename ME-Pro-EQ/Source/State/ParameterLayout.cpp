// ME Pro EQ — ME Audio
#include "ParameterLayout.h"

#include "DSP/DspConfig.h"
#include "DSP/DynamicEQ.h"

#include <cmath>

namespace mepro::params
{

using juce::String;

const char* bandParamSuffix (BandParam p) noexcept
{
    switch (p)
    {
        case BandParam::Used:      return "used";
        case BandParam::On:        return "on";
        case BandParam::Type:      return "type";
        case BandParam::Freq:      return "freq";
        case BandParam::Gain:      return "gain";
        case BandParam::Q:         return "q";
        case BandParam::Slope:     return "slope";
        case BandParam::Stereo:    return "stereo";
        case BandParam::DynOn:     return "dyn";
        case BandParam::Threshold: return "thr";
        case BandParam::Ratio:     return "ratio";
        case BandParam::Attack:    return "att";
        case BandParam::Release:   return "rel";
        case BandParam::Range:     return "range";
        case BandParam::Detector:  return "det";
        case BandParam::Sidechain: return "sc";
        case BandParam::Count:     break;
    }
    return "?";
}

static const char* bandParamLabel (BandParam p) noexcept
{
    switch (p)
    {
        case BandParam::Used:      return "Used";
        case BandParam::On:        return "Enabled";
        case BandParam::Type:      return "Type";
        case BandParam::Freq:      return "Frequency";
        case BandParam::Gain:      return "Gain";
        case BandParam::Q:         return "Q";
        case BandParam::Slope:     return "Slope";
        case BandParam::Stereo:    return "Stereo Mode";
        case BandParam::DynOn:     return "Dynamic";
        case BandParam::Threshold: return "Dyn Threshold";
        case BandParam::Ratio:     return "Dyn Ratio";
        case BandParam::Attack:    return "Dyn Attack";
        case BandParam::Release:   return "Dyn Release";
        case BandParam::Range:     return "Dyn Range";
        case BandParam::Detector:  return "Dyn Detector";
        case BandParam::Sidechain: return "Dyn Sidechain";
        case BandParam::Count:     break;
    }
    return "?";
}

String bandParamId (int band, BandParam p)
{
    return "b" + String (band + 1) + "_" + bandParamSuffix (p);
}

String bandParamName (int band, BandParam p)
{
    return "Band " + String (band + 1) + " " + bandParamLabel (p);
}

float defaultFrequencyForBand (int band) noexcept
{
    // Spread the 16 bands log-uniformly between 40 Hz and 16 kHz.
    return (float) (40.0 * std::pow (400.0, band / 15.0));
}

//==============================================================================
String formatFrequency (double hz, bool withUnit)
{
    if (hz >= 1000.0)
    {
        const double k = hz / 1000.0;
        return String (k, k >= 10.0 ? 2 : 3) + (withUnit ? " kHz" : "k");
    }
    return String (hz, hz >= 100.0 ? 1 : 2) + (withUnit ? " Hz" : "");
}

String formatGain (double db, bool withUnit)
{
    const double r = std::abs (db) < 0.005 ? 0.0 : db;
    return (r > 0 ? "+" : "") + String (r, 2) + (withUnit ? " dB" : "");
}

String formatQ (double q)
{
    return String (q, q >= 10.0 ? 2 : 3);
}

String formatMs (double ms)
{
    return ms >= 1000.0 ? String (ms / 1000.0, 2) + " s" : String (ms, ms < 10.0 ? 2 : 1) + " ms";
}

String formatRatio (double r)
{
    return String (r, r >= 10.0 ? 1 : 2) + ":1";
}

double parseFrequency (const String& text)
{
    auto t = text.trim().toLowerCase().removeCharacters (" ");
    double mult = 1.0;
    if (t.endsWith ("khz"))
    {
        mult = 1000.0;
        t = t.dropLastCharacters (3);
    }
    else if (t.endsWith ("hz"))
    {
        t = t.dropLastCharacters (2);
    }
    else if (t.endsWith ("k"))
    {
        mult = 1000.0;
        t = t.dropLastCharacters (1);
    }
    return t.getDoubleValue() * mult;
}

static double parseNumber (const String& text)
{
    return text.trim().retainCharacters ("0123456789.-+eE").getDoubleValue();
}

//==============================================================================
namespace
{
    juce::NormalisableRange<float> logRange (float lo, float hi)
    {
        const double ratio = (double) hi / lo;
        return juce::NormalisableRange<float> (
            lo, hi,
            [lo, ratio] (float, float, float x) { return (float) (lo * std::pow (ratio, (double) x)); },
            [lo, ratio] (float, float, float v) { return (float) (std::log ((double) v / lo) / std::log (ratio)); },
            [lo, hi] (float, float, float v) { return juce::jlimit (lo, hi, v); });
    }

    using Attr = juce::AudioParameterFloatAttributes;
    using ChoiceAttr = juce::AudioParameterChoiceAttributes;

    std::unique_ptr<juce::AudioParameterFloat> makeFloat (const String& pid, const String& name,
                                                          juce::NormalisableRange<float> range, float def,
                                                          std::function<String (float, int)> toText,
                                                          std::function<float (const String&)> fromText,
                                                          const String& label = {})
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { pid, kParameterVersion }, name, range, def,
            Attr().withLabel (label).withStringFromValueFunction (std::move (toText)).withValueFromStringFunction (std::move (fromText)));
    }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace dsp;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto gainText = [] (float v, int) { return formatGain (v); };
    auto gainParse = [] (const String& s) { return (float) parseNumber (s); };

    layout.add (makeFloat (id::inputTrim, "Input Trim", { (float) EQEngine::kTrimMinDb, (float) EQEngine::kTrimMaxDb }, 0.0f, gainText, gainParse, "dB"));
    layout.add (makeFloat (id::outputTrim, "Output Trim", { (float) EQEngine::kTrimMinDb, (float) EQEngine::kTrimMaxDb }, 0.0f, gainText, gainParse, "dB"));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::bypass, kParameterVersion }, "Bypass", false));

    juce::StringArray typeNames, slopeNames, stereoNames;
    for (int t = 0; t < (int) FilterType::NumTypes; ++t)
        typeNames.add (filterTypeName ((FilterType) t));
    for (int s : kSlopesDbPerOct)
        slopeNames.add (String (s) + " dB/oct");
    for (int m = 0; m < (int) StereoMode::NumModes; ++m)
        stereoNames.add (stereoModeName ((StereoMode) m));

    for (int b = 0; b < kNumBands; ++b)
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("band" + String (b + 1), "Band " + String (b + 1), "|");
        auto pid = [b] (BandParam p) { return juce::ParameterID { bandParamId (b, p), kParameterVersion }; };
        auto nm = [b] (BandParam p) { return bandParamName (b, p); };

        group->addChild (std::make_unique<juce::AudioParameterBool> (pid (BandParam::Used), nm (BandParam::Used), false));
        group->addChild (std::make_unique<juce::AudioParameterBool> (pid (BandParam::On), nm (BandParam::On), true));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (pid (BandParam::Type), nm (BandParam::Type), typeNames, 0));
        group->addChild (makeFloat (bandParamId (b, BandParam::Freq), nm (BandParam::Freq), logRange ((float) kMinFreqHz, (float) kMaxFreqHz),
                                    defaultFrequencyForBand (b), [] (float v, int) { return formatFrequency (v); },
                                    [] (const String& s) { return (float) parseFrequency (s); }, "Hz"));
        group->addChild (makeFloat (bandParamId (b, BandParam::Gain), nm (BandParam::Gain), { (float) kMinGainDb, (float) kMaxGainDb }, 0.0f,
                                    gainText, gainParse, "dB"));
        group->addChild (makeFloat (bandParamId (b, BandParam::Q), nm (BandParam::Q), logRange ((float) kMinQ, (float) kMaxQ), 1.0f,
                                    [] (float v, int) { return formatQ (v); }, [] (const String& s) { return (float) parseNumber (s); }));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (pid (BandParam::Slope), nm (BandParam::Slope), slopeNames, kDefaultSlopeIndex));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (pid (BandParam::Stereo), nm (BandParam::Stereo), stereoNames, 0));
        group->addChild (std::make_unique<juce::AudioParameterBool> (pid (BandParam::DynOn), nm (BandParam::DynOn), false));
        group->addChild (makeFloat (bandParamId (b, BandParam::Threshold), nm (BandParam::Threshold),
                                    { (float) kMinThresholdDb, (float) kMaxThresholdDb }, -24.0f,
                                    [] (float v, int) { return String (v, 1) + " dBFS"; }, gainParse, "dBFS"));
        group->addChild (makeFloat (bandParamId (b, BandParam::Ratio), nm (BandParam::Ratio), logRange ((float) kMinRatio, (float) kMaxRatio), 2.0f,
                                    [] (float v, int) { return formatRatio (v); }, [] (const String& s) { return (float) parseNumber (s); }));
        group->addChild (makeFloat (bandParamId (b, BandParam::Attack), nm (BandParam::Attack), logRange ((float) kMinAttackMs, (float) kMaxAttackMs), 10.0f,
                                    [] (float v, int) { return formatMs (v); }, [] (const String& s) { return (float) parseNumber (s); }, "ms"));
        group->addChild (makeFloat (bandParamId (b, BandParam::Release), nm (BandParam::Release), logRange ((float) kMinReleaseMs, (float) kMaxReleaseMs), 150.0f,
                                    [] (float v, int) { return formatMs (v); }, [] (const String& s) { return (float) parseNumber (s); }, "ms"));
        group->addChild (makeFloat (bandParamId (b, BandParam::Range), nm (BandParam::Range), { (float) kMinRangeDb, (float) kMaxRangeDb }, -6.0f,
                                    gainText, gainParse, "dB"));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (pid (BandParam::Detector), nm (BandParam::Detector), juce::StringArray { "Peak", "RMS" }, 0));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (pid (BandParam::Sidechain), nm (BandParam::Sidechain), juce::StringArray { "Internal", "External" }, 0));
        layout.add (std::move (group));
    }
    return layout;
}

//==============================================================================
void ParameterReader::init (juce::AudioProcessorValueTreeState& state)
{
    inTrim = state.getRawParameterValue (id::inputTrim);
    outTrim = state.getRawParameterValue (id::outputTrim);
    bypassRaw = state.getRawParameterValue (id::bypass);
    for (int b = 0; b < dsp::kNumBands; ++b)
        for (int p = 0; p < kNumBandParams; ++p)
        {
            band_[(size_t) b][(size_t) p] = state.getRawParameterValue (bandParamId (b, (BandParam) p));
            jassert (band_[(size_t) b][(size_t) p] != nullptr);
        }
    jassert (inTrim && outTrim && bypassRaw);
}

void ParameterReader::read (dsp::EngineSettings& out) const noexcept
{
    using namespace dsp;
    out.inputTrimDb = inTrim->load (std::memory_order_relaxed);
    out.outputTrimDb = outTrim->load (std::memory_order_relaxed);
    out.bypass = bypassRaw->load (std::memory_order_relaxed) > 0.5f;
    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& p = band_[(size_t) b];
        auto v = [&p] (BandParam x) { return p[(size_t) x]->load (std::memory_order_relaxed); };
        auto& s = out.bands[(size_t) b];
        s.active = v (BandParam::Used) > 0.5f && v (BandParam::On) > 0.5f;
        s.type = (FilterType) juce::jlimit (0, (int) FilterType::NumTypes - 1, (int) std::lround (v (BandParam::Type)));
        s.freqHz = v (BandParam::Freq);
        s.gainDb = v (BandParam::Gain);
        s.q = v (BandParam::Q);
        s.slopeIndex = juce::jlimit (0, (int) kSlopesDbPerOct.size() - 1, (int) std::lround (v (BandParam::Slope)));
        s.stereo = (StereoMode) juce::jlimit (0, (int) StereoMode::NumModes - 1, (int) std::lround (v (BandParam::Stereo)));
        s.dyn.enabled = v (BandParam::DynOn) > 0.5f;
        s.dyn.thresholdDb = v (BandParam::Threshold);
        s.dyn.ratio = v (BandParam::Ratio);
        s.dyn.attackMs = v (BandParam::Attack);
        s.dyn.releaseMs = v (BandParam::Release);
        s.dyn.rangeDb = v (BandParam::Range);
        s.dyn.detector = v (BandParam::Detector) > 0.5f ? DetectorMode::Rms : DetectorMode::Peak;
        s.dyn.source = v (BandParam::Sidechain) > 0.5f ? SidechainSource::External : SidechainSource::Internal;
    }
}

} // namespace mepro::params
