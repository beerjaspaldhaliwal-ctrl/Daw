// ME Pro EQ — ME Audio
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

using namespace mepro;

namespace
{
const juce::Identifier kUiStateId ("UISTATE");
}

MEProEQAudioProcessor::MEProEQAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "MEProEQ", params::createParameterLayout())
{
    reader.init (apvts);
    engine.setAnalyzerTaps (&preTap, &postTap);
    // Prepare with defaults so that a host calling process() before prepareToPlay() is still safe.
    engine.prepare (48000.0, 512, 2);
}

MEProEQAudioProcessor::~MEProEQAudioProcessor() = default;

void MEProEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int channels = juce::jlimit (1, 2, getMainBusNumInputChannels());
    engine.prepare (sampleRate, juce::jmax (1, samplesPerBlock), channels);
    reader.read (settings);
    engine.setSettings (settings);
    engine.reset(); // start in the settled state of the current parameters (no fade-in on load)
    setLatencySamples (engine.getLatencySamples());
    uiSampleRate.store (sampleRate);
    mainChannels.store (channels);
}

void MEProEQAudioProcessor::releaseResources() {}

bool MEProEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

template <typename SampleType>
void MEProEQAudioProcessor::processInternal (juce::AudioBuffer<SampleType>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    reader.read (settings);

    // Monitoring functions must never end up in a bounce/export.
    const bool offline = isNonRealtime();
    settings.auditionBand = offline ? -1 : auditionBand.load (std::memory_order_relaxed);
    settings.monoAudition = ! offline && monoAudition.load (std::memory_order_relaxed);
    engine.setSettings (settings);

    auto mainBus = getBusBuffer (buffer, true, 0);
    const int numChannels = juce::jmin (2, mainBus.getNumChannels());

    const SampleType* scPtrs[2] = { nullptr, nullptr };
    int scChannels = 0;
    if (getBusCount (true) > 1)
    {
        if (auto* scBus = getBus (true, 1); scBus != nullptr && scBus->isEnabled())
        {
            auto sc = getBusBuffer (buffer, true, 1);
            scChannels = juce::jmin (2, sc.getNumChannels());
            for (int c = 0; c < scChannels; ++c)
                scPtrs[c] = sc.getReadPointer (c);
        }
    }
    sidechainAvailable.store (scChannels > 0, std::memory_order_relaxed);

    if (numChannels > 0 && numSamples > 0)
        engine.process (mainBus.getArrayOfWritePointers(), numChannels, numSamples,
                        scChannels > 0 ? scPtrs : nullptr, scChannels);

    // Any output channels beyond the main bus carry no audio from this effect.
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, numSamples);
}

void MEProEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processInternal (buffer);
}

void MEProEQAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&)
{
    processInternal (buffer);
}

double MEProEQAudioProcessor::getTailLengthSeconds() const
{
    // Ringing of the most resonant active band: a pole pair with quality Q at f0 decays by 60 dB in about
    // 6.9 * Q / (pi * f0) seconds. Limited to [0, 5] s.
    double tail = 0.0;
    for (int b = 0; b < dsp::kNumBands; ++b)
    {
        auto raw = [this, b] (params::BandParam p) { return apvts.getRawParameterValue (params::bandParamId (b, p))->load(); };
        if (raw (params::BandParam::Used) < 0.5f || raw (params::BandParam::On) < 0.5f)
            continue;
        const double f0 = juce::jmax (10.0f, raw (params::BandParam::Freq));
        const double q = juce::jmax (0.5f, raw (params::BandParam::Q));
        tail = juce::jmax (tail, 6.9 * q / (juce::MathConstants<double>::pi * f0));
    }
    return juce::jmin (5.0, tail);
}

juce::AudioProcessorParameter* MEProEQAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter (params::id::bypass);
}

juce::AudioProcessorEditor* MEProEQAudioProcessor::createEditor()
{
    return new MEProEQAudioProcessorEditor (*this);
}

//==============================================================================
juce::ValueTree MEProEQAudioProcessor::getUiState()
{
    auto ui = apvts.state.getChildWithName (kUiStateId);
    if (! ui.isValid())
    {
        ui = juce::ValueTree (kUiStateId);
        ui.setProperty ("selectedBand", -1, nullptr);
        ui.setProperty ("dbRange", 12, nullptr);
        apvts.state.appendChild (ui, nullptr);
    }
    return ui;
}

juce::ValueTree MEProEQAudioProcessor::createStateTree()
{
    auto state = apvts.copyState();
    state.setProperty ("pluginVersion", JucePlugin_VersionString, nullptr);
    state.setProperty ("stateVersion", kStateVersion, nullptr);
    const juce::ScopedLock sl (stateLock);
    ab.storeTo (state);
    presets.storeTo (state);
    return state;
}

bool MEProEQAudioProcessor::restoreStateTree (const juce::ValueTree& source)
{
    if (! source.hasType (apvts.state.getType()))
        return false;

    auto tree = source.createCopy();

    // Parameters missing from older/partial states are restored to their defaults so that loading is
    // deterministic (APVTS would otherwise leave them at their current values).
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr)
            continue;
        if (! tree.getChildWithProperty ("id", rp->paramID).isValid())
        {
            juce::ValueTree child ("PARAM");
            child.setProperty ("id", rp->paramID, nullptr);
            child.setProperty ("value", rp->convertFrom0to1 (rp->getDefaultValue()), nullptr);
            tree.appendChild (child, nullptr);
        }
    }
    apvts.replaceState (tree);
    {
        const juce::ScopedLock sl (stateLock);
        ab.loadFrom (tree);
        ab.clearHistories();
        presets.loadFrom (tree);
        undo.clear();
    }
    return true;
}

void MEProEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createStateTree().createXml())
        copyXmlToBinary (*xml, destData);
}

void MEProEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        restoreStateTree (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MEProEQAudioProcessor();
}
