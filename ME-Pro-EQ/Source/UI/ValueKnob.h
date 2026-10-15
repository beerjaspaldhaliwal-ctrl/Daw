// ME Pro EQ — ME Audio
// Rotary value control bound to a host parameter.
//   drag (vertical or horizontal)  : adjust          Shift + drag / wheel : fine adjust (x0.1)
//   double-click                   : type a value    Ctrl/Cmd/Alt + click : reset to default
//   arrow keys (when focused)      : step            mouse wheel          : adjust
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace mepro::ui
{

class ValueKnob final : public juce::Component
{
public:
    explicit ValueKnob (juce::String labelText);
    ~ValueKnob() override;

    void attach (juce::RangedAudioParameter* parameter);
    void setColour (juce::Colour c) { accent = c; repaint(); }
    void setBipolar (bool b) { bipolar = b; repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    juce::RangedAudioParameter* getParameter() const noexcept { return param; }
    float getValue() const noexcept { return value; }
    void setNormalised (float norm, bool asGesture);

private:
    void showEditor();
    void commitEditor();
    juce::Rectangle<float> knobArea() const;

    juce::String label;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float value = 0.0f;
    float dragStartNorm = 0.0f;
    juce::Point<float> dragStart;
    bool dragging = false;
    bool dragFine = false;
    bool bipolar = false;
    juce::Colour accent { 0xfff2b544 };
    std::unique_ptr<juce::TextEditor> editor;
};

} // namespace mepro::ui
