// ME Pro EQ — ME Audio
// Host-level tests: parameter identity, bus layouts, float/double rendering, latency, offline-render
// behaviour of monitoring functions, multi-instance processing and the editor.
#include "PluginEditor.h"
#include "PluginTestUtils.h"
#include "TestFramework.h"

#include "DSP/FilterDesign.h"

#include <set>
#include <thread>

using namespace metest;
using mepro::params::BandParam;

ME_TEST ("Plugin", ParameterSetIsStable)
{
    juceInit();
    MEProEQAudioProcessor p;
    const auto& prms = p.getParameters();
    std::set<juce::String> ids;
    for (auto* prm : prms)
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            ids.insert (rp->paramID);
    measure ("host parameters", (double) prms.size());
    CHECK (prms.size() == 3 + 16 * mepro::params::kNumBandParams);
    CHECK ((int) ids.size() == prms.size()); // unique IDs
    for (const char* id : { "inGain", "outGain", "bypass", "b1_used", "b1_freq", "b16_sc", "b8_range", "b12_slope" })
        CHECK_MSG (ids.count (id) == 1, "missing parameter " << id);
    CHECK (p.getBypassParameter() != nullptr && dynamic_cast<juce::RangedAudioParameter*> (p.getBypassParameter())->paramID == "bypass");
    // Every parameter is automatable and has a readable name and value text.
    for (auto* prm : prms)
    {
        CHECK (prm->isAutomatable());
        CHECK (prm->getName (64).isNotEmpty());
        CHECK (prm->getCurrentValueAsText().isNotEmpty());
    }
    // Text round trip for the frequency parameter (host text entry).
    auto* f = p.getValueTreeState().getParameter ("b1_freq");
    CHECK_NEAR (f->convertFrom0to1 (f->getValueForText ("1.5k")), 1500.0f, 0.01f);
    CHECK_NEAR (f->convertFrom0to1 (f->getValueForText ("440 Hz")), 440.0f, 0.01f);
}

ME_TEST ("Plugin", BusLayouts)
{
    juceInit();
    MEProEQAudioProcessor p;
    using CS = juce::AudioChannelSet;
    auto layout = [] (CS in, CS out, CS sc)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (in);
        l.inputBuses.add (sc);
        l.outputBuses.add (out);
        return l;
    };
    CHECK (p.checkBusesLayoutSupported (layout (CS::stereo(), CS::stereo(), CS::disabled())));
    CHECK (p.checkBusesLayoutSupported (layout (CS::mono(), CS::mono(), CS::disabled())));
    CHECK (p.checkBusesLayoutSupported (layout (CS::stereo(), CS::stereo(), CS::stereo())));
    CHECK (p.checkBusesLayoutSupported (layout (CS::stereo(), CS::stereo(), CS::mono())));
    CHECK (! p.checkBusesLayoutSupported (layout (CS::mono(), CS::stereo(), CS::disabled())));
    CHECK (! p.checkBusesLayoutSupported (layout (CS::create5point1(), CS::create5point1(), CS::disabled())));
    CHECK (p.getBusCount (true) == 2 && ! p.getBus (true, 1)->isEnabledByDefault());
}

ME_TEST ("Plugin", DefaultStateIsTransparent)
{
    juceInit();
    MEProEQAudioProcessor p;
    p.prepareToPlay (44100.0, 256);
    auto x = whiteNoise (44100, 0.8, 3);
    std::vector<float> l (x.begin(), x.end()), r (x.rbegin(), x.rend());
    const auto l0 = l, r0 = r;
    renderStereo (p, l, r, 256);
    CHECK (l == l0 && r == r0);
    CHECK (p.getLatencySamples() == 0);
}

