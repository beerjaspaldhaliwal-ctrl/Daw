// ME Pro EQ — ME Audio
// Two complete EQ states, A and B. The live parameters always belong to the active slot; the inactive slot
// is kept as a snapshot and saved with the project.
//
// Each slot has its own undo history (swapped in and out of UndoHistory on a switch). Switching is therefore
// not itself an undo step: an undo in slot B only ever steps back through edits made in B, and can never
// replace B with A's settings while the selector still shows B. Message thread only.
#pragma once

#include "ParameterSnapshot.h"
#include "UndoHistory.h"

namespace mepro::state
{

class ABComparison
{
public:
    enum class Slot { A, B };

    ABComparison (ParameterSnapshotter& s, UndoHistory& u) : snapshotter (s), undo (u) {}

    Slot getActive() const noexcept { return active; }

    // Returns false (and does nothing) while an edit is in progress.
    bool switchTo (Slot s)
    {
        if (s == active)
            return true;
        if (undo.isInsideAction())
            return false;
        auto live = snapshotter.capture();
        if (! otherValid) // the first switch starts B (or A) as a copy of the current state
            other = live;
        snapshotter.restore (other);
        other = std::move (live);
        otherValid = true;
        undo.swapStacks (otherHistory);
        active = s;
        return true;
    }

    bool toggle() { return switchTo (active == Slot::A ? Slot::B : Slot::A); }

    // The inactive slot becomes a copy of the active (live) one; its old history no longer applies.
    void copyActiveToInactive()
    {
        other = snapshotter.capture();
        otherValid = true;
        otherHistory = {};
    }

    // Project load: the restored slots start with empty histories.
    void clearHistories() { otherHistory = {}; }

    bool slotsDiffer() const
    {
        return otherValid && ! snapshotter.equal (other, snapshotter.capture());
    }

    void storeTo (juce::ValueTree& parent) const
    {
        auto existing = parent.getChildWithName ("AB");
        if (existing.isValid())
            parent.removeChild (existing, nullptr);
        juce::ValueTree ab ("AB");
        ab.setProperty ("active", active == Slot::A ? "A" : "B", nullptr);
        if (otherValid)
            ab.appendChild (snapshotter.toTree (other, "INACTIVE"), nullptr);
        parent.appendChild (ab, nullptr);
    }

    void loadFrom (const juce::ValueTree& parent)
    {
        const auto ab = parent.getChildWithName ("AB");
        active = ab.isValid() && ab.getProperty ("active").toString() == "B" ? Slot::B : Slot::A;
        const auto inactive = ab.getChildWithName ("INACTIVE");
        otherValid = inactive.isValid();
        other = otherValid ? snapshotter.fromTree (inactive) : Snapshot {};
    }

private:
    ParameterSnapshotter& snapshotter;
    UndoHistory& undo;
    Slot active = Slot::A;
    Snapshot other;
    bool otherValid = false;
    UndoHistory::Stacks otherHistory;
};

} // namespace mepro::state
