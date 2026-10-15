// ME Pro EQ — ME Audio
// Lower panel: band selector chips and the detailed controls of the selected band.
#pragma once

#include "Bindings.h"
#include "EditorModel.h"
#include "ValueKnob.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace mepro::ui
{

class BandChip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    BandChip (EditorModel& m, int band);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    EditorModel& model;
    int band;
};

// Shows the gain change the dynamic stage is applying right now (read back from the audio engine).
class DynamicGainMeter final : public juce::Component
{
public:
    void setValue (float gainDb, float rangeDb, bool active);
    void paint (juce::Graphics&) override;

private:
    float gain = 0.0f, range = -6.0f;
    bool active = false;
};

class BandPanel final : public juce::Component, private EditorModel::Listener
{
public:
    explicit BandPanel (EditorModel& m);
    ~BandPanel() override;

    void refresh(); // UI timer
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void selectedBandChanged (int) override { rebind(); }
    void rebind();
    void updateEnablement();
    static void setupCombo (juce::ComboBox& box, const juce::StringArray& items, const juce::String& title);

    EditorModel& model;
    std::array<std::unique_ptr<BandChip>, 16> chips;

    juce::TextButton powerButton { "ON" }, soloButton { "SOLO" }, resetButton { "RESET" }, deleteButton { "DELETE" };
    juce::TextButton dynButton { "DYNAMIC" };
    juce::ComboBox typeBox, slopeBox, stereoBox, detBox, scBox;
    ValueKnob freqKnob { "Freq" }, gainKnob { "Gain" }, qKnob { "Q" };
    ValueKnob thrKnob { "Threshold" }, ratioKnob { "Ratio" }, attKnob { "Attack" }, relKnob { "Release" }, rangeKnob { "Range" };
    DynamicGainMeter dynMeter;
    juce::Label title, hint, dynNote;

    std::unique_ptr<ComboBinding> typeAtt, slopeAtt, stereoAtt, detAtt, scAtt;
    std::unique_ptr<ToggleBinding> powerAtt, dynAtt;
    int boundBand = -2;
    juce::Rectangle<int> dynArea, mainArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandPanel)
};

} // namespace mepro::ui
