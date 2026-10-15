// ME Pro EQ — ME Audio
#include "EQGraph.h"
#include "PluginProcessor.h"
#include "Theme.h"

#include <cmath>

namespace mepro::ui
{

using dsp::FilterType;
using dsp::StereoMode;

namespace
{
    constexpr int kRanges[] = { 6, 12, 18, 30 };
    constexpr float kNodeRadius = 7.0f;

    const char* stereoLetter (StereoMode m)
    {
        switch (m)
        {
            case StereoMode::Left:     return "L";
            case StereoMode::Right:    return "R";
            case StereoMode::Mid:      return "M";
            case StereoMode::Side:     return "S";
            case StereoMode::Stereo:
            case StereoMode::NumModes: break;
        }
        return "";
    }
} // namespace

EQGraph::EQGraph (EditorModel& m) : model (m)
{
    setWantsKeyboardFocus (true);
    setTitle ("EQ graph");
    setDescription ("Interactive frequency response. Double-click to create a band, drag nodes to edit.");
    setRepaintsOnMouseActivity (false);

    auto ui = model.getProcessor().getUiState();
    dbRange = (int) ui.getProperty ("dbRange", 12);
    for (size_t i = 0; i < rangeButtons.size(); ++i)
    {
        auto& b = rangeButtons[i];
        b.setButtonText (juce::String::fromUTF8 ("\xc2\xb1") + juce::String (kRanges[i]));
        b.setClickingTogglesState (true);
        b.setRadioGroupId (4711);
        b.setToggleState (kRanges[i] == dbRange, juce::dontSendNotification);
        b.setTooltip ("Graph range: +/-" + juce::String (kRanges[i]) + " dB");
        b.onClick = [this, i] { setDbRange (kRanges[i]); };
        addAndMakeVisible (b);
    }

    spectrum.setFifos (&model.getProcessor().getPreFifo(), &model.getProcessor().getPostFifo());
    spectrum.loadFrom (ui);
    model.addListener (this);
}

EQGraph::~EQGraph()
{
    model.removeListener (this);
    model.getProcessor().getPreFifo().setActive (false);
    model.getProcessor().getPostFifo().setActive (false);
}

void EQGraph::setDbRange (int r)
{
    dbRange = r;
    model.getProcessor().getUiState().setProperty ("dbRange", r, nullptr);
    for (size_t i = 0; i < rangeButtons.size(); ++i)
        rangeButtons[i].setToggleState (kRanges[i] == r, juce::dontSendNotification);
    signature.clear(); // force a recompute
    repaint();
}

//==============================================================================
juce::Rectangle<float> EQGraph::plotArea() const noexcept
{
    auto r = getLocalBounds().toFloat();
    r.removeFromLeft (34.0f);
    r.removeFromRight (40.0f);
    r.removeFromTop (8.0f);
    r.removeFromBottom (22.0f);
    return r;
}

float EQGraph::freqToX (double hz) const noexcept
{
    const auto p = plotArea();
    return p.getX() + p.getWidth() * (float) (std::log (hz / fMin) / std::log (fMax / fMin));
}

double EQGraph::xToFreq (float x) const noexcept
{
    const auto p = plotArea();
    return fMin * std::pow (fMax / fMin, (double) ((x - p.getX()) / p.getWidth()));
}

float EQGraph::dbToY (double db) const noexcept
{
    const auto p = plotArea();
    return p.getCentreY() - (float) (db / dbRange) * p.getHeight() * 0.46f;
}

double EQGraph::yToDb (float y) const noexcept
{
    const auto p = plotArea();
    return (p.getCentreY() - y) / (p.getHeight() * 0.46f) * dbRange;
}

void EQGraph::resized()
{
    int x = getWidth() - 40 - 4 * 38;
    for (auto& b : rangeButtons)
    {
        b.setBounds (x, 12, 36, 20);
        x += 38;
    }
    signature.clear();
}

//==============================================================================
std::vector<float> EQGraph::computeBandDb (const dsp::DesignSpec& s) const
{
    const auto c = dsp::designCascade (s, sampleRate);
    std::vector<float> out (freqs.size());
    for (size_t i = 0; i < freqs.size(); ++i)
        out[i] = (float) c.magnitudeDb (dsp::kTwoPi * freqs[i] / sampleRate);
    return out;
}

void EQGraph::recomputeCurves()
{
    const auto p = plotArea();
    const int n = juce::jlimit (128, 1600, (int) (p.getWidth() / 1.5f));
    freqs.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
        freqs[(size_t) i] = fMin * std::pow (fMax / fMin, (double) i / (n - 1));

    stereoSum.assign ((size_t) n, 0.0f);
    for (auto& d : domainSum)
        d.assign ((size_t) n, 0.0f);
    domainUsed.fill (false);

    for (int b = 0; b < dsp::kNumBands; ++b)
    {
        if (! model.isUsed (b))
        {
            bandDb[(size_t) b].clear();
            continue;
        }
        bandDb[(size_t) b] = computeBandDb (model.spec (b));
        if (! model.isOn (b))
            continue;
        const auto m = model.stereo (b);
        if (m == StereoMode::Stereo)
            for (int i = 0; i < n; ++i)
                stereoSum[(size_t) i] += bandDb[(size_t) b][(size_t) i];
        else
        {
            const int d = (int) m - 1;
            domainUsed[(size_t) d] = true;
            for (int i = 0; i < n; ++i)
                domainSum[(size_t) d][(size_t) i] += bandDb[(size_t) b][(size_t) i];
        }
    }
    for (int d = 0; d < 4; ++d)
        for (int i = 0; i < n; ++i)
            domainSum[(size_t) d][(size_t) i] += stereoSum[(size_t) i];
}

void EQGraph::recomputeDynamicCurves()
{
    auto& engine = model.getProcessor().getEngine();
    for (int b = 0; b < dsp::kNumBands; ++b)
    {
        const bool candidate = model.isActive (b) && model.get (b, BandParam::DynOn) > 0.5f
                            && dsp::typeSupportsDynamics (model.type (b));
        const float g = candidate ? engine.getBandDynamicGainDb (b) : 0.0f;
        bandDynGain[(size_t) b] = g;
        bandDynShown[(size_t) b] = candidate && std::abs (g) > 0.01f;
        if (bandDynShown[(size_t) b])
        {
            auto s = model.spec (b);
            s.gainDb = juce::jlimit (-30.0, 30.0, s.gainDb + g);
            bandDynDb[(size_t) b] = computeBandDb (s);
        }
    }
}

void EQGraph::refresh()
{
    const double fs = model.getProcessor().getSampleRateForUi();
    sampleRate = fs > 0.0 ? fs : 48000.0;
    fMax = juce::jmin (30000.0, 0.5 * sampleRate);

    std::vector<float> sig;
    sig.reserve (300);
    sig.push_back ((float) sampleRate);
    sig.push_back ((float) getWidth());
    sig.push_back ((float) getHeight());
    sig.push_back ((float) dbRange);
    auto& state = model.getState();
    for (int b = 0; b < dsp::kNumBands; ++b)
        for (int p = 0; p < params::kNumBandParams; ++p)
            sig.push_back (state.getRawParameterValue (params::bandParamId (b, (BandParam) p))->load());

    if (sig != signature)
    {
        signature = std::move (sig);
        recomputeCurves();
    }
    recomputeDynamicCurves();

    const auto p = plotArea();
    spectrum.update (sampleRate, juce::jlimit (64, 1200, (int) (p.getWidth() / 2.0f)), fMin, fMax, 1.0 / 30.0);
    lastAudition = model.getProcessor().getAuditionBand();
    repaint();
}

juce::Path EQGraph::makePath (const std::vector<float>& db) const
{
    juce::Path path;
    const auto p = plotArea();
    for (size_t i = 0; i < db.size(); ++i)
    {
        const float x = freqToX (freqs[i]);
        const float y = juce::jlimit (p.getY() - 50.0f, p.getBottom() + 50.0f, dbToY (db[i]));
        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }
    return path;
}

//==============================================================================
double EQGraph::nodeGainDb (int band) const
{
    const auto t = model.type (band);
    if (t == FilterType::Tilt)
        return 0.5 * model.get (band, BandParam::Gain); // high-frequency plateau
    return dsp::typeUsesGain (t) ? (double) model.get (band, BandParam::Gain) : 0.0;
}

juce::Point<float> EQGraph::nodePosition (int band) const
{
    const auto p = plotArea();
    const float x = juce::jlimit (p.getX(), p.getRight(), freqToX (model.get (band, BandParam::Freq)));
    const float y = juce::jlimit (p.getY() + 4.0f, p.getBottom() - 4.0f, dbToY (nodeGainDb (band)));
    return { x, y };
}

int EQGraph::hitTestNode (juce::Point<float> pt) const
{
    int best = -1;
    float bestD = kNodeRadius + 6.0f;
    // Prefer the selected band when nodes overlap.
    const int sel = model.getSelectedBand();
    for (int b = 0; b < dsp::kNumBands; ++b)
    {
        if (! model.isUsed (b))
            continue;
        const float d = nodePosition (b).getDistanceFrom (pt) - (b == sel ? 2.0f : 0.0f);
        if (d < bestD)
        {
            bestD = d;
            best = b;
        }
    }
    return best;
}

//==============================================================================
void EQGraph::drawGrid (juce::Graphics& g, juce::Rectangle<float> p)
{
    using namespace theme;
    g.setFont (font (11.0f));
    for (double decade = 10.0; decade <= 10000.0; decade *= 10.0)
        for (int k = 1; k <= 9; ++k)
        {
            const double f = decade * k;
            if (f < fMin || f > fMax)
                continue;
            const float x = freqToX (f);
            g.setColour (k == 1 ? gridMajor : gridMinor);
            g.drawVerticalLine ((int) std::round (x), p.getY(), p.getBottom());
        }
    for (double f : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        if (f < fMin || f > fMax)
            continue;
        const float x = freqToX (f);
        g.setColour (textDim);
        g.drawText (f >= 1000.0 ? juce::String ((int) (f / 1000.0)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (x - 20.0f, p.getBottom() + 3.0f, 40.0f, 16.0f), juce::Justification::centred);
    }

    const int step = dbRange <= 6 ? 2 : (dbRange <= 12 ? 3 : 6);
    for (int db = -dbRange; db <= dbRange; db += step)
    {
        const float y = dbToY (db);
        g.setColour (db == 0 ? gridZero : gridMinor);
        g.drawHorizontalLine ((int) std::round (y), p.getX(), p.getRight());
        g.setColour (db == 0 ? text : textDim);
        g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (p.getRight() + 4.0f, y - 8.0f, 34.0f, 16.0f),
                    juce::Justification::centredLeft);
    }

    if (spectrum.getOptions().enabled)
    {
        g.setColour (preSpectrum.withAlpha (0.8f));
        for (int db = -20; db >= -80; db -= 20)
        {
            const float t = (db - SpectrumView::kTopDb) / (SpectrumView::kBottomDb - SpectrumView::kTopDb);
            const float y = p.getY() + t * p.getHeight();
            g.drawText (juce::String (db), juce::Rectangle<float> (0.0f, y - 8.0f, 30.0f, 16.0f), juce::Justification::centredRight);
        }
        g.drawText ("dBFS", juce::Rectangle<float> (0.0f, p.getY(), 32.0f, 14.0f), juce::Justification::centredRight);
    }
}

void EQGraph::paint (juce::Graphics& g)
{
    using namespace theme;
    const auto bounds = getLocalBounds().toFloat();
    const auto p = plotArea();
    g.setGradientFill (juce::ColourGradient (graphTop, 0, bounds.getY(), graphBottom, 0, bounds.getBottom(), false));
    g.fillRect (bounds);

    drawGrid (g, p);
    spectrum.draw (g, p, [this] (double f) { return freqToX (f); });

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (p.toNearestInt());
        const int sel = model.getSelectedBand();
        const float zeroY = dbToY (0.0);

        // Individual band responses
        for (int b = 0; b < dsp::kNumBands; ++b)
        {
            const auto& db = bandDb[(size_t) b];
            if (db.empty())
                continue;
            const auto col = bandColour (b);
            const bool on = model.isOn (b);
            auto path = makePath (db);
            if (b == sel && on)
            {
                juce::Path fill (path);
                fill.lineTo (freqToX (freqs.back()), zeroY);
                fill.lineTo (freqToX (freqs.front()), zeroY);
                fill.closeSubPath();
                g.setColour (col.withAlpha (0.16f));
                g.fillPath (fill);
            }
            g.setColour (col.withAlpha (on ? (b == sel ? 0.95f : 0.55f) : 0.22f));
            if (on)
                g.strokePath (path, juce::PathStrokeType (b == sel ? 1.6f : 1.1f));
            else
            {
                juce::Path dashed;
                const float dashes[] = { 4.0f, 4.0f };
                juce::PathStrokeType (1.0f).createDashedStroke (dashed, path, dashes, 2);
                g.fillPath (dashed);
            }

            if (bandDynShown[(size_t) b])
            {
                auto dyn = makePath (bandDynDb[(size_t) b]);
                g.setColour (col.withAlpha (0.95f));
                g.strokePath (dyn, juce::PathStrokeType (1.6f));
            }
        }

        // Combined responses per channel domain
        const juce::Colour domainColours[] = { leftCurve, rightCurve, midCurve, sideCurve };
        for (int d = 0; d < 4; ++d)
        {
            if (! domainUsed[(size_t) d])
                continue;
            g.setColour (domainColours[d].withAlpha (0.9f));
            g.strokePath (makePath (domainSum[(size_t) d]), juce::PathStrokeType (1.7f));
        }
        if (! stereoSum.empty())
        {
            auto main = makePath (stereoSum);
            g.setColour (curve.withAlpha (0.12f));
            g.strokePath (main, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (curve);
            g.strokePath (main, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // Nodes
    const int sel = model.getSelectedBand();
    for (int b = 0; b < dsp::kNumBands; ++b)
    {
        if (! model.isUsed (b))
            continue;
        const auto col = bandColour (b);
        const auto pos = nodePosition (b);
        const bool on = model.isOn (b);
        const float r = kNodeRadius + (b == sel ? 2.0f : 0.0f);

        if (bandDynShown[(size_t) b])
        {
            const float dynY = juce::jlimit (p.getY(), p.getBottom(), dbToY (nodeGainDb (b) + bandDynGain[(size_t) b] * (model.type (b) == FilterType::Tilt ? 0.5 : 1.0)));
            g.setColour (col.withAlpha (0.8f));
            g.drawLine (pos.x, pos.y, pos.x, dynY, 2.0f);
            g.fillEllipse (pos.x - 3.0f, dynY - 3.0f, 6.0f, 6.0f);
        }
        if (b == hoverBand || b == sel)
        {
            g.setColour (col.withAlpha (0.25f));
            g.fillEllipse (pos.x - r - 5.0f, pos.y - r - 5.0f, 2 * (r + 5.0f), 2 * (r + 5.0f));
        }
        g.setColour (on ? col : panel);
        g.fillEllipse (pos.x - r, pos.y - r, 2 * r, 2 * r);
        g.setColour (on ? col.brighter (0.4f) : col.withAlpha (0.8f));
        g.drawEllipse (pos.x - r, pos.y - r, 2 * r, 2 * r, b == sel ? 2.0f : 1.2f);
        g.setColour (on ? juce::Colour (0xff10141a) : col);
        g.setFont (font (10.5f, true));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (pos.x - r, pos.y - r, 2 * r, 2 * r), juce::Justification::centred);
        if (const auto* letter = stereoLetter (model.stereo (b)); letter[0] != 0)
        {
            g.setColour (col);
            g.setFont (font (10.0f, true));
            g.drawText (letter, juce::Rectangle<float> (pos.x + r, pos.y - r - 10.0f, 12.0f, 12.0f), juce::Justification::centredLeft);
        }
    }

    drawReadouts (g, p);
}

void EQGraph::drawReadouts (juce::Graphics& g, juce::Rectangle<float> p)
{
    using namespace theme;
    const int band = hoverBand >= 0 ? hoverBand : model.getSelectedBand();
    if (band >= 0 && model.isUsed (band))
    {
        const auto t = model.type (band);
        juce::String s = juce::String (band + 1) + "  " + dsp::filterTypeName (t) + "   " + params::formatFrequency (model.get (band, BandParam::Freq));
        if (dsp::typeUsesGain (t))
            s << "   " << params::formatGain (model.get (band, BandParam::Gain));
        if (dsp::typeUsesQ (t, model.slope (band)))
            s << "   Q " << params::formatQ (model.get (band, BandParam::Q));
        if (dsp::typeUsesSlope (t))
            s << "   " << dsp::kSlopesDbPerOct[(size_t) model.slope (band)] << " dB/oct";
        if (model.stereo (band) != StereoMode::Stereo)
            s << "   " << dsp::stereoModeName (model.stereo (band));
        if (bandDynShown[(size_t) band])
            s << "   dyn " << params::formatGain (bandDynGain[(size_t) band]);
        if (! model.isOn (band))
            s << "   (off)";
        g.setFont (font (13.0f, true));
        g.setColour (bandColour (band));
        g.drawText (s, juce::Rectangle<float> (p.getX() + 8.0f, p.getY() + 4.0f, p.getWidth() - 200.0f, 18.0f), juce::Justification::centredLeft);
    }

    if (mouseInside && dragBand < 0 && p.contains (mousePos))
    {
        const juce::String s = params::formatFrequency (xToFreq (mousePos.x)) + "   " + params::formatGain (yToDb (mousePos.y));
        g.setFont (font (12.0f));
        g.setColour (textDim);
        g.drawText (s, juce::Rectangle<float> (p.getX() + 8.0f, p.getBottom() - 20.0f, 220.0f, 16.0f), juce::Justification::centredLeft);
    }

    const int aud = model.getProcessor().getAuditionBand();
    if (aud >= 0 && model.isUsed (aud))
    {
        const juce::String s = "AUDITION   Band " + juce::String (aud + 1) + "  " + params::formatFrequency (model.get (aud, BandParam::Freq))
                             + "  " + dsp::stereoModeName (model.stereo (aud)) + "   -   monitoring only, not rendered or saved";
        auto r = juce::Rectangle<float> (0, 0, juce::jmin (p.getWidth() - 40.0f, 560.0f), 22.0f).withCentre ({ p.getCentreX(), p.getY() + 34.0f });
        g.setColour (accent);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colour (0xff15110a));
        g.setFont (font (12.5f, true));
        g.drawText (s, r, juce::Justification::centred);
    }

    int usedCount = 0;
    for (int b = 0; b < dsp::kNumBands; ++b)
        usedCount += model.isUsed (b) ? 1 : 0;
    if (usedCount == 0)
    {
        g.setColour (textDim);
        g.setFont (font (15.0f));
        g.drawText ("Double-click anywhere on the graph to create a band", p, juce::Justification::centred);
    }
}

//==============================================================================
void EQGraph::mouseMove (const juce::MouseEvent& e)
{
    mouseInside = true;
    mousePos = e.position;
    const int h = hitTestNode (e.position);
    if (h != hoverBand)
        hoverBand = h;
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
    repaint();
}

void EQGraph::mouseExit (const juce::MouseEvent&)
{
    mouseInside = false;
    hoverBand = -1;
    repaint();
}

void EQGraph::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const int hit = hitTestNode (e.position);
    if (e.mods.isPopupMenu())
    {
        if (hit >= 0)
        {
            model.setSelectedBand (hit);
            showBandMenu (hit);
        }
        else
            showGraphMenu (e.position);
        return;
    }
    if (hit < 0)
        return;

    model.setSelectedBand (hit);
    dragBand = hit;
    dragAnchor = e.position;
    dragFine = e.mods.isShiftDown();
    dragGainIsQ = ! dsp::typeUsesGain (model.type (hit));
    dragStartLog2F = std::log2 ((double) model.get (hit, BandParam::Freq));
    dragStartGain = model.get (hit, BandParam::Gain);
    dragStartLogQ = std::log ((double) model.get (hit, BandParam::Q));
    model.beginGesture (hit, BandParam::Freq);
    model.beginGesture (hit, dragGainIsQ ? BandParam::Q : BandParam::Gain);
}

void EQGraph::mouseDrag (const juce::MouseEvent& e)
{
    mousePos = e.position;
    if (dragBand < 0)
        return;
    const int b = dragBand;
    if (e.mods.isShiftDown() != dragFine)
    {
        dragFine = e.mods.isShiftDown();
        dragAnchor = e.position;
        dragStartLog2F = std::log2 ((double) model.get (b, BandParam::Freq));
        dragStartGain = model.get (b, BandParam::Gain);
        dragStartLogQ = std::log ((double) model.get (b, BandParam::Q));
        return;
    }
    const float scale = dragFine ? 0.1f : 1.0f;
    float dx = (e.position.x - dragAnchor.x) * scale;
    float dy = (e.position.y - dragAnchor.y) * scale;
    if (e.mods.isAltDown())
        (std::abs (dx) > std::abs (dy) ? dy : dx) = 0.0f;

    const auto p = plotArea();
    const double pxPerOct = p.getWidth() / std::log2 (fMax / fMin);
    const double log2f = juce::jlimit (std::log2 (dsp::kMinFreqHz), std::log2 (dsp::kMaxFreqHz), dragStartLog2F + dx / pxPerOct);
    model.setDuringGesture (b, BandParam::Freq, (float) std::exp2 (log2f));

    if (dragGainIsQ)
    {
        const double q = std::exp (juce::jlimit (std::log (dsp::kMinQ), std::log (dsp::kMaxQ), dragStartLogQ - dy / 90.0));
        model.setDuringGesture (b, BandParam::Q, (float) q);
    }
    else
    {
        const double dbPerPx = dbRange / (p.getHeight() * 0.46);
        const double factor = model.type (b) == FilterType::Tilt ? 2.0 : 1.0;
        const double gain = juce::jlimit (dsp::kMinGainDb, dsp::kMaxGainDb, dragStartGain - dy * dbPerPx * factor);
        model.setDuringGesture (b, BandParam::Gain, (float) gain);
    }
}

void EQGraph::mouseUp (const juce::MouseEvent&)
{
    if (dragBand >= 0)
    {
        model.endGesture (dragBand, BandParam::Freq);
        model.endGesture (dragBand, dragGainIsQ ? BandParam::Q : BandParam::Gain);
    }
    dragBand = -1;
}

void EQGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int hit = hitTestNode (e.position);
    if (hit >= 0)
    {
        model.toggleBand (hit);
        return;
    }
    if (! plotArea().contains (e.position))
        return;
    const double f = xToFreq (e.position.x);
    const double gdb = yToDb (e.position.y);
    const auto t = f < 25.0 ? FilterType::HighPass : (f > 18000.0 ? FilterType::LowPass : FilterType::Bell);
    model.createBand (f, gdb, t);
}

void EQGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int hit = hitTestNode (e.position);
    if (hit < 0)
    {
        Component::mouseWheelMove (e, w);
        return;
    }
    const double steps = (w.deltaY > 0 ? 1.0 : -1.0) * (w.isReversed ? -1.0 : 1.0);
    const double q = model.get (hit, BandParam::Q) * std::pow (2.0, steps * (e.mods.isShiftDown() ? 1.0 / 24.0 : 1.0 / 6.0));
    model.set (hit, BandParam::Q, (float) juce::jlimit (dsp::kMinQ, dsp::kMaxQ, q));
}

void EQGraph::nudge (int b, double octaves, double gainDb, double qFactor)
{
    if (std::abs (octaves) > 0.0)
        model.set (b, BandParam::Freq, (float) juce::jlimit (dsp::kMinFreqHz, dsp::kMaxFreqHz, model.get (b, BandParam::Freq) * std::exp2 (octaves)));
    if (std::abs (gainDb) > 0.0)
        model.set (b, BandParam::Gain, (float) juce::jlimit (dsp::kMinGainDb, dsp::kMaxGainDb, model.get (b, BandParam::Gain) + gainDb));
    if (std::abs (qFactor - 1.0) > 0.0)
        model.set (b, BandParam::Q, (float) juce::jlimit (dsp::kMinQ, dsp::kMaxQ, model.get (b, BandParam::Q) * qFactor));
}

