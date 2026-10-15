// ME Pro EQ — ME Audio
#include "BandControls.h"
#include "PluginProcessor.h"
#include "Theme.h"

#include "DSP/MathUtils.h"

namespace mepro::ui
{

using dsp::FilterType;

//==============================================================================
BandChip::BandChip (EditorModel& m, int b) : model (m), band (b)
{
    setTitle ("Band " + juce::String (b + 1));
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void BandChip::paint (juce::Graphics& g)
{
    using namespace theme;
    auto r = getLocalBounds().toFloat().reduced (2.0f, 1.0f);
    const auto col = bandColour (band);
    const bool used = model.isUsed (band), on = model.isOn (band);
    const bool sel = model.getSelectedBand() == band;
    if (used)
    {
        g.setColour (on ? col.withAlpha (0.85f) : col.withAlpha (0.22f));
        g.fillRoundedRectangle (r, 4.0f);
    }
    else
    {
        g.setColour (panelRaised);
        g.fillRoundedRectangle (r, 4.0f);
    }
    g.setColour (sel ? accent : (used ? col.brighter (0.3f) : panelEdge));
    g.drawRoundedRectangle (r, 4.0f, sel ? 2.0f : 1.0f);
    g.setColour (used && on ? juce::Colour (0xff10141a) : (used ? col : textFaint));
    g.setFont (font (12.0f, true));
    g.drawText (juce::String (band + 1), r, juce::Justification::centred);
    if (used && model.get (band, BandParam::DynOn) > 0.5f && dsp::typeSupportsDynamics (model.type (band)))
    {
        g.setColour (on ? juce::Colour (0xff10141a) : col);
        g.fillEllipse (r.getRight() - 7.0f, r.getY() + 3.0f, 4.0f, 4.0f);
    }
}

void BandChip::mouseDown (const juce::MouseEvent& e)
{
    if (! model.isUsed (band))
    {
        // Clicking an unused slot creates a bell at that slot's default frequency.
        if (e.mods.isLeftButtonDown())
            model.createBand (params::defaultFrequencyForBand (band), 0.0, FilterType::Bell);
        return;
    }
    model.setSelectedBand (band);
}

//==============================================================================
void DynamicGainMeter::setValue (float g, float r, bool a)
{
    if (! dsp::sameValue (g, gain) || ! dsp::sameValue (r, range) || a != active)
    {
        gain = g;
        range = r;
        active = a;
        repaint();
    }
}

void DynamicGainMeter::paint (juce::Graphics& g)
{
    using namespace theme;
    auto r = getLocalBounds().toFloat();
    auto label = r.removeFromTop (14.0f);
    g.setFont (font (11.0f, true));
    g.setColour (active ? textDim : textFaint);
    g.drawText ("DYN GAIN", label, juce::Justification::centredLeft);
    g.setColour (active ? text : textFaint);
    g.drawText (active ? params::formatGain (gain) : juce::String ("-"), label, juce::Justification::centredRight);

    auto bar = r.reduced (0.0f, 3.0f).withHeight (10.0f);
    g.setColour (panelRaised);
    g.fillRoundedRectangle (bar, 3.0f);
    const float mid = bar.getCentreX();
    const float span = bar.getWidth() * 0.5f;
    g.setColour (panelEdge);
    g.drawVerticalLine ((int) mid, bar.getY(), bar.getBottom());
    if (active)
    {
        const float x = mid + juce::jlimit (-1.0f, 1.0f, gain / 18.0f) * span;
        g.setColour (gain < 0 ? accent : good);
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (mid, x), bar.getY() + 1, juce::jmax (mid, x), bar.getBottom() - 1));
        const float rx = mid + juce::jlimit (-1.0f, 1.0f, range / 18.0f) * span;
        g.setColour (textDim);
        g.drawVerticalLine ((int) rx, bar.getY() - 2, bar.getBottom() + 2);
    }
}

//==============================================================================
void BandPanel::setupCombo (juce::ComboBox& box, const juce::StringArray& items, const juce::String& t)
{
    box.addItemList (items, 1);
    box.setTitle (t);
    box.setTooltip (t);
}

