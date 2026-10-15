// ME Pro EQ — ME Audio
#pragma once

#include "PluginProcessor.h"
#include "UI/BandControls.h"
#include "UI/EQGraph.h"
#include "UI/EditorModel.h"
#include "UI/HeaderBar.h"
#include "UI/LookAndFeel.h"
#include "UI/Metering.h"

#include <juce_audio_processors/juce_audio_processors.h>

class MEProEQAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int kDefaultWidth = 1220, kDefaultHeight = 760;
    static constexpr int kMinWidth = 980, kMinHeight = 620, kMaxWidth = 3200, kMaxHeight = 2000;

    explicit MEProEQAudioProcessorEditor (MEProEQAudioProcessor&);
    ~MEProEQAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Runs one UI refresh cycle immediately (used by the UI timer and by the tests).
    void refreshNow (double dt);

private:
    void timerCallback() override;

    MEProEQAudioProcessor& processor;
    mepro::ui::MEProLookAndFeel lookAndFeel;
    mepro::ui::EditorModel model;
    mepro::ui::HeaderBar header;
    mepro::ui::EQGraph graph;
    mepro::ui::BandPanel bandPanel;
    mepro::ui::MeterStrip meters;
    juce::TooltipWindow tooltips { this, 700 };
    double lastTick = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MEProEQAudioProcessorEditor)
};