bool EQGraph::keyPressed (const juce::KeyPress& k)
{
    const int b = model.getSelectedBand();
    const bool fine = k.getModifiers().isShiftDown();
    auto cycle = [this] (int dir)
    {
        const int start = model.getSelectedBand();
        for (int i = 1; i <= dsp::kNumBands; ++i)
        {
            const int c = ((start < 0 ? (dir > 0 ? -1 : 0) : start) + dir * i + 2 * dsp::kNumBands) % dsp::kNumBands;
            if (model.isUsed (c))
            {
                model.setSelectedBand (c);
                return;
            }
        }
    };
    if (k.getKeyCode() == juce::KeyPress::tabKey)
    {
        cycle (k.getModifiers().isShiftDown() ? -1 : 1);
        return true;
    }
    if (b < 0 || ! model.isUsed (b))
        return false;
    const bool gainType = dsp::typeUsesGain (model.type (b));
    if (k.getKeyCode() == juce::KeyPress::leftKey)       nudge (b, fine ? -1.0 / 96 : -1.0 / 24, 0, 1);
    else if (k.getKeyCode() == juce::KeyPress::rightKey) nudge (b, fine ? 1.0 / 96 : 1.0 / 24, 0, 1);
    else if (k.getKeyCode() == juce::KeyPress::upKey)    gainType ? nudge (b, 0, fine ? 0.1 : 0.5, 1) : nudge (b, 0, 0, std::exp2 (1.0 / 12));
    else if (k.getKeyCode() == juce::KeyPress::downKey)  gainType ? nudge (b, 0, fine ? -0.1 : -0.5, 1) : nudge (b, 0, 0, std::exp2 (-1.0 / 12));
    else if (k.getKeyCode() == juce::KeyPress::pageUpKey)   nudge (b, 0, 0, std::exp2 (fine ? 1.0 / 48 : 1.0 / 12));
    else if (k.getKeyCode() == juce::KeyPress::pageDownKey) nudge (b, 0, 0, std::exp2 (fine ? -1.0 / 48 : -1.0 / 12));
    else if (k.getKeyCode() == juce::KeyPress::deleteKey || k.getKeyCode() == juce::KeyPress::backspaceKey) model.deleteBand (b);
    else if (k.getKeyCode() == juce::KeyPress::spaceKey) model.toggleBand (b);
    else if (k.getTextCharacter() == 's' || k.getTextCharacter() == 'S')
    {
        auto& proc = model.getProcessor();
        proc.setAuditionBand (proc.getAuditionBand() == b ? -1 : b);
    }
    else
        return false;
    return true;
}

