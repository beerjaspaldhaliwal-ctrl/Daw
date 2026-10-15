// ME Pro EQ — ME Audio
// Interactive frequency-response graph.
//
// Every curve is the exact digital response of the coefficients the audio engine uses (same design code),
// evaluated at the current sample rate; dynamic bands additionally show the response at the gain the
// engine is applying right now.
//
// Mouse:   double-click empty area      create a band (bell; below 25 Hz high-pass, above 18 kHz low-pass)
//          drag node                    frequency (horizontal) and gain (vertical; Q for non-gain types)
//          Shift + drag                 fine adjustment          Alt + drag   lock to the dominant axis
//          wheel over node              Q (Shift = fine)
//          double-click node            enable / disable band
//          right-click                  band menu (type, slope, stereo, dynamics, audition, reset, delete)
//                                       or graph menu (add band, display range, analyzer settings)
// Keys:    arrows  frequency / gain     PageUp/PageDown  Q        Delete  delete band
//          Space   enable/disable       Tab / Shift+Tab  next / previous band     S  audition (solo)
#pragma once

#include "EditorModel.h"
#include "SpectrumView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <vector>

namespace mepro::ui
{

class EQGraph final : public juce::Component, private EditorModel::Listener
{
public:
    explicit EQGraph (EditorModel& model);
    ~EQGraph() override;

    void refresh(); // called from the editor's UI timer

    void setDbRange (int range);
    int getDbRange() const noexcept { return dbRange; }

    SpectrumView& getSpectrumView() noexcept { return spectrum; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    // Mapping (public for tests / overlays)
    float freqToX (double hz) const noexcept;
    double xToFreq (float x) const noexcept;
    float dbToY (double db) const noexcept;
    double yToDb (float y) const noexcept;
    juce::Rectangle<float> plotArea() const noexcept;

private:
    void selectedBandChanged (int) override { repaint(); }

    void recomputeCurves();
    void recomputeDynamicCurves();
    std::vector<float> computeBandDb (const dsp::DesignSpec& s) const;
    juce::Path makePath (const std::vector<float>& db) const;

    double nodeGainDb (int band) const;
    juce::Point<float> nodePosition (int band) const;
    int hitTestNode (juce::Point<float> p) const;

    void showBandMenu (int band);
    void showGraphMenu (juce::Point<float> where);
    void nudge (int band, double octaves, double gainDb, double qFactor);

    void drawGrid (juce::Graphics&, juce::Rectangle<float> plot);
    void drawReadouts (juce::Graphics&, juce::Rectangle<float> plot);

    EditorModel& model;
    SpectrumView spectrum;
    std::array<juce::TextButton, 4> rangeButtons;

    double sampleRate = 48000.0;
    double fMin = 10.0, fMax = 22050.0;
    int dbRange = 12;

    std::vector<double> freqs;            // x grid (Hz)
    std::array<std::vector<float>, 16> bandDb;
    std::array<std::vector<float>, 16> bandDynDb;
    std::array<bool, 16> bandDynShown {};
    std::array<float, 16> bandDynGain {};
    std::vector<float> stereoSum;
    std::array<std::vector<float>, 4> domainSum; // L, R, M, S (each includes the stereo-linked bands)
    std::array<bool, 4> domainUsed {};
    std::vector<float> signature;
    int lastAudition = -2;

    int hoverBand = -1;
    juce::Point<float> mousePos;
    bool mouseInside = false;

    int dragBand = -1;
    bool dragGainIsQ = false;
    bool dragFine = false;
    juce::Point<float> dragAnchor;
    double dragStartLog2F = 0.0, dragStartGain = 0.0, dragStartLogQ = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQGraph)
};

} // namespace mepro::ui
