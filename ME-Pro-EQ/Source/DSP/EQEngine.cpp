// ME Pro EQ — ME Audio
#include "EQEngine.h"
#include "MathUtils.h"
#include "StereoProcessing.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace mepro::dsp
{

namespace
{
    int controlPeriodForRate (double fs) noexcept
    {
        // Fine period ~0.17 ms at every rate: 8 samples at 44.1/48 kHz, 16 at 88.2/96 kHz, 32 at 176.4/192 kHz.
        const int mult = std::max (1, (int) std::lround (fs / 48000.0));
        int p = 1;
        while (p < mult)
            p <<= 1;
        return 8 * p;
    }

    CascadeCoeffs designAuditionFilter (const BandSettings& b, double fs) noexcept
    {
        DesignSpec spec;
        spec.freqHz = b.freqHz;
        spec.slopeIndex = 1;
        switch (b.type)
        {
            case FilterType::LowShelf:  spec.type = FilterType::LowPass;  spec.q = kButterworthQ; break;
            case FilterType::HighShelf: spec.type = FilterType::HighPass; spec.q = kButterworthQ; break;
            case FilterType::HighPass:
            case FilterType::LowPass:
            case FilterType::Tilt:      spec.type = FilterType::BandPass; spec.q = kButterworthQ; break;
            default:                    spec.type = FilterType::BandPass; spec.q = std::max (0.5, b.q); break;
        }
        return designCascade (spec, fs);
    }
} // namespace

EQEngine::~EQEngine()
{
    deleteAllLinearPhase();
}

void EQEngine::deleteAllLinearPhase() noexcept
{
    // Only while no audio is processed (destructor / prepare).
    lpForUi.store (nullptr, std::memory_order_release);
    delete lpActive;
    lpActive = nullptr;
    delete lpNext;
    lpNext = nullptr;
    delete lpIncoming.exchange (nullptr);
    delete lpRetired.exchange (nullptr);
    lpRemoveRequest.store (false);
    usingLinear = false;
    usingLinearForUi.store (false);
    currentLatency.store (0);
}

void EQEngine::installLinearPhaseNow (std::unique_ptr<LinearPhaseEngine> lp) noexcept
{
    deleteAllLinearPhase();
    lpActive = lp.release();
    reset();
}

void EQEngine::installLinearPhase (std::unique_ptr<LinearPhaseEngine> lp) noexcept
{
    lpRemoveRequest.store (false, std::memory_order_release);
    delete lpIncoming.exchange (lp.release(), std::memory_order_acq_rel); // a previous one was never taken
}

void EQEngine::removeLinearPhase() noexcept
{
    delete lpIncoming.exchange (nullptr, std::memory_order_acq_rel);
    lpRemoveRequest.store (true, std::memory_order_release);
}

void EQEngine::collectGarbage() noexcept
{
    delete lpRetired.exchange (nullptr, std::memory_order_acq_rel);
}

void EQEngine::retire (LinearPhaseEngine* e) noexcept
{
    if (e == nullptr)
        return;
    if (lpForUi.load (std::memory_order_relaxed) == e)
        lpForUi.store (nullptr, std::memory_order_release);
    lpRetired.store (e, std::memory_order_release); // callers guarantee the slot is empty
}

void EQEngine::updatePhaseMode (int nch) noexcept
{
    // Hand-over of new / removed linear-phase engines (only while the retired slot is free).
    if (lpNext == nullptr && lpRetired.load (std::memory_order_acquire) == nullptr)
    {
        if (auto* e = lpIncoming.exchange (nullptr, std::memory_order_acq_rel))
        {
            if (lpActive == nullptr)
                lpActive = e;
            else if (! usingLinear && modeFadeDir == 0)
            {
                retire (lpActive);
                lpActive = e;
            }
            else
                lpNext = e;
            if (e == lpActive)
                lpActive->reset();
        }
        else if (lpActive != nullptr && ! usingLinear && modeFadeDir == 0
                 && lpRemoveRequest.exchange (false, std::memory_order_acq_rel))
        {
            retire (lpActive);
            lpActive = nullptr;
        }
    }

    const bool lpWanted = settings.phaseMode == PhaseMode::Linear;
    LinearPhaseEngine* target = lpNext != nullptr ? lpNext : lpActive;
    if (lpWanted && target != nullptr)
        target->setBands (settings.bands, nch, nonRealtime);
    const bool wantLinear = lpWanted && target != nullptr && target->isReady();

    if (modeFadeDir < 0 && modeFadePos == 0)
    {
        // The output is silent: switch paths now, then fade back in.
        if (lpNext != nullptr)
        {
            retire (lpActive);
            lpActive = lpNext;
            lpNext = nullptr;
        }
        usingLinear = wantLinear;
        if (usingLinear)
            lpActive->reset();
        else
            for (auto& b : bands)
                b.reset();
        // The new path starts from silence; its first output (an abrupt onset of the running signal) appears
        // after its latency. Stay silent until then, then fade in.
        modeHold = usingLinear ? lpActive->getLatency() : 0;
        modeFadeDir = 1;
    }
    else if (modeFadeDir == 0 && (wantLinear != usingLinear || (usingLinear && lpNext != nullptr)))
    {
        modeFadeDir = -1;
    }

    lpForUi.store (usingLinear ? lpActive : nullptr, std::memory_order_release);
    usingLinearForUi.store (usingLinear, std::memory_order_relaxed);
    currentLatency.store (usingLinear ? lpActive->getLatency() : 0, std::memory_order_relaxed);
}

void EQEngine::prepare (double newSampleRate, int maxBlockSize, int numChannels)
{
    deleteAllLinearPhase(); // built for a sample rate; the owner installs a new one after prepare()
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    controlPeriod = controlPeriodOverride > 0 ? controlPeriodOverride : controlPeriodForRate (sampleRate);
    maxBlock = std::max ({ maxBlockSize, controlPeriod * BandProcessor::kFinePerCoarse, 64 });
    channelsPrepared = clampValue (numChannels, 1, kMaxChannels);

    const size_t n = (size_t) maxBlock * kMaxChannels;
    for (auto* v : { &work, &dry, &scBuf, &s0, &s1, &s2, &s3, &s4 })
        v->assign (n, 0.0);

    for (auto& b : bands)
        b.prepare (sampleRate, controlPeriod);

    trimCoef = std::exp (-1.0 / (kTrimSmoothingSec * sampleRate));
    {
        const double hi = std::min (20000.0, kMaxDesignFreqRatio * sampleRate);
        for (int i = 0; i < kAutoGainPoints; ++i)
            agOmega[(size_t) i] = kTwoPi * (20.0 * std::pow (hi / 20.0, (double) i / (kAutoGainPoints - 1))) / sampleRate;
        for (auto& c : agCache)
            c.valid = false;
    }
    bypassLen = std::max (1, (int) std::lround (kBypassFadeSec * sampleRate));
    auditionLen = std::max (1, (int) std::lround (kAuditionFadeSec * sampleRate));
    modeFadeLen = std::max (1, (int) std::lround (kModeFadeSec * sampleRate));
    reset();
}

void EQEngine::reset() noexcept
{
    for (auto& b : bands)
    {
        b.reset();
    }
    for (size_t i = 0; i < bands.size(); ++i)
        bands[i].setSettings (settings.bands[i]);

    inGain = dbToGain (clampValue (settings.inputTrimDb, kTrimMinDb, kTrimMaxDb));
    autoGainDb = settings.autoGain ? computeAutoGainDb() : 0.0;
    outGain = dbToGain (clampValue (settings.outputTrimDb + autoGainDb, 2 * kTrimMinDb, 2 * kTrimMaxDb));
    msIn = msOut = 0.0;
    bypassPos = settings.bypass ? bypassLen : 0;

    SectionLayout l;
    l.numSections = 1;
    l.orders[0] = 2;
    auditionFilter.setLayout (l);
    auditionBandActive = -1;
    auditionPos = 0;

    // Phase mode: start directly in the requested mode (renders are processed from the first sample).
    if (lpNext != nullptr && lpRetired.load() == nullptr)
    {
        retire (lpActive);
        lpActive = lpNext;
        lpNext = nullptr;
    }
    if (lpActive != nullptr && settings.phaseMode == PhaseMode::Linear)
        lpActive->setBands (settings.bands, channelsPrepared, true);
    if (lpActive != nullptr)
        lpActive->reset();
    usingLinear = settings.phaseMode == PhaseMode::Linear && lpActive != nullptr && lpActive->isReady();
    modeFadePos = modeFadeLen;
    modeFadeDir = 0;
    modeHold = 0;
    lpForUi.store (usingLinear ? lpActive : nullptr, std::memory_order_release);
    usingLinearForUi.store (usingLinear);
    currentLatency.store (usingLinear ? lpActive->getLatency() : 0);
}

void EQEngine::setSettings (const EngineSettings& s) noexcept
{
    settings = s;
    for (size_t i = 0; i < bands.size(); ++i)
        bands[i].setSettings (s.bands[i]);
}

void EQEngine::storeMax (std::atomic<float>& a, float v) noexcept
{
    float cur = a.load (std::memory_order_relaxed);
    while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed))
    {
    }
}

