// ME Pro EQ — ME Audio
// Host-level tests for the message-thread state services: factory/user presets, A/B comparison, undo/redo
// and auto gain, all driving the real AudioProcessor.
#include "PluginTestUtils.h"
#include "TestFramework.h"

#include <map>
#include <set>

using namespace metest;
using mepro::params::BandParam;
using mepro::state::ABComparison;

namespace
{
    // Edit a parameter the way the editor does: inside an undo bracket and a host gesture.
    void uiEdit (MEProEQAudioProcessor& p, const juce::String& id, float realValue)
    {
        mepro::state::UndoHistory::Scope scope (p.getUndoHistory());
        setParam (p, id, realValue);
    }

    struct TempDir
    {
        TempDir()
        {
            dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                      .getChildFile ("meproeq_test_" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
            dir.createDirectory();
        }
        ~TempDir() { dir.deleteRecursively(); }
        juce::File dir;
    };

    double dbToGainForTest (double db) { return std::pow (10.0, db / 20.0); }

    double rmsFrom (const std::vector<float>& x, size_t from)
    {
        double s = 0.0;
        for (size_t i = from; i < x.size(); ++i)
            s += (double) x[i] * x[i];
        return std::sqrt (s / (double) (x.size() - from));
    }
} // namespace

ME_TEST ("Presets", FactoryPresetsAreCompleteAndValid)
{
    juceInit();
    MEProEQAudioProcessor p;
    TempDir tmp; // keep the real user folder out of the test
    auto& pm = p.getPresets();
    pm.setUserPresetDirectoryForTesting (tmp.dir);

    std::set<juce::String> ids;
    std::map<juce::String, juce::RangedAudioParameter*> byId;
    for (auto* prm : p.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            ids.insert (rp->paramID);
            byId[rp->paramID] = rp;
        }

    std::map<juce::String, int> perCategory;
    std::set<juce::String> names;
    int badIds = 0, outOfRange = 0;
    for (const auto& info : pm.getPresets())
    {
        CHECK (info.factory);
        perCategory[info.category]++;
        CHECK_MSG (names.insert (info.name).second, "duplicate preset name " << info.name);
        CHECK_MSG (info.description.isNotEmpty(), info.name << " has no description");
        auto xml = mepro::state::PresetManager::parse (info);
        CHECK_MSG (xml != nullptr, info.name << " does not parse");
        if (xml == nullptr)
            continue;
        for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        {
            const auto id = e->getStringAttribute ("id");
            if (ids.count (id) == 0)
            {
                ++badIds;
                CHECK_MSG (false, info.name << ": unknown parameter " << id);
                continue;
            }
            // Every stored value must be representable exactly (not silently clamped by the parameter range).
            auto* rp = byId[id];
            const float v = (float) e->getDoubleAttribute ("value");
            const float back = rp->convertFrom0to1 (rp->convertTo0to1 (v));
            if (std::abs (back - v) > 1e-3f * std::max (1.0f, std::abs (v)))
            {
                ++outOfRange;
                CHECK_MSG (false, info.name << ": " << id << " = " << v << " is outside its range (-> " << back << ")");
            }
        }
    }
    measure ("factory presets", (double) pm.getPresets().size());
    for (auto& [cat, n] : perCategory)
        measure ("factory presets in category " + cat.toStdString(), (double) n);
    CHECK (pm.getPresets().size() == 35);
    for (const char* cat : { "Vocals", "Drums", "Bass", "Instruments", "Punjabi Production", "Bus Processing" })
        CHECK_MSG (perCategory[cat] >= 4, "category " << cat << " has " << perCategory[cat] << " presets");
    CHECK (badIds == 0 && outOfRange == 0);
}

ME_TEST ("Presets", EveryFactoryPresetLoadsAndRendersSafely)
{
    juceInit();
    const double fs = 48000.0;
    MEProEQAudioProcessor p;
    TempDir tmp;
    auto& pm = p.getPresets();
    pm.setUserPresetDirectoryForTesting (tmp.dir);
    p.prepareToPlay (fs, 256);

    const auto x = pinkNoise (24000, 0.6, 9);
    measure ("test signal: pink noise RMS (dBFS) / peak (dBFS)",
             str (20.0 * std::log10 (metest::rms (x))) + " / " + str (20.0 * std::log10 (peakAbs (x))));
    double worstPeak = 0.0;
    juce::String worstName;
    int failures = 0;
    for (const auto& info : pm.getPresets())
    {
        const auto before = p.getUndoHistory().undoSteps();
        CHECK_MSG (pm.load (info), "load " << info.name);
        CHECK (pm.getCurrentName() == info.name);
        CHECK (! pm.isModified());
        CHECK (p.getUndoHistory().undoSteps() <= before + 1); // one undoable action (none if nothing changed)
        p.getEngine().reset();
        auto l = std::vector<float> (x.begin(), x.end()), r = l;
        renderStereo (p, l, r, 256);
        double peak = 0.0;
        bool finite = true;
        for (size_t i = 0; i < l.size(); ++i)
        {
            finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]);
            peak = std::max (peak, (double) std::max (std::abs (l[i]), std::abs (r[i])));
        }
        if (! finite)
            ++failures;
        CHECK_MSG (finite, info.name << " produced non-finite output");
        if (peak > worstPeak)
        {
            worstPeak = peak;
            worstName = info.name;
        }
    }
    measure ("highest output peak over all factory presets (dBFS)",
             20.0 * std::log10 (std::max (worstPeak, 1e-12)));
    measure ("preset with the highest peak", worstName.toStdString());
    CHECK (failures == 0);
    // No factory preset raises the peak level by more than 12 dB.
    CHECK_MSG (worstPeak < peakAbs (x) * dbToGainForTest (12.0), worstName << " peak " << worstPeak);

    // Changing anything marks the preset as modified; stepping through the list wraps around.
    setParam (p, bp (0, BandParam::Gain), getParam (p, bp (0, BandParam::Gain)) + 1.0f);
    CHECK (pm.isModified());
    const auto last = pm.getCurrentName();
    CHECK (pm.loadNext (1));
    CHECK (pm.getCurrentName() == pm.getPresets().front().name);
    CHECK (pm.loadNext (-1));
    CHECK (pm.getCurrentName() == last);
}

