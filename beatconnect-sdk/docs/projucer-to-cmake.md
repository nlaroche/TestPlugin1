# Projucer to CMake: Quick Reference

If you're used to Projucer, here's how your familiar settings map to CMake.

## The Basics

**Your .jucer file** → **CMakeLists.txt**

That's it. One file replaces the other. CMake generates the Xcode/Visual Studio projects for you.

---

## Settings Mapping

### Project Settings (Projucer left panel)

| Projucer Field | CMake Equivalent |
|----------------|------------------|
| Project Name | `project(MyPlugin VERSION 1.0.0)` |
| Project Version | `project(MyPlugin VERSION 1.0.0)` |
| Company Name | `COMPANY_NAME "MyCompany"` |
| Bundle Identifier | `BUNDLE_ID "com.mycompany.MyPlugin"` |

### Plugin Settings

| Projucer Field | CMake Equivalent |
|----------------|------------------|
| Plugin Name | `PRODUCT_NAME "My Plugin"` |
| Plugin Manufacturer Code | `PLUGIN_MANUFACTURER_CODE Myco` |
| Plugin Code | `PLUGIN_CODE Mypl` |
| Plugin is a Synth | `IS_SYNTH TRUE` or `FALSE` |
| Plugin Wants MIDI Input | `NEEDS_MIDI_INPUT TRUE` or `FALSE` |
| Plugin Produces MIDI Output | `NEEDS_MIDI_OUTPUT TRUE` or `FALSE` |
| Plugin is a MIDI Effect | `IS_MIDI_EFFECT TRUE` or `FALSE` |

### Plugin Formats (checkboxes in Projucer)

```cmake
# Projucer: Check VST3, AU, Standalone boxes
# CMake:
FORMATS VST3 AU Standalone
```

### Modules (checkboxes in Projucer)

```cmake
# Projucer: Check juce_audio_utils, juce_dsp, etc.
# CMake:
target_link_libraries(MyPlugin
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_audio_processors
)
```

Common modules:
- `juce::juce_audio_utils` - AudioProcessor, editors
- `juce::juce_audio_processors` - Plugin formats
- `juce::juce_dsp` - DSP classes (filters, delays, etc.)
- `juce::juce_gui_basics` - UI components
- `juce::juce_gui_extra` - WebBrowserComponent (for WebUI)

### Source Files

```cmake
# Projucer: Files shown in the tree view
# CMake:
target_sources(MyPlugin
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)
```

### Preprocessor Definitions

```cmake
# Projucer: Preprocessor Definitions field
# CMake:
target_compile_definitions(MyPlugin
    PUBLIC
        MY_CUSTOM_DEFINE=1
        SOME_FLAG=0
)
```

---

## Minimal CMakeLists.txt for Native UI Plugin

Copy this, replace the obvious parts:

```cmake
cmake_minimum_required(VERSION 3.22)
project(MyPlugin VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)

# Fetch JUCE (no need to download manually)
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
)
FetchContent_MakeAvailable(JUCE)

# Create the plugin
juce_add_plugin(MyPlugin
    COMPANY_NAME "My Company"
    PLUGIN_MANUFACTURER_CODE Myco       # 4 chars, unique to you
    PLUGIN_CODE Mypl                    # 4 chars, unique per plugin
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "My Plugin"
    IS_SYNTH FALSE
)

# Your source files
target_sources(MyPlugin PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

# JUCE modules you use
target_link_libraries(MyPlugin PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
)
```

---

## Building

```bash
# Configure (generates Xcode/VS project)
cmake -B build

# Build
cmake --build build --config Release

# Your plugin is in:
# build/MyPlugin_artefacts/Release/VST3/
# build/MyPlugin_artefacts/Release/AU/        (macOS)
# build/MyPlugin_artefacts/Release/Standalone/
```

---

## FAQ

**Q: Where's my .jucer file?**
A: You don't need it anymore. CMakeLists.txt replaces it.

**Q: How do I add a new source file?**
A: Add it to the `target_sources()` list.

**Q: How do I add a JUCE module?**
A: Add `juce::juce_module_name` to `target_link_libraries()`.

**Q: I get "JUCE not found" errors**
A: The `FetchContent` block downloads JUCE automatically. Make sure you have internet.

**Q: Can I still use Projucer alongside CMake?**
A: Not recommended. Pick one. CMake is what BeatConnect's build system uses.

**Q: What about my custom Xcode/VS settings?**
A: Most can be set via CMake. Ask Claude Code for help with specific settings.

---

## BeatConnect Integration

To use BeatConnect's cloud builds, just add:

```cmake
# Add the SDK (after JUCE, before juce_add_plugin)
include(beatconnect-sdk/cmake/BeatConnectPlugin.cmake)

# After juce_add_plugin and target_sources:
beatconnect_configure_plugin(MyPlugin)
```

That's it. Push to GitHub, link in BeatConnect dashboard, and you get cloud builds.
