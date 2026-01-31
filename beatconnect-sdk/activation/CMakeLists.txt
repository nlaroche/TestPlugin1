# ==============================================================================
# BeatConnect Activation SDK
# ==============================================================================
# This library provides license activation and asset downloading for plugins
# distributed through the BeatConnect platform.
#
# Usage in your plugin's CMakeLists.txt:
#   add_subdirectory(beatconnect-sdk/sdk/activation)
#   target_link_libraries(${PROJECT_NAME} PRIVATE beatconnect_activation)
# ==============================================================================

cmake_minimum_required(VERSION 3.15)
project(beatconnect_activation VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# Source Files
# ==============================================================================

set(BEATCONNECT_SDK_SOURCES
    src/Activation.cpp
    src/MachineId.cpp
    src/AssetDownloader.cpp
)

set(BEATCONNECT_SDK_HEADERS
    include/beatconnect/Activation.h
    include/beatconnect/MachineId.h
    include/beatconnect/AssetDownloader.h
)

# ==============================================================================
# Library Target
# ==============================================================================

add_library(beatconnect_activation STATIC
    ${BEATCONNECT_SDK_SOURCES}
    ${BEATCONNECT_SDK_HEADERS}
)

target_include_directories(beatconnect_activation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# ==============================================================================
# JUCE Integration (Optional but Recommended)
# ==============================================================================
# The SDK uses JUCE for HTTP client and JSON parsing when available.
# If JUCE is not found, you'll need to implement standalone HTTP/JSON.

if(TARGET juce::juce_core)
    target_link_libraries(beatconnect_activation
        PRIVATE
            juce::juce_core
    )
    target_compile_definitions(beatconnect_activation
        PRIVATE
            BEATCONNECT_USE_JUCE=1
    )
    message(STATUS "BeatConnect SDK: Using JUCE for HTTP/JSON")
else()
    message(WARNING "BeatConnect SDK: JUCE not found. SDK requires JUCE for HTTP client.")
endif()

# ==============================================================================
# Platform-Specific Libraries
# ==============================================================================

if(WIN32)
    target_link_libraries(beatconnect_activation
        PRIVATE
            advapi32  # For CryptoAPI (SHA-256)
    )
elseif(APPLE)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_link_libraries(beatconnect_activation
        PRIVATE
            ${IOKIT_FRAMEWORK}
            ${COREFOUNDATION_FRAMEWORK}
    )
endif()

# ==============================================================================
# Compiler Flags
# ==============================================================================

if(MSVC)
    target_compile_options(beatconnect_activation PRIVATE /W4)
else()
    target_compile_options(beatconnect_activation PRIVATE -Wall -Wextra)
endif()
