// ME Pro EQ — ME Audio
// Real-time spectrum analysis.
//
//  AnalyzerFifo     wait-free single-producer/single-consumer ring of stereo float frames. The audio thread
//                   pushes (never blocks; drops and counts frames if the reader falls behind), the UI pulls.
//  SpectrumAnalyzer runs on the UI thread: windowed (4-term Blackman-Harris) real FFT with 75 % overlap,
//                   power averaged over channels, calibrated so that a full-scale sine centred on a bin reads
//                   0 dBFS; then mapped to log-spaced display points with optional fractional-octave power
//                   smoothing, release ballistics (dB/s) and peak hold.
//
// Calibration note: the sine calibration is exact for stationary sinusoids. Broadband signals (noise) read
// lower per bin as the FFT size grows (energy is spread over more bins); this is inherent to FFT analysers
// and is documented in the user manual.
#pragma once

#include "FFT.h"

#include <atomic>
#include <memory>
#include <vector>

namespace mepro::dsp
{

class AnalyzerFifo
{
public:
    explicit AnalyzerFifo (int capacityFrames = 1 << 16);

    // Audio thread. Interleaved input with 1 or 2 channels; mono is duplicated.
    void push (const double* interleaved, int numChannels, int numFrames) noexcept;

    // UI thread. Returns the number of frames read into `dest` (interleaved stereo, 2 * maxFrames floats).
    int pull (float* dest, int maxFrames) noexcept;

    int getDroppedFrames() const noexcept { return dropped.load (std::memory_order_relaxed); }
    void setActive (bool a) noexcept { active.store (a, std::memory_order_relaxed); }
    bool isActive() const noexcept { return active.load (std::memory_order_relaxed); }

private:
    std::vector<float> buffer; // 2 * capacity
    int capacity, mask;
    std::atomic<int> writePos { 0 }, readPos { 0 };
    std::atomic<int> dropped { 0 };
    std::atomic<bool> active { false };
};

class SpectrumAnalyzer
{
public:
    struct Settings
    {
        int fftSize = 8192;
        double smoothingOctaves = 1.0 / 6.0; // 0 = off (peak-preserving per display point)
        double releaseDbPerSec = 30.0;       // display fall rate
        bool peakHold = false;
        double slopeDbPerOct = 0.0;          // optional display tilt, pivot 1 kHz (0 = calibrated)
    };

    // Not real-time safe (allocates); call from the UI thread.
    void configure (const Settings& s, double sampleRate, int numDisplayPoints, double fMin, double fMax);

    // Feed interleaved stereo frames. Runs FFTs for every completed hop.
    void feed (const float* interleavedStereo, int numFrames) noexcept;

    // Advance display ballistics by dt seconds using the frames analysed since the last call.
    void updateDisplay (double dt) noexcept;

    const std::vector<float>& getDisplayDb() const noexcept { return display; }
    const std::vector<float>& getPeakDb() const noexcept { return peaks; }
    const std::vector<double>& getDisplayFreqs() const noexcept { return displayFreqs; }
    void resetPeaks() noexcept;
    int getFramesAnalysed() const noexcept { return framesAnalysed; }

    // Latest raw power spectrum in dB (per bin), exposed for tests.
    const std::vector<float>& getBinDb() const noexcept { return binDb; }
    double binFrequency (int bin) const noexcept { return bin * sampleRate / settings.fftSize; }

private:
    void analyseFrame() noexcept;

    Settings settings;
    double sampleRate = 48000.0;
    std::unique_ptr<RealFFT> fft;
    std::vector<double> window, frame;
    std::vector<std::complex<double>> spectrum;
    std::vector<float> history; // ring of the last fftSize stereo frames (interleaved)
    int histPos = 0, sinceHop = 0, hop = 2048;
    double windowNorm = 1.0;

    std::vector<double> accPower; // power accumulated over frames since the last display update
    int accFrames = 0;
    std::vector<float> binDb;

    std::vector<double> displayFreqs;
    std::vector<int> binLo, binHi; // aggregation range per display point (half the smoothing width)
    std::vector<int> ptLo, ptHi;   // neighbouring display points for the second (triangular) smoothing pass
    std::vector<double> prefix;    // prefix sums of bin power
    std::vector<double> boxed;     // first-pass (box) smoothed power per display point
    std::vector<double> tiltDb;
    std::vector<float> display, peaks;
    int framesAnalysed = 0;
};

} // namespace mepro::dsp