//==============================================================================
void EQGraph::showBandMenu (int b)
{
    juce::PopupMenu m, types, slopes, modes;
    const auto t = model.type (b);
    for (int i = 0; i < (int) FilterType::NumTypes; ++i)
        types.addItem (100 + i, dsp::filterTypeName ((FilterType) i), true, (int) t == i);
    for (int i = 0; i < (int) dsp::kSlopesDbPerOct.size(); ++i)
    {
        const bool allowed = dsp::typeUsesSlope (t) && ! (t != FilterType::HighPass && t != FilterType::LowPass && i > dsp::kMaxShelfSlopeIndex);
        slopes.addItem (200 + i, juce::String (dsp::kSlopesDbPerOct[(size_t) i]) + " dB/oct", allowed, model.slope (b) == i);
    }
    for (int i = 0; i < (int) StereoMode::NumModes; ++i)
        modes.addItem (300 + i, dsp::stereoModeName ((StereoMode) i), true, (int) model.stereo (b) == i);

    auto& proc = model.getProcessor();
    m.addSectionHeader ("Band " + juce::String (b + 1));
    m.addSubMenu ("Filter type", types);
    m.addSubMenu ("Slope", slopes, dsp::typeUsesSlope (t));
    m.addSubMenu ("Stereo mode", modes);
    m.addSeparator();
    m.addItem (1, "Enabled", true, model.isOn (b));
    m.addItem (2, "Dynamic EQ", dsp::typeSupportsDynamics (t), model.get (b, BandParam::DynOn) > 0.5f);
    m.addItem (3, "Audition (solo)", true, proc.getAuditionBand() == b);
    m.addSeparator();
    m.addItem (4, "Reset band to defaults");
    m.addItem (5, "Delete band");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [sp = juce::Component::SafePointer<EQGraph> (this), b] (int r)
                     {
                         if (sp == nullptr || r == 0)
                             return;
                         auto& md = sp->model;
                         auto& pr = md.getProcessor();
                         if (r >= 100 && r < 200)      md.set (b, BandParam::Type, (float) (r - 100));
                         else if (r >= 200 && r < 300) md.set (b, BandParam::Slope, (float) (r - 200));
                         else if (r >= 300 && r < 400) md.set (b, BandParam::Stereo, (float) (r - 300));
                         else if (r == 1) md.toggleBand (b);
                         else if (r == 2) md.set (b, BandParam::DynOn, md.get (b, BandParam::DynOn) > 0.5f ? 0.0f : 1.0f);
                         else if (r == 3) pr.setAuditionBand (pr.getAuditionBand() == b ? -1 : b);
                         else if (r == 4)
                         {
                             const auto f = md.get (b, BandParam::Freq);
                             md.resetBand (b);
                             md.set (b, BandParam::Freq, f);
                         }
                         else if (r == 5) md.deleteBand (b);
                     });
}

