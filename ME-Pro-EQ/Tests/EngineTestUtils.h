// ME Pro EQ — ME Audio
// Helpers for driving the EQ engine in tests.
#pragma once

#include "TestSignals.h"

#include "DSP/EQEngine.h"
#include "DSP/FilterDesign.h"

#include <vector>

namespace metest
{
using namespace mepro::dsp;

struct Stereo
{
    std::vector<double> l, r;
    Stereo() = default;
    Stereo (std::vector<double> a, std::vector<double> b) : l (std::move (a)), r (std::move (b)) {}
    int size() const { return (int) l.size(); }
};

inline void runEngine (EQEngine& e, Stereo& s, int block, const Stereo* sc = nullptr)
{
    for (int pos = 0; pos < s.size(); pos += block)
    {
        const int n = std::min (block, s.size() - pos);
        double* ch[2] = { s.l.data() + pos, s.r.data() + pos };
        if (sc != nullptr)
        {
            const double* scp[2] = { sc->l.data() + pos, sc->r.data() + pos };
            e.process (ch, 2, n, scp, 2);
        }
        else
        {
            e.process (ch, 2, n);
        }
    }
}

inline void runEngineMono (EQEngine& e, std::vector<double>& x, int block)
{
    for (int pos = 0; pos < (int) x.size(); pos += block)
    {
        const int n = std::min (block, (int) x.size() - pos);
        double* ch[1] = { x.data() + pos };
        e.process (ch, 1, n);
    }
}

inline BandSettings bell (double f, double g, double q, StereoMode m = StereoMode::Stereo)
{
    BandSettings b;
    b.active = true;
    b.type = FilterType::Bell;
    b.freqHz = f;
    b.gainDb = g;
    b.q = q;
    b.stereo = m;
    return b;
}

inline BandSettings band (FilterType t, double f, double g, double q, int slope, StereoMode m = StereoMode::Stereo)
{
    BandSettings b;
    b.active = true;
    b.type = t;
    b.freqHz = f;
    b.gainDb = g;
    b.q = q;
    b.slopeIndex = slope;
    b.stereo = m;
    return b;
}

// Combined designed response of the active bands (all in Stereo mode) at frequency f.
inline double designedGainDb (const EngineSettings& s, double fs, double f)
{
    double total = 0.0;
    for (const auto& b : s.bands)
        if (b.active)
            total += designCascade ({ b.type, b.freqHz, b.gainDb, b.q, b.slopeIndex }, fs).magnitudeDb (kTwoPi * f / fs);
    return total + s.inputTrimDb + s.outputTrimDb;
}

// High-frequency residual of `y` (energy above `fc`, 8th-order HP) relative to its total RMS, in dB.
// For a pure low-frequency tone through a linear EQ this is ~-300 dB; parameter discontinuities (clicks,
// zipper noise) show up as broadband energy.
inline double hfResidualDb (const std::vector<double>& y, double fs, double fc, int skip)
{
    auto h = y;
    SvfCascade hp;
    hp.setLayout (sectionLayout (FilterType::HighPass, 5));
    hp.setCoefficients (designCascade ({ FilterType::HighPass, fc, 0.0, kButterworthQ, 5 }, fs));
    hp.processMono (h.data(), (int) h.size(), 1, 0);
    const double r = rms (h, skip, (int) h.size() - skip);
    const double t = rms (y, skip, (int) y.size() - skip);
    return db (r / std::max (t, 1e-300));
}

} // namespace metest