ME_TEST ("Plugin", FloatAndDoublePrecisionRendering)
{
    juceInit();
    for (double fs : allSampleRates())
    {
        MEProEQAudioProcessor p;
        setParam (p, bp (0, BandParam::Used), 1.0f);
        setParam (p, bp (0, BandParam::Freq), 1000.0f);
        setParam (p, bp (0, BandParam::Gain), 6.0f);
        setParam (p, bp (0, BandParam::Q), 1.0f);
        setParam (p, bp (1, BandParam::Used), 1.0f);
        setParam (p, bp (1, BandParam::Type), (float) mepro::dsp::FilterType::LowShelf);
        setParam (p, bp (1, BandParam::Freq), 120.0f);
        setParam (p, bp (1, BandParam::Gain), -4.0f);
        const auto expect = [&] (double f)
        {
            using namespace mepro::dsp;
            return designCascade ({ FilterType::Bell, 1000.0f, 6.0f, 1.0f, 1 }, fs).magnitudeDb (kTwoPi * f / fs)
                 + designCascade ({ FilterType::LowShelf, 120.0f, -4.0f, 1.0f, 1 }, fs).magnitudeDb (kTwoPi * f / fs);
        };
        p.setProcessingPrecision (juce::AudioProcessor::doublePrecision);
        p.prepareToPlay (fs, 512);
        const int n = (int) fs;
        for (double f : { 100.0, 1000.0, 5000.0 })
        {
            auto ld = sine (f, fs, n, 0.25), rd = ld;
            renderStereo (p, ld, rd, 512);
            const double gd = db (std::abs (toneAmplitude (ld, f, fs, n / 2, n / 2)) / 0.25);
            std::vector<float> lf (n), rf (n);
            auto s = sine (f, fs, n, 0.25);
            std::copy (s.begin(), s.end(), lf.begin());
            std::copy (s.begin(), s.end(), rf.begin());
            p.setProcessingPrecision (juce::AudioProcessor::singlePrecision);
            p.prepareToPlay (fs, 512);
            renderStereo (p, lf, rf, 512);
            std::vector<double> lfd (lf.begin(), lf.end());
            const double gf = db (std::abs (toneAmplitude (lfd, f, fs, n / 2, n / 2)) / 0.25);
            p.setProcessingPrecision (juce::AudioProcessor::doublePrecision);
            p.prepareToPlay (fs, 512);
            CHECK_MSG (std::abs (gd - expect (f)) < 1e-6, "fs " << fs << " f " << f << " double " << gd << " expected " << expect (f));
            CHECK_MSG (std::abs (gf - expect (f)) < 1e-3, "fs " << fs << " f " << f << " float " << gf << " expected " << expect (f));
        }
    }
}

ME_TEST ("Plugin", MonitoringFunctionsNeverRenderOffline)
{
    juceInit();
    const double fs = 48000.0;
    auto setup = [] (MEProEQAudioProcessor& p)
    {
        setParam (p, bp (0, BandParam::Used), 1.0f);
        setParam (p, bp (0, BandParam::Freq), 3000.0f);
        setParam (p, bp (0, BandParam::Gain), 6.0f);
    };
    MEProEQAudioProcessor ref, offline;
    setup (ref);
    setup (offline);
    offline.setAuditionBand (0);
    offline.setMonoAudition (true);
    offline.setNonRealtime (true);
    ref.prepareToPlay (fs, 512);
    offline.prepareToPlay (fs, 512);
    auto x = pinkNoise (24000, 0.3, 8), y = whiteNoise (24000, 0.3, 9);
    auto l1 = x, r1 = y, l2 = x, r2 = y;
    renderStereo (ref, l1, r1, 512);
    renderStereo (offline, l2, r2, 512);
    CHECK (l1 == l2 && r1 == r2);

    // ...while in real time they are audible.
    MEProEQAudioProcessor rt;
    setup (rt);
    rt.setMonoAudition (true);
    rt.prepareToPlay (fs, 512);
    auto l3 = x, r3 = y;
    renderStereo (rt, l3, r3, 512);
    CHECK (l3 == r3);
}

