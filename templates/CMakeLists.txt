# ==============================================================================
# BeatConnect Plugin Template - CMakeLists.txt
# ==============================================================================
# Replace {{PLUGIN_NAME}}, {{PLUGIN_NAME_UPPER}}, {{COMPANY_NAME}},
# {{PLUGIN_CODE}}, {{MANUFACTURER_CODE}}
# ==============================================================================

cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# Project Configuration
# ==============================================================================
project({{PLUGIN_NAME}} VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# Development Mode
# ==============================================================================
# Set to ON for development (hot reload from Vite dev server)
# Set to OFF for production (load bundled web assets)
option({{PLUGIN_NAME_UPPER}}_DEV_MODE "Enable development mode with Vite hot reload" OFF)

# ==============================================================================
# JUCE
# ==============================================================================
# Use JUCE_PATH if provided (CI builds), otherwise fetch from GitHub
if(DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(STATUS "Using JUCE from: ${JUCE_PATH}")
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    message(STATUS "Fetching JUCE from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

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
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE

    # Product Name (shown in DAW)
    PRODUCT_NAME "${PROJECT_NAME}"

    # WebView2 support (Windows)
    NEEDS_WEBVIEW2 TRUE

    # Bundle ID (macOS)
    BUNDLE_ID "com.{{COMPANY_NAME_LOWER}}.${PROJECT_NAME}"

    # Manufacturer Info
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
        src/ParameterIDs.h
)

# ==============================================================================
# Compile Definitions
# ==============================================================================
target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        # JUCE Configuration
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0

        # Dev mode toggle (0 = production, 1 = development)
        $<IF:$<BOOL:${{{PLUGIN_NAME_UPPER}}_DEV_MODE}>,{{PLUGIN_NAME_UPPER}}_DEV_MODE=1,{{PLUGIN_NAME_UPPER}}_DEV_MODE=0>
)

# Windows-specific WebView2 support
if(WIN32)
    target_compile_definitions(${PROJECT_NAME}
        PUBLIC
            JUCE_USE_WIN_WEBVIEW2=1
            # Static linking embeds the WebView2 loader into the binary - no DLL needed at runtime
            JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    )
endif()

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
# Web UI Assets (Production Only)
# ==============================================================================
# Copy web UI build output to plugin bundle for production builds
if(EXISTS "${CMAKE_SOURCE_DIR}/web/dist" AND NOT ${{{PLUGIN_NAME_UPPER}}_DEV_MODE})
    # Copy to Standalone
    add_custom_command(TARGET ${PROJECT_NAME}_Standalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/web/dist"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}_Standalone>/Resources/WebUI"
        COMMENT "Copying WebUI to Standalone..."
    )

    # Copy to VST3
    add_custom_command(TARGET ${PROJECT_NAME}_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/web/dist"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}_VST3>/../Resources/WebUI"
        COMMENT "Copying WebUI to VST3..."
    )

    # macOS: Also copy to AU
    if(APPLE)
        add_custom_command(TARGET ${PROJECT_NAME}_AU POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/web/dist"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}_AU>/../Resources/WebUI"
            COMMENT "Copying WebUI to AU..."
        )
    endif()
endif()

# ==============================================================================
# BeatConnect SDK Integration
# ==============================================================================

# Option to enable BeatConnect activation (set by CI or manually)
option(BEATCONNECT_ENABLE_ACTIVATION "Enable BeatConnect activation key support" OFF)

# Embed project_data.json if it exists (injected at build time by BeatConnect CI)
if(EXISTS "${CMAKE_SOURCE_DIR}/resources/project_data.json")
    message(STATUS "Found project_data.json - embedding as BinaryData")
    juce_add_binary_data(${PROJECT_NAME}_BinaryData
        HEADER_NAME "ProjectData.h"
        NAMESPACE ProjectData
        SOURCES
            resources/project_data.json
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_BinaryData)
    target_compile_definitions(${PROJECT_NAME} PUBLIC HAS_PROJECT_DATA=1)
else()
    message(STATUS "No project_data.json found - BeatConnect features disabled")
    target_compile_definitions(${PROJECT_NAME} PUBLIC HAS_PROJECT_DATA=0)
endif()

# BeatConnect Activation SDK (requires beatconnect-sdk submodule)
if(BEATCONNECT_ENABLE_ACTIVATION AND EXISTS "${CMAKE_SOURCE_DIR}/beatconnect-sdk/sdk/activation/CMakeLists.txt")
    message(STATUS "BeatConnect Activation SDK enabled")
    add_subdirectory(beatconnect-sdk/sdk/activation)
    target_link_libraries(${PROJECT_NAME} PRIVATE beatconnect_activation)
    target_compile_definitions(${PROJECT_NAME} PUBLIC BEATCONNECT_ACTIVATION_ENABLED=1)
else()
    target_compile_definitions(${PROJECT_NAME} PUBLIC BEATCONNECT_ACTIVATION_ENABLED=0)
endif()
