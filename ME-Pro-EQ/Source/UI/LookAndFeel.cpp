// ME Pro EQ — ME Audio
#include "LookAndFeel.h"
#include "Theme.h"

namespace mepro::ui
{

MEProLookAndFeel::MEProLookAndFeel()
{
    using namespace theme;
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::TextButton::buttonColourId, panelRaised);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff15110a));
    setColour (juce::ComboBox::backgroundColourId, panelRaised);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, panelRaised);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, accent);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0d1117));
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (juce::TextEditor::outlineColourId, accent);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::TooltipWindow::backgroundColourId, panelRaised);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, panelEdge);
}

juce::Typeface::Ptr MEProLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
#if JUCE_WINDOWS
    // Segoe UI ships with every Windows 10/11 installation and renders crisply at all DPI scales.
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
    {
        auto copy = f;
        copy.setTypefaceName ("Segoe UI");
        return juce::Typeface::createSystemTypefaceFor (copy);
    }
#endif
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void MEProLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                             bool over, bool down)
{
    using namespace theme;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto custom = b.findColour (juce::TextButton::buttonOnColourId);
    juce::Colour fill = on ? custom.withAlpha (0.9f) : panelRaised;
    if (! b.isEnabled())
        fill = fill.withMultipliedAlpha (0.4f);
    else if (down)
        fill = fill.brighter (0.15f);
    else if (over)
        fill = fill.brighter (0.07f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? custom.brighter (0.2f) : panelEdge.brighter (over ? 0.25f : 0.0f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

juce::Font MEProLookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return theme::font (juce::jmin (13.0f, h * 0.48f), true);
}

void MEProLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    auto col = b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
    if (! b.isEnabled())
        col = col.withMultipliedAlpha (0.45f);
    g.setColour (col);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1);
}

void MEProLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& box)
{
    using namespace theme;
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.isEnabled() ? (down ? panelRaised.brighter (0.1f) : panelRaised) : panelRaised.withMultipliedAlpha (0.4f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? accent.withAlpha (0.7f) : panelEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = w - 14.0f, ay = h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.isEnabled() ? textDim : textFaint);
    g.fillPath (arrow);
}

juce::Font MEProLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return theme::font (13.0f);
}

void MEProLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font MEProLookAndFeel::getPopupMenuFont()
{
    return theme::font (14.0f);
}

void MEProLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (theme::panelRaised);
    g.setColour (theme::panelEdge);
    g.drawRect (0, 0, w, h);
}

juce::Font MEProLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

void MEProLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 3.0f);
}

void MEProLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (theme::accent);
    g.drawRoundedRectangle (0.5f, 0.5f, w - 1.0f, h - 1.0f, 3.0f, 1.0f);
}

void MEProLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.fillAll (theme::panelRaised);
    g.setColour (theme::panelEdge);
    g.drawRect (0, 0, w, h);
    g.setColour (theme::text);
    g.setFont (theme::font (13.0f));
    g.drawFittedText (text, 6, 2, w - 12, h - 4, juce::Justification::centredLeft, 4);
}

} // namespace mepro::ui
