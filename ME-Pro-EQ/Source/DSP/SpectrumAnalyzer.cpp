// ME Pro EQ — ME Audio
#include "SpectrumAnalyzer.h"
#include "MathUtils.h"

#include <algorithm>
#include <cmath>

namespace mepro::dsp
{

//==============================================================================
AnalyzerFifo::AnalyzerFifo (int capacityFrames)
{
    int c = 1;
    while (c < capacityFrames)
        c <<= 1;
    capacity = c;
    mask = c - 1;
    buffer.assign ((size_t) c * 2, 0.0f);
}

void AnalyzerFifo::push (const double* x, int numChannels, int numFrames) noexcept
{
    if (! active.load (std::memory_order_relaxed))
        return;
    const int w = writePos.load (std::memory_order_relaxed);
    const int r = readPos.load (std::memory_order_acquire);
    const int free = capacity - 1 - ((w - r) & mask);
    const int n = std::min (numFrames, free);
    if (n < numFrames)
        dropped.fetch_add (numFrames - n, std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        const int idx = ((w + i) & mask) * 2;
        const double l = x[i * numChannels];
        const double rr = numChannels > 1 ? x[i * numChannels + 1] : l;
        buffer[(size_t) idx] = (float) l;
        buffer[(size_t) idx + 1] = (float) rr;
    }
    writePos.store ((w + n) & mask, std::memory_order_release);
}

int AnalyzerFifo::pull (float* dest, int maxFrames) noexcept
{
    const int r = readPos.load (std::memory_order_relaxed);
    const int w = writePos.load (std::memory_order_acquire);
    const int avail = (w - r) & mask;
    const int n = std::min (avail, maxFrames);
    for (int i = 0; i < n; ++i)
    {
        const int idx = ((r + i) & mask) * 2;
        dest[2 * i] = buffer[(size_t) idx];
        dest[2 * i + 1] = buffer[(size_t) idx + 1];
    }
    readPos.store ((r + n) & mask, std::memory_order_release);
    return n;
}

//==============================================================================
void SpectrumAnalyzer::configure (const Settings& s, double fs, int numPoints, double fMin, double fMax)
{
    settings = s;
    int n = 1;
    while (n < s.fftSize)
        n <<= 1;
    settings.fftSize = clampValue (n, 1024, 32768);
    sampleRate = fs;
    const int N = settings.fftSize;
    fft = std::make_unique<RealFFT> (N);
    window.resize ((size_t) N);
    // 4-term Blackman-Harris: highest side lobe -92 dB.
    double sum = 0.0;
    for (int i = 0; i < N; ++i)
    {
        const double t = kTwoPi * i / N;
        window[(size_t) i] = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t);
        sum += window[(size_t) i];
    }
    // A sine of peak amplitude A centred on a bin has |X[k]| = A * sum(w) / 2  =>  normalise by (sum/2)^2.
    windowNorm = 1.0 / ((sum * 0.5) * (sum * 0.5));
    frame.assign ((size_t) N, 0.0);
    spectrum.assign ((size_t) N / 2 + 1, {});
    history.assign ((size_t) N * 2, 0.0f);
    histPos = 0;
    sinceHop = 0;
    hop = N / 4;
    accPower.assign ((size_t) N / 2 + 1, 0.0);
    accFrames = 0;
    binDb.assign ((size_t) N / 2 + 1, -200.0f);

    numPoints = std::max (2, numPoints);
    displayFreqs.resize ((size_t) numPoints);
    binLo.resize ((size_t) numPoints);
    binHi.resize ((size_t) numPoints);
    ptLo.resize ((size_t) numPoints);
    ptHi.resize ((size_t) numPoints);
    tiltDb.resize ((size_t) numPoints);
    prefix.assign ((size_t) N / 2 + 2, 0.0);
    boxed.assign ((size_t) numPoints, 0.0);
    const double binHz = fs / N;
    const double ratio = fMax / fMin;
    const double pointSpanOct = std::log2 (ratio) / (numPoints - 1);
    // Fractional-octave smoothing is realised as two box passes of half the width each (= triangular
    // kernel): the first over FFT bins, the second over neighbouring display points.
    const double halfWidthOct = 0.5 * settings.smoothingOctaves;
    const int pointRadius = settings.smoothingOctaves > 0.0 ? (int) std::lround (0.5 * halfWidthOct / pointSpanOct) : 0;
    for (int p = 0; p < numPoints; ++p)
    {
        const double f = fMin * std::pow (ratio, (double) p / (numPoints - 1));
        displayFreqs[(size_t) p] = f;
        const double halfOct = 0.5 * std::max (halfWidthOct, pointSpanOct);
        const double lo = f * std::pow (2.0, -halfOct), hi = f * std::pow (2.0, halfOct);
        binLo[(size_t) p] = clampValue ((int) std::ceil (lo / binHz), 1, N / 2);
        binHi[(size_t) p] = clampValue ((int) std::floor (hi / binHz), binLo[(size_t) p], N / 2);
        ptLo[(size_t) p] = std::max (0, p - pointRadius);
        ptHi[(size_t) p] = std::min (numPoints - 1, p + pointRadius);
        tiltDb[(size_t) p] = settings.slopeDbPerOct * std::log2 (f / 1000.0);
    }
    display.assign ((size_t) numPoints, -200.0f);
    peaks.assign ((size_t) numPoints, -200.0f);
    framesAnalysed = 0;
}

