# ==============================================================================
# BeatConnect Plugin - Root CMakeLists.txt
# ==============================================================================
# This wrapper CMakeLists.txt sits at the repository root and includes the
# plugin subdirectory. It handles the case where:
#   - beatconnect-sdk/ is at the repo root
#   - plugin/ contains the actual plugin code
#
# This file is used by CI systems that run `cmake -B build -S .` at repo root.
# ==============================================================================

cmake_minimum_required(VERSION 3.22)
project(BeatConnectPluginWrapper LANGUAGES CXX)

# Detect project structure and include the appropriate CMakeLists.txt
if(EXISTS "${CMAKE_SOURCE_DIR}/plugin/CMakeLists.txt")
    # SDK-at-root structure: beatconnect-sdk/ and plugin/ are siblings
    message(STATUS "[BeatConnect] Detected SDK-at-root structure, including plugin/ subdirectory")
    add_subdirectory(plugin)
else()
    message(FATAL_ERROR "[BeatConnect] Could not find plugin/CMakeLists.txt")
endif()
