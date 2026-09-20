#include "Presets.h"

namespace slicetribe
{

const juce::StringArray& presetParameterIds()
{
    static const juce::StringArray ids { "pattern", "length", "motif", "sliceMode", "sliceSize", "stretch", "style", "chaos",
                                         "variation", "gate", "swing", "amount", "reverse", "octave", "fade", "sensitivity",
                                         "fill", "fxCutoff", "fxReso", "fxEnv", "fxDecay", "fxPump", "fxDrive", "fxLowCut", "fxWidth" };
    return ids;
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s)
{
    currentValues = factoryPresets().front().values;
    rescanUserPresets();
}

juce::File PresetManager::fileForName (const juce::String& name)
{
    auto fileName = juce::File::createLegalFileName (name.trim()).trim();
    if (fileName.isEmpty() || fileName.startsWithChar ('.'))
        fileName = "Preset " + fileName.trimCharactersAtStart (".");   // never a hidden file
    return getUserFolder().getChildFile (fileName + fileExtension);
}

juce::File PresetManager::getUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Chupa Loops").getChildFile ("Presets");
}

std::map<juce::String, float> PresetManager::readPresetFile (const juce::File& f, juce::String& name, juce::String& category)
{
    std::map<juce::String, float> v;
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("ChupaLoopsPreset"))
        return v;
    name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension()).substring (0, 64);
    category = xml->getStringAttribute ("category", "User");
    for (auto* p : xml->getChildWithTagNameIterator ("Param"))
    {
        const auto id = p->getStringAttribute ("id");
        if (presetParameterIds().contains (id))
        {
            const float value = (float) p->getDoubleAttribute ("value");
            if (std::isfinite (value))
                v[id] = value;
        }
    }
    return v;
}

void PresetManager::rescanUserPresets()
{
    std::vector<PresetData> found;
    auto folder = getUserFolder();
    if (folder.isDirectory())
    {
        auto files = folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
        files.sort();
        for (auto& f : files)
        {
            PresetData p;
            juce::String name, category;
            p.values = readPresetFile (f, name, category);
            if (p.values.empty())
                continue;
            p.name = name;
            p.category = "User";
            p.file = f;
            found.push_back (std::move (p));
        }
    }
    const juce::ScopedLock sl (lock);
    const juce::String keepName = currentName;
    const bool wasUser = currentIndex >= (int) factoryPresets().size();
    userPresets = std::move (found);
    if (wasUser)
    {
        currentIndex = -1;
        for (size_t i = 0; i < userPresets.size(); ++i)
            if (userPresets[i].name == keepName)
                currentIndex = (int) (factoryPresets().size() + i);
    }
}

int PresetManager::getNumPresets() const
{
    const juce::ScopedLock sl (lock);
    return (int) (factoryPresets().size() + userPresets.size());
}

PresetData PresetManager::getPreset (int index) const
{
    const juce::ScopedLock sl (lock);
    const int nf = (int) factoryPresets().size();
    if (juce::isPositiveAndBelow (index, nf))
        return factoryPresets()[(size_t) index];
    if (juce::isPositiveAndBelow (index - nf, (int) userPresets.size()))
        return userPresets[(size_t) (index - nf)];
    return {};
}

juce::StringArray PresetManager::getCategories() const
{
    juce::StringArray c;
    for (auto& p : factoryPresets())
        if (p.category != "Init")
            c.addIfNotAlreadyThere (p.category);
    c.add ("User");
    return c;
}

void PresetManager::apply (const PresetData& p)
{
    for (auto& id : presetParameterIds())
    {
        if (auto* param = apvts.getParameter (id))
        {
            float plain = param->convertFrom0to1 (param->getDefaultValue());
            if (auto it = p.values.find (id); it != p.values.end())
                plain = it->second;
            else if (id.startsWith ("fx") || id == "fill")
            {
                // the finishing layer (FX, fill) stays while you browse presets that don't set it; Init resets it
                if (p.category != "Init")
                    continue;
            }
            const float norm = juce::jlimit (0.0f, 1.0f, param->convertTo0to1 (plain));
            if (std::abs (param->getValue() - norm) > 1.0e-6f)
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (norm);
                param->endChangeGesture();
            }
        }
    }
}

void PresetManager::loadPreset (int index, bool fromHost)
{
    const auto p = getPreset (index);
    if (p.name.isEmpty())
        return;
    {
        const juce::ScopedLock sl (lock);
        currentIndex = index;
        currentName = p.name;
        currentValues = p.values;
        for (auto& id : presetParameterIds())   // missing values mean "default"
            if (currentValues.find (id) == currentValues.end())
                if (auto* param = apvts.getParameter (id))
                    currentValues[id] = param->convertFrom0to1 (param->getDefaultValue());
    }
    apply (p);
    if (onPresetLoaded)
        onPresetLoaded (fromHost);
}

void PresetManager::loadFactoryFromHost (int factoryIndex)
{
    if (juce::isPositiveAndBelow (factoryIndex, (int) factoryPresets().size()))
        loadPreset (factoryIndex, true);
}