BandPanel::BandPanel (EditorModel& m) : model (m)
{
    for (int b = 0; b < dsp::kNumBands; ++b)
    {
        chips[(size_t) b] = std::make_unique<BandChip> (model, b);
        chips[(size_t) b]->setTooltip ("Band " + juce::String (b + 1) + " (click an empty slot to create a band)");
        addAndMakeVisible (*chips[(size_t) b]);
    }

    juce::StringArray types, slopes, modes;
    for (int t = 0; t < (int) FilterType::NumTypes; ++t)
        types.add (dsp::filterTypeName ((FilterType) t));
    for (int s : dsp::kSlopesDbPerOct)
        slopes.add (juce::String (s) + " dB/oct");
    for (int s = 0; s < (int) dsp::StereoMode::NumModes; ++s)
        modes.add (dsp::stereoModeName ((dsp::StereoMode) s));
    setupCombo (typeBox, types, "Filter type");
    setupCombo (slopeBox, slopes, "Slope");
    setupCombo (stereoBox, modes, "Stereo mode");
    setupCombo (detBox, { "Peak", "RMS" }, "Detector");
    setupCombo (scBox, { "Internal SC", "External SC" }, "Sidechain source");

    for (auto* b : { &powerButton, &dynButton, &soloButton })
        b->setClickingTogglesState (true);
    powerButton.setTooltip ("Enable / disable (bypass) this band");
    soloButton.setTooltip ("Audition: hear only this band's frequency region (monitoring only, never rendered or saved)");
    resetButton.setTooltip ("Reset this band's settings to defaults (frequency is kept)");
    deleteButton.setTooltip ("Delete this band");
    dynButton.setTooltip ("Dynamic EQ: the band gain follows the level measured in the band's own frequency region");
    soloButton.setColour (juce::TextButton::buttonOnColourId, theme::accent);
    dynButton.setColour (juce::TextButton::buttonOnColourId, theme::good);

    soloButton.onClick = [this]
    {
        auto& p = model.getProcessor();
        const int b = model.getSelectedBand();
        p.setAuditionBand (soloButton.getToggleState() && b >= 0 ? b : -1);
    };
    resetButton.onClick = [this] { model.resetBandKeepFrequency (model.getSelectedBand()); };
    deleteButton.onClick = [this] { model.deleteBand (model.getSelectedBand()); };

    gainKnob.setBipolar (true);
    rangeKnob.setBipolar (true);
    for (auto* k : { &freqKnob, &gainKnob, &qKnob, &thrKnob, &ratioKnob, &attKnob, &relKnob, &rangeKnob })
        k->setUndoHistory (&model.getUndo());
    title.setFont (theme::font (14.0f, true));
    hint.setFont (theme::font (14.0f));
    hint.setColour (juce::Label::textColourId, theme::textDim);
    hint.setText ("No band selected. Double-click the graph or click an empty band slot to create one.", juce::dontSendNotification);
    hint.setJustificationType (juce::Justification::centred);
    dynNote.setFont (theme::font (11.5f));
    dynNote.setColour (juce::Label::textColourId, theme::accent);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &powerButton, &soloButton, &resetButton, &deleteButton, &dynButton, &typeBox, &slopeBox, &stereoBox, &detBox, &scBox,
             &freqKnob, &gainKnob, &qKnob, &thrKnob, &ratioKnob, &attKnob, &relKnob, &rangeKnob, &dynMeter, &title, &dynNote })
        addAndMakeVisible (c);
    addChildComponent (hint);

    model.addListener (this);
    rebind();
}

BandPanel::~BandPanel()
{
    model.removeListener (this);
}