void EQGraph::showGraphMenu (juce::Point<float> where)
{
    const double f = xToFreq (where.x), gdb = yToDb (where.y);
    juce::PopupMenu m, add, range, an, fft, smooth, release, tilt;
    add.addItem (1, "Bell");
    add.addItem (2, "Low shelf");
    add.addItem (3, "High shelf");
    add.addItem (4, "High pass");
    add.addItem (5, "Low pass");
    add.addItem (6, "Notch");
    add.addItem (7, "Tilt");
    for (int i = 0; i < 4; ++i)
        range.addItem (20 + i, juce::String::fromUTF8 ("\xc2\xb1") + juce::String (kRanges[i]) + " dB", true, dbRange == kRanges[i]);

    const auto o = spectrum.getOptions();
    an.addItem (40, "Analyzer enabled", true, o.enabled);
    an.addItem (41, "Show input (pre EQ)", o.enabled, o.showPre);
    an.addItem (42, "Show output (post EQ)", o.enabled, o.showPost);
    an.addItem (43, "Freeze", o.enabled, o.freeze);
    an.addItem (44, "Peak hold", o.enabled, o.analyzer.peakHold);
    an.addItem (45, "Reset peaks", o.enabled && o.analyzer.peakHold);
    const int sizes[] = { 2048, 4096, 8192, 16384, 32768 };
    for (int i = 0; i < 5; ++i)
        fft.addItem (50 + i, juce::String (sizes[i]) + " points", true, o.analyzer.fftSize == sizes[i]);
    an.addSubMenu ("Resolution (FFT size)", fft, o.enabled);
    const double sm[] = { 0.0, 1.0 / 12.0, 1.0 / 6.0, 1.0 / 3.0 };
    const char* smName[] = { "Off (peak per pixel)", "1/12 octave", "1/6 octave", "1/3 octave" };
    for (int i = 0; i < 4; ++i)
        smooth.addItem (60 + i, smName[i], true, std::abs (o.analyzer.smoothingOctaves - sm[i]) < 1e-6);
    an.addSubMenu ("Smoothing", smooth, o.enabled);
    const double rel[] = { 10.0, 20.0, 30.0, 60.0, 120.0 };
    for (int i = 0; i < 5; ++i)
        release.addItem (70 + i, juce::String ((int) rel[i]) + " dB/s", true, dsp::sameValue (o.analyzer.releaseDbPerSec, rel[i]));
    an.addSubMenu ("Decay", release, o.enabled);
    const double tl[] = { 0.0, 3.0, 4.5 };
    for (int i = 0; i < 3; ++i)
        tilt.addItem (80 + i, i == 0 ? juce::String ("Off (calibrated)") : juce::String (tl[i], 1) + " dB/oct (display only)", true,
                      dsp::sameValue (o.analyzer.slopeDbPerOct, tl[i]));
    an.addSubMenu ("Display tilt", tilt, o.enabled);

    m.addSubMenu ("Add band here", add);
    m.addSubMenu ("Display range", range);
    m.addSubMenu ("Analyzer", an);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [sp = juce::Component::SafePointer<EQGraph> (this), f, gdb] (int r)
                     {
                         if (sp == nullptr || r == 0)
                             return;
                         const FilterType adds[] = { FilterType::Bell, FilterType::LowShelf, FilterType::HighShelf, FilterType::HighPass,
                                                     FilterType::LowPass, FilterType::Notch, FilterType::Tilt };
                         if (r >= 1 && r <= 7)
                         {
                             sp->model.createBand (f, gdb, adds[r - 1]);
                             return;
                         }
                         if (r >= 20 && r < 24)
                         {
                             sp->setDbRange (kRanges[r - 20]);
                             return;
                         }
                         auto o2 = sp->spectrum.getOptions();
                         const int sizes2[] = { 2048, 4096, 8192, 16384, 32768 };
                         const double sm2[] = { 0.0, 1.0 / 12.0, 1.0 / 6.0, 1.0 / 3.0 };
                         const double rel2[] = { 10.0, 20.0, 30.0, 60.0, 120.0 };
                         const double tl2[] = { 0.0, 3.0, 4.5 };
                         if (r == 40) o2.enabled = ! o2.enabled;
                         else if (r == 41) o2.showPre = ! o2.showPre;
                         else if (r == 42) o2.showPost = ! o2.showPost;
                         else if (r == 43) o2.freeze = ! o2.freeze;
                         else if (r == 44) o2.analyzer.peakHold = ! o2.analyzer.peakHold;
                         else if (r == 45) sp->spectrum.resetPeaks();
                         else if (r >= 50 && r < 55) o2.analyzer.fftSize = sizes2[r - 50];
                         else if (r >= 60 && r < 64) o2.analyzer.smoothingOctaves = sm2[r - 60];
                         else if (r >= 70 && r < 75) o2.analyzer.releaseDbPerSec = rel2[r - 70];
                         else if (r >= 80 && r < 83) o2.analyzer.slopeDbPerOct = tl2[r - 80];
                         sp->spectrum.setOptions (o2);
                         auto ui = sp->model.getProcessor().getUiState();
                         sp->spectrum.storeTo (ui);
                     });
}

} // namespace mepro::ui