ME_TEST ("Presets", UserPresetSaveLoadDelete)
{
    juceInit();
    MEProEQAudioProcessor p;
    TempDir tmp;
    auto& pm = p.getPresets();
    pm.setUserPresetDirectoryForTesting (tmp.dir);
    const auto factoryCount = pm.getPresets().size();

    setParam (p, bp (4, BandParam::Used), 1.0f);
    setParam (p, bp (4, BandParam::Freq), 3210.0f);
    setParam (p, bp (4, BandParam::Gain), -7.25f);
    setParam (p, bp (4, BandParam::Q), 3.3f);
    setParam (p, mepro::params::id::outputTrim, 1.5f);

    juce::String error;
    CHECK (! pm.saveUserPreset ("   ", "", error) && error.isNotEmpty());
    CHECK (! pm.saveUserPreset (pm.getPresets().front().name.toUpperCase(), "", error)); // factory names are reserved
    CHECK_MSG (pm.saveUserPreset ("My Vocal: Test/1", "desc", error), error);
    CHECK (pm.getPresets().size() == factoryCount + 1);
    CHECK (pm.getCurrentName() == "My Vocal: Test/1");
    CHECK (! pm.isModified());
    CHECK (tmp.dir.getNumberOfChildFiles (juce::File::findFiles, "*.mepreset") == 1);

    // A corrupt file next to it is ignored, never loaded.
    tmp.dir.getChildFile ("Broken.mepreset").replaceWithText ("<MEProEQPreset name='x'><PARAM id=");
    pm.rescan();
    CHECK (pm.getPresets().size() == factoryCount + 1);

    pm.loadDefault();
    CHECK (getParam (p, bp (4, BandParam::Used)) == 0.0f);
    CHECK (pm.loadByName ("My Vocal: Test/1"));
    CHECK (getParam (p, bp (4, BandParam::Used)) == 1.0f);
    CHECK_NEAR (getParam (p, bp (4, BandParam::Freq)), 3210.0f, 0.05f);
    CHECK_NEAR (getParam (p, bp (4, BandParam::Gain)), -7.25f, 1e-4f);
    CHECK_NEAR (getParam (p, bp (4, BandParam::Q)), 3.3f, 1e-4f);
    CHECK_NEAR (getParam (p, mepro::params::id::outputTrim), 1.5f, 1e-4f);

    // The preset name and its modified flag are saved with the project.
    setParam (p, bp (4, BandParam::Gain), -3.0f);
    juce::MemoryBlock blob;
    p.getStateInformation (blob);
    MEProEQAudioProcessor q;
    q.getPresets().setUserPresetDirectoryForTesting (tmp.dir);
    q.setStateInformation (blob.getData(), (int) blob.getSize());
    CHECK (q.getPresets().getCurrentName() == "My Vocal: Test/1");
    CHECK (q.getPresets().isModified());

    const auto* user = [&]() -> const mepro::state::PresetInfo*
    {
        for (const auto& i : pm.getPresets())
            if (! i.factory)
                return &i;
        return nullptr;
    }();
    CHECK (user != nullptr);
    if (user != nullptr)
    {
        const auto info = *user;
        CHECK (pm.deleteUserPreset (info));
        CHECK (pm.getPresets().size() == factoryCount);
        CHECK (! pm.deleteUserPreset (pm.getPresets().front())); // factory presets cannot be deleted
    }
}

