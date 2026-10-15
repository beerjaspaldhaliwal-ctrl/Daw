// ME Pro EQ — ME Audio
// Top-level real-time EQ engine (framework independent).
//
// Signal flow (per block):
//   input -> sanitize (non-finite -> 0) -> input meter -> [dry copy] -> input trim -> pre-EQ analyzer tap
//   -> 16 serial bands -> band audition (monitoring only) -> output trim -> bypass crossfade with dry
//   -> mono-compatibility audition (monitoring only) -> non-finite guard -> output meter -> post analyzer tap
//
// Auto gain (optional, off by default): the output is offset by the negative of the EQ's *static* loudness
// change, estimated as the pink-noise-weighted power gain of the combined curve (48 log-spaced points,
// 20 Hz - 20 kHz). Dynamic gain changes are deliberately not compensated. The applied value is published
// for the UI so the behaviour is always visible.
//
// Level comparison: input and output RMS (3 s time constant) are published so the UI can show the level
// difference and offer a one-click, undoable output-trim match.
//
// Phase mode: minimum phase (the 16 IIR bands, zero latency) or linear phase (LinearPhaseEngine: the same
// magnitude curve as one FIR system, latency N/2 + P). The linear-phase engine is built on the message
// thread and handed to the audio thread lock-free; switching between the two paths fades the output out
// (10 ms), switches at silence and fades back in.
//
// Everything runs in double precision. No allocation, locks or I/O happen in process() during real-time
// operation (offline rendering may design linear-phase filters synchronously).
#pragma once

#include "DspConfig.h"
#include "FilterBand.h"
#include "LinearPhase.h"

#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace mepro::dsp
{

struct EngineSettings
{
    std::array<BandSettings, kNumBands> bands {};
    double inputTrimDb = 0.0;
    double outputTrimDb = 0.0;
    bool bypass = false;
    bool autoGain = false;     // compensate the EQ curve's static loudness change (see AutoGain notes below)
    int auditionBand = -1;     // monitoring only: >= 0 solos that band's frequency region
    bool monoAudition = false; // monitoring only: fold output to mono
    PhaseMode phaseMode = PhaseMode::Minimum; // takes effect once a LinearPhaseEngine is installed
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
    static constexpr double kModeFadeSec = 0.010;

    EQEngine() = default;
    ~EQEngine();
    EQEngine (const EQEngine&) = delete;
    EQEngine& operator= (const EQEngine&) = delete;

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

    //== Linear phase ===========================================================================================
    // Message thread while audio is stopped (e.g. right after prepare()): installs immediately.
    void installLinearPhaseNow (std::unique_ptr<LinearPhaseEngine> lp) noexcept;
    // Message thread while audio runs: handed over lock-free; used from the next block (switching with a fade
    // if linear phase is already producing audio).
    void installLinearPhase (std::unique_ptr<LinearPhaseEngine> lp) noexcept;
    // Message thread: releases the linear-phase engine as soon as it is not producing audio.
    void removeLinearPhase() noexcept;
    // Message thread: deletes engines the audio thread has retired. Call regularly (e.g. from a timer).
    void collectGarbage() noexcept;
    // Offline rendering: linear-phase filters are designed synchronously, so renders are deterministic.
    void setNonRealtime (bool b) noexcept { nonRealtime = b; }

    // Latency of the path currently producing audio (0 in minimum-phase mode).
    int getLatencySamples() const noexcept { return currentLatency.load (std::memory_order_relaxed); }
    bool isUsingLinearPhase() const noexcept { return usingLinearForUi.load (std::memory_order_relaxed); }
    // The linear-phase engine producing audio, or nullptr. Message thread only; valid until the next
    // collectGarbage() call (which also runs on the message thread).
    const LinearPhaseEngine* getLinearPhaseForUi() const noexcept { return lpForUi.load (std::memory_order_acquire); }
    int getControlPeriod() const noexcept { return controlPeriod; }
    double getSampleRate() const noexcept { return sampleRate; }

    // Metering: peak since last read (linear), read-and-reset from the UI thread.
    float takeInputPeak (int ch) noexcept { return inPeak[(size_t) (ch & 1)].exchange (0.0f, std::memory_order_relaxed); }
    float takeOutputPeak (int ch) noexcept { return outPeak[(size_t) (ch & 1)].exchange (0.0f, std::memory_order_relaxed); }
    bool hasClipped() const noexcept { return clipFlag.load (std::memory_order_relaxed); }
    void resetClip() noexcept { clipFlag.store (false, std::memory_order_relaxed); }
    int getNonFiniteEvents() const noexcept { return nonFiniteEvents.load (std::memory_order_relaxed); }

    float getAutoGainDb() const noexcept { return autoGainForUi.load (std::memory_order_relaxed); }
    float getInputRmsDb() const noexcept { return inRmsDb.load (std::memory_order_relaxed); }
    float getOutputRmsDb() const noexcept { return outRmsDb.load (std::memory_order_relaxed); }

    // Exposed for tests: the compensation the auto-gain stage targets for the current settings.
    double computeAutoGainDb() noexcept;

    float getBandDynamicGainDb (int band) const noexcept { return bands[(size_t) band].getDynamicGainDb(); }
    float getBandDetectorLevelDb (int band) const noexcept { return bands[(size_t) band].getDetectorLevelDb(); }
    const BandProcessor& getBand (int band) const noexcept { return bands[(size_t) band]; }

private:
    void processInterleaved (int numChannels, int n, const double* scInterleaved, int scChannels) noexcept;
    void updatePhaseMode (int numChannels) noexcept;
    void retire (LinearPhaseEngine* e) noexcept;
    void deleteAllLinearPhase() noexcept;
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

    static constexpr int kAutoGainPoints = 48;
    struct AutoGainBandCache
    {
        BandSettings settings;
        bool valid = false;
        bool identity = true;
        std::array<double, kAutoGainPoints> power {};
    };
    std::array<AutoGainBandCache, kNumBands> agCache;
    std::array<double, kAutoGainPoints> agOmega {};
    double autoGainDb = 0.0;
    std::atomic<float> autoGainForUi { 0.0f };
    double msIn = 0.0, msOut = 0.0;
    std::atomic<float> inRmsDb { -200.0f }, outRmsDb { -200.0f };

    // Linear phase (see LinearPhase.h)
    LinearPhaseEngine* lpActive = nullptr; // audio thread
    LinearPhaseEngine* lpNext = nullptr;   // audio thread: replacement waiting for a silent point
    std::atomic<LinearPhaseEngine*> lpIncoming { nullptr };
    std::atomic<LinearPhaseEngine*> lpRetired { nullptr };
    std::atomic<const LinearPhaseEngine*> lpForUi { nullptr };
    std::atomic<bool> lpRemoveRequest { false };
    bool usingLinear = false;
    bool nonRealtime = false;
    int modeFadePos = 480, modeFadeLen = 480, modeFadeDir = 0; // output gain = fadeShape(pos / len)
    int modeHold = 0; // samples to stay silent before fading in (the new path's latency)
    std::atomic<int> currentLatency { 0 };
    std::atomic<bool> usingLinearForUi { false };

    std::array<std::atomic<float>, 2> inPeak { 0.0f, 0.0f };
    std::array<std::atomic<float>, 2> outPeak { 0.0f, 0.0f };
    std::atomic<bool> clipFlag { false };
    std::atomic<int> nonFiniteEvents { 0 };
};

} // namespace mepro::dsp
