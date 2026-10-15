// ME Pro EQ — ME Audio
// Captures / restores the complete sound-relevant parameter set (everything except global bypass).
// Used by A/B comparison, undo/redo and presets. Message thread only.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace mepro::state
{

using Snapshot = std::vector<float>; // normalised values, in the order of getParameters()

class ParameterSnapshotter
{
public:
    explicit ParameterSnapshotter (juce::AudioProcessor& processor, const juce::String& excludedId = "bypass");

    Snapshot capture() const;
    // Applies the snapshot; each changed parameter is set inside a complete host gesture.
    void restore (const Snapshot& s) const;
    bool equal (const Snapshot& a, const Snapshot& b) const;

    // Serialisation by parameter ID with real (denormalised) values: robust against future additions.
    juce::ValueTree toTree (const Snapshot& s, const juce::Identifier& type) const;
    Snapshot fromTree (const juce::ValueTree& t) const; // parameters missing in `t` take their defaults
    Snapshot defaults() const;

    int size() const noexcept { return (int) params.size(); }

private:
    std::vector<juce::RangedAudioParameter*> params;
};

} // namespace mepro::state
