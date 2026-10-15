// ME Pro EQ — ME Audio
// Top bar: identity, global bypass, mono-compatibility audition and engine status.
#pragma once

#include "EditorModel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace mepro::ui
{

class HeaderBar final : public juce::Component
{
public:
    explicit HeaderBar (EditorModel& m);
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

    // Area in the middle reserved for preset / A-B controls (filled by the editor).
    juce::Rectangle<int> getCentreArea() const noexcept { return centreArea; }

private:
    EditorModel& model;
    juce::TextButton bypassButton { "BYPASS" }, monoButton { "MONO" };
    std::unique_ptr<juce::ButtonParameterAttachment> bypassAtt;
    juce::Label status;
    juce::Rectangle<int> centreArea;
};

} // namespace mepro::ui
