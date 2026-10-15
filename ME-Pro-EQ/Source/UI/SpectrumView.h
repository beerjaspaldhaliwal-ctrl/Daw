// ME Pro EQ — ME Audio
// Draws the pre/post EQ spectrum behind the EQ curves. Pulls audio from the processor's analyser FIFOs on
// the UI thread; FFTs never run on the audio thread.
#pragma once

#include "DSP/SpectrumAnalyzer.h"
#include "DSP/ToneAnalysis.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace mepro::ui
{

class SpectrumView
{
public:
    struct Options
    {
        bool enabled = true;
        bool showPre = true;
        bool showPost = true;
        bool freeze = false;
        dsp::SpectrumAnalyzer::Settings analyzer;
    };

    static constexpr float kTopDb = 0.0f;
    static constexpr float kBottomDb = -100.0f;

    void setFifos (dsp::AnalyzerFifo* pre, dsp::AnalyzerFifo* post) noexcept;
    // While set, the post-EQ audio is also fed to this analysis (even if the display is off).
    void setCapture (dsp::ToneAnalysis* a) noexcept { capture = a; }
    void setOptions (const Options& o);
    const Options& getOptions() const noexcept { return opts; }

    // UI thread: (re)configure if needed, pull new audio, advance display ballistics.
    void update (double sampleRate, int numPoints, double fMin, double fMax, double dt);
    void draw (juce::Graphics& g, juce::Rectangle<float> plot, const std::function<float (double)>& freqToX) const;
    void resetPeaks();

    void storeTo (juce::ValueTree& v) const;
    void loadFrom (const juce::ValueTree& v);

private:
    void drawCurve (juce::Graphics& g, juce::Rectangle<float> plot, const std::function<float (double)>& freqToX,
                    const std::vector<float>& db, juce::Colour c, bool fill, float alpha) const;

    Options opts;
    dsp::AnalyzerFifo* preFifo = nullptr;
    dsp::AnalyzerFifo* postFifo = nullptr;
    dsp::ToneAnalysis* capture = nullptr;
    dsp::SpectrumAnalyzer pre, post;
    std::vector<float> pullBuffer;
    double cfgRate = 0.0, cfgFMin = 0.0, cfgFMax = 0.0;
    int cfgPoints = 0;
    bool needsConfigure = true;
};

} // namespace mepro::ui