void EQEngine::process (double* const* channels, int numChannels, int numSamples,
                        const double* const* sidechain, int sidechainChannels) noexcept
{
    numChannels = std::min (numChannels, kMaxChannels);
    sidechainChannels = sidechain != nullptr ? std::min (sidechainChannels, kMaxChannels) : 0;
    int pos = 0;
    while (pos < numSamples)
    {
        const int n = std::min (maxBlock, numSamples - pos);
        for (int c = 0; c < numChannels; ++c)
            for (int i = 0; i < n; ++i)
                work[(size_t) (i * numChannels + c)] = channels[c][pos + i];
        for (int c = 0; c < sidechainChannels; ++c)
            for (int i = 0; i < n; ++i)
                scBuf[(size_t) (i * sidechainChannels + c)] = sidechain[c][pos + i];

        processInterleaved (numChannels, n, sidechainChannels > 0 ? scBuf.data() : nullptr, sidechainChannels);

        for (int c = 0; c < numChannels; ++c)
            for (int i = 0; i < n; ++i)
                channels[c][pos + i] = work[(size_t) (i * numChannels + c)];
        pos += n;
    }
}

void EQEngine::process (float* const* channels, int numChannels, int numSamples,
                        const float* const* sidechain, int sidechainChannels) noexcept
{
    numChannels = std::min (numChannels, kMaxChannels);
    sidechainChannels = sidechain != nullptr ? std::min (sidechainChannels, kMaxChannels) : 0;
    int pos = 0;
    while (pos < numSamples)
    {
        const int n = std::min (maxBlock, numSamples - pos);
        for (int c = 0; c < numChannels; ++c)
            for (int i = 0; i < n; ++i)
                work[(size_t) (i * numChannels + c)] = (double) channels[c][pos + i];
        for (int c = 0; c < sidechainChannels; ++c)
            for (int i = 0; i < n; ++i)
                scBuf[(size_t) (i * sidechainChannels + c)] = (double) sidechain[c][pos + i];

        processInterleaved (numChannels, n, sidechainChannels > 0 ? scBuf.data() : nullptr, sidechainChannels);

        for (int c = 0; c < numChannels; ++c)
            for (int i = 0; i < n; ++i)
                channels[c][pos + i] = (float) work[(size_t) (i * numChannels + c)];
        pos += n;
    }
}

