/*
  ==============================================================================
    PresetManager - User Preset Save/Load System

    Handles saving, loading, renaming, and deleting user presets.
    User presets are stored as XML files in the app data directory.
  ==============================================================================
*/

#include "PresetManager.h"

//==============================================================================
PresetManager::PresetManager(juce::AudioProcessorValueTreeState& state,
                             const juce::String& name)
    : apvts(state), pluginName(name)
{
    // Set up user presets directory
    userPresetsDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile(pluginName)
        .getChildFile("UserPresets");

    ensureUserPresetsDirExists();
}

//==============================================================================
void PresetManager::ensureUserPresetsDirExists()
{
    if (!userPresetsDir.exists())
    {
        userPresetsDir.createDirectory();
    }
}

juce::File PresetManager::getUserPresetsDirectory() const
{
    return userPresetsDir;
}

juce::File PresetManager::getPresetFile(const juce::String& name) const
{
    return userPresetsDir.getChildFile(sanitizeFileName(name) + ".xml");
}

juce::String PresetManager::sanitizeFileName(const juce::String& name) const
{
    // Remove characters that aren't safe for file names
    juce::String safe = name;
    safe = safe.replaceCharacters("<>:\"/\\|?*", "_________");
    safe = safe.trim();

    // Limit length
    if (safe.length() > 100)
        safe = safe.substring(0, 100);

    return safe.isEmpty() ? "Untitled" : safe;
}

//==============================================================================
juce::StringArray PresetManager::getFactoryPresetNames() const
{
    return factoryPresetNames;
}

void PresetManager::setFactoryPresets(const juce::StringArray& names)
{
    factoryPresetNames = names;
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;

    auto files = userPresetsDir.findChildFiles(
        juce::File::findFiles, false, "*.xml");

    // Sort alphabetically
    files.sort();

    for (const auto& file : files)
    {
        names.add(file.getFileNameWithoutExtension());
    }

    return names;
}

//==============================================================================
bool PresetManager::saveUserPreset(const juce::String& name)
{
    if (name.isEmpty())
        return false;

    ensureUserPresetsDirExists();

    // Get current APVTS state as XML
    auto state = apvts.copyState();
    auto xml = state.createXml();

    if (!xml)
        return false;

    // Add preset metadata
    xml->setAttribute("presetName", name);
    xml->setAttribute("presetVersion", 1);

    // Save to file
    auto file = getPresetFile(name);
    return xml->writeTo(file);
}

bool PresetManager::loadPreset(const juce::String& name, bool isFactory)
{
    if (isFactory)
    {
        // Factory presets are loaded via the preset parameter index
        int index = factoryPresetNames.indexOf(name);
        if (index >= 0)
        {
            if (auto* param = apvts.getParameter("preset"))
            {
                param->setValueNotifyingHost(param->convertTo0to1((float)index));
                return true;
            }
        }
        return false;
    }

    // Load user preset from file
    auto file = getPresetFile(name);
    if (!file.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse(file);
    if (!xml)
        return false;

    auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid())
        return false;

    // Set parameters one by one from saved state
    // Skip the "preset" parameter to prevent factory preset being re-applied
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild(i);
        if (child.hasType("PARAM"))
        {
            auto paramId = child.getProperty("id").toString();

            // Skip the preset selector - we don't want to trigger factory presets
            if (paramId == "preset")
                continue;

            if (auto* param = apvts.getParameter(paramId))
            {
                // Values in APVTS state are stored as normalized (0-1)
                float normalizedValue = (float)child.getProperty("value");

                // Clamp to valid range to prevent any invalid values
                normalizedValue = juce::jlimit(0.0f, 1.0f, normalizedValue);

                param->setValueNotifyingHost(normalizedValue);
            }
        }
    }

    return true;
}

juce::ValueTree PresetManager::loadUserPresetState(const juce::String& name)
{
    // Load user preset and return the state for atomic application
    auto file = getPresetFile(name);
    if (!file.existsAsFile())
        return {};

    auto xml = juce::XmlDocument::parse(file);
    if (!xml)
        return {};

    auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid())
        return {};

    return state;
}

bool PresetManager::renameUserPreset(const juce::String& oldName, const juce::String& newName)
{
    if (oldName.isEmpty() || newName.isEmpty() || oldName == newName)
        return false;

    auto oldFile = getPresetFile(oldName);
    auto newFile = getPresetFile(newName);

    if (!oldFile.existsAsFile())
        return false;

    if (newFile.existsAsFile())
        return false; // Don't overwrite existing preset

    // Load, update metadata, save with new name
    auto xml = juce::XmlDocument::parse(oldFile);
    if (!xml)
        return false;

    xml->setAttribute("presetName", newName);

    if (!xml->writeTo(newFile))
        return false;

    // Delete old file
    oldFile.deleteFile();
    return true;
}

bool PresetManager::deleteUserPreset(const juce::String& name)
{
    if (name.isEmpty())
        return false;

    auto file = getPresetFile(name);
    if (!file.existsAsFile())
        return false;

    return file.deleteFile();
}

//==============================================================================
juce::String PresetManager::getPresetListAsJson() const
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();

    // Factory presets array
    juce::Array<juce::var> factoryArray;
    for (const auto& name : factoryPresetNames)
    {
        juce::DynamicObject::Ptr preset = new juce::DynamicObject();
        preset->setProperty("name", name);
        preset->setProperty("isFactory", true);
        factoryArray.add(juce::var(preset.get()));
    }
    root->setProperty("factory", factoryArray);

    // User presets array
    juce::Array<juce::var> userArray;
    for (const auto& name : getUserPresetNames())
    {
        juce::DynamicObject::Ptr preset = new juce::DynamicObject();
        preset->setProperty("name", name);
        preset->setProperty("isFactory", false);
        userArray.add(juce::var(preset.get()));
    }
    root->setProperty("user", userArray);

    return juce::JSON::toString(juce::var(root.get()));
}
