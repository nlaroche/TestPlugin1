/*
  ==============================================================================
    PresetManager - User Preset Save/Load System

    Handles saving, loading, renaming, and deleting user presets.
    User presets are stored as XML files in the app data directory.
  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class PresetManager
{
public:
    PresetManager(juce::AudioProcessorValueTreeState& apvts,
                  const juce::String& pluginName);

    //==============================================================================
    // Factory Presets (read-only, defined in code)
    juce::StringArray getFactoryPresetNames() const;
    void setFactoryPresets(const juce::StringArray& names);

    //==============================================================================
    // User Presets
    juce::StringArray getUserPresetNames() const;
    bool saveUserPreset(const juce::String& name);
    bool loadPreset(const juce::String& name, bool isFactory);
    juce::ValueTree loadUserPresetState(const juce::String& name);
    bool renameUserPreset(const juce::String& oldName, const juce::String& newName);
    bool deleteUserPreset(const juce::String& name);

    //==============================================================================
    // JSON for UI communication
    juce::String getPresetListAsJson() const;

    //==============================================================================
    // Directory management
    juce::File getUserPresetsDirectory() const;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String pluginName;
    juce::File userPresetsDir;
    juce::StringArray factoryPresetNames;

    void ensureUserPresetsDirExists();
    juce::File getPresetFile(const juce::String& name) const;
    juce::String sanitizeFileName(const juce::String& name) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
