// ME Pro EQ — ME Audio
// Analysis suggestions: listens to the plugin's output for a few seconds and proposes EQ moves, each with
// the measured evidence behind it. Nothing is ever applied automatically - the editor shows the list and
// the user applies (undoably) or dismisses each item.
//
// Measurements (UI thread, from the post-EQ analyser stream, mono sum):
//   * long-term average spectrum (LTAS): Hann-windowed FFT, ~2.9 Hz bins at every sample rate, 75 % overlap,
//     condensed to 1/24-octave bands (power sums). Frames quieter than -70 dBFS are ignored.
//   * per-frame 1/24-octave band levels (long frames, ~0.34 s) for persistence statistics;
//   * per-frame 1/6-octave band levels from 1 to 16 kHz (short frames, ~43 ms) for dynamic statistics.
//
// Rules (all levels relative to the material itself - no genre "target curve"):
//   Trend      robust straight-line fit of the 1/3-octave-smoothed LTAS against log-frequency, 100 Hz-8 kHz,
//              leaving out the region under test.
//   High-pass  the main content starts at f_low (first 1/3-oct level within 15 dB of the strongest region,
//              40 Hz-2 kHz); if f_low >= 50 Hz and there is energy below f_low/2 within 45 dB of that
//              region -> 18 dB/oct high-pass at 0.6 * f_low.
//   Resonance  narrow peak (1/24-oct LTAS >= 6 dB above the median of its +-1/2-octave surroundings, outside
//              +-1/6 octave), present in >= 60 % of the analysed frames -> bell cut of 0.7 x the excess at the
//              measured width (Q 2..16). Up to three, at least 1/3 octave apart, 80 Hz-12 kHz.
//   Low-mid    150-500 Hz typically (median) >= 2.5 dB above the trend -> broad bell cut (Q 1).
//   Harshness  2-5 kHz typically (median) >= 2.5 dB above the trend -> bell cut (Q 1.2); dynamic if the region's
//              short-term level swings strongly (90th - 10th percentile >= 6 dB; threshold halfway between).
//   Sibilance  5-10 kHz short-term level bursts (>= 8 dB above the median in 2-30 % of the frames) ->
//              dynamic bell (RMS detector) at the burst centroid, threshold between median and burst level.
//   Air        9-16 kHz >= 8 dB below the extrapolated trend while 2-8 kHz carries substantial content ->
//              gentle high shelf (+2 dB at 10 kHz), low confidence.
#pragma once

#include "DspConfig.h"
#include "FFT.h"
#include "FilterBand.h"

#include <memory>
#include <string>
#include <vector>

namespace mepro::dsp
{

struct Suggestion
{
    enum class Kind { HighPass, Resonance, LowMid, Harshness, Sibilance, Air };
    enum class Confidence { Low, Medium, High };

    Kind kind = Kind::Resonance;
    Confidence confidence = Confidence::Medium;
    BandSettings band;  // complete band to create (Stereo routing); band.active = true
    std::string title;  // e.g. "Resonance at 2.48 kHz"
    std::string reason; // the measured evidence, in plain words
};

class ToneAnalysis
{
public:
    static constexpr double kMinSeconds = 3.0;   // of non-silent material before suggestions are offered
    static constexpr double kMaxSeconds = 60.0;  // statistics keep the first minute
    static constexpr double kSilenceDb = -70.0;  // frames below this RMS level are ignored

    // Not real-time safe (allocates). UI thread.
    void prepare (double sampleRate);
    void reset();

    // Feed audio (interleaved, 1 or 2 channels; the mono sum is analysed).
    void feed (const float* interleaved, int numFrames, int numChannels = 2);

    double getActiveSeconds() const noexcept;
    bool hasEnoughMaterial() const noexcept { return getActiveSeconds() >= kMinSeconds; }

    // Evaluate the rules on everything fed so far.
    std::vector<Suggestion> analyse() const;

    // Measurement access (tests, display): LTAS level of the 1/24-octave band containing f, dB (mean-square).
    double bandLevelDb (double freqHz) const;
    double getSampleRate() const noexcept { return fs; }

private:
    void longFrame();
    void shortFrame();
    int bandIndex (double f) const noexcept;
    double bandFreq (int j) const noexcept;

    double fs = 48000.0;
    int longSize = 16384, shortSize = 2048;
    std::unique_ptr<RealFFT> longFft, shortFft;
    std::vector<double> longWindow, shortWindow, longFrameBuf, shortFrameBuf;
    std::vector<std::complex<double>> longSpec, shortSpec;
    std::vector<float> longHist, shortHist; // mono input rings
    int longPos = 0, shortPos = 0, longSinceHop = 0, shortSinceHop = 0, longFill = 0, shortFill = 0;
    double longNorm = 1.0, shortNorm = 1.0;

    // 1/24-octave bands from 16 Hz
    int numBands = 0;
    std::vector<double> bandPowerSum;   // accumulated over active long frames
    std::vector<float> bandSeries;      // [frame][band] dB, active long frames
    int activeLongFrames = 0, maxLongFrames = 0;

    // 1/6-octave bands 1-16 kHz, short frames
    static constexpr int kShortBands = 25;
    std::vector<double> shortBandFreq;
    std::vector<float> shortSeries;     // [frame][band] dB, active short frames
    int activeShortFrames = 0, maxShortFrames = 0;
    std::vector<double> scratch;
};

} // namespace mepro::dsp
