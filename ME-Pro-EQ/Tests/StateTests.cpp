// ME Pro EQ — ME Audio
// State save/restore tests at the AudioProcessor level (what the host stores in a project).
#include "PluginTestUtils.h"
#include "TestFramework.h"

using namespace metest;
using mepro::params::BandParam;

ME_TEST ("State", RoundTripIsExact)
{
    juceInit();
    MEProEQAudioProcessor a;
    Rng r (1234);
    // Randomise every parameter to arbitrary (non-grid) values.
    for (auto* prm : a.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost ((float) r.uniform());
            rp->endChangeGesture();
        }
    a.getUiState().setProperty ("selectedBand", 7, nullptr);
    a.getUiState().setProperty ("dbRange", 18, nullptr);

    juce::MemoryBlock blob;
    a.getStateInformation (blob);
    measure ("state size for all parameters set (bytes)", (double) blob.getSize());

    MEProEQAudioProcessor b;
    b.setStateInformation (blob.getData(), (int) blob.getSize());
    int mismatches = 0;
    for (int i = 0; i < a.getParameters().size(); ++i)
    {
        auto* pa = dynamic_cast<juce::RangedAudioParameter*> (a.getParameters()[i]);
        auto* pb = dynamic_cast<juce::RangedAudioParameter*> (b.getParameters()[i]);
        if (pa->paramID != pb->paramID || pa->convertFrom0to1 (pa->getValue()) != pb->convertFrom0to1 (pb->getValue()))
        {
            if (++mismatches < 5)
                CHECK_MSG (false, pa->paramID << ": " << pa->convertFrom0to1 (pa->getValue()) << " != " << pb->convertFrom0to1 (pb->getValue()));
        }
    }
    measure ("parameters compared", (double) a.getParameters().size());
    measure ("parameter values differing after restore", (double) mismatches);
    CHECK (mismatches == 0);
    CHECK ((int) b.getUiState().getProperty ("selectedBand") == 7);
    CHECK ((int) b.getUiState().getProperty ("dbRange") == 18);

    // Saving the restored state again yields byte-identical data (no drift across save/load cycles).
    juce::MemoryBlock blob2;
    b.getStateInformation (blob2);
    CHECK (blob == blob2);
}

ME_TEST ("State", PartialAndOlderStatesLoadDeterministically)
{
    juceInit();
    MEProEQAudioProcessor a;
    setParam (a, bp (0, BandParam::Used), 1.0f);
    setParam (a, bp (0, BandParam::Gain), 6.5f);
    setParam (a, bp (3, BandParam::Freq), 345.0f);

    // A state that only contains band 1's gain (e.g. written by an older version) must reset everything
    // else to defaults rather than keeping the current values.
    juce::ValueTree t ("MEProEQ");
    juce::ValueTree p ("PARAM");
    p.setProperty ("id", bp (0, BandParam::Gain), nullptr);
    p.setProperty ("value", -4.25, nullptr);
    t.appendChild (p, nullptr);
    juce::MemoryBlock blob;
    juce::AudioProcessor::copyXmlToBinary (*t.createXml(), blob);
    a.setStateInformation (blob.getData(), (int) blob.getSize());
    CHECK_NEAR (getParam (a, bp (0, BandParam::Gain)), -4.25f, 1e-6f);
    CHECK (getParam (a, bp (0, BandParam::Used)) == 0.0f);
    CHECK_NEAR (getParam (a, bp (3, BandParam::Freq)), mepro::params::defaultFrequencyForBand (3), 1e-3f);
}

ME_TEST ("State", CorruptDataIsRejectedSafely)
{
    juceInit();
    MEProEQAudioProcessor a;
    setParam (a, bp (2, BandParam::Used), 1.0f);
    setParam (a, bp (2, BandParam::Gain), 3.0f);
    Rng r (77);
    for (int trial = 0; trial < 200; ++trial)
    {
        std::vector<uint8_t> junk ((size_t) (1 + trial * 13));
        for (auto& b : junk)
            b = (uint8_t) (r.next() & 0xff);
        a.setStateInformation (junk.data(), (int) junk.size());
    }
    // A well-formed XML state for a different plugin is ignored as well.
    juce::ValueTree other ("SomeOtherPlugin");
    juce::MemoryBlock blob;
    juce::AudioProcessor::copyXmlToBinary (*other.createXml(), blob);
    a.setStateInformation (blob.getData(), (int) blob.getSize());
    a.setStateInformation (nullptr, 0);
    CHECK (getParam (a, bp (2, BandParam::Used)) == 1.0f);
    CHECK_NEAR (getParam (a, bp (2, BandParam::Gain)), 3.0f, 1e-6f);
}

ME_TEST ("State", RestoredStateRendersIdentically)
{
    juceInit();
    const double fs = 48000.0;
    MEProEQAudioProcessor a;
    setParam (a, bp (0, BandParam::Used), 1.0f);
    setParam (a, bp (0, BandParam::Type), 0.0f);
    setParam (a, bp (0, BandParam::Freq), 2345.6f);
    setParam (a, bp (0, BandParam::Gain), 4.321f);
    setParam (a, bp (0, BandParam::Q), 2.718f);
    setParam (a, bp (1, BandParam::Used), 1.0f);
    setParam (a, bp (1, BandParam::Type), 3.0f); // high pass
    setParam (a, bp (1, BandParam::Slope), 3.0f);
    setParam (a, bp (1, BandParam::Freq), 47.0f);
    setParam (a, bp (2, BandParam::Used), 1.0f);
    setParam (a, bp (2, BandParam::DynOn), 1.0f);
    setParam (a, bp (2, BandParam::Freq), 180.0f);
    setParam (a, bp (2, BandParam::Threshold), -30.0f);
    juce::MemoryBlock blob;
    a.getStateInformation (blob);

    MEProEQAudioProcessor b;
    b.setStateInformation (blob.getData(), (int) blob.getSize());
    a.prepareToPlay (fs, 512);
    b.prepareToPlay (fs, 512);
    auto x = pinkNoise (48000, 0.4, 5);
    auto la = x, ra = x, lb = x, rb = x;
    renderStereo (a, la, ra, 512);
    renderStereo (b, lb, rb, 512);
    CHECK (la == lb && ra == rb);
}
