// ME Pro EQ — ME Audio
// Factory presets (embedded, read-only) and user presets (Documents/ME Audio/ME Pro EQ/Presets).
//
// Preset file format (.mepreset, XML):
//   <MEProEQPreset name=".." category=".." author=".." description=".." version="1">
//     <PARAM id="b1_used" value="1"/> ...      (real values; parameters not listed take their defaults)
//   </MEProEQPreset>
// Loading a preset is one undoable action, and the browser marks the preset as modified once anything is
// changed afterwards, so a preset never silently replaces work without a way back. Message thread only.
#pragma once

#include "ParameterSnapshot.h"
#include "UndoHistory.h"

#include <vector>

namespace mepro::state
{

struct PresetInfo
{
    juce::String name, category, description, author;
    bool factory = true;
    juce::File file;      // user presets
    juce::String xmlText; // factory presets (embedded)
};

class PresetManager
{
public:
    static constexpr const char* kFileExtension = ".mepreset";
    static constexpr const char* kRootTag = "MEProEQPreset";

    PresetManager (ParameterSnapshotter& s, UndoHistory& u);

    static juce::File getUserPresetDirectory();
    void setUserPresetDirectoryForTesting (const juce::File& dir) { userDirOverride = dir; rescan(); }

    void rescan();
    const std::vector<PresetInfo>& getPresets() const noexcept { return presets; }

    bool load (const PresetInfo& p);
    bool loadByName (const juce::String& name);
    bool loadDefault(); // flat, all bands unused
    bool saveUserPreset (const juce::String& name, const juce::String& description, juce::String& error);
    bool deleteUserPreset (const PresetInfo& p);
    int indexOfCurrent() const;
    bool loadNext (int direction); // step through the list (wraps)

    const juce::String& getCurrentName() const noexcept { return currentName; }
    bool isModified() const;

    void storeTo (juce::ValueTree& parent) const;
    void loadFrom (const juce::ValueTree& parent);

    static std::unique_ptr<juce::XmlElement> parse (const PresetInfo& p);

private:
    juce::File userDir() const { return userDirOverride != juce::File() ? userDirOverride : getUserPresetDirectory(); }

    ParameterSnapshotter& snapshotter;
    UndoHistory& undo;
    std::vector<PresetInfo> presets;
    juce::String currentName { "Default" };
    Snapshot loadedSnapshot;
    bool hasLoadedSnapshot = false;
    bool forcedModified = false;
    juce::File userDirOverride;
};

} // namespace mepro::state
