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

state::UndoHistory& EditorModel::getUndo() noexcept
{
    return processor.getUndoHistory();
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
    state::UndoHistory::Scope action (getUndo());
    if (auto* rp = param (band, p))
    {
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (v));
        rp->endChangeGesture();
    }
}

void EditorModel::beginGesture (int band, BandParam p)
{
    getUndo().begin();
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
    getUndo().end();
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
    state::UndoHistory::Scope action (getUndo());
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

void EditorModel::resetBandKeepFrequency (int band)
{
    if (band < 0)
        return;
    state::UndoHistory::Scope action (getUndo());
    const auto f = get (band, BandParam::Freq);
    resetBand (band);
    set (band, BandParam::Freq, f);
}

int EditorModel::createBand (double freqHz, double gainDb, dsp::FilterType t)
{
    state::UndoHistory::Scope action (getUndo());
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

int EditorModel::createBandFrom (const dsp::BandSettings& s)
{
    state::UndoHistory::Scope action (getUndo());
    for (int b = 0; b < dsp::kNumBands; ++b)
    {
        if (isUsed (b))
            continue;
        resetBand (b);
        set (b, BandParam::Type, (float) s.type);
        set (b, BandParam::Freq, (float) juce::jlimit (dsp::kMinFreqHz, dsp::kMaxFreqHz, s.freqHz));
        set (b, BandParam::Gain, (float) juce::jlimit (dsp::kMinGainDb, dsp::kMaxGainDb, s.gainDb));
        set (b, BandParam::Q, (float) juce::jlimit (dsp::kMinQ, dsp::kMaxQ, s.q));
        set (b, BandParam::Slope, (float) s.slopeIndex);
        set (b, BandParam::Stereo, (float) s.stereo);
        set (b, BandParam::DynOn, s.dyn.enabled ? 1.0f : 0.0f);
        if (s.dyn.enabled)
        {
            set (b, BandParam::Threshold, (float) s.dyn.thresholdDb);
            set (b, BandParam::Ratio, (float) s.dyn.ratio);
            set (b, BandParam::Attack, (float) s.dyn.attackMs);
            set (b, BandParam::Release, (float) s.dyn.releaseMs);
            set (b, BandParam::Range, (float) s.dyn.rangeDb);
            set (b, BandParam::Detector, (float) s.dyn.detector);
            set (b, BandParam::Sidechain, (float) s.dyn.source);
        }
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
    set (band, BandParam::Used, 0.0f); // one action; the band's settings stay in place for undo
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
