// ME Pro EQ — ME Audio
#include "SuggestionPanel.h"
#include "Theme.h"

namespace mepro::ui
{

namespace
{
    juce::Colour confidenceColour (dsp::Suggestion::Confidence c)
    {
        switch (c)
        {
            case dsp::Suggestion::Confidence::High:   return theme::good;
            case dsp::Suggestion::Confidence::Medium: return theme::accent;
            case dsp::Suggestion::Confidence::Low:    return theme::textDim;
        }
        return theme::textDim;
    }

    const char* confidenceName (dsp::Suggestion::Confidence c)
    {
        switch (c)
        {
            case dsp::Suggestion::Confidence::High:   return "HIGH";
            case dsp::Suggestion::Confidence::Medium: return "MEDIUM";
            case dsp::Suggestion::Confidence::Low:    return "LOW";
        }
        return "";
    }

    juce::String describeBand (const dsp::BandSettings& b)
    {
        juce::String s = dsp::filterTypeName (b.type);
        s << "  " << params::formatFrequency ((float) b.freqHz);
        if (dsp::typeUsesGain (b.type) && ! b.dyn.enabled)
            s << "  " << params::formatGain ((float) b.gainDb);
        if (dsp::typeUsesQ (b.type, b.slopeIndex))
            s << "  Q " << params::formatQ ((float) b.q);
        if (dsp::typeUsesSlope (b.type))
            s << "  " << dsp::kSlopesDbPerOct[(size_t) b.slopeIndex] << " dB/oct";
        if (b.dyn.enabled)
            s << "  dynamic: thr " << juce::String (b.dyn.thresholdDb, 0) << " dBFS, range " << params::formatGain ((float) b.dyn.rangeDb);
        return s;
    }
} // namespace

struct SuggestionPanel::Item final : public juce::Component
{
    Item (SuggestionPanel& p, dsp::Suggestion s) : panel (p), suggestion (std::move (s))
    {
        applyButton.setTooltip ("Create this band (one undoable step). Adjust it by ear afterwards.");
        dismissButton.setTooltip ("Hide this suggestion");
        applyButton.setColour (juce::TextButton::buttonColourId, theme::accent.withAlpha (0.25f));
        addAndMakeVisible (applyButton);
        addAndMakeVisible (dismissButton);
    }

    juce::TextLayout layoutReason (float width) const
    {
        juce::AttributedString a;
        a.append (juce::String::fromUTF8 (suggestion.reason.c_str()), theme::font (11.5f), theme::textDim);
        a.setWordWrap (juce::AttributedString::byWord);
        juce::TextLayout t;
        t.createLayout (a, width);
        return t;
    }

    // Layout (top to bottom): 2 px margin, 6 px padding, title 16, gap 3, band 14, gap 3, reason text, gap 6,
    // buttons 24, padding 8, margin 2.
    int preferredHeight (int width) const
    {
        return 2 + 6 + 16 + 3 + 14 + 3 + (int) std::ceil (layoutReason ((float) width - 20.0f).getHeight()) + 6 + 24 + 8 + 2;
    }

    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (applied ? panelRaised.withAlpha (0.5f) : panelRaised);
        g.fillRoundedRectangle (r, 5.0f);
        auto in = r.reduced (8.0f, 6.0f);
        auto top = in.removeFromTop (16.0f);
        const auto cc = confidenceColour (suggestion.confidence);
        auto chip = top.removeFromRight (62.0f);
        g.setColour (cc.withAlpha (0.2f));
        g.fillRoundedRectangle (chip, 3.0f);
        g.setColour (cc);
        g.setFont (font (9.5f, true));
        g.drawText (confidenceName (suggestion.confidence), chip, juce::Justification::centred);
        g.setColour (applied ? textDim : text);
        g.setFont (font (13.0f, true));
        g.drawText (juce::String::fromUTF8 (suggestion.title.c_str()) + (applied ? "  (applied)" : ""), top, juce::Justification::centredLeft, true);
        in.removeFromTop (3.0f);
        g.setColour (accent.withAlpha (applied ? 0.5f : 0.95f));
        g.setFont (font (11.0f, true));
        g.drawText (describeBand (suggestion.band), in.removeFromTop (14.0f), juce::Justification::centredLeft, true);
        in.removeFromTop (3.0f);
        auto layout = layoutReason (in.getWidth());
        layout.draw (g, in.removeFromTop (layout.getHeight()));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 8).removeFromBottom (24);
        dismissButton.setBounds (r.removeFromRight (80));
        r.removeFromRight (6);
        applyButton.setBounds (r.removeFromRight (80));
    }

    SuggestionPanel& panel;
    dsp::Suggestion suggestion;
    bool applied = false;
    juce::TextButton applyButton { "APPLY" }, dismissButton { "DISMISS" };
};

