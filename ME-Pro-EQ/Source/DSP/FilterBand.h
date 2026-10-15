// ME Pro EQ — ME Audio
// One EQ band in the real-time engine.
//
// Responsibilities:
//  * parameter smoothing (log-frequency, dB gain, log-Q) evaluated on a control grid;
//  * redesign of the analog-matched cascade when smoothed values or the dynamic gain change, applied as a
//    per-sample coefficient ramp up to the next redesign (no zipper noise);
//  * structural changes (filter type, section layout, stereo routing) via a raised-cosine crossfade
//    between two independent filter instances;
//  * enable/disable via a raised-cosine wet/dry fade;
//  * stereo routing: Stereo (linked), Left, Right, Mid, Side;
//  * dynamic EQ: frequency-selective detector -> gain computer -> gain added to the static band gain.
//
// Control grid: a "fine" tick every finePeriod samples (~0.083 ms) and a "coarse" tick every
// kFinePerCoarse fine ticks (~0.33 ms). Static bands are smoothed/redesigned on coarse ticks only; bands
// with active dynamics are redesigned on every fine tick (measured: a coarser grid adds audible-range
// zipper products, see Tests/DynamicEQTests.cpp GainModulationIsSmooth).
//
// All fades and ramps are integer-indexed, so the output is bit-identical for any host block size.
// Real-time safety: no allocation, locking or I/O. All buffers are supplied by the engine.
#pragma once

#include "DspConfig.h"
#include "DynamicEQ.h"
#include "FilterDesign.h"
#include "SvfFilter.h"

#include <atomic>

namespace mepro::dsp
{

struct BandSettings
{
    bool active = false; // band exists ("used") and is enabled
    FilterType type = FilterType::Bell;
    double freqHz = 1000.0;
    double gainDb = 0.0;
    double q = 1.0;
    int slopeIndex = kDefaultSlopeIndex;
    StereoMode stereo = StereoMode::Stereo;
    DynamicSettings dyn;
};

// Buffers the engine lends to a band for one call (each at least 2 * coarse period doubles).
struct BandScratch
{
    double* dry = nullptr;
    double* aux = nullptr;
    double* mid = nullptr;
    double* side = nullptr;
    double* det = nullptr;
};

// Raised-cosine fade shape: 0 -> 0, 1 -> 1, zero slope at both ends.
inline double fadeShape (double t) noexcept
{
    return t <= 0.0 ? 0.0 : (t >= 1.0 ? 1.0 : 0.5 - 0.5 * std::cos (3.14159265358979323846 * t));
}

class BandProcessor
{
public:
    static constexpr double kSmoothingTimeSec = 0.020;
    static constexpr double kFadeTimeSec = 0.010;
    static constexpr double kCrossfadeTimeSec = 0.015;
    static constexpr double kMaxCombinedGainDb = 30.0;
    static constexpr int kFinePerCoarse = 4;

    void prepare (double sampleRate, int finePeriod) noexcept;
    void reset() noexcept;

    void setSettings (const BandSettings& s) noexcept { target = s; }
    void setDynamicsAllowed (bool allowed) noexcept { dynamicsAllowed = allowed; }

    // In-place processing of interleaved audio (1 or 2 channels). `sidechain` may be null.
    void process (double* buffer, int numChannels, int numSamples,
                  const double* sidechain, int sidechainChannels, const BandScratch& scratch) noexcept;

    bool isIdle() const noexcept { return state == State::Idle; }
    bool dynamicsActive() const noexcept;

    // For the UI (written on the audio thread, read anywhere).
    float getDynamicGainDb() const noexcept { return dynGainForUi.load (std::memory_order_relaxed); }
    float getDetectorLevelDb() const noexcept { return detLevelForUi.load (std::memory_order_relaxed); }

    double getSmoothedGainDb() const noexcept { return smGainDb; }
    double getAppliedDynamicGainDb() const noexcept { return dynGainDb; }

private:
    enum class State { Idle, Active };

    struct Slot
    {
        SvfCascade filter;
        FilterType type = FilterType::Bell;
        StereoMode stereo = StereoMode::Stereo;
        SectionLayout layout;
        int slopeIndex = kDefaultSlopeIndex;
        bool targetIsIdentity = true;
    };

    void coarseTick() noexcept;
    void updateResponse (bool fineTickOnly, bool forceImmediate) noexcept;
    void configureSlot (Slot& slot) noexcept;
    DesignSpec currentSpec (double extraGainDb) const noexcept;
    void snapSmoothing() noexcept;
    bool structureDiffers (const Slot& slot) const noexcept;
    void releaseBand() noexcept;

    void processSlot (Slot& slot, double* x, int numChannels, int n, const BandScratch& scratch) noexcept;
    void runDetector (const double* x, int numChannels, int n, const double* sc, int scChannels,
                      const BandScratch& scratch) noexcept;

    BandSettings target;
    double sampleRate = 48000.0;
    int finePeriod = 4, coarsePeriod = 16;
    int samplesUntilTick = 0;
    int fineTickIndex = 0;
    double smoothingCoefFine = 0.0, smoothingCoefCoarse = 0.0;
    int fadeLength = 480, crossfadeLength = 720;

    double smLog2Freq = 0.0, smGainDb = 0.0, smLogQ = 0.0;
    double lastDesignedGain = 0.0, lastDesignedLog2F = -1.0, lastDesignedLogQ = -100.0;
    FilterType lastDesignedType = FilterType::NumTypes;
    int lastDesignedSlope = -1;

    State state = State::Idle;
    Slot slots[2];
    int activeSlot = 0;
    int crossfadePos = 0, crossfadeLen = 0; // > 0 while crossfading from the inactive slot to the active one
    int fadePos = 0;                         // 0..fadeLength (wet amount = fadeShape(fadePos / fadeLength))
    int fadeDir = 0;                         // +1 fading in, -1 fading out, 0 steady

    bool dynamicsAllowed = true;
    DynamicDetector detector;
    double dynGainDb = 0.0;
    std::atomic<float> dynGainForUi { 0.0f };
    std::atomic<float> detLevelForUi { -200.0f };
};

} // namespace mepro::dsp
