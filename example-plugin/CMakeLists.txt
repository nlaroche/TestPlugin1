# ==============================================================================
# BeatConnect Example Plugin - CMakeLists.txt
# ==============================================================================
# This is a complete, buildable example demonstrating all correct patterns.
# ==============================================================================

cmake_minimum_required(VERSION 3.22)
project(ExamplePlugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# Development Mode
# ==============================================================================
option(EXAMPLE_PLUGIN_DEV_MODE "Enable development mode with Vite hot reload" OFF)
option(BEATCONNECT_ENABLE_ACTIVATION "Enable BeatConnect activation" OFF)

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
# Compile Definitions
# ==============================================================================
target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        $<IF:$<BOOL:${EXAMPLE_PLUGIN_DEV_MODE}>,EXAMPLE_PLUGIN_DEV_MODE=1,EXAMPLE_PLUGIN_DEV_MODE=0>
)

# Windows WebView2
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC JUCE_USE_WIN_WEBVIEW2=1)
endif()

# macOS WebView
if(APPLE)
    target_compile_definitions(${PROJECT_NAME} PUBLIC JUCE_USE_WKWEBVIEW=1)
endif()

# ==============================================================================
# Link Libraries
# ==============================================================================
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ==============================================================================
# BeatConnect SDK Integration (Optional)
# ==============================================================================
if(EXISTS "${CMAKE_SOURCE_DIR}/resources/project_data.json")
    juce_add_binary_data(${PROJECT_NAME}_ProjectData
        HEADER_NAME "ProjectData.h"
        NAMESPACE ProjectData
        SOURCES resources/project_data.json
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_ProjectData)
    target_compile_definitions(${PROJECT_NAME} PUBLIC HAS_PROJECT_DATA=1)
else()
    target_compile_definitions(${PROJECT_NAME} PUBLIC HAS_PROJECT_DATA=0)
endif()

if(BEATCONNECT_ENABLE_ACTIVATION AND EXISTS "${CMAKE_SOURCE_DIR}/../sdk/activation/CMakeLists.txt")
    add_subdirectory(../sdk/activation ${CMAKE_BINARY_DIR}/beatconnect_activation)
    target_link_libraries(${PROJECT_NAME} PRIVATE beatconnect_activation)
    target_compile_definitions(${PROJECT_NAME} PUBLIC BEATCONNECT_ACTIVATION_ENABLED=1)
else()
    target_compile_definitions(${PROJECT_NAME} PUBLIC BEATCONNECT_ACTIVATION_ENABLED=0)
endif()

# ==============================================================================
# Web UI Resources - Copy AFTER build using POST_BUILD
# ==============================================================================
add_custom_command(TARGET ${PROJECT_NAME}_Standalone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Resources/WebUI"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}_Standalone>/Resources/WebUI"
    COMMENT "Copying WebUI resources to Standalone..."
)

add_custom_command(TARGET ${PROJECT_NAME}_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Resources/WebUI"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}_VST3>/../Resources/WebUI"
    COMMENT "Copying WebUI resources to VST3..."
)
