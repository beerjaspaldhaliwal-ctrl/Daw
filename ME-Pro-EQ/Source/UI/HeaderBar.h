// ME Pro EQ — ME Audio
// Top bar: identity, undo/redo, preset browser, A/B comparison, auto gain, mono check and global bypass.
#pragma once

#include "Bindings.h"
#include "EditorModel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace mepro::ui
{

class HeaderBar final : public juce::Component
{
public:
    explicit HeaderBar (EditorModel& m);
    ~HeaderBar() override;

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

    // Keyboard shortcuts routed from the editor (Ctrl+Z, Ctrl+Shift+Z / Ctrl+Y).
    bool performUndo();
    bool performRedo();

    // Exposed for the editor tests.
    void showPresetMenu();
    void stepPreset (int direction);
    void selectSlot (bool slotB);
    void copySlot();

private:
    void openSaveDialog();
    void updatePresetLabel();

    EditorModel& model;
    juce::TextButton undoButton { "UNDO" }, redoButton { "REDO" };
    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, presetName, saveButton { "SAVE" };
    juce::TextButton slotA { "A" }, slotB { "B" }, copyButton { "COPY" };
    juce::TextButton autoGainButton { "AUTO GAIN" };
    juce::TextButton bypassButton { "BYPASS" }, monoButton { "MONO" };
    std::unique_ptr<juce::ButtonParameterAttachment> bypassAtt;
    std::unique_ptr<ToggleBinding> autoGainAtt;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    juce::String lastPresetText;
};

} // namespace mepro::ui
