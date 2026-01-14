# ==============================================================================
# BeatConnect Plugin Template - CMakeLists.txt
# ==============================================================================
# Replace {{PLUGIN_NAME}}, {{COMPANY_NAME}}, {{PLUGIN_CODE}}, {{MANUFACTURER_CODE}}
# ==============================================================================

cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# Project Configuration
# ==============================================================================
project({{PLUGIN_NAME}} VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# JUCE
# ==============================================================================
include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

# ==============================================================================
# Plugin Target
# ==============================================================================
juce_add_plugin(${PROJECT_NAME}
    # Basic Info
    COMPANY_NAME "{{COMPANY_NAME}}"
    PLUGIN_MANUFACTURER_CODE {{MANUFACTURER_CODE}}
    PLUGIN_CODE {{PLUGIN_CODE}}

    # Plugin Formats
    FORMATS VST3 AU Standalone

    # Plugin Characteristics
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    # Product Name (shown in DAW)
    PRODUCT_NAME "${PROJECT_NAME}"

    # Web Browser for UI
    NEEDS_WEB_BROWSER TRUE

    # Bundle ID (macOS)
    BUNDLE_ID "com.{{COMPANY_NAME_LOWER}}.${PROJECT_NAME}"

    # Manufacturer URL
    COMPANY_WEBSITE "https://beatconnect.io"
    COMPANY_EMAIL "support@beatconnect.io"
)

# ==============================================================================
# Source Files
# ==============================================================================
target_sources(${PROJECT_NAME}
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginProcessor.h
        src/PluginEditor.cpp
        src/PluginEditor.h
)

# ==============================================================================
# Compile Definitions
# ==============================================================================
target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0

        # BeatConnect Integration
        BEATCONNECT_PLUGIN_ID="${BEATCONNECT_PLUGIN_ID}"
        BEATCONNECT_API_URL="${BEATCONNECT_API_URL}"
)

# ==============================================================================
# Link Libraries
# ==============================================================================
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ==============================================================================
# Web UI Assets
# ==============================================================================
# Copy web UI build output to plugin bundle
if(EXISTS "${CMAKE_SOURCE_DIR}/web/dist")
    if(APPLE)
        # macOS: Copy to Resources inside bundle
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/web/dist"
                "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}_VST3>/Resources/web"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/web/dist"
                "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}_AU>/Resources/web"
            COMMENT "Copying web assets to macOS bundles"
        )
    else()
        # Windows/Linux: Copy alongside plugin binary
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/web/dist"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}_VST3>/web"
            COMMENT "Copying web assets to build output"
        )
    endif()
endif()

# ==============================================================================
# BeatConnect SDK (optional - for activation)
# ==============================================================================
# Uncomment when SDK is ready:
# add_subdirectory(beatconnect-sdk/sdk/activation)
# target_link_libraries(${PROJECT_NAME} PRIVATE beatconnect_activation)
