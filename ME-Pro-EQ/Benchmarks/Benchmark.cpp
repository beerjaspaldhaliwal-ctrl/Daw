// ME Pro EQ — ME Audio
// CPU benchmark: times AudioProcessor::processBlock for 1/8/16/32 instances in realistic configurations and
// reports mean / worst-case block times, real-time load, memory and latency. Every number is measured on the
// machine running the tool; nothing is estimated.
//
// Usage: meproeq_benchmark [--seconds N] [--rate HZ] [--block N] [--markdown path]
#include "PluginProcessor.h"

#include <juce_events/juce_events.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <functional>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#if defined(_MSC_VER)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <intrin.h>
    #include <windows.h>
    #include <psapi.h>
#elif defined(__x86_64__) || defined(__i386__)
    #include <cpuid.h>
#endif
#if defined(__linux__)
    #include <sys/resource.h>
    #include <unistd.h>
#endif

using mepro::params::BandParam;

namespace
{
std::string cpuName()
{
#if defined(_MSC_VER) || defined(__x86_64__) || defined(__i386__)
    unsigned int regs[12] {};
    for (unsigned int i = 0; i < 3; ++i)
    {
    #if defined(_MSC_VER)
        int r[4];
        __cpuid (r, (int) (0x80000002 + i));
        for (int k = 0; k < 4; ++k)
            regs[i * 4 + (unsigned) k] = (unsigned int) r[k];
    #else
        __get_cpuid (0x80000002 + i, &regs[i * 4], &regs[i * 4 + 1], &regs[i * 4 + 2], &regs[i * 4 + 3]);
    #endif
    }
    std::string s (reinterpret_cast<const char*> (regs), 48);
    s = s.c_str();
    while (! s.empty() && s.front() == ' ')
        s.erase (s.begin());
    return s;
#else
    return juce::SystemStats::getCpuModel().toStdString();
#endif
}

double residentMemoryMB()
{
#if defined(_MSC_VER)
    PROCESS_MEMORY_COUNTERS pmc {};
    GetProcessMemoryInfo (GetCurrentProcess(), &pmc, sizeof (pmc));
    return pmc.WorkingSetSize / (1024.0 * 1024.0);
#elif defined(__linux__)
    std::ifstream f ("/proc/self/statm");
    long pages = 0, rss = 0;
    f >> pages >> rss;
    return rss * (double) sysconf (_SC_PAGESIZE) / (1024.0 * 1024.0);
#else
    return 0.0;
#endif
}

void set (juce::AudioProcessor& p, const juce::String& id, float v)
{
    auto* rp = dynamic_cast<juce::RangedAudioParameter*> (dynamic_cast<MEProEQAudioProcessor&> (p).getValueTreeState().getParameter (id));
    rp->setValueNotifyingHost (rp->convertTo0to1 (v));
}

struct Config
{
    std::string name;
    int bands;
    int dynamicBands;
    bool analyzer;
    std::function<void (MEProEQAudioProcessor&)> apply;
};

void band (MEProEQAudioProcessor& p, int b, mepro::dsp::FilterType t, float f, float g, float q, int slope = 1, bool dyn = false)
{
    using mepro::params::bandParamId;
    set (p, bandParamId (b, BandParam::Used), 1.0f);
    set (p, bandParamId (b, BandParam::Type), (float) t);
    set (p, bandParamId (b, BandParam::Freq), f);
    set (p, bandParamId (b, BandParam::Gain), g);
    set (p, bandParamId (b, BandParam::Q), q);
    set (p, bandParamId (b, BandParam::Slope), (float) slope);
    set (p, bandParamId (b, BandParam::DynOn), dyn ? 1.0f : 0.0f);
    set (p, bandParamId (b, BandParam::Threshold), -40.0f);
}

void typicalVocal (MEProEQAudioProcessor& p)
{
    using T = mepro::dsp::FilterType;
    band (p, 0, T::HighPass, 80, 0, 0.707f, 3);
    band (p, 1, T::Bell, 250, -3, 1.2f);
    band (p, 2, T::Bell, 900, -1.5f, 2.0f);
    band (p, 3, T::Bell, 3200, 2, 1.0f);
    band (p, 4, T::HighShelf, 11000, 2.5f, 0.707f);
    band (p, 5, T::Notch, 5600, 0, 8.0f);
}

void worstCase (MEProEQAudioProcessor& p)
{
    using T = mepro::dsp::FilterType;
    for (int b = 0; b < 16; ++b)
    {
        const auto t = b < 4 ? (b % 2 ? T::LowPass : T::HighPass) : (b % 3 == 0 ? T::LowShelf : T::Bell);
        band (p, b, t, 40.0f * std::pow (1.45f, (float) b), (b % 2 ? 3.0f : -3.0f), 1.5f, 7, b >= 8 && t != T::LowPass && t != T::HighPass);
        set (p, mepro::params::bandParamId (b, BandParam::Stereo), (float) (b % 5));
    }
}

std::vector<Config> configs()
{
    using T = mepro::dsp::FilterType;
    return {
        { "Typical vocal (6 static bands)", 6, 0, false, typicalVocal },
        { "Vocal + 2 dynamic bands + analyzer", 8, 2, true, [] (auto& p)
          {
              band (p, 0, T::HighPass, 80, 0, 0.707f, 3);
              band (p, 1, T::Bell, 250, -3, 1.2f);
              band (p, 2, T::Bell, 900, -1.5f, 2.0f);
              band (p, 3, T::Bell, 3200, 2, 1.0f);
              band (p, 4, T::HighShelf, 11000, 2.5f, 0.707f);
              band (p, 5, T::Notch, 5600, 0, 8.0f);
              band (p, 6, T::Bell, 6500, 0, 3.0f, 1, true);
              band (p, 7, T::Bell, 300, 0, 2.0f, 1, true);
          } },
        { "Worst case: 16 bands, 96 dB/oct cuts, 8 dynamic, M/S, analyzer", 16, 8, true, worstCase },
        { "Typical vocal, linear phase (Balanced, 16k taps)", 6, 0, false, [] (auto& p)
          {
              typicalVocal (p);
              set (p, mepro::params::id::phaseMode, 1.0f);
              set (p, mepro::params::id::lpQuality, 1.0f);
          } },
        { "Typical vocal, linear phase (High resolution, 32k taps)", 6, 0, false, [] (auto& p)
          {
              typicalVocal (p);
              set (p, mepro::params::id::phaseMode, 1.0f);
              set (p, mepro::params::id::lpQuality, 2.0f);
          } },
        { "Worst case routing (L/R + M/S bands), linear phase (High resolution)", 16, 0, true, [] (auto& p)
          {
              worstCase (p);
              set (p, mepro::params::id::phaseMode, 1.0f);
              set (p, mepro::params::id::lpQuality, 2.0f);
          } },
    };
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    double seconds = 10.0, rate = 48000.0;
    int block = 256;
    std::string mdPath;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--seconds" && i + 1 < argc) seconds = std::atof (argv[++i]);
        else if (a == "--rate" && i + 1 < argc) rate = std::atof (argv[++i]);
        else if (a == "--block" && i + 1 < argc) block = std::atoi (argv[++i]);
        else if (a == "--markdown" && i + 1 < argc) mdPath = argv[++i];
    }