void SpectrumAnalyzer::resetPeaks() noexcept
{
    std::fill (peaks.begin(), peaks.end(), -200.0f);
}

void SpectrumAnalyzer::feed (const float* x, int numFrames) noexcept
{
    if (fft == nullptr)
        return;
    const int N = settings.fftSize;
    for (int i = 0; i < numFrames; ++i)
    {
        history[(size_t) histPos * 2] = x[2 * i];
        history[(size_t) histPos * 2 + 1] = x[2 * i + 1];
        histPos = (histPos + 1) % N;
        if (++sinceHop >= hop)
        {
            sinceHop = 0;
            analyseFrame();
        }
    }
}

void SpectrumAnalyzer::analyseFrame() noexcept
{
    const int N = settings.fftSize;
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < N; ++i)
        {
            const int idx = (histPos + i) % N; // oldest first
            frame[(size_t) i] = history[(size_t) idx * 2 + (size_t) ch] * window[(size_t) i];
        }
        fft->forward (frame.data(), spectrum.data());
        for (int k = 0; k <= N / 2; ++k)
            accPower[(size_t) k] += 0.5 * std::norm (spectrum[(size_t) k]) * windowNorm; // mean over channels
    }
    ++accFrames;
    ++framesAnalysed;
}

void SpectrumAnalyzer::updateDisplay (double dt) noexcept
{
    if (fft == nullptr)
        return;
    const int N = settings.fftSize;
    const float fall = (float) (settings.releaseDbPerSec * dt);

    if (accFrames > 0)
    {
        const double inv = 1.0 / accFrames;
        for (int k = 0; k <= N / 2; ++k)
        {
            binDb[(size_t) k] = (float) powerToDb (accPower[(size_t) k] * inv, -200.0);
            accPower[(size_t) k] = 0.0;
        }
        accFrames = 0;
    }

    const double binHz = sampleRate / N;
    const bool smoothing = settings.smoothingOctaves > 0.0;
    if (smoothing)
    {
        prefix[0] = 0.0;
        for (int k = 0; k <= N / 2; ++k)
            prefix[(size_t) k + 1] = prefix[(size_t) k] + std::pow (10.0, binDb[(size_t) k] * 0.1);
    }

    auto interpolatedPower = [&] (double f)
    {
        // Display resolution finer than the FFT: interpolate between neighbouring bins (in dB).
        const double pos = f / binHz;
        const int k0 = clampValue ((int) std::floor (pos), 0, N / 2 - 1);
        const double t = clampValue (pos - k0, 0.0, 1.0);
        return std::pow (10.0, 0.1 * (binDb[(size_t) k0] * (1.0 - t) + binDb[(size_t) k0 + 1] * t));
    };

    // Pass 1: per display point.
    for (size_t p = 0; p < display.size(); ++p)
    {
        const int lo = binLo[p], hi = binHi[p];
        double power;
        if (hi - lo + 1 < 3)
            power = interpolatedPower (displayFreqs[p]);
        else if (smoothing)
            power = (prefix[(size_t) hi + 1] - prefix[(size_t) lo]) / (hi - lo + 1); // mean power in the band
        else
        {
            float m = -200.0f; // no smoothing: keep the highest bin under this pixel (peaks are never hidden)
            for (int k = lo; k <= hi; ++k)
                m = std::max (m, binDb[(size_t) k]);
            power = std::pow (10.0, 0.1 * m);
        }
        boxed[p] = power;
    }

    // Pass 2 (smoothing only): average neighbouring points -> triangular kernel; then ballistics.
    for (size_t p = 0; p < display.size(); ++p)
    {
        double power = boxed[p];
        if (smoothing && ptHi[p] > ptLo[p])
        {
            double acc = 0.0;
            for (int q = ptLo[p]; q <= ptHi[p]; ++q)
                acc += boxed[(size_t) q];
            power = acc / (ptHi[p] - ptLo[p] + 1);
        }
        const float lv = (float) (powerToDb (power, -200.0) + tiltDb[p]);
        display[p] = std::max (lv, display[p] - fall); // instant attack, linear-dB release
        if (settings.peakHold)
            peaks[p] = std::max (peaks[p], display[p]);
    }
}

} // namespace mepro::dsp