ME_TEST ("Plugin", HostAutomationChangesProcessing)
{
    juceInit();
    const double fs = 44100.0;
    MEProEQAudioProcessor p;
    p.prepareToPlay (fs, 441);
    setParam (p, bp (0, BandParam::Used), 1.0f);
    setParam (p, bp (0, BandParam::Freq), 500.0f);
    const int n = 22050;
    double prev = -1000.0;
    for (float g : { -12.0f, -3.0f, 0.0f, 5.0f, 15.0f })
    {
        setParam (p, bp (0, BandParam::Gain), g);
        auto l = sine (500, fs, n, 0.1), r = l;
        renderStereo (p, l, r, 441);
        const double got = db (std::abs (toneAmplitude (l, 500, fs, n / 2, n / 2)) / 0.1);
        CHECK_NEAR (got, g, 0.01);
        CHECK (got > prev);
        prev = got;
    }
}

ME_TEST ("Plugin", ManyInstancesOnManyThreads)
{
    juceInit();
    const double fs = 48000.0;
    const int instances = 32;
    std::vector<std::unique_ptr<MEProEQAudioProcessor>> procs;
    for (int i = 0; i < instances; ++i)
    {
        auto p = std::make_unique<MEProEQAudioProcessor>();
        setParam (*p, bp (0, BandParam::Used), 1.0f);
        setParam (*p, bp (0, BandParam::Freq), 100.0f * (1 + i % 8));
        setParam (*p, bp (0, BandParam::Gain), (float) (i % 5) - 2.0f);
        setParam (*p, bp (1, BandParam::Used), 1.0f);
        setParam (*p, bp (1, BandParam::DynOn), 1.0f);
        setParam (*p, bp (1, BandParam::Freq), 2500.0f);
        p->prepareToPlay (fs, 256);
        procs.push_back (std::move (p));
    }
    const auto x = pinkNoise (48000, 0.3, 11);
    std::vector<std::vector<float>> outs ((size_t) instances);
    std::vector<std::thread> threads;
    for (int i = 0; i < instances; ++i)
        threads.emplace_back ([&, i]
        {
            std::vector<float> l (x.begin(), x.end()), r = l;
            renderStereo (*procs[(size_t) i], l, r, 256);
            outs[(size_t) i] = l;
        });
    for (auto& t : threads)
        t.join();
    // Each concurrently processed instance must match a single-threaded render of the same settings.
    int mismatches = 0;
    for (int i = 0; i < instances; i += 7)
    {
        MEProEQAudioProcessor solo;
        setParam (solo, bp (0, BandParam::Used), 1.0f);
        setParam (solo, bp (0, BandParam::Freq), 100.0f * (1 + i % 8));
        setParam (solo, bp (0, BandParam::Gain), (float) (i % 5) - 2.0f);
        setParam (solo, bp (1, BandParam::Used), 1.0f);
        setParam (solo, bp (1, BandParam::DynOn), 1.0f);
        setParam (solo, bp (1, BandParam::Freq), 2500.0f);
        solo.prepareToPlay (fs, 256);
        std::vector<float> l (x.begin(), x.end()), r = l;
        renderStereo (solo, l, r, 256);
        if (l != outs[(size_t) i])
            ++mismatches;
    }
    measure ("instances processed concurrently", (double) instances);
    CHECK (mismatches == 0);
}

ME_TEST ("Plugin", RepeatedCreateDestroy)
{
    juceInit();
    for (int i = 0; i < 50; ++i)
    {
        MEProEQAudioProcessor p;
        p.prepareToPlay (i % 2 ? 44100.0 : 96000.0, 64 + i);
        std::vector<float> l (1000, 0.1f), r (1000, -0.1f);
        renderStereo (p, l, r, 64 + i);
        p.releaseResources();
    }
    CHECK (true); // JUCE's leak detector aborts the process on leaks at shutdown
}

