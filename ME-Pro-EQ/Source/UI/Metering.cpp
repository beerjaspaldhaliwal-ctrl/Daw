// ME Pro EQ — ME Audio
#include "Metering.h"
#include "PluginProcessor.h"
#include "Theme.h"

namespace mepro::ui
{

LevelMeter::LevelMeter (juce::String name) : label (std::move (name))
{
    setTitle (label + " level meter");
}

void LevelMeter::push (float pl, float pr, int numChannels, double dt)
{
    channels = numChannels;
    const float in[2] = { pl, numChannels > 1 ? pr : pl };
    constexpr float fallDbPerSec = 26.0f;
    for (int c = 0; c < 2; ++c)
    {
        const float db = in[c] > 0.0f ? juce::Decibels::gainToDecibels (in[c], -100.0f) : -100.0f;
        level[c] = juce::jmax (db, level[c] - fallDbPerSec * (float) dt);
        holdAge[c] += dt;
        if (db >= hold[c])
        {
            hold[c] = db; // new peak: hold it
            holdAge[c] = 0.0;
        }
        else if (holdAge[c] > 1.5)
        {
            hold[c] = level[c]; // hold time (1.5 s) expired: drop to the current level
            holdAge[c] = 0.0;
        }
    }
    repaint();
}

float LevelMeter::dbToY (float db, juce::Rectangle<float> r) const
{
    const float t = (juce::jlimit (kMinDb, kMaxDb, db) - kMinDb) / (kMaxDb - kMinDb);
    return r.getBottom() - t * r.getHeight();
}

void LevelMeter::paint (juce::Graphics& g)
{
    using namespace theme;
    auto r = getLocalBounds().toFloat();
    g.setFont (font (11.0f, true));
    g.setColour (textDim);
    g.drawText (label, r.removeFromTop (14.0f), juce::Justification::centred);
    r.removeFromTop (2.0f);
    auto scale = r.removeFromRight (18.0f);
    const float barW = (r.getWidth() - 3.0f) * 0.5f;
    for (int c = 0; c < 2; ++c)
    {
        auto bar = juce::Rectangle<float> (r.getX() + c * (barW + 3.0f), r.getY(), barW, r.getHeight());
        g.setColour (panelRaised);
        g.fillRect (bar);
        const float y = dbToY (level[c], bar);
        const float y0 = dbToY (0.0f, bar), y6 = dbToY (-6.0f, bar);
        g.setColour (good.withAlpha (0.9f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (bar.getX(), juce::jmax (y, y6), bar.getRight(), bar.getBottom()));
        if (y < y6)
        {
            g.setColour (accent);
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (bar.getX(), juce::jmax (y, y0), bar.getRight(), y6));
        }
        if (y < y0)
        {
            g.setColour (danger);
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (bar.getX(), y, bar.getRight(), y0));
        }
        if (hold[c] > kMinDb)
        {
            g.setColour (hold[c] > 0.0f ? danger : text);
            g.fillRect (bar.getX(), dbToY (hold[c], bar) - 1.0f, bar.getWidth(), 2.0f);
        }
        if (channels == 1)
            break;
    }
    g.setFont (font (9.5f));
    for (float db : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
    {
        const float y = dbToY (db, r);
        g.setColour (db >= 0.0f ? text : textFaint);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (scale.getX(), y - 6.0f, scale.getWidth(), 12.0f), juce::Justification::centredRight);
    }
}

//==============================================================================
MeterStrip::MeterStrip (EditorModel& m) : model (m)
{
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);
    addAndMakeVisible (inTrim);
    addAndMakeVisible (outTrim);
    addAndMakeVisible (clipButton);
    addAndMakeVisible (inPeak);
    addAndMakeVisible (outPeak);
    inTrim.attach (model.globalParam (params::id::inputTrim));
    outTrim.attach (model.globalParam (params::id::outputTrim));
    inTrim.setBipolar (true);
    outTrim.setBipolar (true);
    clipButton.setTooltip ("Output clip indicator (sample peak above 0 dBFS). Click to reset.");
    clipButton.onClick = [this] { model.getProcessor().getEngine().resetClip(); };
    for (auto* l : { &inPeak, &outPeak })
    {
        l->setFont (theme::font (11.0f));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, theme::textDim);
    }
}

void MeterStrip::refresh (double dt)
{
    auto& e = model.getProcessor().getEngine();
    const int ch = model.getProcessor().getMainChannelCount();
    const float il = e.takeInputPeak (0), ir = e.takeInputPeak (1);
    const float ol = e.takeOutputPeak (0), orr = e.takeOutputPeak (1);
    inMeter.push (il, ir, ch, dt);
    outMeter.push (ol, orr, ch, dt);
    auto fmt = [] (float db) { return db <= -99.0f ? juce::String ("-inf") : juce::String (db, 1); };
    inPeak.setText (fmt (inMeter.getHeldDb()), juce::dontSendNotification);
    outPeak.setText (fmt (outMeter.getHeldDb()), juce::dontSendNotification);
    const bool clipped = e.hasClipped();
    clipButton.setColour (juce::TextButton::buttonColourId, clipped ? theme::danger : theme::panelRaised);
    clipButton.setColour (juce::TextButton::textColourOffId, clipped ? juce::Colours::white : theme::textFaint);
}

void MeterStrip::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    g.setColour (theme::panelEdge);
    g.drawVerticalLine (0, 0.0f, (float) getHeight());
}

void MeterStrip::resized()
{
    auto r = getLocalBounds().reduced (8, 8);
    clipButton.setBounds (r.removeFromTop (22));
    r.removeFromTop (6);
    auto knobs = r.removeFromBottom (150);
    inTrim.setBounds (knobs.removeFromTop (75));
    outTrim.setBounds (knobs);
    auto peaks = r.removeFromBottom (16);
    inPeak.setBounds (peaks.removeFromLeft (peaks.getWidth() / 2));
    outPeak.setBounds (peaks);
    inMeter.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (2, 0));
    outMeter.setBounds (r.reduced (2, 0));
}

} // namespace mepro::ui
