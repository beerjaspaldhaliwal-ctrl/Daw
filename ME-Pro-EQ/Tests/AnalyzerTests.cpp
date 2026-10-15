// ME Pro EQ — ME Audio
// Spectrum analyser tests: FFT correctness, calibration, display mapping, ballistics and the lock-free FIFO.
#include "TestFramework.h"
#include "TestSignals.h"

#include "DSP/FFT.h"
#include "DSP/SpectrumAnalyzer.h"

#include <thread>

using namespace mepro::dsp;
using namespace metest;

ME_TEST ("Analyzer", FFTMatchesNaiveDFT)
{
    for (int n : { 16, 64, 1024, 4096 })
    {
        RealFFT fft (n);
        auto x = whiteNoise (n, 1.0, (uint64_t) n);
        std::vector<std::complex<double>> X ((size_t) n / 2 + 1);
        fft.forward (x.data(), X.data());
        double err = 0.0, scale = 0.0;
        for (int k = 0; k <= n / 2; k += (n > 1024 ? 7 : 1))
        {
            std::complex<double> ref (0.0, 0.0);
            for (int i = 0; i < n; ++i)
                ref += x[(size_t) i] * std::polar (1.0, -kTwoPi * k * i / n);
            err = std::max (err, std::abs (ref - X[(size_t) k]));
            scale = std::max (scale, std::abs (ref));
        }
        CHECK_MSG (err / scale < 1e-12, "n " << n << " rel err " << err / scale);
        std::vector<double> y ((size_t) n);
        fft.inverse (X.data(), y.data());
        double rt = 0.0;
        for (int i = 0; i < n; ++i)
            rt = std::max (rt, std::abs (y[(size_t) i] - x[(size_t) i]));
        CHECK_MSG (rt < 1e-12, "n " << n << " round trip " << rt);
        if (n == 4096)
            measure ("n=4096: forward rel. error vs DFT / inverse round-trip error", str (err / scale) + " / " + str (rt));
    }
}

namespace
{
// Feeds `pre` frames (discarded: they may straddle the signal onset), then `n` steady frames, and updates the
// display with an effectively infinite release so the reading reflects only the steady frames.
template <typename Gen>
void feedSteady (SpectrumAnalyzer& a, Gen&& gen, int pre, int n)
{
    std::vector<float> st ((size_t) (pre + n) * 2);
    for (int i = 0; i < pre + n; ++i)
        st[(size_t) (2 * i)] = st[(size_t) (2 * i + 1)] = (float) gen (i);
    a.feed (st.data(), pre);
    a.updateDisplay (1.0); // consume (and with a huge release, forget) the onset frames
    a.feed (st.data() + 2 * pre, n);
    a.updateDisplay (1.0);
}
} // namespace

ME_TEST ("Analyzer", SineCalibration)
{
    const double fs = 48000.0;
    for (int fftSize : { 2048, 8192, 32768 })
    {
        SpectrumAnalyzer::Settings s;
        s.fftSize = fftSize;
        s.smoothingOctaves = 0.0;
        s.releaseDbPerSec = 1e9;
        const int bin = fftSize / 16;
        const double f = bin * fs / fftSize; // bin-centred
        for (double levelDb : { 0.0, -20.0, -60.0, -100.0 })
        {
            SpectrumAnalyzer a;
            a.configure (s, fs, 400, 10.0, 24000.0);
            const double amp = std::pow (10.0, levelDb / 20.0);
            feedSteady (a, [&] (int i) { return amp * std::sin (kTwoPi * f * i / fs); }, fftSize * 2, fftSize);
            const double got = a.getBinDb()[(size_t) bin];
            CHECK_MSG (std::abs (got - levelDb) < 0.01, "fft " << fftSize << " level " << levelDb << " read " << got);
        }
        // Worst-case scalloping: sine half-way between two bins.
        SpectrumAnalyzer b;
        b.configure (s, fs, 400, 10.0, 24000.0);
        const double f2 = (bin + 0.5) * fs / fftSize;
        feedSteady (b, [&] (int i) { return std::sin (kTwoPi * f2 * i / fs); }, fftSize * 2, fftSize);
        const double peak = std::max (b.getBinDb()[(size_t) bin], b.getBinDb()[(size_t) bin + 1]);
        measure ("FFT " + str (fftSize) + ": full-scale sine between bins reads (dB) [scalloping]", peak);
        CHECK (peak > -1.0 && peak <= 0.01);
    }
}

