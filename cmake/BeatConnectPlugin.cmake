# ==============================================================================
# BeatConnectPlugin.cmake
# ==============================================================================
# Shared CMake configuration for BeatConnect plugin projects.
# Include this file to get sensible defaults for both WebUI and native plugins.
#
# Usage:
#   include(path/to/beatconnect-sdk/cmake/BeatConnectPlugin.cmake)
#
#   juce_add_plugin(MyPlugin ...)
#
#   # Apply BeatConnect configuration
#   beatconnect_configure_plugin(MyPlugin)
#
# Options (set BEFORE including this file):
#   BEATCONNECT_USE_WEBUI          - Enable WebView UI (default: auto-detect)
#   BEATCONNECT_ENABLE_ACTIVATION  - Enable license activation (default: OFF)
#   BEATCONNECT_DEV_MODE           - Enable hot reload for WebUI (default: OFF)
#
# ==============================================================================

cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# Options
# ==============================================================================

# Auto-detect WebUI based on directory existence (can be overridden)
if(NOT DEFINED BEATCONNECT_USE_WEBUI)
    if(EXISTS "${CMAKE_SOURCE_DIR}/web-ui" OR EXISTS "${CMAKE_SOURCE_DIR}/web")
        set(BEATCONNECT_USE_WEBUI ON)
        message(STATUS "[BeatConnect] WebUI directory found - enabling WebView support")
    else()
        set(BEATCONNECT_USE_WEBUI OFF)
        message(STATUS "[BeatConnect] No WebUI directory - building native JUCE UI plugin")
    endif()
endif()

option(BEATCONNECT_ENABLE_ACTIVATION "Enable BeatConnect license activation" OFF)
option(BEATCONNECT_DEV_MODE "Enable development mode with hot reload" OFF)

# ==============================================================================
# JUCE Fetch (if not already available)
# ==============================================================================
macro(beatconnect_fetch_juce)
    if(NOT TARGET juce::juce_core)
        include(FetchContent)
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.4
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(JUCE)
        message(STATUS "[BeatConnect] Fetched JUCE 8.0.4")
    endif()
endmacro()

# ==============================================================================
# Main Configuration Function
# ==============================================================================
function(beatconnect_configure_plugin TARGET_NAME)
    # =========================================================================
    # Base compile definitions (all plugins)
    # =========================================================================
    target_compile_definitions(${TARGET_NAME}
        PUBLIC
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
    )

    # =========================================================================
    # WebUI Configuration
    # =========================================================================
    if(BEATCONNECT_USE_WEBUI)
        message(STATUS "[BeatConnect] Configuring ${TARGET_NAME} with WebView UI")

        target_compile_definitions(${TARGET_NAME}
            PUBLIC
                JUCE_WEB_BROWSER=1
                BEATCONNECT_USE_WEBUI=1
        )

        # Platform-specific WebView
        if(WIN32)
            target_compile_definitions(${TARGET_NAME}
                PUBLIC
                    JUCE_USE_WIN_WEBVIEW2=1
            )
        endif()

        if(APPLE)
            target_compile_definitions(${TARGET_NAME}
                PUBLIC
                    JUCE_USE_WKWEBVIEW=1
            )
        endif()

        # Dev mode for hot reload
        string(TOUPPER "${TARGET_NAME}" TARGET_UPPER)
        if(BEATCONNECT_DEV_MODE)
            target_compile_definitions(${TARGET_NAME}
                PUBLIC
                    ${TARGET_UPPER}_DEV_MODE=1
            )
            message(STATUS "[BeatConnect] Dev mode enabled - WebUI will load from localhost:5173")
        else()
            target_compile_definitions(${TARGET_NAME}
                PUBLIC
                    ${TARGET_UPPER}_DEV_MODE=0
            )
        endif()

        # WebUI resource copy commands
        _beatconnect_setup_webui_copy(${TARGET_NAME})
    else()
        message(STATUS "[BeatConnect] Configuring ${TARGET_NAME} with native JUCE UI")
        target_compile_definitions(${TARGET_NAME}
            PUBLIC
                JUCE_WEB_BROWSER=0
                BEATCONNECT_USE_WEBUI=0
        )
    endif()

    # =========================================================================
    # BeatConnect Activation SDK
    # =========================================================================
    _beatconnect_setup_activation(${TARGET_NAME})

    # =========================================================================
    # Project Data (injected by BeatConnect CI)
    # =========================================================================
    _beatconnect_setup_project_data(${TARGET_NAME})

    # =========================================================================
    # Recommended libraries and flags
    # =========================================================================
    target_link_libraries(${TARGET_NAME}
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )

endfunction()