void EQEngine::processInterleaved (int nch, int n, const double* sc, int scCh) noexcept
{
    double* x = work.data();
    const int ns = n * nch;

    updatePhaseMode (nch);

    // 1. Sanitize input and meter it.
    float pk[2] = { 0.0f, 0.0f };
    for (int j = 0; j < ns; ++j)
    {
        double v = x[j];
        if (! std::isfinite (v))
        {
            v = 0.0;
            x[j] = 0.0;
        }
        const float a = (float) std::abs (v);
        float& p = pk[j % nch];
        p = a > p ? a : p;
    }
    for (int c = 0; c < nch; ++c)
        storeMax (inPeak[(size_t) c], pk[c]);
    if (sc != nullptr)
        for (int j = 0; j < n * scCh; ++j)
            if (! std::isfinite (sc[j]))
                scBuf[(size_t) j] = 0.0;

    std::memcpy (dry.data(), x, sizeof (double) * (size_t) ns);

    // 2. Input trim (per-sample one-pole smoothing of the linear gain).
    const double inTarget = dbToGain (clampValue (settings.inputTrimDb, kTrimMinDb, kTrimMaxDb));
    if (inGain != inTarget || inTarget != 1.0)
    {
        for (int i = 0; i < n; ++i)
        {
            inGain = inTarget + (inGain - inTarget) * trimCoef;
            if (std::abs (inGain - inTarget) < 1e-12)
                inGain = inTarget;
            for (int c = 0; c < nch; ++c)
                x[i * nch + c] *= inGain;
        }
    }

    if (preTap != nullptr)
        preTap->push (x, nch, n);

    // 3. Bands (serial, minimum phase) or the linear-phase FIR system (which also delays the dry signal so the
    //    bypass crossfade and the level comparison stay time-aligned).
    if (usingLinear)
    {
        lpActive->process (x, dry.data(), nch, n);
    }
    else
    {
        const BandScratch scratch { s0.data(), s1.data(), s2.data(), s3.data(), s4.data() };
        for (auto& b : bands)
            b.process (x, nch, n, sc, scCh, scratch);
    }

    // 4. Band audition (monitoring only).
    applyAudition (nch, n);

    // 5. Output trim (+ optional auto gain).
    autoGainDb = settings.autoGain ? computeAutoGainDb() : 0.0;
    autoGainForUi.store ((float) autoGainDb, std::memory_order_relaxed);
    const double outTarget = dbToGain (clampValue (settings.outputTrimDb, kTrimMinDb, kTrimMaxDb) + autoGainDb);
    if (outGain != outTarget || outTarget != 1.0)
    {
        for (int i = 0; i < n; ++i)
        {
            outGain = outTarget + (outGain - outTarget) * trimCoef;
            if (std::abs (outGain - outTarget) < 1e-12)
                outGain = outTarget;
            for (int c = 0; c < nch; ++c)
                x[i * nch + c] *= outGain;
        }
    }

    // 6. Global bypass: raised-cosine crossfade to the untouched input. Fully bypassed => bit-exact input.
    const int bypassDir = settings.bypass ? 1 : -1;
    if (bypassPos > 0 || bypassDir > 0)
    {
        const double inv = 1.0 / bypassLen;
        for (int i = 0; i < n; ++i)
        {
            bypassPos = clampValue (bypassPos + bypassDir, 0, bypassLen);
            if (bypassPos == 0)
                continue; // fully active: processed sample untouched
            const double w = fadeShape (bypassPos * inv);
            for (int c = 0; c < nch; ++c)
            {
                const int j = i * nch + c;
                x[j] = bypassPos == bypassLen ? dry[(size_t) j] : x[j] + w * (dry[(size_t) j] - x[j]);
            }
        }
    }

    // 6b. Phase-mode switch: fade out, switch at silence (next block), fade in.
    if (modeFadeDir != 0 || modeFadePos < modeFadeLen)
    {
        const double inv = 1.0 / modeFadeLen;
        for (int i = 0; i < n; ++i)
        {
            if (modeHold > 0)
            {
                --modeHold;
                for (int c = 0; c < nch; ++c)
                    x[i * nch + c] = 0.0;
                continue;
            }
            if (modeFadeDir != 0)
            {
                modeFadePos = clampValue (modeFadePos + modeFadeDir, 0, modeFadeLen);
                if (modeFadeDir > 0 && modeFadePos == modeFadeLen)
                    modeFadeDir = 0;
            }
            const double g = fadeShape (modeFadePos * inv);
            for (int c = 0; c < nch; ++c)
                x[i * nch + c] *= g;
        }
    }

    // 7. Mono compatibility audition (monitoring only).
    if (settings.monoAudition && nch == 2)
        foldToMono (x, n);

    // 8. Non-finite guard: should never trigger; if it does, recover silently-safe and count it.
    float opk[2] = { 0.0f, 0.0f };
    bool bad = false;
    for (int j = 0; j < ns; ++j)
    {
        const double v = x[j];
        if (! std::isfinite (v))
        {
            bad = true;
            break;
        }
        const float a = (float) std::abs (v);
        float& p = opk[j % nch];
        p = a > p ? a : p;
    }
    if (bad)
    {
        std::fill (x, x + ns, 0.0);
        for (auto& b : bands)
            b.reset();
        if (usingLinear)
            lpActive->reset();
        auditionFilter.reset();
        nonFiniteEvents.fetch_add (1, std::memory_order_relaxed);
        opk[0] = opk[1] = 0.0f;
    }
    for (int c = 0; c < nch; ++c)
    {
        storeMax (outPeak[(size_t) c], opk[c]);
        if (opk[c] > 1.0f)
            clipFlag.store (true, std::memory_order_relaxed);
    }

    // Level comparison: RMS of input and output (mean over channels), 3 s time constant.
    {
        double si = 0.0, so = 0.0;
        for (int j = 0; j < ns; ++j)
        {
            si += dry[(size_t) j] * dry[(size_t) j];
            so += x[j] * x[j];
        }
        const double a = std::exp (-(double) n / (3.0 * sampleRate));
        msIn = si / ns + (msIn - si / ns) * a;
        msOut = so / ns + (msOut - so / ns) * a;
        inRmsDb.store ((float) powerToDb (msIn, -200.0), std::memory_order_relaxed);
        outRmsDb.store ((float) powerToDb (msOut, -200.0), std::memory_order_relaxed);
    }

    if (postTap != nullptr)
        postTap->push (x, nch, n);
}