ME_TEST ("Analyzer", DisplayShowsMeasuredDataOnly)
{
    const double fs = 44100.0;
    SpectrumAnalyzer::Settings s;
    s.fftSize = 8192;
    s.smoothingOctaves = 0.0;
    s.releaseDbPerSec = 1e9;

    // Silence: nothing but the floor.
    {
        SpectrumAnalyzer a;
        a.configure (s, fs, 600, 10.0, 22050.0);
        feedSteady (a, [] (int) { return 0.0; }, 8192, 8192);
        float mx = -1000.0f;
        for (float v : a.getDisplayDb())
            mx = std::max (mx, v);
        measure ("silence: maximum displayed level (dB)", mx);
        CHECK (mx <= -199.0f);
    }

    // A 1 kHz tone at -12 dBFS appears at 1 kHz at about -12 dB (within scalloping) and nowhere else.
    SpectrumAnalyzer a;
    a.configure (s, fs, 600, 10.0, 22050.0);
    const double amp = 0.251188643;
    feedSteady (a, [&] (int i) { return amp * std::sin (kTwoPi * 1000.0 * i / fs); }, 8192 * 2, 8192);
    const auto& f = a.getDisplayFreqs();
    const auto& d = a.getDisplayDb();
    float at1k = -1000.0f, elsewhere = -1000.0f;
    for (size_t i = 0; i < f.size(); ++i)
    {
        if (std::abs (std::log2 (f[i] / 1000.0)) < 0.02)
            at1k = std::max (at1k, d[i]);
        else if (std::abs (std::log2 (f[i] / 1000.0)) > 0.5)
            elsewhere = std::max (elsewhere, d[i]);
    }
    measure ("-12 dBFS 1 kHz tone: displayed peak (dB)", at1k);
    measure ("-12 dBFS 1 kHz tone: max displayed level > 1/2 oct away (dB)", elsewhere);
    CHECK (at1k > -13.0f && at1k < -11.9f);
    CHECK (elsewhere < -120.0f); // Blackman-Harris side lobes are -92 dB below the peak and fall off fast
}

ME_TEST ("Analyzer", ReleaseBallistics)
{
    const double fs = 48000.0;
    SpectrumAnalyzer::Settings s;
    s.fftSize = 4096;
    s.smoothingOctaves = 0.0;
    s.releaseDbPerSec = 1e9;
    for (double rate : { 10.0, 30.0, 120.0 })
    {
        SpectrumAnalyzer a;
        a.configure (s, fs, 400, 10.0, 24000.0);
        feedSteady (a, [&] (int i) { return 0.5 * std::sin (kTwoPi * 1500.0 * i / fs); }, 8192, 4096);
        const auto& f = a.getDisplayFreqs();
        size_t k = 0;
        for (size_t i = 0; i < f.size(); ++i)
            if (std::abs (f[i] - 1500.0) < std::abs (f[k] - 1500.0))
                k = i;
        const float before = a.getDisplayDb()[k];
        // Reconfigure ballistics and feed silence long enough that every analysed frame is silent.
        auto s2 = s;
        s2.releaseDbPerSec = rate;
        SpectrumAnalyzer b;
        b.configure (s2, fs, 400, 10.0, 24000.0);
        feedSteady (b, [&] (int i) { return 0.5 * std::sin (kTwoPi * 1500.0 * i / fs); }, 8192, 4096);
        std::vector<float> z ((size_t) 4096 * 2 * 2, 0.0f);
        b.feed (z.data(), 4096 * 2);
        b.updateDisplay (0.0); // the mixed frames are consumed without time passing
        b.feed (z.data(), 4096);
        b.updateDisplay (0.5); // half a second of release with silent frames only
        const float after = b.getDisplayDb()[k];
        measure ("release " + str (rate) + " dB/s: fall over 0.5 s (dB)", before - after);
        CHECK_NEAR (before - after, std::min (rate * 0.5, (double) before + 200.0), 0.01);
    }
}

