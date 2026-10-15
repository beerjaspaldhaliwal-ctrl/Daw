// ME Pro EQ — ME Audio
// JUCE AudioProcessor: host integration (buses, parameters, state, latency) around the DSP engine.
#pragma once

#include "DSP/EQEngine.h"
#include "DSP/SpectrumAnalyzer.h"
#include "State/ABComparison.h"
#include "State/ParameterLayout.h"
#include "State/ParameterSnapshot.h"
#include "State/PresetManager.h"
#include "State/UndoHistory.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

class MEProEQAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int kStateVersion = 1;

    MEProEQAudioProcessor();
    ~MEProEQAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    //==============================================================================
    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }
    mepro::dsp::EQEngine& getEngine() noexcept { return engine; }

    // Monitoring-only state: never saved, never automatable, ignored while rendering offline.
    void setAuditionBand (int band) noexcept { auditionBand.store (band); }
    int getAuditionBand() const noexcept { return auditionBand.load(); }
    void setMonoAudition (bool on) noexcept { monoAudition.store (on); }
    bool getMonoAudition() const noexcept { return monoAudition.load(); }

    double getSampleRateForUi() const noexcept { return uiSampleRate.load(); }
    int getMainChannelCount() const noexcept { return mainChannels.load(); }

    // Analyzer taps (audio thread pushes, editor pulls). Inactive (zero cost) while no editor shows them.
    mepro::dsp::AnalyzerFifo& getPreFifo() noexcept { return preFifo; }
    mepro::dsp::AnalyzerFifo& getPostFifo() noexcept { return postFifo; }
    bool isSidechainAvailable() const noexcept { return sidechainAvailable.load(); }

    // Non-parameter UI state stored inside the plugin state (selected band, display range, window size).
    juce::ValueTree getUiState();

    // Serialises / restores the complete state (parameters, A/B slots, preset name, UI state).
    juce::ValueTree createStateTree();
    bool restoreStateTree (const juce::ValueTree& tree);

    // Message-thread services. Lock getStateLock() while mutating A/B or preset state.
    mepro::state::ParameterSnapshotter& getSnapshotter() noexcept { return snapshotter; }
    mepro::state::UndoHistory& getUndoHistory() noexcept { return undo; }
    mepro::state::ABComparison& getAB() noexcept { return ab; }
    mepro::state::PresetManager& getPresets() noexcept { return presets; }
    juce::CriticalSection& getStateLock() noexcept { return stateLock; }

private:
    template <typename SampleType>
    void processInternal (juce::AudioBuffer<SampleType>& buffer);

    juce::AudioProcessorValueTreeState apvts;
    mepro::state::ParameterSnapshotter snapshotter { *this, mepro::params::id::bypass };
    mepro::state::UndoHistory undo { snapshotter };
    mepro::state::ABComparison ab { snapshotter, undo };
    mepro::state::PresetManager presets { snapshotter, undo };
    juce::CriticalSection stateLock;
    mepro::params::ParameterReader reader;
    mepro::dsp::EQEngine engine;
    mepro::dsp::EngineSettings settings;

    struct FifoTap final : mepro::dsp::AnalyzerTap
    {
        explicit FifoTap (mepro::dsp::AnalyzerFifo& f) : fifo (f) {}
        void push (const double* x, int ch, int n) noexcept override { fifo.push (x, ch, n); }
        mepro::dsp::AnalyzerFifo& fifo;
    };
    mepro::dsp::AnalyzerFifo preFifo, postFifo;
    FifoTap preTap { preFifo }, postTap { postFifo };

    std::atomic<int> mainChannels { 2 };
    std::atomic<int> auditionBand { -1 };
    std::atomic<bool> monoAudition { false };
    std::atomic<double> uiSampleRate { 48000.0 };
    std::atomic<bool> sidechainAvailable { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MEProEQAudioProcessor)
};