double EQEngine::computeAutoGainDb() noexcept
{
    // Update the per-band power responses that changed (designs are allocation-free).
    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& s = settings.bands[(size_t) b];
        auto& c = agCache[(size_t) b];
        const bool same = c.valid && c.settings.active == s.active && c.settings.type == s.type
                       && sameValue (c.settings.freqHz, s.freqHz) && sameValue (c.settings.gainDb, s.gainDb)
                       && sameValue (c.settings.q, s.q) && c.settings.slopeIndex == s.slopeIndex && c.settings.stereo == s.stereo;
        if (same)
            continue;
        c.settings = s;
        c.valid = true;
        c.identity = ! s.active;
        if (s.active)
        {
            const auto cc = designCascade ({ s.type, s.freqHz, s.gainDb, s.q, s.slopeIndex }, sampleRate);
            for (int i = 0; i < kAutoGainPoints; ++i)
                c.power[(size_t) i] = std::norm (cc.response (agOmega[(size_t) i]));
        }
    }

    bool lr = false, ms = false;
    for (const auto& c : agCache)
        if (! c.identity)
        {
            lr = lr || c.settings.stereo == StereoMode::Left || c.settings.stereo == StereoMode::Right;
            ms = ms || c.settings.stereo == StereoMode::Mid || c.settings.stereo == StereoMode::Side;
        }

    double sum = 0.0;
    for (int i = 0; i < kAutoGainPoints; ++i)
    {
        // Per channel-domain power gain (channels assumed uncorrelated, equal energy).
        double st = 1.0, l = 1.0, r = 1.0, m = 1.0, sd = 1.0;
        for (const auto& c : agCache)
        {
            if (c.identity)
                continue;
            const double p = c.power[(size_t) i];
            switch (c.settings.stereo)
            {
                case StereoMode::Stereo:   st *= p; break;
                case StereoMode::Left:     l *= p; break;
                case StereoMode::Right:    r *= p; break;
                case StereoMode::Mid:      m *= p; break;
                case StereoMode::Side:     sd *= p; break;
                case StereoMode::NumModes: break;
            }
        }
        double g;
        if (lr && ms)
            g = st * (l + r + m + sd) * 0.25;
        else if (lr)
            g = st * (l + r) * 0.5;
        else if (ms)
            g = st * (m + sd) * 0.5;
        else
            g = st;
        sum += g;
    }
    return clampValue (-powerToDb (sum / kAutoGainPoints, -200.0), kTrimMinDb, kTrimMaxDb);
}