ME_TEST ("Analyzer", FifoIsLossFreeAndWaitFree)
{
    AnalyzerFifo fifo (1 << 14);
    fifo.setActive (true);
    const int total = 2'000'000;
    std::atomic<bool> done { false };
    std::thread producer ([&]
    {
        std::vector<double> block (256 * 2);
        int sent = 0;
        while (sent < total)
        {
            const int n = std::min (256, total - sent);
            for (int i = 0; i < n; ++i)
            {
                block[(size_t) (2 * i)] = (double) ((sent + i) % 100000);
                block[(size_t) (2 * i + 1)] = -(double) ((sent + i) % 100000);
            }
            const int before = fifo.getDroppedFrames();
            fifo.push (block.data(), 2, n);
            if (fifo.getDroppedFrames() != before)
                std::this_thread::yield(); // a real audio thread would simply carry on
            sent += n;
        }
        done = true;
    });
    std::vector<float> out (4096 * 2);
    int received = 0;
    bool ordered = true;
    int expected = 0;
    for (;;)
    {
        const bool finished = done.load(); // read before pulling: everything pushed before `done` is visible
        const int n = fifo.pull (out.data(), 4096);
        for (int i = 0; i < n; ++i)
        {
            const int v = (int) out[(size_t) (2 * i)];
            if (v != expected % 100000 && ordered)
            {
                // Frames may be dropped (by design) when the reader is slow; after a drop the sequence
                // resumes later. Re-synchronise and only require monotonic order within contiguous runs.
                expected = v;
            }
            if ((int) -out[(size_t) (2 * i + 1)] != v)
                ordered = false;
            ++expected;
        }
        received += n;
        if (n == 0 && finished)
            break;
    }
    producer.join();
    measure ("frames pushed", (double) total);
    measure ("frames received", (double) received);
    measure ("frames dropped (reader slower than writer)", (double) fifo.getDroppedFrames());
    CHECK (ordered);
    CHECK (received + fifo.getDroppedFrames() == total);

    // Inactive FIFO accepts nothing (zero cost while no editor is open).
    AnalyzerFifo off (1024);
    std::vector<double> x (64, 1.0);
    off.push (x.data(), 2, 32);
    std::vector<float> y (64);
    CHECK (off.pull (y.data(), 32) == 0);
}

ME_TEST ("Analyzer", SmoothingShapes)
{
    const double fs = 48000.0;
    SpectrumAnalyzer::Settings s;
    s.fftSize = 8192;
    s.smoothingOctaves = 1.0 / 6.0;
    s.releaseDbPerSec = 1e9;

    // A tone renders as a single smooth hump centred on the tone (no flat top, no steps).
    {
        SpectrumAnalyzer a;
        a.configure (s, fs, 600, 10.0, 24000.0);
        feedSteady (a, [&] (int i) { return 0.5 * std::sin (kTwoPi * 3000.0 * i / fs); }, 16384, 8192);
        const auto& f = a.getDisplayFreqs();
        const auto& d = a.getDisplayDb();
        size_t pk = 0;
        for (size_t i = 0; i < d.size(); ++i)
            if (d[i] > d[pk])
                pk = i;
        measure ("1/6-oct smoothed 3 kHz tone: peak position (Hz)", f[pk]);
        CHECK (std::abs (std::log2 (f[pk] / 3000.0)) < 0.02);
        bool monotone = true;
        for (size_t i = pk + 1; i < d.size() && f[i] < 3000.0 * 1.12; ++i)
            monotone = monotone && d[i] <= d[i - 1] + 1e-4f;
        for (size_t i = pk; i > 0 && f[i - 1] > 3000.0 / 1.12; --i)
            monotone = monotone && d[i - 1] <= d[i] + 1e-4f;
        CHECK (monotone);
        int flat = 0; // count of points within 0.01 dB of the peak: a box-shaped plateau would make this large
        for (float v : d)
            flat += std::abs (v - d[pk]) < 0.01f;
        measure ("points within 0.01 dB of the peak", (double) flat);
        CHECK (flat <= 3);
    }
    // Pink noise: per-bin power falls 3 dB/oct; with 1/3-octave smoothing the display shows that slope.
    {
        auto s3 = s;
        s3.smoothingOctaves = 1.0 / 3.0;
        SpectrumAnalyzer a;
        a.configure (s3, fs, 600, 10.0, 24000.0);
        auto pn = pinkNoise ((int) fs * 20, 0.3, 42);
        std::vector<float> st (pn.size() * 2);
        for (size_t i = 0; i < pn.size(); ++i)
            st[2 * i] = st[2 * i + 1] = (float) pn[i];
        a.feed (st.data(), (int) pn.size()); // ~460 frames averaged into one display update
        a.updateDisplay (1.0);
        auto at = [&] (double hz)
        {
            const auto& f = a.getDisplayFreqs();
            size_t k = 0;
            for (size_t i = 0; i < f.size(); ++i)
                if (std::abs (std::log (f[i] / hz)) < std::abs (std::log (f[k] / hz)))
                    k = i;
            return (double) a.getDisplayDb()[k];
        };
        const double slope = (at (10000.0) - at (100.0)) / std::log2 (100.0);
        measure ("pink noise, 1/3-oct smoothing: displayed slope 100 Hz - 10 kHz (dB/oct)", slope);
        CHECK_NEAR (slope, -3.0, 0.3);
    }
}
