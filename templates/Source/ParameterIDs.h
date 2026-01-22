/*
  ==============================================================================
    {{PLUGIN_NAME}} - Parameter IDs
    BeatConnect Plugin Template

    Define all parameter identifiers as constexpr strings.
    These IDs must match exactly between:
    - C++ (APVTS parameter creation)
    - C++ (relay creation)
    - TypeScript (getSliderState(), etc.)
  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamIDs
{
    // Example parameters - modify for your plugin
    inline constexpr const char* gain   = "gain";
    inline constexpr const char* mix    = "mix";
    inline constexpr const char* bypass = "bypass";
    // inline constexpr const char* mode   = "mode";
}

// Default values and choice options
namespace ParamDefaults
{
    // Example choice options for ComboBox parameters
    // inline const juce::StringArray modeOptions { "Clean", "Warm", "Hot" };
}