# ==============================================================================
# Internal: Setup WebUI resource copying
# ==============================================================================
function(_beatconnect_setup_webui_copy TARGET_NAME)
    # Determine WebUI source directory
    if(EXISTS "${CMAKE_SOURCE_DIR}/web-ui/dist")
        set(WEBUI_DIST "${CMAKE_SOURCE_DIR}/web-ui/dist")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/web/dist")
        set(WEBUI_DIST "${CMAKE_SOURCE_DIR}/web/dist")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/Resources/WebUI")
        set(WEBUI_DIST "${CMAKE_SOURCE_DIR}/Resources/WebUI")
    else()
        message(WARNING "[BeatConnect] WebUI enabled but no dist directory found. Run 'npm run build' in web-ui/")
        return()
    endif()

    # Copy to Standalone
    if(TARGET ${TARGET_NAME}_Standalone)
        add_custom_command(TARGET ${TARGET_NAME}_Standalone POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${WEBUI_DIST}"
                "$<TARGET_FILE_DIR:${TARGET_NAME}_Standalone>/Resources/WebUI"
            COMMENT "[BeatConnect] Copying WebUI to Standalone..."
        )
    endif()

    # Copy to VST3
    if(TARGET ${TARGET_NAME}_VST3)
        add_custom_command(TARGET ${TARGET_NAME}_VST3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${WEBUI_DIST}"
                "$<TARGET_FILE_DIR:${TARGET_NAME}_VST3>/../Resources/WebUI"
            COMMENT "[BeatConnect] Copying WebUI to VST3..."
        )
    endif()

    # Copy to AU (macOS)
    if(TARGET ${TARGET_NAME}_AU)
        add_custom_command(TARGET ${TARGET_NAME}_AU POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${WEBUI_DIST}"
                "$<TARGET_FILE_DIR:${TARGET_NAME}_AU>/../Resources/WebUI"
            COMMENT "[BeatConnect] Copying WebUI to AU..."
        )
    endif()
endfunction()

# ==============================================================================
# Internal: Setup BeatConnect Activation SDK
# ==============================================================================
function(_beatconnect_setup_activation TARGET_NAME)
    if(BEATCONNECT_ENABLE_ACTIVATION)
        # Find the SDK activation directory
        set(ACTIVATION_PATHS
            "${CMAKE_SOURCE_DIR}/beatconnect-sdk/sdk/activation"
            "${CMAKE_SOURCE_DIR}/../sdk/activation"
            "${CMAKE_SOURCE_DIR}/sdk/activation"
        )

        set(ACTIVATION_FOUND OFF)
        foreach(ACTIVATION_PATH ${ACTIVATION_PATHS})
            if(EXISTS "${ACTIVATION_PATH}/CMakeLists.txt")
                add_subdirectory(${ACTIVATION_PATH} ${CMAKE_BINARY_DIR}/beatconnect_activation)
                target_link_libraries(${TARGET_NAME} PRIVATE beatconnect_activation)
                target_compile_definitions(${TARGET_NAME} PUBLIC BEATCONNECT_ACTIVATION_ENABLED=1)
                set(ACTIVATION_FOUND ON)
                message(STATUS "[BeatConnect] Activation SDK enabled from ${ACTIVATION_PATH}")
                break()
            endif()
        endforeach()

        if(NOT ACTIVATION_FOUND)
            message(WARNING "[BeatConnect] BEATCONNECT_ENABLE_ACTIVATION is ON but SDK not found")
            target_compile_definitions(${TARGET_NAME} PUBLIC BEATCONNECT_ACTIVATION_ENABLED=0)
        endif()
    else()
        target_compile_definitions(${TARGET_NAME} PUBLIC BEATCONNECT_ACTIVATION_ENABLED=0)
    endif()
endfunction()

# ==============================================================================
# Internal: Setup project_data.json (injected by BeatConnect CI)
# ==============================================================================
function(_beatconnect_setup_project_data TARGET_NAME)
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources/project_data.json")
        juce_add_binary_data(${TARGET_NAME}_ProjectData
            HEADER_NAME "ProjectData.h"
            NAMESPACE ProjectData
            SOURCES resources/project_data.json
        )
        target_link_libraries(${TARGET_NAME} PRIVATE ${TARGET_NAME}_ProjectData)
        target_compile_definitions(${TARGET_NAME} PUBLIC HAS_PROJECT_DATA=1)
        message(STATUS "[BeatConnect] Project data embedded from resources/project_data.json")
    else()
        target_compile_definitions(${TARGET_NAME} PUBLIC HAS_PROJECT_DATA=0)
    endif()
endfunction()

# ==============================================================================
# Helper: Get appropriate NEEDS_WEBVIEW2 value for juce_add_plugin
# ==============================================================================
function(beatconnect_get_webview2_setting OUT_VAR)
    if(BEATCONNECT_USE_WEBUI)
        set(${OUT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${OUT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()