void EQEngine::applyAudition (int nch, int n) noexcept
{
    int req = settings.auditionBand;
    if (req >= 0 && (req >= kNumBands || ! settings.bands[(size_t) req].active))
        req = -1;

    if (req >= 0)
    {
        const auto& b = settings.bands[(size_t) req];
        if (req != auditionBandActive || b.stereo != auditionMode)
        {
            auditionFilter.reset();
            auditionBandActive = req;
            auditionMode = b.stereo;
            auditionLastFreq = -1.0;
        }
        if (b.freqHz != auditionLastFreq || b.q != auditionLastQ)
        {
            auditionFilter.setCoefficients (designAuditionFilter (b, sampleRate));
            auditionLastFreq = b.freqHz;
            auditionLastQ = b.q;
        }
    }
    const int dir = req >= 0 ? 1 : -1;

    if (auditionPos == 0 && dir < 0)
    {
        auditionBandActive = -1;
        return;
    }

    double* x = work.data();
    double* a = s0.data();
    const int ns = n * nch;
    std::memcpy (a, x, sizeof (double) * (size_t) ns);

    if (nch == 1 || auditionMode == StereoMode::Stereo)
    {
        if (nch == 2)
            auditionFilter.processStereo (a, n);
        else
            auditionFilter.processMono (a, n, 1, 0);
    }
    else
    {
        double* comp = s1.data();
        for (int i = 0; i < n; ++i)
        {
            const double l = x[2 * i], r = x[2 * i + 1];
            switch (auditionMode)
            {
                case StereoMode::Left:  comp[i] = l; break;
                case StereoMode::Right: comp[i] = r; break;
                case StereoMode::Mid:   comp[i] = 0.5 * (l + r); break;
                default:                comp[i] = 0.5 * (l - r); break;
            }
        }
        auditionFilter.processMono (comp, n, 1, 0);
        for (int i = 0; i < n; ++i)
            a[2 * i] = a[2 * i + 1] = comp[i]; // the auditioned component on both speakers
    }

    const double inv = 1.0 / auditionLen;
    for (int i = 0; i < n; ++i)
    {
        auditionPos = clampValue (auditionPos + dir, 0, auditionLen);
        if (auditionPos == 0)
            continue;
        const double w = fadeShape (auditionPos * inv);
        for (int c = 0; c < nch; ++c)
        {
            const int j = i * nch + c;
            x[j] = auditionPos == auditionLen ? a[j] : x[j] + w * (a[j] - x[j]);
        }
    }
    if (auditionPos == 0)
        auditionBandActive = -1;
}

} // namespace mepro::dsp
