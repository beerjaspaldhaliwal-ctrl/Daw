// ME Pro EQ — ME Audio
#include "EditorModel.h"
#include "PluginProcessor.h"

namespace mepro::ui
{

EditorModel::EditorModel (MEProEQAudioProcessor& p)
    : processor (p), state (p.getValueTreeState())
{
    selected = (int) processor.getUiState().getProperty ("selectedBand", -1);
    if (selected < -1 || selected >= dsp::kNumBands)
        selected = -1;
}

juce::RangedAudioParameter* EditorModel::param (int band, BandParam p) const
{
    return state.getParameter (params::bandParamId (band, p));
}

juce::RangedAudioParameter* EditorModel::globalParam (const char* id) const
{
    return state.getParameter (id);
}

float EditorModel::get (int band, BandParam p) const
{
    return state.getRawParameterValue (params::bandParamId (band, p))->load();
}

dsp::FilterType EditorModel::type (int band) const
{
    return (dsp::FilterType) juce::jlimit (0, (int) dsp::FilterType::NumTypes - 1, juce::roundToInt (get (band, BandParam::Type)));
}

dsp::StereoMode EditorModel::stereo (int band) const
{
    return (dsp::StereoMode) juce::jlimit (0, (int) dsp::StereoMode::NumModes - 1, juce::roundToInt (get (band, BandParam::Stereo)));
}

int EditorModel::slope (int band) const
{
    return juce::jlimit (0, (int) dsp::kSlopesDbPerOct.size() - 1, juce::roundToInt (get (band, BandParam::Slope)));
}

dsp::DesignSpec EditorModel::spec (int band) const
{
    return { type (band), get (band, BandParam::Freq), get (band, BandParam::Gain), get (band, BandParam::Q), slope (band) };
}

void EditorModel::set (int band, BandParam p, float v)
{
    if (auto* rp = param (band, p))
    {
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (v));
        rp->endChangeGesture();
    }
}

void EditorModel::beginGesture (int band, BandParam p)
{
    if (auto* rp = param (band, p))
        rp->beginChangeGesture();
}

void EditorModel::setDuringGesture (int band, BandParam p, float v)
{
    if (auto* rp = param (band, p))
        rp->setValueNotifyingHost (rp->convertTo0to1 (v));
}

void EditorModel::endGesture (int band, BandParam p)
{
    if (auto* rp = param (band, p))
        rp->endChangeGesture();
}

void EditorModel::setSelectedBand (int band)
{
    band = juce::jlimit (-1, dsp::kNumBands - 1, band);
    if (band == selected)
        return;
    selected = band;
    processor.getUiState().setProperty ("selectedBand", band, nullptr);
    listeners.call ([band] (Listener& l) { l.selectedBandChanged (band); });
}

void EditorModel::resetBand (int band)
{
    for (int p = 0; p < params::kNumBandParams; ++p)
    {
        const auto bp = (BandParam) p;
        if (bp == BandParam::Used)
            continue;
        if (auto* rp = param (band, bp))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
    }
}

int EditorModel::createBand (double freqHz, double gainDb, dsp::FilterType t)
{
    for (int b = 0; b < dsp::kNumBands; ++b)
    {
        if (isUsed (b))
            continue;
        resetBand (b);
        set (b, BandParam::Type, (float) t);
        set (b, BandParam::Freq, (float) juce::jlimit (dsp::kMinFreqHz, dsp::kMaxFreqHz, freqHz));
        set (b, BandParam::Gain, dsp::typeUsesGain (t) ? (float) juce::jlimit (dsp::kMinGainDb, dsp::kMaxGainDb, gainDb) : 0.0f);
        set (b, BandParam::On, 1.0f);
        set (b, BandParam::Used, 1.0f); // last: the band becomes audible with all settings in place
        setSelectedBand (b);
        return b;
    }
    return -1;
}

void EditorModel::deleteBand (int band)
{
    if (band < 0)
        return;
    set (band, BandParam::Used, 0.0f);
    if (processor.getAuditionBand() == band)
        processor.setAuditionBand (-1);
    if (selected == band)
    {
        int next = -1;
        for (int b = 0; b < dsp::kNumBands && next < 0; ++b)
            if (b != band && isUsed (b))
                next = b;
        setSelectedBand (next);
    }
}

void EditorModel::toggleBand (int band)
{
    set (band, BandParam::On, isOn (band) ? 0.0f : 1.0f);
}

} // namespace mepro::ui
