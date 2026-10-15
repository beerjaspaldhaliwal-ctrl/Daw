// ME Pro EQ — ME Audio
// Linear-phase engine: the magnitude response of the complete static EQ curve realised as a symmetric FIR
// filter, run with uniformly partitioned FFT convolution.
//
// Design (see Documentation/DSP_Design.md, "Linear-phase mode"):
//   * The target is the exact magnitude response of the same digital band designs the minimum-phase engine
//     runs, so switching the phase mode changes the phase, not the intended magnitude.
//   * Stereo routing is exact for every combination of Stereo / Left / Right / Mid / Side bands: per frequency
//     the bands form a chain of real 2x2 matrices (multiplied in band order). Depending on the routing in use
//     the FIR system is a single filter for both channels, two independent channel filters, or a full 2x2
//     filter matrix.
//   * FIR: zero-phase target sampled on a 2N-point grid, inverse FFT, centred and Hann-windowed to N taps
//     (frequency-sampling / window method). Latency = N/2 + P samples (P = convolution partition size).
//   * Dynamic EQ is not applied in linear-phase mode: dynamic bands contribute their static settings.
//
// Threading:
//   * Construction / destruction (allocation): message thread, while the object is not used for audio.
//   * setBands(), process(), reset(): audio thread only; allocation- and lock-free in real-time operation.
//     New designs are computed by an internal worker thread and handed over lock-free; the audio thread
//     crossfades from the old to the new filter over two partitions.
//   * Offline (non-real-time) rendering: setBands(..., synchronous = true) designs on the calling thread so
//     renders are deterministic.
//   * getRealisedResponse(): any non-audio thread.
#pragma once

#include "DspConfig.h"
#include "FFT.h"
#include "FilterBand.h"

