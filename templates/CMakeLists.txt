# ==============================================================================
# BeatConnect Plugin Template - CMakeLists.txt
# ==============================================================================
# Replace {{PLUGIN_NAME}}, {{PLUGIN_NAME_UPPER}}, {{COMPANY_NAME}},
# {{COMPANY_NAME_LOWER}}, {{PLUGIN_CODE}}, {{MANUFACTURER_CODE}}
# ==============================================================================
#
# VALIDATION CHECKLIST (verify before building):
# [ ] beatconnect-sdk submodule initialized (git submodule update --init)
# [ ] JUCE 8.0.4+ available (via JUCE_PATH or will be fetched)
# [ ] All placeholders replaced (search for {{ to verify)
# [ ] Source/ folder has PluginProcessor.cpp/h, PluginEditor.cpp/h, ParameterIDs.h
# [ ] web-ui/ folder has package.json with build script
# [ ] Resources/WebUI/ folder exists (even if empty - CI will populate)
#
# BUILD FLAGS (set by BeatConnect CI):
# - BEATCONNECT_ENABLE_ACTIVATION=ON  : Enable activation key validation
# - JUCE_PATH=/path/to/juce           : Use local JUCE instead of fetching
# - {{PLUGIN_NAME_UPPER}}_DEV_MODE=ON : Use Vite dev server instead of bundled assets
#
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
# BeatConnect Plugin SDK (activation, asset downloads)
# ==============================================================================
# IMPORTANT: SDK must be added BEFORE the plugin target so it can be linked
add_subdirectory(beatconnect-sdk/sdk/activation)
# SDK needs cryptography module for SHA256 hashing
target_link_libraries(beatconnect_activation PRIVATE juce::juce_cryptography)

# ==============================================================================
# Determine Plugin Formats
# ==============================================================================
set(PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

# ==============================================================================
# Plugin Target
# ==============================================================================
juce_add_plugin(${PROJECT_NAME}
    # Basic Info
    COMPANY_NAME "{{COMPANY_NAME}}"
    PLUGIN_MANUFACTURER_CODE {{MANUFACTURER_CODE}}
    PLUGIN_CODE {{PLUGIN_CODE}}

    # Plugin Formats (AU only on macOS)
    FORMATS ${PLUGIN_FORMATS}

    # Plugin Characteristics
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE

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
        # JUCE Configuration
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        # Static linking embeds the WebView2 loader into the binary - no DLL needed at runtime!
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0

        # Dev mode toggle (0 = production, 1 = development)
        $<IF:$<BOOL:${{{PLUGIN_NAME_UPPER}}_DEV_MODE}>,{{PLUGIN_NAME_UPPER}}_DEV_MODE=1,{{PLUGIN_NAME_UPPER}}_DEV_MODE=0>
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
        beatconnect_activation
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ==============================================================================
# Web UI Resources
# ==============================================================================
# Copy WebUI resources after build (for production)
# NOTE: CI builds web-ui/ and copies output to Resources/WebUI/ before C++ build

# Copy to Standalone
add_custom_command(TARGET ${PROJECT_NAME}_Standalone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Resources/WebUI"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}_Standalone>/Resources/WebUI"
    COMMENT "Copying WebUI resources to Standalone..."
)

# Copy to VST3 build output
add_custom_command(TARGET ${PROJECT_NAME}_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Resources/WebUI"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}_VST3>/../Resources/WebUI"
    COMMENT "Copying WebUI resources to VST3..."
)

# Copy to AU build output (macOS only)
if(APPLE)
    add_custom_command(TARGET ${PROJECT_NAME}_AU POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/Resources/WebUI"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}_AU>/../Resources/WebUI"
        COMMENT "Copying WebUI resources to AU..."
    )
endif()

# Also copy WebUI to installed VST3 location (Windows)
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME}_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/Resources/WebUI"
            "$ENV{CommonProgramFiles}/VST3/${PROJECT_NAME}.vst3/Contents/Resources/WebUI"
        COMMENT "Copying WebUI resources to installed VST3..."
    )
endif()

# ==============================================================================
# BeatConnect SDK Integration
# ==============================================================================

# Option to enable BeatConnect activation (set by CI or manually)
option(BEATCONNECT_ENABLE_ACTIVATION "Enable BeatConnect activation key support" OFF)

# Embed project_data.json if it exists (injected at build time by BeatConnect CI)
message(STATUS "=== BeatConnect SDK Configuration ===")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message(STATUS "Looking for project_data.json at: ${CMAKE_SOURCE_DIR}/resources/project_data.json")
message(STATUS "BEATCONNECT_ENABLE_ACTIVATION option: ${BEATCONNECT_ENABLE_ACTIVATION}")

if(EXISTS "${CMAKE_SOURCE_DIR}/resources/project_data.json")
    message(STATUS "Found project_data.json - embedding as BinaryData")
    # Show file info for debugging
    file(SIZE "${CMAKE_SOURCE_DIR}/resources/project_data.json" PROJECT_DATA_SIZE)
    message(STATUS "  File size: ${PROJECT_DATA_SIZE} bytes")
    file(READ "${CMAKE_SOURCE_DIR}/resources/project_data.json" PROJECT_DATA_CONTENT)
    string(LENGTH "${PROJECT_DATA_CONTENT}" PROJECT_DATA_LEN)
    message(STATUS "  Content length: ${PROJECT_DATA_LEN} characters")
    # Show first 200 chars to verify content without leaking sensitive data
    string(SUBSTRING "${PROJECT_DATA_CONTENT}" 0 200 PROJECT_DATA_PREVIEW)
    message(STATUS "  Content preview: ${PROJECT_DATA_PREVIEW}...")

    juce_add_binary_data(${PROJECT_NAME}_ProjectData
        HEADER_NAME "ProjectData.h"
        NAMESPACE ProjectData
        SOURCES
            resources/project_data.json
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_ProjectData)
    target_compile_definitions(${PROJECT_NAME} PUBLIC HAS_PROJECT_DATA=1)
    message(STATUS "  HAS_PROJECT_DATA = 1")
else()
    message(STATUS "No project_data.json found - BeatConnect features will use SDK defaults")
    message(STATUS "  Listing contents of ${CMAKE_SOURCE_DIR}/resources/ (if it exists):")
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources")
        file(GLOB RESOURCES_FILES "${CMAKE_SOURCE_DIR}/resources/*")
        foreach(FILE ${RESOURCES_FILES})
            message(STATUS "    - ${FILE}")
        endforeach()
    else()
        message(STATUS "    Directory does not exist!")
    endif()
    target_compile_definitions(${PROJECT_NAME} PUBLIC HAS_PROJECT_DATA=0)
    message(STATUS "  HAS_PROJECT_DATA = 0")
endif()

# BeatConnect Activation SDK integration (already linked above, just need compile def)
if(BEATCONNECT_ENABLE_ACTIVATION)
    target_compile_definitions(${PROJECT_NAME} PUBLIC BEATCONNECT_ACTIVATION_ENABLED=1)
    message(STATUS "  BEATCONNECT_ACTIVATION_ENABLED = 1 (activation will be compiled in)")
else()
    target_compile_definitions(${PROJECT_NAME} PUBLIC BEATCONNECT_ACTIVATION_ENABLED=0)
    message(STATUS "  BEATCONNECT_ACTIVATION_ENABLED = 0 (activation disabled)")
endif()

message(STATUS "=== End BeatConnect SDK Configuration ===")
