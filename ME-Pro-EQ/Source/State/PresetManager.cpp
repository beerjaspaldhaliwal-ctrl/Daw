// ME Pro EQ — ME Audio
#include "PresetManager.h"

#include "BinaryData.h"

namespace mepro::state
{

PresetManager::PresetManager (ParameterSnapshotter& s, UndoHistory& u) : snapshotter (s), undo (u)
{
    rescan();
}

juce::File PresetManager::getUserPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("ME Audio").getChildFile ("ME Pro EQ").getChildFile ("Presets");
}

std::unique_ptr<juce::XmlElement> PresetManager::parse (const PresetInfo& p)
{
    std::unique_ptr<juce::XmlElement> xml = p.factory ? juce::parseXML (p.xmlText) : juce::parseXML (p.file);
    if (xml == nullptr || ! xml->hasTagName (kRootTag))
        return nullptr;
    return xml;
}

void PresetManager::rescan()
{
    presets.clear();

    // Factory presets embedded in the binary.
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String file (BinaryData::originalFilenames[i]);
        if (! file.endsWith (kFileExtension))
            continue;
        int size = 0;
        const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
        if (data == nullptr || size <= 0)
            continue;
        PresetInfo info;
        info.factory = true;
        info.xmlText = juce::String::fromUTF8 (data, size);
        if (auto xml = parse (info))
        {
            info.name = xml->getStringAttribute ("name", file.upToLastOccurrenceOf (".", false, false));
            info.category = xml->getStringAttribute ("category", "Factory");
            info.description = xml->getStringAttribute ("description");
            info.author = xml->getStringAttribute ("author", "ME Audio");
            presets.push_back (std::move (info));
        }
    }
    std::stable_sort (presets.begin(), presets.end(), [] (const PresetInfo& a, const PresetInfo& b)
    {
        return a.category == b.category ? a.name.compareNatural (b.name) < 0 : a.category.compareNatural (b.category) < 0;
    });

    // User presets
    const auto dir = userDir();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, juce::String ("*") + kFileExtension);
        files.sort();
        for (const auto& f : files)
        {
            PresetInfo info;
            info.factory = false;
            info.file = f;
            if (auto xml = parse (info))
            {
                info.name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
                info.category = "User";
                info.description = xml->getStringAttribute ("description");
                info.author = xml->getStringAttribute ("author");
                presets.push_back (std::move (info));
            }
        }
    }
}

bool PresetManager::load (const PresetInfo& p)
{
    auto xml = parse (p);
    if (xml == nullptr)
        return false;
    const auto snap = snapshotter.fromTree (juce::ValueTree::fromXml (*xml));
    {
        UndoHistory::Scope scope (undo);
        snapshotter.restore (snap);
    }
    loadedSnapshot = snapshotter.capture();
    hasLoadedSnapshot = true;
    forcedModified = false;
    currentName = p.name;
    return true;
}

bool PresetManager::loadByName (const juce::String& name)
{
    for (const auto& p : presets)
        if (p.name == name)
            return load (p);
    return false;
}

bool PresetManager::loadDefault()
{
    {
        UndoHistory::Scope scope (undo);
        snapshotter.restore (snapshotter.defaults());
    }
    loadedSnapshot = snapshotter.capture();
    hasLoadedSnapshot = true;
    forcedModified = false;
    currentName = "Default";
    return true;
}

bool PresetManager::saveUserPreset (const juce::String& rawName, const juce::String& description, juce::String& error)
{
    const auto name = rawName.trim();
    const auto safe = juce::File::createLegalFileName (name);
    if (name.isEmpty() || safe.isEmpty())
    {
        error = "Please enter a preset name.";
        return false;
    }
    for (const auto& p : presets)
        if (p.factory && p.name.equalsIgnoreCase (name))
        {
            error = "\"" + name + "\" is the name of a factory preset. Please choose another name.";
            return false;
        }
    const auto dir = userDir();
    if (! dir.createDirectory())
    {
        error = "Could not create " + dir.getFullPathName();
        return false;
    }
    auto tree = snapshotter.toTree (snapshotter.capture(), kRootTag);
    tree.setProperty ("name", name, nullptr);
    tree.setProperty ("category", "User", nullptr);
    tree.setProperty ("description", description, nullptr);
    tree.setProperty ("author", juce::SystemStats::getFullUserName(), nullptr);
    tree.setProperty ("version", 1, nullptr);
    const auto file = dir.getChildFile (safe + kFileExtension);
    auto xml = tree.createXml();
    if (xml == nullptr || ! xml->writeTo (file))
    {
        error = "Could not write " + file.getFullPathName();
        return false;
    }
    rescan();
    loadedSnapshot = snapshotter.capture();
    hasLoadedSnapshot = true;
    forcedModified = false;
    currentName = name;
    return true;
}

bool PresetManager::deleteUserPreset (const PresetInfo& p)
{
    if (p.factory || ! p.file.existsAsFile())
        return false;
    const bool ok = p.file.deleteFile();
    rescan();
    return ok;
}

int PresetManager::indexOfCurrent() const
{
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == currentName)
            return (int) i;
    return -1;
}

bool PresetManager::loadNext (int direction)
{
    if (presets.empty())
        return false;
    const int n = (int) presets.size();
    int i = indexOfCurrent();
    i = i < 0 ? (direction > 0 ? 0 : n - 1) : ((i + direction) % n + n) % n;
    return load (presets[(size_t) i]);
}

bool PresetManager::isModified() const
{
    return forcedModified || (hasLoadedSnapshot && ! snapshotter.equal (loadedSnapshot, snapshotter.capture()));
}

void PresetManager::storeTo (juce::ValueTree& parent) const
{
    parent.setProperty ("presetName", currentName, nullptr);
    parent.setProperty ("presetModified", isModified(), nullptr);
}

void PresetManager::loadFrom (const juce::ValueTree& parent)
{
    currentName = parent.getProperty ("presetName", "Default").toString();
    // The reference snapshot for "modified" is the restored state itself, unless the session was saved with
    // a modified preset (then it stays flagged as modified).
    forcedModified = (bool) parent.getProperty ("presetModified", false);
    hasLoadedSnapshot = ! forcedModified;
    loadedSnapshot = hasLoadedSnapshot ? snapshotter.capture() : Snapshot {};
}

} // namespace mepro::state