ME_TEST ("Plugin", EditorOpensRendersAndEdits)
{
    juceInit();
    MEProEQAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    setParam (p, bp (0, BandParam::Used), 1.0f);
    setParam (p, bp (0, BandParam::Type), (float) mepro::dsp::FilterType::HighPass);
    setParam (p, bp (0, BandParam::Freq), 40.0f);
    setParam (p, bp (0, BandParam::Slope), 3.0f);
    setParam (p, bp (1, BandParam::Used), 1.0f);
    setParam (p, bp (1, BandParam::Freq), 250.0f);
    setParam (p, bp (1, BandParam::Gain), -3.5f);
    setParam (p, bp (1, BandParam::Q), 1.4f);
    setParam (p, bp (2, BandParam::Used), 1.0f);
    setParam (p, bp (2, BandParam::Freq), 3200.0f);
    setParam (p, bp (2, BandParam::Gain), 2.5f);
    setParam (p, bp (2, BandParam::Q), 0.9f);
    setParam (p, bp (2, BandParam::Stereo), (float) mepro::dsp::StereoMode::Mid);
    setParam (p, bp (3, BandParam::Used), 1.0f);
    setParam (p, bp (3, BandParam::Type), (float) mepro::dsp::FilterType::HighShelf);
    setParam (p, bp (3, BandParam::Freq), 11000.0f);
    setParam (p, bp (3, BandParam::Gain), 3.0f);
    setParam (p, bp (4, BandParam::Used), 1.0f);
    setParam (p, bp (4, BandParam::Freq), 6500.0f);
    setParam (p, bp (4, BandParam::Q), 3.0f);
    setParam (p, bp (4, BandParam::DynOn), 1.0f);
    setParam (p, bp (4, BandParam::Threshold), -40.0f);
    setParam (p, bp (4, BandParam::Stereo), (float) mepro::dsp::StereoMode::Side);
    p.getUiState().setProperty ("selectedBand", 1, nullptr);

    // Feed some audio so meters / analyser / dynamics have real data.
    p.getPreFifo().setActive (true);
    p.getPostFifo().setActive (true);
    auto x = pinkNoise (48000, 0.25, 21);
    auto t = sine (6500, 48000, 48000, 0.2);
    std::vector<float> l (48000), r (48000);
    for (int i = 0; i < 48000; ++i)
    {
        l[(size_t) i] = (float) (x[(size_t) i] + t[(size_t) i]);
        r[(size_t) i] = (float) (x[(size_t) i] - t[(size_t) i] * 0.5);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    CHECK (ed != nullptr);
    ed->setSize (1220, 760);
    for (int k = 0; k < 20; ++k)
    {
        std::vector<float> lc (l.begin() + k * 2400, l.begin() + (k + 1) * 2400), rc (r.begin() + k * 2400, r.begin() + (k + 1) * 2400);
        renderStereo (p, lc, rc, 480);
        if (auto* e = dynamic_cast<MEProEQAudioProcessorEditor*> (ed.get()))
            e->refreshNow (0.05);
    }

    for (auto size : { std::pair { 1220, 760 }, std::pair { 980, 620 }, std::pair { 1800, 1100 } })
    {
        ed->setSize (size.first, size.second);
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        CHECK (img.isValid() && img.getWidth() == size.first);
        const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile ("reports/screenshots");
        dir.createDirectory();
        auto file = dir.getChildFile ("editor_" + juce::String (size.first) + "x" + juce::String (size.second) + ".png");
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }
    // High-DPI rendering (200 % scale)
    {
        ed->setSize (1220, 760);
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        CHECK (img.getWidth() == 2440);
        auto file = juce::File::getCurrentWorkingDirectory().getChildFile ("reports/screenshots/editor_1220x760@2x.png");
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }
    ed.reset();
    CHECK (p.getAuditionBand() == -1);
}
