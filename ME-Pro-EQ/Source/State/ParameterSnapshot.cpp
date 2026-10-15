// ME Pro EQ — ME Audio
#include "ParameterSnapshot.h"

#include <cmath>

namespace mepro::state
{

ParameterSnapshotter::ParameterSnapshotter (juce::AudioProcessor& p, const juce::String& excludedId)
{
    for (auto* prm : p.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm); rp != nullptr && rp->paramID != excludedId)
            params.push_back (rp);
}

Snapshot ParameterSnapshotter::capture() const
{
    Snapshot s;
    s.reserve (params.size());
    for (auto* p : params)
        s.push_back (p->getValue());
    return s;
}

void ParameterSnapshotter::restore (const Snapshot& s) const
{
    if (s.size() != params.size())
        return;
    // "Used" flags last when switching a band on and first when switching it off would avoid transient
    // states, but band changes are smoothed/crossfaded in the engine anyway; restore in declaration order.
    for (size_t i = 0; i < params.size(); ++i)
    {
        auto* p = params[i];
        if (std::abs (p->getValue() - s[i]) > 0.0f)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (s[i]);
            p->endChangeGesture();
        }
    }
}

bool ParameterSnapshotter::equal (const Snapshot& a, const Snapshot& b) const
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::abs (a[i] - b[i]) > 0.0f)
            return false;
    return true;
}

juce::ValueTree ParameterSnapshotter::toTree (const Snapshot& s, const juce::Identifier& type) const
{
    juce::ValueTree t (type);
    for (size_t i = 0; i < params.size() && i < s.size(); ++i)
    {
        juce::ValueTree c ("PARAM");
        c.setProperty ("id", params[i]->paramID, nullptr);
        c.setProperty ("value", params[i]->convertFrom0to1 (s[i]), nullptr);
        t.appendChild (c, nullptr);
    }
    return t;
}

Snapshot ParameterSnapshotter::fromTree (const juce::ValueTree& t) const
{
    Snapshot s = defaults();
    for (size_t i = 0; i < params.size(); ++i)
    {
        const auto c = t.getChildWithProperty ("id", params[i]->paramID);
        if (c.isValid() && c.hasProperty ("value"))
            s[i] = params[i]->convertTo0to1 ((float) (double) c.getProperty ("value"));
    }
    return s;
}

Snapshot ParameterSnapshotter::defaults() const
{
    Snapshot s;
    s.reserve (params.size());
    for (auto* p : params)
        s.push_back (p->getDefaultValue());
    return s;
}

} // namespace mepro::state
