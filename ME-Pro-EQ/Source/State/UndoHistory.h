// ME Pro EQ — ME Audio
// Plugin-internal undo/redo for edits made in the ME Pro EQ window (independent of the host's own undo).
//
// An "action" is bracketed by begin()/end(); nested brackets merge into one action. The parameter state is
// captured when the outermost bracket opens and compared when it closes; only real changes create an undo
// step. Host automation is never recorded (it does not pass through begin()/end()).
// Message thread only.
#pragma once

#include "ParameterSnapshot.h"

#include <deque>
#include <utility>

namespace mepro::state
{

class UndoHistory
{
public:
    static constexpr size_t kMaxSteps = 100;

    explicit UndoHistory (ParameterSnapshotter& s) : snapshotter (s) {}

    void begin()
    {
        if (depth++ == 0)
            before = snapshotter.capture();
    }

    void end()
    {
        if (depth <= 0)
            return;
        if (--depth == 0)
        {
            auto after = snapshotter.capture();
            if (! snapshotter.equal (before, after))
            {
                undoStack.push_back (std::move (before));
                if (undoStack.size() > kMaxSteps)
                    undoStack.pop_front();
                redoStack.clear();
            }
        }
    }

    bool canUndo() const noexcept { return ! undoStack.empty() && depth == 0; }
    bool canRedo() const noexcept { return ! redoStack.empty() && depth == 0; }

    bool undo()
    {
        if (! canUndo())
            return false;
        redoStack.push_back (snapshotter.capture());
        const auto s = undoStack.back();
        undoStack.pop_back();
        snapshotter.restore (s);
        return true;
    }

    bool redo()
    {
        if (! canRedo())
            return false;
        undoStack.push_back (snapshotter.capture());
        const auto s = redoStack.back();
        redoStack.pop_back();
        snapshotter.restore (s);
        return true;
    }

    void clear()
    {
        undoStack.clear();
        redoStack.clear();
    }

    // The undo/redo stacks of one A/B slot. ABComparison swaps them in and out so that each slot keeps its own
    // history and an undo can never move the other slot's state into the live one.
    struct Stacks
    {
        std::deque<Snapshot> undoStack, redoStack;
    };

    void swapStacks (Stacks& other) noexcept
    {
        std::swap (undoStack, other.undoStack);
        std::swap (redoStack, other.redoStack);
    }

    bool isInsideAction() const noexcept { return depth > 0; }

    size_t undoSteps() const noexcept { return undoStack.size(); }
    size_t redoSteps() const noexcept { return redoStack.size(); }

    // RAII bracket
    struct Scope
    {
        explicit Scope (UndoHistory& h) : history (h) { history.begin(); }
        ~Scope() { history.end(); }
        UndoHistory& history;
    };

private:
    ParameterSnapshotter& snapshotter;
    std::deque<Snapshot> undoStack, redoStack;
    Snapshot before;
    int depth = 0;
};

} // namespace mepro::state