void BandPanel::rebind()
{
    const int b = model.getSelectedBand();
    boundBand = b;
    typeAtt.reset();
    slopeAtt.reset();
    stereoAtt.reset();
    detAtt.reset();
    scAtt.reset();
    powerAtt.reset();
    dynAtt.reset();

    const bool has = b >= 0 && model.isUsed (b);
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &powerButton, &soloButton, &resetButton, &deleteButton, &dynButton, &typeBox, &slopeBox, &stereoBox, &detBox, &scBox,
             &freqKnob, &gainKnob, &qKnob, &thrKnob, &ratioKnob, &attKnob, &relKnob, &rangeKnob, &dynMeter, &title, &dynNote })
        c->setVisible (has);
    hint.setVisible (! has);

    if (! has)
    {
        for (auto* k : { &freqKnob, &gainKnob, &qKnob, &thrKnob, &ratioKnob, &attKnob, &relKnob, &rangeKnob })
            k->attach (nullptr);
        return;
    }

    auto& st = model.getState();
    auto id = [b] (BandParam p) { return params::bandParamId (b, p); };
    auto& u = model.getUndo();
    typeAtt = std::make_unique<ComboBinding> (*st.getParameter (id (BandParam::Type)), typeBox, u);
    slopeAtt = std::make_unique<ComboBinding> (*st.getParameter (id (BandParam::Slope)), slopeBox, u);
    stereoAtt = std::make_unique<ComboBinding> (*st.getParameter (id (BandParam::Stereo)), stereoBox, u);
    detAtt = std::make_unique<ComboBinding> (*st.getParameter (id (BandParam::Detector)), detBox, u);
    scAtt = std::make_unique<ComboBinding> (*st.getParameter (id (BandParam::Sidechain)), scBox, u);
    powerAtt = std::make_unique<ToggleBinding> (*st.getParameter (id (BandParam::On)), powerButton, u);
    dynAtt = std::make_unique<ToggleBinding> (*st.getParameter (id (BandParam::DynOn)), dynButton, u);

    freqKnob.attach (st.getParameter (id (BandParam::Freq)));
    gainKnob.attach (st.getParameter (id (BandParam::Gain)));
    qKnob.attach (st.getParameter (id (BandParam::Q)));
    thrKnob.attach (st.getParameter (id (BandParam::Threshold)));
    ratioKnob.attach (st.getParameter (id (BandParam::Ratio)));
    attKnob.attach (st.getParameter (id (BandParam::Attack)));
    relKnob.attach (st.getParameter (id (BandParam::Release)));
    rangeKnob.attach (st.getParameter (id (BandParam::Range)));

    const auto col = theme::bandColour (b);
    for (auto* k : { &freqKnob, &gainKnob, &qKnob })
        k->setColour (col);
    title.setColour (juce::Label::textColourId, col);
    title.setText ("BAND " + juce::String (b + 1), juce::dontSendNotification);
    updateEnablement();
}

void BandPanel::updateEnablement()
{
    const int b = model.getSelectedBand();
    if (b < 0 || ! model.isUsed (b))
        return;
    const auto t = model.type (b);
    const int slope = model.slope (b);
    gainKnob.setEnabled (dsp::typeUsesGain (t));
    qKnob.setEnabled (dsp::typeUsesQ (t, slope));
    slopeBox.setEnabled (dsp::typeUsesSlope (t));
    for (int i = 0; i < (int) dsp::kSlopesDbPerOct.size(); ++i)
        slopeBox.setItemEnabled (i + 1, t == FilterType::HighPass || t == FilterType::LowPass || i <= dsp::kMaxShelfSlopeIndex);

    const bool dynSupported = dsp::typeSupportsDynamics (t);
    const bool dynOn = dynSupported && model.get (b, BandParam::DynOn) > 0.5f;
    dynButton.setEnabled (dynSupported);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &thrKnob, &ratioKnob, &attKnob, &relKnob, &rangeKnob, &detBox, &scBox })
        c->setEnabled (dynOn);

    juce::String note;
    if (! dynSupported)
        note = "Dynamics are available for bell and shelf bands";
    else if (dynOn && model.getProcessor().getEngine().isUsingLinearPhase())
        note = "Linear-phase mode: dynamic EQ is inactive, the static settings apply";
    else if (dynOn && model.get (b, BandParam::Sidechain) > 0.5f && ! model.getProcessor().isSidechainAvailable())
        note = "External sidechain selected, but the host is not sending a sidechain signal";
    else if (model.getProcessor().getMainChannelCount() == 1
             && (model.stereo (b) == dsp::StereoMode::Left || model.stereo (b) == dsp::StereoMode::Right || model.stereo (b) == dsp::StereoMode::Side))
        note = "Mono track: Left/Right/Side bands have no effect";
    dynNote.setText (note, juce::dontSendNotification);
}

