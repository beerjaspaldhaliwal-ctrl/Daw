// ME Pro EQ — ME Audio
#include "LinearPhase.h"
#include "FilterDesign.h"
#include "MathUtils.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

namespace mepro::dsp
{

LinearPhaseGeometry linearPhaseGeometry (double sampleRate, int quality) noexcept
{
    LinearPhaseGeometry g;
    const int mult = std::max (1, (int) std::lround ((sampleRate > 0.0 ? sampleRate : 48000.0) / 48000.0));
    int p = 1;
    while (p < mult)
        p <<= 1;
    quality = clampValue (quality, 0, kNumLinearPhaseQualities - 1);
    g.rateMultiplier = p;
    g.partition = 256 * p;
    g.firLength = (8192 << quality) * p;
    g.numPartitions = g.firLength / g.partition;
    g.latency = g.firLength / 2 + g.partition;
    return g;
}

//==============================================================================
void LinearPhaseEngine::RequestMailbox::publish() noexcept
{
    writeIdx = middle.exchange (writeIdx | kDirty, std::memory_order_acq_rel) & 3;
}

bool LinearPhaseEngine::RequestMailbox::fetch() noexcept
{
    if ((middle.load (std::memory_order_acquire) & kDirty) == 0)
        return false;
    readIdx = middle.exchange (readIdx, std::memory_order_acq_rel) & 3;
    return true;
}

//==============================================================================
LinearPhaseEngine::LinearPhaseEngine (double sampleRate, int q, bool startWorker)
    : fs (sampleRate > 0.0 ? sampleRate : 48000.0),
      quality (clampValue (q, 0, kNumLinearPhaseQualities - 1)),
      geo (linearPhaseGeometry (fs, quality)),
      P (geo.partition), K (geo.numPartitions), N (geo.firLength), Nd (2 * geo.firLength), bins (geo.partition + 1),
      designFft (2 * geo.firLength), designBlockFft (2 * geo.partition), blockFft (2 * geo.partition)
{
    const int nb = N + 1; // design bins 0 .. Nd/2
    tanHalf.resize ((size_t) nb);
    for (int k = 0; k < nb; ++k)
        tanHalf[(size_t) k] = k == N ? std::numeric_limits<double>::infinity() : std::tan (kPi * k / Nd);
    for (auto* v : { &m00, &m01, &m10, &m11, &bandMag })
        v->assign ((size_t) nb, 0.0);
    spec.assign ((size_t) nb, {});
    timeBuf.assign ((size_t) Nd, 0.0);
    respScratch.assign ((size_t) Nd, 0.0);
    firScratch.assign ((size_t) (4 * N), 0.0);
    window.resize ((size_t) N);
    for (int n = 0; n < N; ++n)
        window[(size_t) n] = 0.5 - 0.5 * std::cos (kTwoPi * n / N); // periodic Hann: w[0] = 0, symmetric about N/2
    blockIn.assign ((size_t) (2 * P), 0.0);
    blockSpec.assign ((size_t) bins, {});

    for (auto& k : kernels)
    {
        k.re.assign ((size_t) (4 * K * bins), 0.0f);
        k.im.assign ((size_t) (4 * K * bins), 0.0f);
    }

    fdlRe.assign ((size_t) (kMaxChannels * K * bins), 0.0);
    fdlIm.assign ((size_t) (kMaxChannels * K * bins), 0.0);
    inBuf.assign ((size_t) (kMaxChannels * 2 * P), 0.0);
    outBuf.assign ((size_t) (kMaxChannels * P), 0.0);
    yOld.assign ((size_t) (kMaxChannels * P), 0.0);
    accRe.assign ((size_t) (kMaxChannels * bins), 0.0);
    accIm.assign ((size_t) (kMaxChannels * bins), 0.0);
    ifftIn.assign ((size_t) bins, {});
    convSpec.assign ((size_t) bins, {});
    ifftOut.assign ((size_t) (2 * P), 0.0);
    dryDelay.assign ((size_t) (kMaxChannels * geo.latency), 0.0);

    published.domainA.reserve ((size_t) nb);
    published.domainB.reserve ((size_t) nb);
    published.binHz = fs / Nd;

    if (startWorker)
        worker = std::thread ([this] { workerLoop(); });
}

LinearPhaseEngine::~LinearPhaseEngine()
{
    quit.store (true, std::memory_order_release);
    if (worker.joinable())
        worker.join();
}

//==============================================================================
bool LinearPhaseEngine::sameBands (const LinearPhaseBands& a, const LinearPhaseBands& b) noexcept
{
    for (size_t i = 0; i < a.size(); ++i)
    {
        const auto& x = a[i];
        const auto& y = b[i];
        if (x.active != y.active)
            return false;
        if (! x.active)
            continue;
        if (x.type != y.type || x.slopeIndex != y.slopeIndex || x.stereo != y.stereo || ! sameValue (x.freqHz, y.freqHz)
            || ! sameValue (x.gainDb, y.gainDb) || ! sameValue (x.q, y.q))
            return false;
    }
    return true;
}

int LinearPhaseEngine::findFreeKernel() noexcept
{
    for (int i = 0; i < (int) kernels.size(); ++i)
    {
        int expected = 0;
        if (kernels[(size_t) i].state.compare_exchange_strong (expected, 1, std::memory_order_acq_rel))
            return i;
    }
    return -1;
}

void LinearPhaseEngine::publishKernel (int index) noexcept
{
    kernels[(size_t) index].state.store (2, std::memory_order_release);
    const int old = pendingKernel.exchange (index, std::memory_order_acq_rel);
    if (old >= 0)
        kernels[(size_t) old].state.store (0, std::memory_order_release); // never picked up: reclaim
    designCount.fetch_add (1, std::memory_order_relaxed);
}

void LinearPhaseEngine::setBands (const LinearPhaseBands& bands, int numChannels, bool synchronous) noexcept
{
    numChannels = clampValue (numChannels, 1, kMaxChannels);
    if (haveRequest && numChannels == lastRequest.numChannels && sameBands (bands, lastRequest.bands))
        return;
    lastRequest.bands = bands;
    lastRequest.numChannels = numChannels;
    haveRequest = true;

    if (synchronous || ! worker.joinable())
    {
        // Offline rendering / no worker: design now. Blocking on the worker's current design is acceptable here.
        const std::lock_guard<std::mutex> lock (designMutex);
        const int i = findFreeKernel(); // at most 3 kernels are held elsewhere (current, next, pending)
        if (i < 0)
            return;
        designInto (kernels[(size_t) i], lastRequest, Publish::IfFree);
        publishKernel (i);
    }
    else
    {
        mailbox.writeBuffer() = lastRequest;
        mailbox.publish();
    }
}

void LinearPhaseEngine::workerLoop()
{
    Request local;
    bool havePending = false;
    auto lastWork = std::chrono::steady_clock::now() - std::chrono::seconds (10);
    while (! quit.load (std::memory_order_acquire))
    {
        if (mailbox.fetch())
        {
            local = mailbox.readBuffer();
            havePending = true;
        }
        if (havePending)
        {
            const std::lock_guard<std::mutex> lock (designMutex);
            const int i = findFreeKernel();
            if (i >= 0)
            {
                designInto (kernels[(size_t) i], local, Publish::Blocking);
                publishKernel (i);
                havePending = false;
                lastWork = std::chrono::steady_clock::now();
                continue;
            }
        }
        // Poll quickly while edits are happening, slowly when idle.
        const bool busy = std::chrono::steady_clock::now() - lastWork < std::chrono::seconds (1);
        std::this_thread::sleep_for (std::chrono::milliseconds (busy || havePending ? 2 : 15));
    }
}

//==============================================================================
void LinearPhaseEngine::designInto (Kernel& kernel, const Request& r, Publish publish)
{
    const int nb = N + 1;
    const bool mono = r.numChannels == 1;

    // 1. Routing in use -> filter form.
    bool lr = false, ms = false;
    for (const auto& b : r.bands)
    {
        if (! b.active || mono)
            continue;
        lr = lr || b.stereo == StereoMode::Left || b.stereo == StereoMode::Right;
        ms = ms || b.stereo == StereoMode::Mid || b.stereo == StereoMode::Side;
    }
    const int form = (mono || (! lr && ! ms)) ? kScalar : (lr && ! ms ? kDiagonal : kFull);

    std::fill (m00.begin(), m00.end(), 1.0);
    std::fill (m11.begin(), m11.end(), 1.0);
    std::fill (m01.begin(), m01.end(), 0.0);
    std::fill (m10.begin(), m10.end(), 0.0);

    // 2. Exact magnitude of every active band (the same digital designs the minimum-phase engine runs),
    //    combined per channel domain in band order.
    for (const auto& b : r.bands)
    {
        if (! b.active)
            continue;
        StereoMode mode = b.stereo;
        if (mono)
        {
            // Mono bus: Stereo and Mid bands process the single channel; Left/Right/Side have no meaning.
            if (mode != StereoMode::Stereo && mode != StereoMode::Mid)
                continue;
            mode = StereoMode::Stereo;
        }
        const DesignSpec spec { b.type, b.freqHz, clampValue (b.gainDb, -BandProcessor::kMaxCombinedGainDb, BandProcessor::kMaxCombinedGainDb),
                                b.q, b.slopeIndex };
        const auto cc = designCascade (spec, fs);
        if (cc.identity)
            continue;

        for (int k = 0; k < nb; ++k)
        {
            const double t = tanHalf[(size_t) k];
            double power = 1.0;
            for (int s = 0; s < cc.numSections; ++s)
            {
                const auto& c = cc.sections[(size_t) s];
                if (! std::isfinite (t))
                {
                    power *= c.chp * c.chp; // Nyquist
                    continue;
                }
                const double u = t / c.g, u2 = u * u;
                if (c.order == 1)
                {
                    power *= (c.clp * c.clp + c.chp * c.chp * u2) / (1.0 + u2);
                }
                else
                {
                    const double nr = c.clp - c.chp * u2, ni = c.cbp * u;
                    const double dr = 1.0 - u2, di = c.k * u;
                    power *= (nr * nr + ni * ni) / (dr * dr + di * di);
                }
            }
            bandMag[(size_t) k] = std::sqrt (power);
        }

        switch (form)
        {
            case kScalar:
                for (int k = 0; k < nb; ++k)
                    m00[(size_t) k] *= bandMag[(size_t) k];
                break;
            case kDiagonal:
                for (int k = 0; k < nb; ++k)
                {
                    const double m = bandMag[(size_t) k];
                    if (mode != StereoMode::Right)
                        m00[(size_t) k] *= m;
                    if (mode != StereoMode::Left)
                        m11[(size_t) k] *= m;
                }
                break;
            default:
                for (int k = 0; k < nb; ++k)
                {
                    const double m = bandMag[(size_t) k];
                    const auto i = (size_t) k;
                    switch (mode)
                    {
                        case StereoMode::Stereo:
                            m00[i] *= m; m01[i] *= m; m10[i] *= m; m11[i] *= m;
                            break;
                        case StereoMode::Left:
                            m00[i] *= m; m01[i] *= m;
                            break;
                        case StereoMode::Right:
                            m10[i] *= m; m11[i] *= m;
                            break;
                        case StereoMode::Mid:
                        case StereoMode::Side:
                        {
                            // B = T^-1 diag(gm, gs) T with M = (L+R)/2, S = (L-R)/2:  B = 0.5 [[gm+gs, gm-gs], [gm-gs, gm+gs]]
                            const double gm = mode == StereoMode::Mid ? m : 1.0, gs = mode == StereoMode::Side ? m : 1.0;
                            const double a = 0.5 * (gm + gs), d = 0.5 * (gm - gs);
                            const double n00 = a * m00[i] + d * m10[i], n01 = a * m01[i] + d * m11[i];
                            const double n10 = d * m00[i] + a * m10[i], n11 = d * m01[i] + a * m11[i];
                            m00[i] = n00; m01[i] = n01; m10[i] = n10; m11[i] = n11;
                            break;
                        }
                        case StereoMode::NumModes:
                            break;
                    }
                }
                break;
        }
    }

    // 3. Each path: zero-phase target -> inverse FFT -> centre at N/2 -> Hann window -> partition spectra.
    const std::vector<double>* paths[4] = { &m00, &m11, nullptr, nullptr };
    int numPaths = 1;
    if (form == kDiagonal)
        numPaths = 2;
    else if (form == kFull)
    {
        paths[0] = &m00; paths[1] = &m01; paths[2] = &m10; paths[3] = &m11;
        numPaths = 4;
    }

    for (int p = 0; p < numPaths; ++p)
    {
        const auto& mag = *paths[p];
        for (int k = 0; k < nb; ++k)
            spec[(size_t) k] = { mag[(size_t) k], 0.0 };
        designFft.inverse (spec.data(), timeBuf.data());
        double* fir = firScratch.data() + (size_t) p * (size_t) N;
        for (int n = 0; n < N; ++n)
            fir[n] = timeBuf[(size_t) ((n - N / 2 + Nd) % Nd)] * window[(size_t) n];

        for (int kp = 0; kp < K; ++kp)
        {
            std::memcpy (blockIn.data(), fir + (size_t) kp * (size_t) P, sizeof (double) * (size_t) P);
            std::fill (blockIn.begin() + P, blockIn.end(), 0.0);
            designBlockFft.forward (blockIn.data(), blockSpec.data());
            const size_t base = ((size_t) p * (size_t) K + (size_t) kp) * (size_t) bins;
            for (int i = 0; i < bins; ++i)
            {
                kernel.re[base + (size_t) i] = (float) blockSpec[(size_t) i].real();
                kernel.im[base + (size_t) i] = (float) blockSpec[(size_t) i].imag();
            }
        }
    }
    kernel.form = form;
    lastForm.store (form, std::memory_order_relaxed);

    if (publish == Publish::No)
        return;

    // 4. Realised response of the FIR actually designed (for the editor), per channel domain. From the audio
    //    thread (offline designs) the lock is only taken if free.
    std::unique_lock<std::mutex> lock (responseMutex, std::defer_lock);
    if (publish == Publish::Blocking)
        lock.lock();
    else if (! lock.try_lock())
        return;
    auto responseOf = [&] (int pa, int pb, double sign, std::vector<float>& out)
    {
        const double* a = firScratch.data() + (size_t) pa * (size_t) N;
        const double* b = pb >= 0 ? firScratch.data() + (size_t) pb * (size_t) N : nullptr;
        for (int n = 0; n < N; ++n)
            respScratch[(size_t) n] = a[n] + (b != nullptr ? sign * b[n] : 0.0);
        std::fill (respScratch.begin() + N, respScratch.end(), 0.0);
        designFft.forward (respScratch.data(), spec.data());
        out.resize ((size_t) nb); // capacity reserved in the constructor: no allocation
        for (int k = 0; k < nb; ++k)
            out[(size_t) k] = (float) (20.0 * std::log10 (std::max (std::abs (spec[(size_t) k]), 1e-15)));
    };
    switch (form)
    {
        case kScalar:
            responseOf (0, -1, 0.0, published.domainA);
            published.domainB.clear();
            published.nameA = 'C';
            published.nameB = ' ';
            break;
        case kDiagonal:
            responseOf (0, -1, 0.0, published.domainA);
            responseOf (1, -1, 0.0, published.domainB);
            published.nameA = 'L';
            published.nameB = 'R';
            break;
        default:
            if (! lr)
            {
                // Only Stereo and Mid/Side bands: M and S are the system's eigen-domains.
                responseOf (0, 1, 1.0, published.domainA);
                responseOf (0, 1, -1.0, published.domainB);
                published.nameA = 'M';
                published.nameB = 'S';
            }
            else
            {
                responseOf (0, -1, 0.0, published.domainA);
                responseOf (3, -1, 0.0, published.domainB);
                published.nameA = 'L';
                published.nameB = 'R';
            }
            break;
    }
    published.binHz = fs / Nd;
    ++published.version;
}

size_t LinearPhaseEngine::getAllocatedBytes() const noexcept
{
    size_t b = 0;
    auto add = [&b] (const auto& v) { b += v.capacity() * sizeof (v[0]); };
    for (const auto* v : { &tanHalf, &m00, &m01, &m10, &m11, &bandMag, &timeBuf, &firScratch, &window, &blockIn,
                           &fdlRe, &fdlIm, &inBuf, &outBuf, &accRe, &accIm, &yOld, &ifftOut, &dryDelay, &respScratch })
        add (*v);
    add (spec);
    add (blockSpec);
    add (convSpec);
    add (ifftIn);
    for (const auto& k : kernels)
    {
        add (k.re);
        add (k.im);
    }
    b += (published.domainA.capacity() + published.domainB.capacity()) * sizeof (float);
    // FFT tables: roughly 3 complex values per point for each transform size
    b += (size_t) (Nd + 4 * P) * 3 * sizeof (std::complex<double>);
    return b;
}

bool LinearPhaseEngine::getRealisedResponse (RealisedResponse& out) const
{
    const std::lock_guard<std::mutex> lock (responseMutex);
    if (published.version == 0)
        return false;
    out = published;
    return true;
}

bool LinearPhaseEngine::copyLastDesignedFir (int path, std::vector<double>& taps)
{
    const std::lock_guard<std::mutex> lock (designMutex);
    if (designCount.load() == 0 || path < 0 || path > 3)
        return false;
    taps.assign (firScratch.begin() + (std::ptrdiff_t) path * N, firScratch.begin() + (std::ptrdiff_t) (path + 1) * N);
    return true;
}

//==============================================================================
void LinearPhaseEngine::reset() noexcept
{
    std::fill (fdlRe.begin(), fdlRe.end(), 0.0);
    std::fill (fdlIm.begin(), fdlIm.end(), 0.0);
    std::fill (inBuf.begin(), inBuf.end(), 0.0);
    std::fill (outBuf.begin(), outBuf.end(), 0.0);
    std::fill (dryDelay.begin(), dryDelay.end(), 0.0);
    fdlHead = 0;
    blockPos = 0;
    dryPos = 0;

    // Take the newest design immediately (no crossfade after a reset).
    if (nextKernel >= 0)
    {
        kernels[(size_t) currentKernel].state.store (0, std::memory_order_release);
        currentKernel = nextKernel;
        nextKernel = -1;
    }
    const int p = pendingKernel.exchange (-1, std::memory_order_acq_rel);
    if (p >= 0)
    {
        kernels[(size_t) p].state.store (3, std::memory_order_release);
        if (currentKernel >= 0)
            kernels[(size_t) currentKernel].state.store (0, std::memory_order_release);
        currentKernel = p;
    }
    fadeBlock = 0;
}

void LinearPhaseEngine::process (double* x, double* dry, int numChannels, int numSamples) noexcept
{
    const int nch = clampValue (numChannels, 1, kMaxChannels);
    channelsInUse = nch;
    const int L = geo.latency;
    for (int i = 0; i < numSamples; ++i)
    {
        for (int c = 0; c < nch; ++c)
        {
            double& io = x[i * nch + c];
            inBuf[(size_t) (c * 2 * P + P + blockPos)] = io;
            io = outBuf[(size_t) (c * P + blockPos)];

            double& slot = dryDelay[(size_t) (c * L + dryPos)];
            double& d = dry[i * nch + c];
            const double delayed = slot;
            slot = d;
            d = delayed;
        }
        if (++dryPos == L)
            dryPos = 0;
        if (++blockPos == P)
        {
            blockPos = 0;
            convolveBlock();
        }
    }
}

void LinearPhaseEngine::convolveBlock() noexcept
{
    const int nch = channelsInUse;

    // New input spectra (overlap-save: [previous block, current block]) into the frequency-domain delay line.
    fdlHead = fdlHead + 1 == K ? 0 : fdlHead + 1;
    for (int c = 0; c < nch; ++c)
    {
        double* in = inBuf.data() + (size_t) (c * 2 * P);
        blockFft.forward (in, convSpec.data());
        const size_t base = ((size_t) c * (size_t) K + (size_t) fdlHead) * (size_t) bins;
        for (int i = 0; i < bins; ++i)
        {
            fdlRe[base + (size_t) i] = convSpec[(size_t) i].real();
            fdlIm[base + (size_t) i] = convSpec[(size_t) i].imag();
        }
        std::memmove (in, in + P, sizeof (double) * (size_t) P);
    }

    // Pick up a new design (not while a crossfade is running).
    if (nextKernel < 0)
    {
        const int p = pendingKernel.exchange (-1, std::memory_order_acq_rel);
        if (p >= 0)
        {
            kernels[(size_t) p].state.store (3, std::memory_order_release);
            if (currentKernel < 0)
                currentKernel = p;
            else
            {
                nextKernel = p;
                fadeBlock = 0;
            }
        }
    }

    if (currentKernel < 0)
    {
        std::fill (outBuf.begin(), outBuf.end(), 0.0);
        return;
    }

    auto render = [this, nch] (const Kernel& kern, double* out)
    {
        // (path, input channel) pairs contributing to each output channel.
        int pathIdx[2][2] = { { 0, -1 }, { 0, -1 } }, inCh[2][2] = { { 0, -1 }, { 1, -1 } };
        if (nch == 2 && kern.form == kDiagonal)
            pathIdx[1][0] = 1;
        else if (nch == 2 && kern.form == kFull)
        {
            pathIdx[0][0] = 0; inCh[0][0] = 0; pathIdx[0][1] = 1; inCh[0][1] = 1;
            pathIdx[1][0] = 2; inCh[1][0] = 0; pathIdx[1][1] = 3; inCh[1][1] = 1;
        }

        std::fill (accRe.begin(), accRe.end(), 0.0);
        std::fill (accIm.begin(), accIm.end(), 0.0);
        // Partition-major order: a kernel partition shared by both channels is used while it is in cache.
        for (int kp = 0; kp < K; ++kp)
        {
            const int slot = fdlHead - kp < 0 ? fdlHead - kp + K : fdlHead - kp;
            for (int oc = 0; oc < nch; ++oc)
                for (int t = 0; t < 2; ++t)
                {
                    if (pathIdx[oc][t] < 0)
                        continue;
                    const size_t hb = ((size_t) pathIdx[oc][t] * (size_t) K + (size_t) kp) * (size_t) bins;
                    const size_t xb = ((size_t) inCh[oc][t] * (size_t) K + (size_t) slot) * (size_t) bins;
                    const float* hr = kern.re.data() + hb;
                    const float* hi = kern.im.data() + hb;
                    const double* xr = fdlRe.data() + xb;
                    const double* xi = fdlIm.data() + xb;
                    double* ar = accRe.data() + (size_t) oc * (size_t) bins;
                    double* ai = accIm.data() + (size_t) oc * (size_t) bins;
                    for (int i = 0; i < bins; ++i)
                    {
                        const double a = hr[i], b = hi[i];
                        ar[i] += a * xr[i] - b * xi[i];
                        ai[i] += a * xi[i] + b * xr[i];
                    }
                }
        }
        for (int oc = 0; oc < nch; ++oc)
        {
            const size_t base = (size_t) oc * (size_t) bins;
            for (int i = 0; i < bins; ++i)
                ifftIn[(size_t) i] = { accRe[base + (size_t) i], accIm[base + (size_t) i] };
            blockFft.inverse (ifftIn.data(), ifftOut.data());
            std::memcpy (out + (size_t) (oc * P), ifftOut.data() + P, sizeof (double) * (size_t) P);
        }
    };

    render (kernels[(size_t) currentKernel], outBuf.data());

    if (nextKernel >= 0)
    {
        // Crossfade (raised cosine over kFadeBlocks partitions). Both filters see the same input history and
        // have the same delay, so the fade interpolates magnitudes without phase cancellation.
        std::memcpy (yOld.data(), outBuf.data(), sizeof (double) * (size_t) (nch * P));
        render (kernels[(size_t) nextKernel], outBuf.data());
        const double inv = 1.0 / (kFadeBlocks * P);
        for (int c = 0; c < nch; ++c)
            for (int i = 0; i < P; ++i)
            {
                const double w = fadeShape ((fadeBlock * P + i + 1) * inv);
                double& y = outBuf[(size_t) (c * P + i)];
                const double o = yOld[(size_t) (c * P + i)];
                y = o + w * (y - o);
            }
        if (++fadeBlock == kFadeBlocks)
        {
            kernels[(size_t) currentKernel].state.store (0, std::memory_order_release);
            currentKernel = nextKernel;
            nextKernel = -1;
        }
    }
}

} // namespace mepro::dsp
