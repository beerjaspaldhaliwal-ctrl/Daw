// ME Pro EQ — ME Audio
// Lists the analysis suggestions (DSP/ToneAnalysis.h) with their measured evidence. Nothing is applied until
// the user presses APPLY on an item; each APPLY creates one band as a single undoable action.
#pragma once

#include "EditorModel.h"

#include "DSP/ToneAnalysis.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

namespace mepro::ui
{

class SuggestionPanel final : public juce::Component
{
public:
    explicit SuggestionPanel (EditorModel& m);
    ~SuggestionPanel() override;

    void setSuggestions (std::vector<dsp::Suggestion> list, const juce::String& statusText);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Exposed for the editor tests.
    int getNumItems() const noexcept { return (int) items.size(); }
    const dsp::Suggestion& getSuggestion (int i) const;
    bool apply (int i);    // returns false if no free band is left
    void dismiss (int i);

private:
    struct Item;
    void layoutItems();

    EditorModel& model;
    juce::String status;
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<Item>> items;
    juce::TextButton closeButton { "CLOSE" };
};

} // namespace mepro::ui
