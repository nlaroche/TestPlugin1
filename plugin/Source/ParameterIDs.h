/*
  ==============================================================================
    DelayWave - Parameter IDs
    A wavey modulated delay effect
  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamIDs
{
    // Core delay parameters
    inline constexpr const char* time     = "time";      // Delay time in ms
    inline constexpr const char* feedback = "feedback";  // Feedback amount 0-1
    inline constexpr const char* mix      = "mix";       // Dry/wet mix 0-1

    // Modulation (the wavey stuff!)
    inline constexpr const char* modRate  = "modRate";   // LFO rate in Hz
    inline constexpr const char* modDepth = "modDepth";  // Modulation depth 0-1

    // Tone control
    inline constexpr const char* tone     = "tone";      // Filter brightness 0-1

    // Bypass
    inline constexpr const char* bypass   = "bypass";
}