ME_TEST ("Undo", EditorEditsAreUndoableHostAutomationIsNot)
{
    juceInit();
    MEProEQAudioProcessor p;
    auto& u = p.getUndoHistory();
    CHECK (! u.canUndo() && ! u.canRedo());

    uiEdit (p, bp (0, BandParam::Used), 1.0f);
    uiEdit (p, bp (0, BandParam::Gain), 4.0f);
    {
        // One drag = one step, however many intermediate values it passes through.
        mepro::state::UndoHistory::Scope drag (u);
        for (float f = 1000.0f; f < 2000.0f; f += 50.0f)
            setParam (p, bp (0, BandParam::Freq), f);
    }
    CHECK (u.undoSteps() == 3);
    uiEdit (p, bp (0, BandParam::Gain), 4.0f); // no change -> no step
    CHECK (u.undoSteps() == 3);

    setParam (p, bp (1, BandParam::Gain), 2.0f); // host automation: not recorded
    CHECK (u.undoSteps() == 3);

    CHECK (u.undo());
    CHECK_NEAR (getParam (p, bp (0, BandParam::Freq)), mepro::params::defaultFrequencyForBand (0), 0.01f);
    CHECK_NEAR (getParam (p, bp (0, BandParam::Gain)), 4.0f, 1e-5f);
    CHECK (u.undo());
    CHECK_NEAR (getParam (p, bp (0, BandParam::Gain)), 0.0f, 1e-5f);
    CHECK (u.redo());
    CHECK_NEAR (getParam (p, bp (0, BandParam::Gain)), 4.0f, 1e-5f);
    // A new edit clears the redo branch.
    uiEdit (p, bp (0, BandParam::Q), 2.0f);
    CHECK (! u.canRedo());

    // The history is bounded.
    for (int i = 0; i < 150; ++i)
        uiEdit (p, bp (2, BandParam::Gain), (float) (i % 20) - 10.0f + 0.5f);
    CHECK (u.undoSteps() == mepro::state::UndoHistory::kMaxSteps);
    measure ("undo steps kept", (double) u.undoSteps());

    // Loading a project starts a fresh history.
    juce::MemoryBlock blob;
    p.getStateInformation (blob);
    p.setStateInformation (blob.getData(), (int) blob.getSize());
    CHECK (! u.canUndo() && ! u.canRedo());
}

