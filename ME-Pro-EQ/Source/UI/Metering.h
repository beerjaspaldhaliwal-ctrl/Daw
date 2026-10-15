// ME Pro EQ — ME Audio
// Input/output sample-peak meters with peak hold, clip indicator, trims, level comparison and engine status.
#pragma once

#include "EditorModel.h"
#include "ValueKnob.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace mepro::ui
{

class LevelMeter final : public juce::Component
{
public:
    static constexpr float kMinDb = -60.0f, kMaxDb = 6.0f;

    explicit LevelMeter (juce::String name);
    // Linear sample peaks measured since the previous call; dt = seconds since the previous call.
    void push (float peakL, float peakR, int numChannels, double dt);
    void paint (juce::Graphics&) override;
    float getHeldDb() const noexcept { return juce::jmax (hold[0], hold[1]); }

private:
    float dbToY (float db, juce::Rectangle<float> r) const;
    juce::String label;
    float level[2] { -100.0f, -100.0f };
    float hold[2] { -100.0f, -100.0f };
    double holdAge[2] { 0.0, 0.0 };
    int channels = 2;
};

class MeterStrip final : public juce::Component
{
public:
    explicit MeterStrip (EditorModel& m);
    void refresh (double dt);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EditorModel& model;
    LevelMeter inMeter { "IN" }, outMeter { "OUT" };
    ValueKnob inTrim { "In Trim" }, outTrim { "Out Trim" };
    juce::TextButton clipButton { "CLIP" }, matchButton { "MATCH" };
    juce::Label inPeak, outPeak, levelTitle, levelDiff, status;
    float lastDiffDb = 0.0f;
    bool diffValid = false;

public:
    // Sets the output trim so that the output RMS (3 s) matches the input RMS. One undoable action.
    // Returns false when there is no usable level reading (silence).
    bool matchOutputLevel();
};

} // namespace mepro::ui
