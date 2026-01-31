# ==============================================================================
# BeatConnect Example Plugin (WebUI)
# ==============================================================================
# This example demonstrates a modern plugin with React/WebView UI.
# The UI is built with React + TypeScript and communicates with C++ via JUCE 8's
# native WebBrowserComponent relay system.
#
# Use this as a starting point if:
# - You want a modern, web-based UI with hot reload during development
# - You're comfortable with React/TypeScript
# - You want maximum UI flexibility and easy styling
# ==============================================================================

cmake_minimum_required(VERSION 3.22)
project(ExamplePlugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# BeatConnect Configuration
# ==============================================================================
# WebUI will be auto-detected from web-ui/ directory
option(EXAMPLE_PLUGIN_DEV_MODE "Enable development mode with Vite hot reload" OFF)
option(BEATCONNECT_ENABLE_ACTIVATION "Enable BeatConnect activation" OFF)

# Map project-specific dev mode to generic name
set(BEATCONNECT_DEV_MODE ${EXAMPLE_PLUGIN_DEV_MODE})

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
    PLUGIN_CODE Expl
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PRODUCT_NAME "Example Plugin"
    NEEDS_WEBVIEW2 TRUE                 # Required for WebUI
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
        juce::juce_gui_extra              # Required for WebBrowserComponent
)

# ==============================================================================
# Apply BeatConnect Configuration
# ==============================================================================
beatconnect_configure_plugin(${PROJECT_NAME})
