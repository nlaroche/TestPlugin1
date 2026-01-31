# ==============================================================================
# DelayWave - A wavey modulated delay effect
# ==============================================================================

cmake_minimum_required(VERSION 3.22)
project(DelayWave VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# BeatConnect Configuration
# ==============================================================================
option(DELAYWAVE_DEV_MODE "Enable development mode with Vite hot reload" OFF)
option(BEATCONNECT_ENABLE_ACTIVATION "Enable BeatConnect activation" OFF)

# Map project-specific dev mode to generic name
set(BEATCONNECT_DEV_MODE ${DELAYWAVE_DEV_MODE})

# Include BeatConnect CMake helpers
include(${CMAKE_SOURCE_DIR}/../beatconnect-sdk/cmake/BeatConnectPlugin.cmake)

# Fetch JUCE
beatconnect_fetch_juce()

# ==============================================================================
# Plugin Target
# ==============================================================================
juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME "BeatConnect"
    PLUGIN_MANUFACTURER_CODE Beat
    PLUGIN_CODE Dwav
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PRODUCT_NAME "DelayWave"
    NEEDS_WEBVIEW2 TRUE
)

# ==============================================================================
# Source Files
# ==============================================================================
target_sources(${PROJECT_NAME}
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/ParameterIDs.h
)

# ==============================================================================
# Link Libraries
# ==============================================================================
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
)

# ==============================================================================
# Apply BeatConnect Configuration
# ==============================================================================
beatconnect_configure_plugin(${PROJECT_NAME})
