// ME Pro EQ — ME Audio
// Regression fingerprints: every factory preset (minimum phase, plus two presets in linear phase) is rendered
// with the same pink noise and its output/input level per 1/3 octave is compared with the reference file
// Tests/Reference/preset_fingerprints.csv. Any change to the DSP, the presets or the parameter mapping that
// alters the sound by more than 0.02 dB in any band makes this test fail.
//
// After an intended change, regenerate the reference with MEPROEQ_UPDATE_REFERENCE=1 and review the diff.
#include "PluginTestUtils.h"
#include "TestFramework.h"

#include "DSP/FFT.h"

#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

using namespace metest;

namespace
{
    constexpr double kFs = 48000.0;
    constexpr int kFft = 8192;
    constexpr double kTolDb = 0.02;

    std::vector<double> thirdOctaveCentres()
    {
        std::vector<double> c;
        for (int i = -17; i <= 13; ++i) // 20 Hz ... 20 kHz
            c.push_back (1000.0 * std::pow (2.0, i / 3.0));
        return c;
    }

    // Welch power per bin (Hann, 50 % overlap), skipping the first `skip` samples.
    std::vector<double> welch (const std::vector<float>& x, int skip)
    {
        mepro::dsp::RealFFT fft (kFft);
        std::vector<double> w (kFft), frame (kFft), pow (kFft / 2 + 1, 0.0);
        std::vector<std::complex<double>> spec (kFft / 2 + 1);
        for (int i = 0; i < kFft; ++i)
            w[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * i / kFft);
        for (size_t pos = (size_t) skip; pos + kFft <= x.size(); pos += kFft / 2)
        {
            for (int i = 0; i < kFft; ++i)
                frame[(size_t) i] = x[pos + (size_t) i] * w[(size_t) i];
            fft.forward (frame.data(), spec.data());
            for (size_t k = 0; k < spec.size(); ++k)
                pow[k] += std::norm (spec[k]);
        }
        return pow;
    }

    std::vector<double> fingerprint (MEProEQAudioProcessor& p, int latency)
    {
        const int n = (int) kFs * 3;
        const auto x = pinkNoise (n + latency, 0.3, 4242);
        std::vector<float> l (x.begin(), x.end()), r = l;
        renderStereo (p, l, r, 512);
        // Align the output with the input (linear-phase latency).
        std::vector<float> in (x.begin(), x.begin() + n), out (l.begin() + latency, l.begin() + latency + n);
        const auto pi = welch (in, 4800), po = welch (out, 4800);
        std::vector<double> fp;
        for (double fc : thirdOctaveCentres())
        {
            double si = 0.0, so = 0.0;
            for (size_t k = 1; k < pi.size(); ++k)
            {
                const double f = (double) k * kFs / kFft;
                if (f >= fc * std::pow (2.0, -1.0 / 6.0) && f < fc * std::pow (2.0, 1.0 / 6.0))
                {
                    si += pi[k];
                    so += po[k];
                }
            }
            fp.push_back (10.0 * std::log10 (std::max (so, 1e-300) / std::max (si, 1e-300)));
        }
        return fp;
    }

    std::string referencePath()
    {
        return std::string (MEPROEQ_SOURCE_DIR) + "/Tests/Reference/preset_fingerprints.csv";
    }
} // namespace

ME_TEST ("Regression", PresetFingerprints)
{
    juceInit();
    std::map<std::string, std::vector<double>> current;
    {
        MEProEQAudioProcessor probe;
        const auto presets = probe.getPresets().getPresets();
        for (const auto& info : presets)
            for (int lp = 0; lp < 2; ++lp)
            {
                if (lp == 1 && info.name != "Vocal Bus" && info.name != "Mastering Corrective EQ Starting Point")
                    continue;
                MEProEQAudioProcessor p;
                p.getPresets().load (info);
                setParam (p, mepro::params::id::phaseMode, (float) lp);
                p.setNonRealtime (true);
                p.prepareToPlay (kFs, 512);
                current[(info.category + "/" + info.name + (lp ? " [linear phase]" : "")).toStdString()] = fingerprint (p, p.getLatencySamples());
            }
    }
    measure ("fingerprinted renders", (double) current.size());

    const bool update = std::getenv ("MEPROEQ_UPDATE_REFERENCE") != nullptr;
    std::map<std::string, std::vector<double>> reference;
    {
        std::ifstream f (referencePath());
        std::string line;
        while (std::getline (f, line))
        {
            if (line.empty() || line[0] == '#')
                continue;
            const auto sep = line.find (';');
            std::vector<double> v;
            std::stringstream ss (line.substr (sep + 1));
            std::string tok;
            while (std::getline (ss, tok, ';'))
                v.push_back (std::stod (tok));
            reference[line.substr (0, sep)] = v;
        }
    }

    if (update || reference.empty())
    {
        std::ofstream f (referencePath());
        f << "# ME Pro EQ regression fingerprints: output/input level (dB) per 1/3 octave, 20 Hz - 20 kHz,\n"
             "# 48 kHz, pink noise (seed 4242). Generated by Tests/RegressionTests.cpp (MEPROEQ_UPDATE_REFERENCE=1).\n";
        for (const auto& [name, fp] : current)
        {
            f << name;
            for (double v : fp)
            {
                char b[32];
                std::snprintf (b, sizeof b, ";%.4f", v);
                f << b;
            }
            f << "\n";
        }
        measure ("reference file", update ? "regenerated (MEPROEQ_UPDATE_REFERENCE)" : "created (none existed)");
        CHECK_MSG (! reference.empty() || update, "no reference fingerprints existed; created " << referencePath());
        return;
    }

    double worst = 0.0;
    std::string worstName;
    for (const auto& [name, fp] : current)
    {
        const auto it = reference.find (name);
        CHECK_MSG (it != reference.end(), "no reference fingerprint for " << name);
        if (it == reference.end())
            continue;
        CHECK (it->second.size() == fp.size());
        for (size_t i = 0; i < fp.size() && i < it->second.size(); ++i)
        {
            const double d = std::abs (fp[i] - it->second[i]);
            if (d > worst)
            {
                worst = d;
                worstName = name + " @ " + str (thirdOctaveCentres()[i]) + " Hz";
            }
        }
    }
    CHECK (reference.size() == current.size());
    measure ("largest deviation from the reference fingerprints (dB)", worst);
    measure ("where", worstName);
    CHECK_MSG (worst <= kTolDb, worstName << " deviates by " << worst << " dB");
}
