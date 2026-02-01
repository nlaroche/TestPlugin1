# ==============================================================================
# {{PLUGIN_NAME}} - BeatConnect Plugin
# ==============================================================================
#
# BUILD FLAGS:
# - {{PLUGIN_NAME_UPPER}}_DEV_MODE=ON  : Use Vite dev server for hot reload
# - BEATCONNECT_ENABLE_ACTIVATION=ON   : Enable license activation
#
# ==============================================================================

cmake_minimum_required(VERSION 3.22)
project({{PLUGIN_NAME}} VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# BeatConnect Configuration
# ==============================================================================
option({{PLUGIN_NAME_UPPER}}_DEV_MODE "Enable development mode with Vite hot reload" OFF)
option(BEATCONNECT_ENABLE_ACTIVATION "Enable BeatConnect activation" OFF)

# Map project-specific dev mode to generic name
set(BEATCONNECT_DEV_MODE ${{{PLUGIN_NAME_UPPER}}_DEV_MODE})

# Include BeatConnect CMake helpers
# Use CMAKE_CURRENT_SOURCE_DIR to handle both direct and subdirectory builds
include(${CMAKE_CURRENT_SOURCE_DIR}/../beatconnect-sdk/cmake/BeatConnectPlugin.cmake)

# Fetch JUCE
beatconnect_fetch_juce()

# ==============================================================================
# Plugin Target
# ==============================================================================
juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME "{{COMPANY_NAME}}"
    PLUGIN_MANUFACTURER_CODE {{MANUFACTURER_CODE}}
    PLUGIN_CODE {{PLUGIN_CODE}}
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PRODUCT_NAME "{{PLUGIN_NAME}}"
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