void PresetManager::loadNext (int direction)
{
    const int n = getNumPresets();
    if (n <= 0) return;
    int i = getCurrentIndex();
    i = i < 0 ? 0 : ((i + direction) % n + n) % n;
    loadPreset (i);
}

bool PresetManager::saveUserPreset (const juce::String& rawName, juce::String& error)
{
    const auto name = rawName.trim().substring (0, 64);
    if (name.isEmpty())
    {
        error = "Please enter a name.";
        return false;
    }
    auto folder = getUserFolder();
    if (! folder.createDirectory())
    {
        error = "Can't create the preset folder:\n" + folder.getFullPathName();
        return false;
    }
    juce::XmlElement xml ("ChupaLoopsPreset");
    xml.setAttribute ("version", 1);
    xml.setAttribute ("name", name);
    xml.setAttribute ("category", "User");
    std::map<juce::String, float> values;
    for (auto& id : presetParameterIds())
    {
        const float v = apvts.getRawParameterValue (id)->load();
        values[id] = v;
        auto* e = xml.createNewChildElement ("Param");
        e->setAttribute ("id", id);
        e->setAttribute ("value", (double) v);
    }
    const auto file = fileForName (name);
    if (! xml.writeTo (file))
    {
        error = "Can't write the preset file:\n" + file.getFullPathName();
        return false;
    }
    {
        const juce::ScopedLock sl (lock);
        currentIndex = (int) factoryPresets().size();   // marks "user" for the rescan below
        currentName = name;
        currentValues = values;
    }
    rescanUserPresets();
    return true;
}

bool PresetManager::deleteUserPreset (const juce::File& file)
{
    if (! file.existsAsFile() || ! file.isAChildOf (getUserFolder()))
        return false;
    const bool ok = file.moveToTrash() || file.deleteFile();
    {
        const juce::ScopedLock sl (lock);
        const int nf = (int) factoryPresets().size();
        if (juce::isPositiveAndBelow (currentIndex - nf, (int) userPresets.size()) && userPresets[(size_t) (currentIndex - nf)].file == file)
            currentIndex = -1;
    }
    rescanUserPresets();
    return ok;
}

juce::File PresetManager::importPresetFile (const juce::File& f)
{
    juce::String name, category;
    if (readPresetFile (f, name, category).empty())
        return {};
    auto folder = getUserFolder();
    folder.createDirectory();
    auto target = folder.getChildFile (f.getFileName());
    if (target != f)
    {
        target = folder.getNonexistentChildFile (f.getFileNameWithoutExtension(), fileExtension, false);
        if (! f.copyFileTo (target))
            return {};
    }
    rescanUserPresets();
    return target;
}

int PresetManager::getCurrentIndex() const
{
    const juce::ScopedLock sl (lock);
    return currentIndex;
}

juce::String PresetManager::getCurrentName() const
{
    const juce::ScopedLock sl (lock);
    return currentName;
}

bool PresetManager::isModified() const
{
    const juce::ScopedLock sl (lock);
    for (auto& [id, v] : currentValues)
        if (auto* raw = apvts.getRawParameterValue (id))
            if (std::abs (raw->load() - v) > 0.051f)
                return true;
    return false;
}

void PresetManager::saveTo (juce::ValueTree& state) const
{
    const juce::ScopedLock sl (lock);
    juce::ValueTree t ("PRESET");
    t.setProperty ("name", currentName, nullptr);
    t.setProperty ("user", currentIndex >= (int) factoryPresets().size(), nullptr);
    for (auto& [id, v] : currentValues)
        t.setProperty (juce::Identifier (id), v, nullptr);
    state.appendChild (t, nullptr);
}

void PresetManager::restoreFrom (const juce::ValueTree& state)
{
    auto t = state.getChildWithName ("PRESET");
    if (! t.isValid())
    {
        // an older project without preset info: treat its settings as the reference
        const juce::ScopedLock sl (lock);
        currentIndex = -1;
        currentName = "Project settings";
        currentValues.clear();
        return;
    }
    const auto name = t.getProperty ("name").toString();
    const bool user = t.getProperty ("user", false);
    std::map<juce::String, float> values;
    for (auto& id : presetParameterIds())
        if (t.hasProperty (juce::Identifier (id)))
            values[id] = (float) t.getProperty (juce::Identifier (id));

    const juce::ScopedLock sl (lock);
    currentName = name.isNotEmpty() ? name : juce::String ("Init");
    currentValues = values;
    currentIndex = -1;
    if (! user)
    {
        for (size_t i = 0; i < factoryPresets().size(); ++i)
            if (factoryPresets()[i].name == currentName)
                currentIndex = (int) i;
    }
    else
    {
        for (size_t i = 0; i < userPresets.size(); ++i)
            if (userPresets[i].name == currentName)
                currentIndex = (int) (factoryPresets().size() + i);
    }
}

} // namespace slicetribe
