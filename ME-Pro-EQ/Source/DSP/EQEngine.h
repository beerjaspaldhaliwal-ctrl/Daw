// ME Pro EQ — ME Audio
// Top-level real-time EQ engine (framework independent).
//
// Signal flow (per block):
//   input -> sanitize (non-finite -> 0) -> input meter -> [dry copy] -> input trim -> pre-EQ analyzer tap
//   -> 16 serial bands -> band audition (monitoring only) -> output trim -> bypass crossfade with dry
//   -> mono-compatibility audition (monitoring only) -> non-finite guard -> output meter -> post analyzer tap
//
// Everything runs in double precision. No allocation, locks or I/O happen in process().
#pragma once

#include "DspConfig.h"
#include "FilterBand.h"

#include <array>
#include <atomic>
#include <vector>

namespace mepro::dsp
{

struct EngineSettings
{
    std::array<BandSettings, kNumBands> bands {};
    double inputTrimDb = 0.0;
    double outputTrimDb = 0.0;
    bool bypass = false;
    int auditionBand = -1;     // monitoring only: >= 0 solos that band's frequency region
    bool monoAudition = false; // monitoring only: fold output to mono
};

// Receives audio for analysis. Called on the audio thread: implementations must be wait-free.
class AnalyzerTap
{
public:
    virtual ~AnalyzerTap() = default;
    virtual void push (const double* interleaved, int numChannels, int numSamples) noexcept = 0;
};

class EQEngine
{
public:
    static constexpr double kTrimMinDb = -24.0, kTrimMaxDb = 24.0;
    static constexpr double kTrimSmoothingSec = 0.020;
    static constexpr double kBypassFadeSec = 0.020;
    static constexpr double kAuditionFadeSec = 0.010;

    // Not real-time safe (allocates). Call before processing / on sample-rate or block-size change.
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;

    void setSettings (const EngineSettings& s) noexcept;
    const EngineSettings& getSettings() const noexcept { return settings; }

    // Process in place. channels[c][i]; numChannels 1 or 2. sidechain may be null.
    void process (double* const* channels, int numChannels, int numSamples,
                  const double* const* sidechain = nullptr, int sidechainChannels = 0) noexcept;
    void process (float* const* channels, int numChannels, int numSamples,
                  const float* const* sidechain = nullptr, int sidechainChannels = 0) noexcept;

    void setAnalyzerTaps (AnalyzerTap* pre, AnalyzerTap* post) noexcept
    {
        preTap = pre;
        postTap = post;
    }

    // Research/test hook: force the fine control period (samples between dynamic redesigns; static bands
    // use 2x this). 0 = automatic (8 samples at 44.1/48 kHz, scaled with the sample rate).
    void setControlPeriodOverride (int samples) noexcept { controlPeriodOverride = samples; }

    int getLatencySamples() const noexcept { return 0; }
    int getControlPeriod() const noexcept { return controlPeriod; }
    double getSampleRate() const noexcept { return sampleRate; }

    // Metering: peak since last read (linear), read-and-reset from the UI thread.
    float takeInputPeak (int ch) noexcept { return inPeak[(size_t) (ch & 1)].exchange (0.0f, std::memory_order_relaxed); }
    float takeOutputPeak (int ch) noexcept { return outPeak[(size_t) (ch & 1)].exchange (0.0f, std::memory_order_relaxed); }
    bool hasClipped() const noexcept { return clipFlag.load (std::memory_order_relaxed); }
    void resetClip() noexcept { clipFlag.store (false, std::memory_order_relaxed); }
    int getNonFiniteEvents() const noexcept { return nonFiniteEvents.load (std::memory_order_relaxed); }

    float getBandDynamicGainDb (int band) const noexcept { return bands[(size_t) band].getDynamicGainDb(); }
    float getBandDetectorLevelDb (int band) const noexcept { return bands[(size_t) band].getDetectorLevelDb(); }
    const BandProcessor& getBand (int band) const noexcept { return bands[(size_t) band]; }

private:
    void processInterleaved (int numChannels, int n, const double* scInterleaved, int scChannels) noexcept;
    void applyAudition (int numChannels, int n) noexcept;
    static void storeMax (std::atomic<float>& a, float v) noexcept;

    EngineSettings settings;
    double sampleRate = 48000.0;
    int maxBlock = 512;
    int channelsPrepared = 2;
    int controlPeriod = 16;
    int controlPeriodOverride = 0;

    std::array<BandProcessor, kNumBands> bands;

    std::vector<double> work, dry, scBuf, s0, s1, s2, s3, s4;

    double inGain = 1.0, outGain = 1.0, trimCoef = 0.0;
    int bypassPos = 0, bypassLen = 960; // raised-cosine crossfade position (0 = processed, bypassLen = dry)

    // Band audition (monitoring)
    SvfCascade auditionFilter;
    int auditionBandActive = -1;
    int auditionPos = 0, auditionLen = 480;
    StereoMode auditionMode = StereoMode::Stereo;
    double auditionLastFreq = -1.0, auditionLastQ = -1.0;

    AnalyzerTap* preTap = nullptr;
    AnalyzerTap* postTap = nullptr;

    std::array<std::atomic<float>, 2> inPeak { 0.0f, 0.0f };
    std::array<std::atomic<float>, 2> outPeak { 0.0f, 0.0f };
    std::atomic<bool> clipFlag { false };
    std::atomic<int> nonFiniteEvents { 0 };
};

} // namespace mepro::dsp
