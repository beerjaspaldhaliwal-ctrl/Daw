// ME Pro EQ — ME Audio
#include "FilterBand.h"
#include "MathUtils.h"
#include "StereoProcessing.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace mepro::dsp
{

namespace
{
    double targetLog2Freq (const BandSettings& s) noexcept
    {
        return std::log2 (clampValue (std::isfinite (s.freqHz) ? s.freqHz : 1000.0, kMinFreqHz, kMaxFreqHz));
    }
    double targetGain (const BandSettings& s) noexcept
    {
        return clampValue (std::isfinite (s.gainDb) ? s.gainDb : 0.0, kMinGainDb, kMaxGainDb);
    }
    double targetLogQ (const BandSettings& s) noexcept
    {
        return std::log (clampValue (std::isfinite (s.q) ? s.q : 1.0, kMinQ, kMaxQ));
    }
} // namespace

void BandProcessor::prepare (double newSampleRate, int newFinePeriod) noexcept
{
    sampleRate = newSampleRate;
    finePeriod = std::max (1, newFinePeriod);
    coarsePeriod = finePeriod * kFinePerCoarse;
    smoothingCoefFine = std::exp (-(double) finePeriod / (kSmoothingTimeSec * sampleRate));
    smoothingCoefCoarse = std::exp (-(double) coarsePeriod / (kSmoothingTimeSec * sampleRate));
    fadeLength = std::max (1, (int) std::lround (kFadeTimeSec * sampleRate));
    crossfadeLength = std::max (1, (int) std::lround (kCrossfadeTimeSec * sampleRate));
    detector.prepare (sampleRate);
    reset();
}

void BandProcessor::reset() noexcept
{
    state = State::Idle;
    fadePos = 0;
    fadeDir = 0;
    crossfadeLen = crossfadePos = 0;
    samplesUntilTick = 0;
    fineTickIndex = 0;
    dynGainDb = 0.0;
    slots[0].filter.reset();
    slots[1].filter.reset();
    detector.reset();
    snapSmoothing();
    lastDesignedType = FilterType::NumTypes;
    dynGainForUi.store (0.0f, std::memory_order_relaxed);
    detLevelForUi.store (-200.0f, std::memory_order_relaxed);
}

bool BandProcessor::dynamicsActive() const noexcept
{
    return dynamicsAllowed && state == State::Active && target.dyn.enabled
        && typeSupportsDynamics (slots[activeSlot].type);
}

void BandProcessor::snapSmoothing() noexcept
{
    smLog2Freq = targetLog2Freq (target);
    smGainDb = targetGain (target);
    smLogQ = targetLogQ (target);
}

DesignSpec BandProcessor::currentSpec (double extraGainDb) const noexcept
{
    const auto& slot = slots[activeSlot];
    DesignSpec spec;
    spec.type = slot.type;
    spec.slopeIndex = slot.slopeIndex;
    spec.freqHz = std::exp2 (smLog2Freq);
    spec.gainDb = clampValue (smGainDb + extraGainDb, -kMaxCombinedGainDb, kMaxCombinedGainDb);
    spec.q = std::exp (smLogQ);
    return spec;
}

bool BandProcessor::structureDiffers (const Slot& slot) const noexcept
{
    return slot.type != target.type || slot.stereo != target.stereo
        || ! (slot.layout == sectionLayout (target.type, target.slopeIndex));
}

void BandProcessor::configureSlot (Slot& slot) noexcept
{
    slot.type = target.type;
    slot.stereo = target.stereo;
    slot.slopeIndex = target.slopeIndex;
    slot.layout = sectionLayout (slot.type, slot.slopeIndex);
    slot.filter.setLayout (slot.layout);
}

void BandProcessor::releaseBand() noexcept
{
    fadePos = 0;
    fadeDir = 0;
    state = State::Idle;
    crossfadeLen = crossfadePos = 0;
    slots[0].filter.reset();
    slots[1].filter.reset();
    detector.reset();
    dynGainDb = 0.0;
    dynGainForUi.store (0.0f, std::memory_order_relaxed);
    detLevelForUi.store (-200.0f, std::memory_order_relaxed);
}

void BandProcessor::coarseTick() noexcept
{
    bool forceImmediate = false;

    // 1. Activation / deactivation -------------------------------------------------------------------
    if (state == State::Idle)
    {
        if (! target.active)
        {
            snapSmoothing();
            return;
        }
        activeSlot = 0;
        snapSmoothing();
        dynGainDb = 0.0;
        detector.reset();
        configureSlot (slots[0]);
        crossfadeLen = crossfadePos = 0;
        fadePos = 0;
        fadeDir = 1;
        state = State::Active;
        forceImmediate = true;
    }
    else if (! target.active)
    {
        fadeDir = -1;
    }
    else if (fadePos < fadeLength)
    {
        fadeDir = 1;
    }

    // 2. Structural changes -> crossfade between the two slots -----------------------------------------
    if (crossfadeLen == 0 && structureDiffers (slots[activeSlot]))
    {
        if (fadePos == 0)
        {
            configureSlot (slots[activeSlot]); // currently inaudible: switch directly
        }
        else
        {
            const int other = 1 - activeSlot;
            configureSlot (slots[other]);
            activeSlot = other;
            crossfadePos = 0;
            crossfadeLen = crossfadeLength;
        }
        forceImmediate = true;
    }
    else if (slots[activeSlot].slopeIndex != target.slopeIndex
             && slots[activeSlot].layout == sectionLayout (target.type, target.slopeIndex))
    {
        slots[activeSlot].slopeIndex = target.slopeIndex;
    }

    updateResponse (dynamicsActive(), forceImmediate);
}

void BandProcessor::updateResponse (bool fineRate, bool forceImmediate) noexcept
{
    // Parameter smoothing (one-pole in perceptual domains), stepped by the time since the last update.
    const double coef = fineRate ? smoothingCoefFine : smoothingCoefCoarse;
    auto smooth = [coef] (double& v, double t, double eps)
    {
        v = t + (v - t) * coef;
        if (std::abs (v - t) < eps)
            v = t;
    };
    smooth (smLog2Freq, targetLog2Freq (target), 1e-7);
    smooth (smGainDb, targetGain (target), 1e-6);
    smooth (smLogQ, targetLogQ (target), 1e-7);

    // Dynamics
    const auto& slot = slots[activeSlot];
    const bool dynOn = dynamicsActive();
    if (dynOn)
    {
        detector.setFilter (slot.type, std::exp2 (smLog2Freq), std::exp (smLogQ));
        detector.setTiming (target.dyn.attackMs, target.dyn.releaseMs, target.dyn.detector);
    }
    dynGainDb = dynOn ? dynamicGainForLevel (detector.levelDb(), target.dyn) : 0.0;
    dynGainForUi.store ((float) dynGainDb, std::memory_order_relaxed);
    detLevelForUi.store (dynOn ? (float) detector.levelDb() : -200.0f, std::memory_order_relaxed);

    // Redesign when anything that affects the response changed.
    const DesignSpec spec = currentSpec (dynGainDb);
    if (forceImmediate || spec.type != lastDesignedType || spec.slopeIndex != lastDesignedSlope
        || smLog2Freq != lastDesignedLog2F || smLogQ != lastDesignedLogQ || spec.gainDb != lastDesignedGain)
    {
        const CascadeCoeffs c = designCascade (spec, sampleRate);
        auto& s = slots[activeSlot];
        s.targetIsIdentity = c.identity;
        if (forceImmediate)
            s.filter.setCoefficients (c);
        else
            s.filter.rampTo (c, dynOn ? finePeriod : coarsePeriod);

        lastDesignedType = spec.type;
        lastDesignedSlope = spec.slopeIndex;
        lastDesignedLog2F = smLog2Freq;
        lastDesignedLogQ = smLogQ;
        lastDesignedGain = spec.gainDb;
    }
}

void BandProcessor::processSlot (Slot& slot, double* x, int numChannels, int n, const BandScratch& scratch) noexcept
{
    if (numChannels == 1)
    {
        // Mono bus: Stereo and Mid bands process the single channel; Left/Right/Side have no meaning.
        if (slot.stereo == StereoMode::Stereo || slot.stereo == StereoMode::Mid)
            slot.filter.processMono (x, n, 1, 0);
        return;
    }

    switch (slot.stereo)
    {
        case StereoMode::Stereo:
            slot.filter.processStereo (x, n);
            break;
        case StereoMode::Left:
            slot.filter.processMono (x, n, 2, 0);
            break;
        case StereoMode::Right:
            slot.filter.processMono (x + 1, n, 2, 0);
            break;
        case StereoMode::Mid:
        case StereoMode::Side:
        {
            const bool mid = slot.stereo == StereoMode::Mid;
            encodeMidSide (x, n, scratch.mid, scratch.side);
            const bool passThrough = slot.targetIsIdentity && ! slot.filter.isRamping();
            slot.filter.processMono (mid ? scratch.mid : scratch.side, n, 1, 0);
            if (! passThrough) // identity: keep the state warm but leave the audio bit-exact
                decodeMidSide (scratch.mid, scratch.side, n, x);
            break;
        }
        default:
            break;
    }
}

void BandProcessor::runDetector (const double* x, int numChannels, int n, const double* sc, int scChannels,
                                 const BandScratch& scratch) noexcept
{
    const bool external = target.dyn.source == SidechainSource::External;
    if (external && sc == nullptr)
    {
        // External sidechain selected but the host provides no sidechain signal: the detector hears silence.
        std::fill (scratch.det, scratch.det + n, 0.0);
        detector.processDerived (scratch.det, n);
        return;
    }

    const double* src = external ? sc : x;
    const int ch = external ? scChannels : numChannels;

    if (ch == 1)
    {
        detector.processMono (src, n, 1, scratch.det);
        return;
    }

    switch (slots[activeSlot].stereo)
    {
        case StereoMode::Stereo:
            detector.processStereoLinked (src, n, scratch.det);
            break;
        case StereoMode::Left:
            detector.processMono (src, n, 2, scratch.det);
            break;
        case StereoMode::Right:
            detector.processMono (src + 1, n, 2, scratch.det);
            break;
        case StereoMode::Mid:
            for (int i = 0; i < n; ++i)
                scratch.det[i] = 0.5 * (src[2 * i] + src[2 * i + 1]);
            detector.processDerived (scratch.det, n);
            break;
        case StereoMode::Side:
            for (int i = 0; i < n; ++i)
                scratch.det[i] = 0.5 * (src[2 * i] - src[2 * i + 1]);
            detector.processDerived (scratch.det, n);
            break;
        default:
            break;
    }
}

void BandProcessor::process (double* buffer, int numChannels, int numSamples,
                             const double* sidechain, int sidechainChannels, const BandScratch& scratch) noexcept
{
    // gridPhase: position within the coarse control period. Fine ticks at multiples of finePeriod, coarse
    // tick at 0. The grid keeps running while the band is idle so timing never depends on block size.
    auto advanceGrid = [this] (int n) { samplesUntilTick = (samplesUntilTick + n) % coarsePeriod; };

    if (state == State::Idle && ! target.active)
    {
        advanceGrid (numSamples);
        snapSmoothing();
        return;
    }

    int pos = 0;
    while (pos < numSamples)
    {
        int& gridPhase = samplesUntilTick;
        if (gridPhase % finePeriod == 0)
        {
            if (gridPhase == 0)
                coarseTick();
            else if (dynamicsActive())
                updateResponse (true, false);
        }

        // Steady state: nothing will change before the end of this block -> one chunk.
        const bool steady = state == State::Active && fadeDir == 0 && fadePos == fadeLength && crossfadeLen == 0
                         && ! dynamicsActive() && ! slots[activeSlot].filter.isRamping()
                         && smLog2Freq == targetLog2Freq (target) && smGainDb == targetGain (target)
                         && smLogQ == targetLogQ (target) && target.active && ! structureDiffers (slots[activeSlot])
                         && slots[activeSlot].slopeIndex == target.slopeIndex;

        int m;
        if (steady)
            m = numSamples - pos;
        else if (dynamicsActive())
            m = std::min (finePeriod - gridPhase % finePeriod, numSamples - pos);
        else
            m = std::min (coarsePeriod - gridPhase, numSamples - pos);

        if (state == State::Active)
        {
            double* x = buffer + (size_t) pos * (size_t) numChannels;
            const double* scx = sidechain != nullptr ? sidechain + (size_t) pos * (size_t) sidechainChannels : nullptr;
            const int ns = m * numChannels;

            if (dynamicsActive())
                runDetector (x, numChannels, m, scx, sidechainChannels, scratch);

            const bool fading = fadeDir != 0 || fadePos < fadeLength;
            const bool needDry = fading || crossfadeLen > 0;
            if (needDry)
                std::memcpy (scratch.dry, x, sizeof (double) * (size_t) ns);

            processSlot (slots[activeSlot], x, numChannels, m, scratch);

            if (crossfadeLen > 0)
            {
                Slot& old = slots[1 - activeSlot];
                std::memcpy (scratch.aux, scratch.dry, sizeof (double) * (size_t) ns);
                processSlot (old, scratch.aux, numChannels, m, scratch);
                const double inv = 1.0 / crossfadeLen;
                for (int i = 0; i < m; ++i)
                {
                    const double b = fadeShape ((crossfadePos + i + 1) * inv);
                    if (b >= 1.0)
                        continue;
                    for (int c = 0; c < numChannels; ++c)
                    {
                        const int j = i * numChannels + c;
                        x[j] = scratch.aux[j] + b * (x[j] - scratch.aux[j]);
                    }
                }
                crossfadePos += m;
                if (crossfadePos >= crossfadeLen)
                {
                    crossfadeLen = crossfadePos = 0;
                    old.filter.reset();
                }
            }

            if (fading)
            {
                const double inv = 1.0 / fadeLength;
                for (int i = 0; i < m; ++i)
                {
                    fadePos = clampValue (fadePos + fadeDir, 0, fadeLength);
                    if (fadePos == fadeLength)
                        continue; // fully wet: leave the processed sample untouched (exact)
                    const double w = fadeShape (fadePos * inv);
                    for (int c = 0; c < numChannels; ++c)
                    {
                        const int j = i * numChannels + c;
                        x[j] = scratch.dry[j] + w * (x[j] - scratch.dry[j]);
                    }
                }
                if (fadeDir > 0 && fadePos == fadeLength)
                    fadeDir = 0;
                else if (fadeDir < 0 && fadePos == 0)
                    releaseBand(); // fully faded out: the output already equals the dry input
            }
        }

        pos += m;
        advanceGrid (m);
    }
}

} // namespace mepro::dsp