#include <array>
#include <atomic>
#include <complex>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace mepro::dsp
{

enum class PhaseMode : int
{
    Minimum = 0,
    Linear = 1
};

inline constexpr int kNumLinearPhaseQualities = 3;
inline constexpr std::array<const char*, kNumLinearPhaseQualities> kLinearPhaseQualityNames { "Low latency", "Balanced", "High resolution" };
inline constexpr int kDefaultLinearPhaseQuality = 1;

struct LinearPhaseGeometry
{
    int rateMultiplier = 1; // 1 at 44.1/48 kHz, 2 at 88.2/96 kHz, 4 at 176.4/192 kHz
    int partition = 256;    // P: convolution block (samples)
    int firLength = 16384;  // N: FIR taps
    int numPartitions = 64; // N / P
    int latency = 8448;     // N/2 + P samples
};

LinearPhaseGeometry linearPhaseGeometry (double sampleRate, int quality) noexcept;

using LinearPhaseBands = std::array<BandSettings, kNumBands>;

class LinearPhaseEngine
{
public:
    // Allocates everything. startWorker = false: designs only happen synchronously (tests, offline tools).
    LinearPhaseEngine (double sampleRate, int quality, bool startWorker = true);
    ~LinearPhaseEngine();

    LinearPhaseEngine (const LinearPhaseEngine&) = delete;
    LinearPhaseEngine& operator= (const LinearPhaseEngine&) = delete;

    const LinearPhaseGeometry& getGeometry() const noexcept { return geo; }
    double getSampleRate() const noexcept { return fs; }
    int getQuality() const noexcept { return quality; }
    int getLatency() const noexcept { return geo.latency; }
    // Heap memory held by this engine (all buffers and FFT tables, approximate for the tables).
    size_t getAllocatedBytes() const noexcept;

    //== Audio thread =========================================================================================
    // Clears the convolution history, the dry delay and any crossfade. The current filter is kept.
    void reset() noexcept;

    // Supplies the current band settings. When they changed since the last call, a new design is requested
    // (real-time: computed by the worker thread; synchronous: computed now, on this thread).
    void setBands (const LinearPhaseBands& bands, int numChannels, bool synchronous) noexcept;

    // True once a filter has been designed (it may still be waiting to be picked up at the next block).
    bool isReady() const noexcept { return currentKernel >= 0 || pendingKernel.load (std::memory_order_acquire) >= 0; }

    // Convolves `x` (interleaved, 1 or 2 channels) in place and delays `dry` (same layout) by the latency.
    void process (double* x, double* dry, int numChannels, int numSamples) noexcept;

    //== Any non-audio thread =================================================================================
    // Magnitude (dB) of the most recently designed FIR, at bin spacing binHz from 0 Hz to fs/2. domainA/B are
    // the curves for the two channel domains ("L"/"R" or "M"/"S"; B is empty when both channels share one
    // filter). Returns false if no design exists yet.
    struct RealisedResponse
    {
        std::vector<float> domainA, domainB;
        char nameA = 'C', nameB = ' '; // 'C' common, 'L','R','M','S'
        double binHz = 1.0;
        unsigned version = 0;
    };
    bool getRealisedResponse (RealisedResponse& out) const;

    // Test hooks
    // Form of the most recent design: 0 one filter for all channels, 1 per-channel filters, 2 full 2x2 matrix.
    int getLastDesignForm() const noexcept { return lastForm.load (std::memory_order_relaxed); }
    unsigned getDesignCount() const noexcept { return designCount.load (std::memory_order_relaxed); }
    // Copies path p (0..3) of the most recently designed FIR (time domain, N taps). Test use only.
    bool copyLastDesignedFir (int path, std::vector<double>& taps);

private:
    enum KernelForm { kScalar = 0, kDiagonal = 1, kFull = 2 };

    struct Request
    {
        LinearPhaseBands bands {};
        int numChannels = 2;
    };

    struct Kernel
    {
        int form = kScalar;
        std::vector<float> re, im;    // [path][partition][bin], paths x K x (P + 1); grows on demand (designer)
        std::atomic<int> state { 0 }; // 0 free, 1 writing, 2 ready (pending), 3 active
    };

    // Triple buffer (single writer: audio thread, single reader: worker), lock-free and race-free.
    class RequestMailbox
    {
    public:
        Request& writeBuffer() noexcept { return buf[(size_t) writeIdx]; }
        void publish() noexcept;
        bool fetch() noexcept; // true if a new request arrived; it is then in readBuffer()
        const Request& readBuffer() const noexcept { return buf[(size_t) readIdx]; }

    private:
        static constexpr int kDirty = 4;
        std::array<Request, 3> buf {};
        std::atomic<int> middle { 1 };
        int writeIdx = 0, readIdx = 2;
    };

    void workerLoop();
    enum class Publish { No, Blocking, IfFree };
    void designInto (Kernel& k, const Request& r, Publish publish); // caller holds designMutex
    void publishKernel (int index) noexcept;
    int findFreeKernel() noexcept;
    static bool sameBands (const LinearPhaseBands& a, const LinearPhaseBands& b) noexcept;
    void convolveBlock() noexcept;

    const double fs;
    const int quality;
    const LinearPhaseGeometry geo;
    const int P, K, N, Nd, bins; // bins = P + 1

    // Design scratch (guarded by designMutex; never touched by the audio-thread convolution)
    std::mutex designMutex;
    RealFFT designFft;      // size Nd = 2N
    RealFFT designBlockFft; // size 2P, partition spectra of new designs (never shared with the audio thread)
    std::vector<double> tanHalf;            // tan(w/2) for the N + 1 design bins
    std::vector<double> m00, m01, m10, m11; // per-bin 2x2 magnitude matrix (or scalar / diagonal)
    std::vector<double> bandMag;            // per-bin magnitude of one band
    std::vector<std::complex<double>> spec; // N + 1 bins
    std::vector<double> timeBuf;            // Nd samples (also the scratch for the realised response)
    std::vector<double> firScratch;         // paths x N taps of the latest design (grows to 4 paths on demand)
    std::vector<double> window;             // N taps (periodic Hann)
    std::vector<double> blockIn;            // 2P
    std::vector<std::complex<double>> blockSpec;

    std::array<Kernel, 4> kernels;
    std::atomic<int> pendingKernel { -1 };
    std::atomic<unsigned> designCount { 0 };
    std::atomic<int> lastForm { -1 };

    // Worker
    RequestMailbox mailbox;
    std::thread worker;
    std::atomic<bool> quit { false };

    // Audio-thread state
    Request lastRequest {};
    bool haveRequest = false;
    int currentKernel = -1, nextKernel = -1;
    int fadeBlock = 0;
    static constexpr int kFadeBlocks = 2;
    std::vector<double> fdlRe, fdlIm;  // [channel][partition][bin]
    int fdlHead = 0;                   // partition slot holding the newest input spectrum
    std::vector<double> inBuf;         // [channel][2P]: previous block + current block (time domain)
    std::vector<double> outBuf;        // [channel][P]: current output block
    std::vector<double> accRe, accIm;  // [2 outputs][bins]
    std::vector<double> yOld;          // [channel][P]: output of the old filter while crossfading
    RealFFT blockFft;                  // size 2P, audio thread only (the FFT objects hold scratch state)
    std::vector<std::complex<double>> convSpec; // bins, audio thread only
    std::vector<double> ifftOut;       // 2P
    std::vector<std::complex<double>> ifftIn; // bins
    std::vector<double> dryDelay;      // [channel][latency]
    int dryPos = 0;
    int blockPos = 0;                  // 0..P-1 position inside the current block
    int channelsInUse = 2;

    // Published realised response (written by the designer, read by the UI)
    mutable std::mutex responseMutex;
    RealisedResponse published;
};

} // namespace mepro::dsp