void BandPanel::refresh()
{
    for (auto& c : chips)
        c->repaint();
    const int b = model.getSelectedBand();
    const bool has = b >= 0 && model.isUsed (b);
    if (b != boundBand || has == hint.isVisible())
        rebind();
    if (! has)
        return;
    updateEnablement();
    soloButton.setToggleState (model.getProcessor().getAuditionBand() == b, juce::dontSendNotification);
    const bool dynOn = dsp::typeSupportsDynamics (model.type (b)) && model.get (b, BandParam::DynOn) > 0.5f && model.isOn (b);
    dynMeter.setValue (model.getProcessor().getEngine().getBandDynamicGainDb (b), model.get (b, BandParam::Range), dynOn);
}

void BandPanel::paint (juce::Graphics& g)
{
    using namespace theme;
    g.fillAll (panel);
    g.setColour (panelEdge);
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
    if (! dynArea.isEmpty() && ! hint.isVisible())
    {
        g.setColour (panelRaised.withAlpha (0.6f));
        g.fillRoundedRectangle (dynArea.toFloat(), 6.0f);
        g.setColour (panelRaised);
        g.fillRoundedRectangle (mainArea.toFloat(), 6.0f);
    }
}

void BandPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 6);
    auto chipRow = r.removeFromTop (26);
    const int chipW = chipRow.getWidth() / dsp::kNumBands;
    for (int b = 0; b < dsp::kNumBands; ++b)
        chips[(size_t) b]->setBounds (chipRow.getX() + b * chipW, chipRow.getY(), chipW, chipRow.getHeight());
    r.removeFromTop (8);
    hint.setBounds (r);

    // Main band section (left) and dynamics section (right)
    const int dynW = juce::jmin (560, (int) (r.getWidth() * 0.52f));
    dynArea = r.removeFromRight (dynW);
    r.removeFromRight (10);
    mainArea = r;

    auto m = mainArea.reduced (8, 6);
    auto col1 = m.removeFromLeft (juce::jmin (210, m.getWidth() / 3));
    title.setBounds (col1.removeFromTop (20));
    col1.removeFromTop (2);
    auto row = col1.removeFromTop (24);
    powerButton.setBounds (row.removeFromLeft (46));
    row.removeFromLeft (6);
    typeBox.setBounds (row);
    col1.removeFromTop (6);
    row = col1.removeFromTop (24);
    slopeBox.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3));
    row.removeFromLeft (6);
    stereoBox.setBounds (row);
    col1.removeFromTop (6);
    row = col1.removeFromTop (24);
    const int bw = (row.getWidth() - 12) / 3;
    soloButton.setBounds (row.removeFromLeft (bw));
    row.removeFromLeft (6);
    resetButton.setBounds (row.removeFromLeft (bw));
    row.removeFromLeft (6);
    deleteButton.setBounds (row);

    m.removeFromLeft (10);
    const int kw = juce::jmin (96, m.getWidth() / 3);
    for (auto* k : { &freqKnob, &gainKnob, &qKnob })
        k->setBounds (m.removeFromLeft (kw).withTrimmedTop (2));

    auto d = dynArea.reduced (8, 6);
    auto dcol = d.removeFromLeft (juce::jmin (150, d.getWidth() / 4));
    dynButton.setBounds (dcol.removeFromTop (24));
    dcol.removeFromTop (6);
    detBox.setBounds (dcol.removeFromTop (24));
    dcol.removeFromTop (6);
    scBox.setBounds (dcol.removeFromTop (24));
    dcol.removeFromTop (6);
    dynMeter.setBounds (dcol.removeFromTop (32));
    d.removeFromLeft (8);
    dynNote.setBounds (d.removeFromBottom (16));
    const int dk = d.getWidth() / 5;
    for (auto* k : { &thrKnob, &ratioKnob, &attKnob, &relKnob, &rangeKnob })
        k->setBounds (d.removeFromLeft (dk));
}

} // namespace mepro::ui
