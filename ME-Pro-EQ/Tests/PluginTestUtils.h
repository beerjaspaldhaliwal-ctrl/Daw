// ME Pro EQ — ME Audio
// Helpers for host-level tests that drive the real AudioProcessor.
#pragma once

#include "PluginProcessor.h"
#include "TestSignals.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

namespace metest
{

// The JUCE runtime (message manager etc.) is initialised by TestMain for the plugin test binary.
inline void juceInit()
{
    jassert (juce::MessageManager::getInstanceWithoutCreating() != nullptr);
}

inline void setParam (juce::AudioProcessor& p, const juce::String& id, float realValue)
{
    for (auto* prm : p.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm); rp != nullptr && rp->paramID == id)
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->convertTo0to1 (realValue));
            rp->endChangeGesture();
            return;
        }
    jassertfalse;
}

inline float getParam (juce::AudioProcessor& p, const juce::String& id)
{
    for (auto* prm : p.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm); rp != nullptr && rp->paramID == id)
            return rp->convertFrom0to1 (rp->getValue());
    return std::numeric_limits<float>::quiet_NaN();
}

inline juce::String bp (int band, mepro::params::BandParam p) { return mepro::params::bandParamId (band, p); }

template <typename T>
void renderStereo (juce::AudioProcessor& p, std::vector<T>& l, std::vector<T>& r, int block)
{
    juce::MidiBuffer midi;
    const int total = p.getTotalNumInputChannels();
    juce::AudioBuffer<T> buf (std::max (2, total), block);
    for (int pos = 0; pos < (int) l.size(); pos += block)
    {
        const int n = std::min (block, (int) l.size() - pos);
        buf.setSize (std::max (2, total), n, false, false, true);
        buf.clear();
        std::copy (l.begin() + pos, l.begin() + pos + n, buf.getWritePointer (0));
        std::copy (r.begin() + pos, r.begin() + pos + n, buf.getWritePointer (1));
        p.processBlock (buf, midi);
        std::copy (buf.getReadPointer (0), buf.getReadPointer (0) + n, l.begin() + pos);
        std::copy (buf.getReadPointer (1), buf.getReadPointer (1) + n, r.begin() + pos);
    }
}

} // namespace metest