ME_TEST ("AB", SlotsAreIndependentAndKeepTheirOwnHistory)
{
    juceInit();
    MEProEQAudioProcessor p;
    auto& ab = p.getAB();
    auto& u = p.getUndoHistory();

    uiEdit (p, bp (0, BandParam::Used), 1.0f);
    uiEdit (p, bp (0, BandParam::Gain), 3.0f); // A: +3 dB
    CHECK (ab.getActive() == ABComparison::Slot::A);
    CHECK (! ab.slotsDiffer());

    CHECK (ab.switchTo (ABComparison::Slot::B)); // B starts as a copy of A
    CHECK (ab.getActive() == ABComparison::Slot::B);
    CHECK_NEAR (getParam (p, bp (0, BandParam::Gain)), 3.0f, 1e-5f);
    CHECK (! u.canUndo()); // B has its own (empty) history
    uiEdit (p, bp (0, BandParam::Gain), -5.0f); // B: -5 dB
    uiEdit (p, bp (6, BandParam::Used), 1.0f);
    CHECK (ab.slotsDiffer());

    // Undo in B steps back through B's edits only and never brings A's state in while B is shown.
    CHECK (u.undo());
    CHECK (getParam (p, bp (6, BandParam::Used)) == 0.0f);
    CHECK (u.undo());
    CHECK_NEAR (getParam (p, bp (0, BandParam::Gain)), 3.0f, 1e-5f);
    CHECK (! u.canUndo());
    CHECK (u.redo() && u.redo());
    CHECK_NEAR (getParam (p, bp (0, BandParam::Gain)), -5.0f, 1e-5f);

    CHECK (ab.toggle());
    CHECK (ab.getActive() == ABComparison::Slot::A);
    CHECK_NEAR (getParam (p, bp (0, BandParam::Gain)), 3.0f, 1e-5f);
    CHECK (getParam (p, bp (6, BandParam::Used)) == 0.0f);
    CHECK (u.undoSteps() == 2); // A's history is back
    CHECK (u.undo());
    CHECK_NEAR (getParam (p, bp (0, BandParam::Gain)), 0.0f, 1e-5f);
    CHECK (u.redo());

    // Switching is refused in the middle of an edit (a drag in progress).
    {
        mepro::state::UndoHistory::Scope drag (u);
        CHECK (! ab.switchTo (ABComparison::Slot::B));
        CHECK (ab.getActive() == ABComparison::Slot::A);
    }

    // Both slots are saved with the project.
    juce::MemoryBlock blob;
    p.getStateInformation (blob);
    MEProEQAudioProcessor q;
    q.setStateInformation (blob.getData(), (int) blob.getSize());
    CHECK (q.getAB().getActive() == ABComparison::Slot::A);
    CHECK_NEAR (getParam (q, bp (0, BandParam::Gain)), 3.0f, 1e-5f);
    CHECK (q.getAB().switchTo (ABComparison::Slot::B));
    CHECK_NEAR (getParam (q, bp (0, BandParam::Gain)), -5.0f, 1e-5f);
    CHECK (getParam (q, bp (6, BandParam::Used)) == 1.0f);

    // Copy: the inactive slot becomes identical to the active one.
    ab.copyActiveToInactive();
    CHECK (! ab.slotsDiffer());
    CHECK (ab.toggle());
    CHECK_NEAR (getParam (p, bp (0, BandParam::Gain)), 3.0f, 1e-5f);
    CHECK (! u.canUndo()); // the overwritten slot's old history was discarded
}

ME_TEST ("AutoGain", PluginLevelCompensation)
{
    juceInit();
    const double fs = 48000.0;
    MEProEQAudioProcessor p;
    p.prepareToPlay (fs, 512);
    setParam (p, bp (0, BandParam::Used), 1.0f);
    setParam (p, bp (0, BandParam::Type), 2.0f); // high shelf
    setParam (p, bp (0, BandParam::Freq), 2000.0f);
    setParam (p, bp (0, BandParam::Gain), 9.0f);

    const auto x = pinkNoise ((int) fs * 4, 0.1, 21);
    auto render = [&]
    {
        p.getEngine().reset();
        auto l = std::vector<float> (x.begin(), x.end()), r = l;
        renderStereo (p, l, r, 512);
        return 20.0 * std::log10 (rmsFrom (l, (size_t) fs) / rmsFrom (std::vector<float> (x.begin(), x.end()), (size_t) fs));
    };
    const double off = render();
    setParam (p, mepro::params::id::autoGain, 1.0f);
    const double on = render();
    measure ("+9 dB high shelf at 2 kHz, pink noise: level change without auto gain (dB)", off);
    measure ("same, with auto gain (dB)", on);
    measure ("auto gain applied (dB)", (double) p.getEngine().getAutoGainDb());
    CHECK (off > 3.0);
    CHECK (std::abs (on) < 1.0);
    CHECK (p.getEngine().getAutoGainDb() < -3.0f);
}
