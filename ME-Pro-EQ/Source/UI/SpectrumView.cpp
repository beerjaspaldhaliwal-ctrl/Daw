// ME Pro EQ — ME Audio
#include "SpectrumView.h"
#include "Theme.h"

#include "DSP/MathUtils.h"

namespace mepro::ui
{

void SpectrumView::setFifos (dsp::AnalyzerFifo* p, dsp::AnalyzerFifo* q) noexcept
{
    preFifo = p;
    postFifo = q;
}

void SpectrumView::setOptions (const Options& o)
{
    using dsp::sameValue;
    const bool reconfig = o.analyzer.fftSize != opts.analyzer.fftSize || ! sameValue (o.analyzer.smoothingOctaves, opts.analyzer.smoothingOctaves)
                       || ! sameValue (o.analyzer.slopeDbPerOct, opts.analyzer.slopeDbPerOct) || o.analyzer.peakHold != opts.analyzer.peakHold
                       || ! sameValue (o.analyzer.releaseDbPerSec, opts.analyzer.releaseDbPerSec);
    opts = o;
    needsConfigure = needsConfigure || reconfig;
}

void SpectrumView::resetPeaks()
{
    pre.resetPeaks();
    post.resetPeaks();
}

void SpectrumView::update (double sampleRate, int numPoints, double fMin, double fMax, double dt)
{
    const bool active = opts.enabled && ! opts.freeze;
    if (preFifo != nullptr)
        preFifo->setActive (opts.enabled && opts.showPre);
    if (postFifo != nullptr)
        postFifo->setActive (opts.enabled && opts.showPost);
    if (! opts.enabled)
        return;

    if (needsConfigure || ! dsp::sameValue (sampleRate, cfgRate) || numPoints != cfgPoints || ! dsp::sameValue (fMin, cfgFMin)
        || ! dsp::sameValue (fMax, cfgFMax))
    {
        pre.configure (opts.analyzer, sampleRate, numPoints, fMin, fMax);
        post.configure (opts.analyzer, sampleRate, numPoints, fMin, fMax);
        cfgRate = sampleRate;
        cfgPoints = numPoints;
        cfgFMin = fMin;
        cfgFMax = fMax;
        needsConfigure = false;
    }

    const int chunk = 4096;
    pullBuffer.resize ((size_t) chunk * 2);
    auto drain = [&] (dsp::AnalyzerFifo* f, dsp::SpectrumAnalyzer& a)
    {
        if (f == nullptr)
            return;
        for (int guard = 0; guard < 64; ++guard)
        {
            const int n = f->pull (pullBuffer.data(), chunk);
            if (n <= 0)
                break;
            if (active)
                a.feed (pullBuffer.data(), n);
        }
    };
    drain (preFifo, pre);
    drain (postFifo, post);
    if (active)
    {
        pre.updateDisplay (dt);
        post.updateDisplay (dt);
    }
}

void SpectrumView::drawCurve (juce::Graphics& g, juce::Rectangle<float> plot, const std::function<float (double)>& freqToX,
                              const std::vector<float>& db, juce::Colour c, bool fill, float alpha) const
{
    const auto& freqs = post.getDisplayFreqs();
    if (db.empty() || freqs.size() != db.size())
        return;
    auto yOf = [&plot] (float v)
    {
        const float t = (v - kTopDb) / (kBottomDb - kTopDb);
        return plot.getY() + juce::jlimit (0.0f, 1.0f, t) * plot.getHeight();
    };
    juce::Path p;
    bool started = false;
    for (size_t i = 0; i < db.size(); ++i)
    {
        const float x = freqToX (freqs[i]);
        const float y = yOf (db[i]);
        if (! started)
        {
            p.startNewSubPath (x, y);
            started = true;
        }
        else
            p.lineTo (x, y);
    }
    if (fill)
    {
        juce::Path f (p);
        f.lineTo (freqToX (freqs.back()), plot.getBottom());
        f.lineTo (freqToX (freqs.front()), plot.getBottom());
        f.closeSubPath();
        g.setGradientFill (juce::ColourGradient (c.withAlpha (alpha * 0.55f), 0, plot.getY(), c.withAlpha (alpha * 0.08f), 0, plot.getBottom(), false));
        g.fillPath (f);
    }
    g.setColour (c.withAlpha (alpha));
    g.strokePath (p, juce::PathStrokeType (1.0f));
}

void SpectrumView::draw (juce::Graphics& g, juce::Rectangle<float> plot, const std::function<float (double)>& freqToX) const
{
    if (! opts.enabled)
        return;
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (plot.toNearestInt());
    if (opts.showPre)
        drawCurve (g, plot, freqToX, pre.getDisplayDb(), theme::preSpectrum, false, 0.75f);
    if (opts.showPost)
        drawCurve (g, plot, freqToX, post.getDisplayDb(), theme::postSpectrum, true, 0.55f);
    if (opts.analyzer.peakHold && opts.showPost)
        drawCurve (g, plot, freqToX, post.getPeakDb(), theme::postSpectrum.brighter (0.3f), false, 0.5f);
}

void SpectrumView::storeTo (juce::ValueTree& v) const
{
    v.setProperty ("anOn", opts.enabled, nullptr);
    v.setProperty ("anPre", opts.showPre, nullptr);
    v.setProperty ("anPost", opts.showPost, nullptr);
    v.setProperty ("anFft", opts.analyzer.fftSize, nullptr);
    v.setProperty ("anSmooth", opts.analyzer.smoothingOctaves, nullptr);
    v.setProperty ("anRelease", opts.analyzer.releaseDbPerSec, nullptr);
    v.setProperty ("anPeak", opts.analyzer.peakHold, nullptr);
    v.setProperty ("anSlope", opts.analyzer.slopeDbPerOct, nullptr);
}

void SpectrumView::loadFrom (const juce::ValueTree& v)
{
    Options o;
    o.enabled = v.getProperty ("anOn", true);
    o.showPre = v.getProperty ("anPre", true);
    o.showPost = v.getProperty ("anPost", true);
    o.analyzer.fftSize = v.getProperty ("anFft", 8192);
    o.analyzer.smoothingOctaves = v.getProperty ("anSmooth", 1.0 / 6.0);
    o.analyzer.releaseDbPerSec = v.getProperty ("anRelease", 30.0);
    o.analyzer.peakHold = v.getProperty ("anPeak", false);
    o.analyzer.slopeDbPerOct = v.getProperty ("anSlope", 0.0);
    setOptions (o);
}

} // namespace mepro::ui
