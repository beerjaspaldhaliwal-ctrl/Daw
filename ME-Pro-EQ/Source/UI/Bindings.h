// ME Pro EQ — ME Audio
// Parameter <-> control bindings whose user edits are undoable (JUCE's stock attachments write to the
// parameter directly, bypassing the plugin's undo history). Display updates from the host or from
// automation still arrive through juce::ParameterAttachment. Message thread only.
#pragma once

#include "State/UndoHistory.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace mepro::ui
{

class ComboBinding final
{
public:
    // Item IDs in the box must be 1..N for choice indices 0..N-1.
    ComboBinding (juce::RangedAudioParameter& p, juce::ComboBox& b, state::UndoHistory& u)
        : box (b), undo (u),
          attachment (p, [this] (float v) { box.setSelectedItemIndex (juce::roundToInt (v), juce::dontSendNotification); })
    {
        box.onChange = [this]
        {
            const int i = box.getSelectedItemIndex();
            if (i < 0)
                return;
            state::UndoHistory::Scope action (undo);
            attachment.setValueAsCompleteGesture ((float) i);
        };
        attachment.sendInitialUpdate();
    }

    ~ComboBinding() { box.onChange = nullptr; }

private:
    juce::ComboBox& box;
    state::UndoHistory& undo;
    juce::ParameterAttachment attachment;
    JUCE_DECLARE_NON_COPYABLE (ComboBinding)
};

class ToggleBinding final
{
public:
    ToggleBinding (juce::RangedAudioParameter& p, juce::Button& b, state::UndoHistory& u)
        : button (b), undo (u),
          attachment (p, [this] (float v) { button.setToggleState (v >= 0.5f, juce::dontSendNotification); })
    {
        button.setClickingTogglesState (true);
        button.onClick = [this]
        {
            state::UndoHistory::Scope action (undo);
            attachment.setValueAsCompleteGesture (button.getToggleState() ? 1.0f : 0.0f);
        };
        attachment.sendInitialUpdate();
    }

    ~ToggleBinding() { button.onClick = nullptr; }

private:
    juce::Button& button;
    state::UndoHistory& undo;
    juce::ParameterAttachment attachment;
    JUCE_DECLARE_NON_COPYABLE (ToggleBinding)
};

} // namespace mepro::ui