//==============================================================================
SuggestionPanel::SuggestionPanel (EditorModel& m) : model (m)
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);
    setTitle ("Analysis suggestions");
}

SuggestionPanel::~SuggestionPanel()
{
    viewport.setViewedComponent (nullptr, false);
}

void SuggestionPanel::setSuggestions (std::vector<dsp::Suggestion> list, const juce::String& statusText)
{
    items.clear();
    status = statusText;
    for (auto& s : list)
    {
        auto item = std::make_unique<Item> (*this, std::move (s));
        auto* raw = item.get();
        item->applyButton.onClick = [this, raw]
        {
            for (size_t i = 0; i < items.size(); ++i)
                if (items[i].get() == raw)
                    apply ((int) i);
        };
        item->dismissButton.onClick = [this, raw]
        {
            for (size_t i = 0; i < items.size(); ++i)
                if (items[i].get() == raw)
                {
                    dismiss ((int) i);
                    return;
                }
        };
        content.addAndMakeVisible (*item);
        items.push_back (std::move (item));
    }
    resized();
    repaint();
}

const dsp::Suggestion& SuggestionPanel::getSuggestion (int i) const
{
    return items[(size_t) i]->suggestion;
}

bool SuggestionPanel::apply (int i)
{
    if (i < 0 || i >= (int) items.size() || items[(size_t) i]->applied)
        return false;
    auto& item = *items[(size_t) i];
    const int band = model.createBandFrom (item.suggestion.band);
    if (band < 0)
    {
        status = "All 16 bands are in use - delete a band to apply more suggestions.";
        resized();
        repaint();
        return false;
    }
    item.applied = true;
    item.applyButton.setEnabled (false);
    item.repaint();
    return true;
}

void SuggestionPanel::dismiss (int i)
{
    if (i < 0 || i >= (int) items.size())
        return;
    content.removeChildComponent (items[(size_t) i].get());
    items.erase (items.begin() + i);
    if (items.empty() && status.isEmpty())
        status = "All suggestions dismissed.";
    layoutItems();
    repaint();
}

void SuggestionPanel::layoutItems()
{
    const int w = juce::jmax (100, viewport.getWidth() - viewport.getScrollBarThickness());
    int y = 0;
    for (auto& it : items)
    {
        const int h = it->preferredHeight (w);
        it->setBounds (0, y, w, h);
        y += h + 4;
    }
    content.setSize (w, juce::jmax (y, 1));
}

void SuggestionPanel::paint (juce::Graphics& g)
{
    using namespace theme;
    g.setColour (panel.withAlpha (0.97f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);
    auto r = getLocalBounds().reduced (12, 10);
    g.setColour (text);
    g.setFont (font (14.0f, true));
    g.drawText ("SUGGESTIONS", r.removeFromTop (20), juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (font (11.0f));
    g.drawText ("Measured on the output. Nothing changes until you press APPLY.", r.removeFromTop (16), juce::Justification::centredLeft, true);
    if (status.isNotEmpty())
    {
        g.setColour (accent);
        g.setFont (font (12.0f));
        g.drawFittedText (status, r.removeFromTop (40), juce::Justification::topLeft, 3);
    }
}

void SuggestionPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    closeButton.setBounds (r.getRight() - 70, r.getY(), 70, 22);
    r.removeFromTop (40 + (status.isNotEmpty() ? 44 : 0));
    viewport.setBounds (r);
    layoutItems();
}

} // namespace mepro::ui
