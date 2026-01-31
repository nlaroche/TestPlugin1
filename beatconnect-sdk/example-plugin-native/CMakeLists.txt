# ==============================================================================
# BeatConnect Example Plugin (Native JUCE UI)
# ==============================================================================
# This example demonstrates a traditional JUCE plugin with native UI components.
# No WebView, no React - just standard JUCE sliders and components.
#
# Use this as a starting point if:
# - You have an existing JUCE plugin you want to build with BeatConnect
# - You prefer native JUCE UI over web technologies
# - You want the simplest possible setup
# ==============================================================================

cmake_minimum_required(VERSION 3.22)
project(ExamplePluginNative VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# BeatConnect Configuration
# ==============================================================================
# Explicitly disable WebUI for this example (would auto-detect anyway)
set(BEATCONNECT_USE_WEBUI OFF)
option(BEATCONNECT_ENABLE_ACTIVATION "Enable BeatConnect activation" OFF)

# Include BeatConnect CMake helpers
include(${CMAKE_SOURCE_DIR}/../cmake/BeatConnectPlugin.cmake)

# Fetch JUCE
beatconnect_fetch_juce()

# ==============================================================================
# Plugin Target
# ==============================================================================
juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME "BeatConnect"
    PLUGIN_MANUFACTURER_CODE Beat
    PLUGIN_CODE Exnt                    # Different from WebUI example
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PRODUCT_NAME "Example Plugin Native"
    # No NEEDS_WEBVIEW2 - not needed for native UI
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
        # Note: juce_gui_extra not needed for native UI
)

# ==============================================================================
# Apply BeatConnect Configuration
# ==============================================================================
beatconnect_configure_plugin(${PROJECT_NAME})
