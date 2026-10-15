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
    float lastY = -1000.0f;
    for (float db : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
    {
        const float y = dbToY (db, r);
        if (y - lastY < 11.0f)
            continue; // not enough room at small window sizes: skip rather than overlap
        lastY = y;
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
    addAndMakeVisible (matchButton);
    addAndMakeVisible (phaseBox);
    addAndMakeVisible (qualityBox);
    phaseBox.addItemList ({ "Min phase", "Linear" }, 1);
    for (int q = 0; q < dsp::kNumLinearPhaseQualities; ++q)
    {
        const auto g = dsp::linearPhaseGeometry (48000.0, q);
        qualityBox.addItem (juce::String (g.firLength / 1024) + "k taps", q + 1);
    }
    phaseBox.setTooltip ("Phase mode. Min phase: zero latency, analog-like phase. Linear: no phase shift between frequencies; "
                         "adds latency (reported to the host) and pre-ringing on steep or narrow low-frequency settings. "
                         "Dynamic EQ is inactive in linear-phase mode. Not part of presets, A/B or undo.");
    qualityBox.setTooltip ("Linear-phase FIR length: longer = more accurate low frequencies, more latency and CPU "
                           "(taps shown for 44.1/48 kHz; they scale with the sample rate).");
    phaseAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*model.globalParam (params::id::phaseMode), phaseBox);
    qualityAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*model.globalParam (params::id::lpQuality), qualityBox);
    addAndMakeVisible (levelTitle);
    addAndMakeVisible (levelDiff);
    addAndMakeVisible (status);
    inTrim.attach (model.globalParam (params::id::inputTrim));
    outTrim.attach (model.globalParam (params::id::outputTrim));
    inTrim.setUndoHistory (&model.getUndo());
    outTrim.setUndoHistory (&model.getUndo());
    matchButton.setTooltip ("Level match: set the output trim so that the output level (RMS over the last ~3 s) equals the "
                            "input level. Play typical material first. Undoable.");
    matchButton.onClick = [this] { matchOutputLevel(); };
    levelTitle.setText ("OUT - IN", juce::dontSendNotification);
    levelTitle.setTooltip ("Output minus input level, RMS over the last ~3 seconds");
    levelDiff.setTooltip (levelTitle.getTooltip());
    for (auto* l : { &levelTitle, &levelDiff, &status })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, theme::textDim);
    }
    levelTitle.setFont (theme::font (10.0f, true));
    levelDiff.setFont (theme::font (12.5f, true));
    status.setFont (theme::font (10.0f));
    status.setMinimumHorizontalScale (0.7f);
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
    // Level comparison (3 s RMS). Readings below -70 dBFS are treated as silence.
    const float inDb = e.getInputRmsDb(), outDb = e.getOutputRmsDb();
    diffValid = inDb > -70.0f && outDb > -100.0f;
    lastDiffDb = diffValid ? outDb - inDb : 0.0f;
    levelDiff.setText (diffValid ? (lastDiffDb >= 0.0f ? "+" : "") + juce::String (lastDiffDb, 1) + " dB" : juce::String ("-"),
                       juce::dontSendNotification);
    levelDiff.setColour (juce::Label::textColourId, diffValid && std::abs (lastDiffDb) > 0.5f ? theme::accent : theme::textDim);
    matchButton.setEnabled (diffValid && std::abs (lastDiffDb) > 0.05f);

    // Engine status: sample rate, channels, latency, recovered numeric faults.
    auto& p = model.getProcessor();
    const double fs = p.getSampleRateForUi();
    const int lat = p.getLatencySamples();
    juce::String s;
    s << juce::String (fs / 1000.0, 1) << " kHz " << (ch == 1 ? "mono" : "stereo") << "\nlatency " << lat << " smp";
    if (lat > 0)
        s << " (" << juce::String (1000.0 * lat / juce::jmax (1.0, fs), 0) << " ms)";
    qualityBox.setEnabled (phaseBox.getSelectedItemIndex() == 1);
    if (e.getNonFiniteEvents() > 0)
        s << "\n! " << e.getNonFiniteEvents() << " faults fixed";
    status.setText (s, juce::dontSendNotification);
    status.setColour (juce::Label::textColourId, e.getNonFiniteEvents() > 0 ? theme::danger : theme::textFaint);
    status.setTooltip ("Latency reported to the host: " + juce::String (lat) + " samples ("
                       + juce::String (1000.0 * lat / juce::jmax (1.0, fs), 2) + " ms)");

    const bool clipped = e.hasClipped();
    clipButton.setColour (juce::TextButton::buttonColourId, clipped ? theme::danger : theme::panelRaised);
    clipButton.setColour (juce::TextButton::textColourOffId, clipped ? juce::Colours::white : theme::textFaint);
}

bool MeterStrip::matchOutputLevel()
{
    if (! diffValid)
        return false;
    auto* trim = model.globalParam (params::id::outputTrim);
    const float current = trim->convertFrom0to1 (trim->getValue());
    const float target = juce::jlimit ((float) dsp::EQEngine::kTrimMinDb, (float) dsp::EQEngine::kTrimMaxDb, current - lastDiffDb);
    state::UndoHistory::Scope action (model.getUndo());
    trim->beginChangeGesture();
    trim->setValueNotifyingHost (trim->convertTo0to1 (target));
    trim->endChangeGesture();
    return true;
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
    phaseBox.setBounds (r.removeFromTop (22));
    r.removeFromTop (3);
    qualityBox.setBounds (r.removeFromTop (22));
    r.removeFromTop (6);
    status.setBounds (r.removeFromBottom (40)); // up to three lines
    r.removeFromBottom (4);
    auto knobs = r.removeFromBottom (juce::jlimit (120, 150, r.getHeight() / 3));
    inTrim.setBounds (knobs.removeFromTop (knobs.getHeight() / 2));
    outTrim.setBounds (knobs);
    r.removeFromBottom (4);
    matchButton.setBounds (r.removeFromBottom (22));
    levelDiff.setBounds (r.removeFromBottom (16));
    levelTitle.setBounds (r.removeFromBottom (13));
    r.removeFromBottom (6);
    auto peaks = r.removeFromBottom (16);
    inPeak.setBounds (peaks.removeFromLeft (peaks.getWidth() / 2));
    outPeak.setBounds (peaks);
    inMeter.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (2, 0));
    outMeter.setBounds (r.reduced (2, 0));
}

} // namespace mepro::ui