    std::ostringstream md;
    md << "# ME Pro EQ — CPU benchmark\n\n";
    md << "| Item | Value |\n|---|---|\n";
    md << "| Processor | " << cpuName() << " |\n";
    md << "| Logical cores | " << juce::SystemStats::getNumCpus() << " |\n";
    md << "| OS | " << juce::SystemStats::getOperatingSystemName() << " |\n";
    md << "| Sample rate | " << rate << " Hz |\n| Buffer size | " << block << " samples |\n";
    md << "| Audio per measurement | " << seconds << " s stereo pink-noise-like signal, 32-bit float path |\n";
    md << "| Threading | all instances processed sequentially on one thread (like one DAW audio thread) |\n\n";
    md << "Load = processing time / audio duration for all instances together (100 % = one core fully busy).\n\n";
    md << "| Configuration | Bands | Dyn bands | Analyzer | Instances | Mean block (us) | Worst block (us) | P99 block (us) | Load (% of one core) | Latency |\n";
    md << "|---|---|---|---|---|---|---|---|---|---|\n";

    const double memBefore = residentMemoryMB();
    const int blocks = (int) (seconds * rate / block);
    juce::Random rng (1234);
    juce::AudioBuffer<float> input (2, block * 64);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < input.getNumSamples(); ++i)
            input.setSample (c, i, (rng.nextFloat() * 2.0f - 1.0f) * 0.25f);

    std::vector<std::pair<std::string, double>> memPerInstance;
    for (const auto& cfg : configs())
    {
        for (int instances : { 1, 8, 16, 32 })
        {
            const double m0 = residentMemoryMB();
            std::vector<std::unique_ptr<MEProEQAudioProcessor>> procs;
            for (int i = 0; i < instances; ++i)
            {
                auto p = std::make_unique<MEProEQAudioProcessor>();
                cfg.apply (*p);
                p->prepareToPlay (rate, block);
                p->getPreFifo().setActive (cfg.analyzer);
                p->getPostFifo().setActive (cfg.analyzer);
                procs.push_back (std::move (p));
            }
            if (instances == 32)
            {
                // Resident-set growth is unreliable once earlier configurations have freed memory (the allocator
                // reuses it), so the linear-phase engine's own allocation is reported as well.
                double lpMB = 0.0;
                if (const auto* lp = procs.front()->getEngine().getLinearPhaseForUi())
                    lpMB = (double) lp->getAllocatedBytes() / (1024.0 * 1024.0);
                memPerInstance.emplace_back (cfg.name + (lpMB > 0.0 ? " (of which linear-phase engine: " + std::to_string (lpMB).substr (0, 5) + " MB)" : ""),
                                             (residentMemoryMB() - m0) / 32.0);
            }

            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            std::vector<double> times;
            times.reserve ((size_t) blocks);
            std::vector<float> drain (8192 * 2);
            for (int k = 0; k < blocks + 50; ++k)
            {
                const int off = (k % 63) * block;
                const auto t0 = std::chrono::steady_clock::now();
                for (auto& p : procs)
                {
                    buf.copyFrom (0, 0, input, 0, off, block);
                    buf.copyFrom (1, 0, input, 1, off, block);
                    p->processBlock (buf, midi);
                }
                const auto t1 = std::chrono::steady_clock::now();
                if (k >= 50) // skip warm-up
                    times.push_back (std::chrono::duration<double, std::micro> (t1 - t0).count());
                if (cfg.analyzer && k % 8 == 0) // emulate the editor draining the analyser FIFOs (UI thread work, not timed)
                    for (auto& p : procs)
                    {
                        p->getPreFifo().pull (drain.data(), 8192);
                        p->getPostFifo().pull (drain.data(), 8192);
                    }
            }
            std::sort (times.begin(), times.end());
            const double mean = std::accumulate (times.begin(), times.end(), 0.0) / (double) times.size();
            const double worst = times.back();
            const double p99 = times[(size_t) (0.99 * (double) (times.size() - 1))];
            const double blockUs = 1e6 * block / rate;
            const double load = 100.0 * mean / blockUs;
            const int lat = procs.front()->getLatencySamples();
            char line[512];
            std::snprintf (line, sizeof line, "| %s | %d | %d | %s | %d | %.1f | %.1f | %.1f | %.2f %% | %d smp (%.2f ms) |\n", cfg.name.c_str(),
                           cfg.bands, cfg.dynamicBands, cfg.analyzer ? "on" : "off", instances, mean, worst, p99, load, lat, 1000.0 * lat / rate);
            md << line;
            std::printf ("%s", line);
            std::fflush (stdout);
        }
    }
    md << "\n| Memory | Value |\n|---|---|\n";
    md << "| Process resident memory before instances | " << memBefore << " MB |\n";
    std::printf ("\nCPU: %s\n", cpuName().c_str());
    for (const auto& [name, mb] : memPerInstance)
    {
        md << "| Approx. resident memory per instance (32 instances): " << name << " | " << mb << " MB |\n";
        std::printf ("memory per instance, %s: %.2f MB\n", name.c_str(), mb);
    }

    if (! mdPath.empty())
    {
        std::ofstream f (mdPath);
        f << md.str();
    }
    return 0;
}
