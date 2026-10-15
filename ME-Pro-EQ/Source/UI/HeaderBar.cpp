// ME Pro EQ — ME Audio
#include "HeaderBar.h"
#include "PluginProcessor.h"
#include "Theme.h"

namespace mepro::ui
{

HeaderBar::HeaderBar (EditorModel& m) : model (m)
{
    bypassButton.setClickingTogglesState (true);
    bypassButton.setColour (juce::TextButton::buttonOnColourId, theme::danger);
    bypassButton.setTooltip ("Global bypass (host-automatable). Bypassed output is the unprocessed input.");
    bypassAtt = std::make_unique<juce::ButtonParameterAttachment> (*model.globalParam (params::id::bypass), bypassButton);

    monoButton.setClickingTogglesState (true);
    monoButton.setTooltip ("Mono compatibility check: listen to (L+R)/2. Monitoring only - never saved or rendered.");
    monoButton.setToggleState (model.getProcessor().getMonoAudition(), juce::dontSendNotification);
    monoButton.onClick = [this] { model.getProcessor().setMonoAudition (monoButton.getToggleState()); };

    status.setFont (theme::font (11.5f));
    status.setColour (juce::Label::textColourId, theme::textDim);
    status.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (bypassButton);
    addAndMakeVisible (monoButton);
    addAndMakeVisible (status);
}

void HeaderBar::refresh()
{
    auto& p = model.getProcessor();
    const double fs = p.getSampleRateForUi();
    const int lat = p.getLatencySamples();
    juce::String s;
    s << juce::String (fs / 1000.0, 1) << " kHz  " << (p.getMainChannelCount() == 1 ? "mono" : "stereo")
      << "   latency " << lat << " smp (" << juce::String (1000.0 * lat / juce::jmax (1.0, fs), 2) << " ms)";
    if (p.getEngine().getNonFiniteEvents() > 0)
        s << "   ! " << p.getEngine().getNonFiniteEvents() << " recovered numeric faults";
    status.setText (s, juce::dontSendNotification);
    monoButton.setToggleState (p.getMonoAudition(), juce::dontSendNotification);
}

void HeaderBar::paint (juce::Graphics& g)
{
    using namespace theme;
    g.fillAll (panel);
    g.setColour (panelEdge);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

    // ME Audio mark: amber rounded square with "ME", then the product name.
    auto r = getLocalBounds().reduced (12, 9).toFloat();
    auto mark = r.removeFromLeft (r.getHeight()).reduced (1.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (mark, 5.0f);
    g.setColour (juce::Colour (0xff15110a));
    g.setFont (font (mark.getHeight() * 0.52f, true));
    g.drawText ("ME", mark, juce::Justification::centred);
    r.removeFromLeft (10.0f);
    g.setColour (text);
    g.setFont (font (19.0f, true));
    g.drawText ("PRO EQ", r.removeFromLeft (90.0f), juce::Justification::centredLeft);
    g.setColour (textFaint);
    g.setFont (font (10.5f, true));
    g.drawText ("ME AUDIO", r.removeFromLeft (70.0f), juce::Justification::centredLeft);
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    r.removeFromLeft (210);
    bypassButton.setBounds (r.removeFromRight (78));
    r.removeFromRight (8);
    monoButton.setBounds (r.removeFromRight (62));
    r.removeFromRight (12);
    status.setBounds (r.removeFromRight (juce::jmin (330, r.getWidth() / 2)));
    centreArea = r;
}

} // namespace mepro::ui
