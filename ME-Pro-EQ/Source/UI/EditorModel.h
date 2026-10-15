// ME Pro EQ — ME Audio
// Editor-side access to the band parameters: reading real-unit values, host-gesture-correct writes,
// band selection and band creation/deletion. Message thread only.
//
// Every write made through the model is one undoable action (a whole drag is one action; composite
// operations such as "create band" or "reset band" are one action as well).
#pragma once

#include "DSP/FilterDesign.h"
#include "State/ParameterLayout.h"
#include "State/UndoHistory.h"

#include <juce_audio_processors/juce_audio_processors.h>

class MEProEQAudioProcessor;

namespace mepro::ui
{

using params::BandParam;

class EditorModel
{
public:
    explicit EditorModel (MEProEQAudioProcessor& p);

    MEProEQAudioProcessor& getProcessor() noexcept { return processor; }
    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    state::UndoHistory& getUndo() noexcept;

    juce::RangedAudioParameter* param (int band, BandParam p) const;
    juce::RangedAudioParameter* globalParam (const char* id) const;

    float get (int band, BandParam p) const;
    bool isUsed (int band) const { return get (band, BandParam::Used) > 0.5f; }
    bool isOn (int band) const { return get (band, BandParam::On) > 0.5f; }
    bool isActive (int band) const { return isUsed (band) && isOn (band); }
    dsp::FilterType type (int band) const;
    dsp::StereoMode stereo (int band) const;
    int slope (int band) const;
    dsp::DesignSpec spec (int band) const;

    // One-off change wrapped in a complete host gesture.
    void set (int band, BandParam p, float realValue);
    // Continuous interaction.
    void beginGesture (int band, BandParam p);
    void setDuringGesture (int band, BandParam p, float realValue);
    void endGesture (int band, BandParam p);

    int getSelectedBand() const noexcept { return selected; }
    void setSelectedBand (int band);

    // Creates a band in the first unused slot (returns its index, or -1 if all 16 are in use).
    int createBand (double freqHz, double gainDb, dsp::FilterType type);
    void deleteBand (int band);
    void resetBand (int band); // restore every setting except existence to its default
    void resetBandKeepFrequency (int band);
    void toggleBand (int band);

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void selectedBandChanged (int newBand) = 0;
    };
    void addListener (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

private:
    MEProEQAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;
    int selected = -1;
    juce::ListenerList<Listener> listeners;
};

} // namespace mepro::ui
