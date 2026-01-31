#pragma once

/**
 * Parameter IDs - Must match identifiers used in:
 * 1. WebSliderRelay/WebToggleButtonRelay constructors
 * 2. TypeScript getSliderState()/getToggleState() calls
 */
namespace ParamIDs
{
    // Use inline constexpr for ODR-safe constants
    inline constexpr const char* gain = "gain";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* bypass = "bypass";
}
