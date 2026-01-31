# CLAUDE.md - BeatConnect Plugin SDK

This file provides instructions for Claude Code when creating or working with VST3/AU plugins that integrate with the BeatConnect platform.

## Project Structure

This repo has a clear separation between the SDK and your plugin:

```
repo-root/
├── beatconnect-sdk/              # SDK (don't modify)
│   ├── cmake/                    # CMake helpers
│   │   └── BeatConnectPlugin.cmake
│   ├── activation/               # License activation SDK
│   ├── example-plugin/           # Reference implementation (WebView UI)
│   ├── example-plugin-native/    # Reference implementation (Native JUCE UI)
│   ├── templates/                # File templates for new plugins
│   └── docs/                     # Documentation
├── plugin/                       # YOUR PLUGIN CODE GOES HERE
│   ├── CMakeLists.txt
│   ├── Source/
│   │   ├── PluginProcessor.cpp/h
│   │   ├── PluginEditor.cpp/h
│   │   └── ParameterIDs.h
│   ├── web-ui/                   # React/TypeScript UI
│   └── Resources/WebUI/          # Built web assets (CI populates)
├── .claude/skills/               # Claude Code skills
├── CLAUDE.md                     # This file
└── .gitignore
```

## Quick Start

Use the `/bc-create-plugin` skill to create a plugin interactively. It will:
1. Ask what you want to build (delay, reverb, distortion, etc.)
2. Scaffold the code in `plugin/`
3. Help you build and test

## Architecture Overview

BeatConnect plugins use a **Web/JUCE 8 Hybrid Architecture**:
- **C++ (JUCE 8)** - Audio processing, parameter management, state persistence
- **React/TypeScript** - User interface rendered in WebView2 (Windows) / WKWebView (macOS)
- **JUCE 8 Relay System** - Native bidirectional parameter sync (NOT postMessage!)

## CMakeLists.txt - The Simple Way

Your plugin's CMakeLists.txt should use the SDK's cmake helper:

```cmake
cmake_minimum_required(VERSION 3.22)
project(MyPlugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# BeatConnect Configuration
option(MYPLUGIN_DEV_MODE "Enable development mode" OFF)
set(BEATCONNECT_DEV_MODE ${MYPLUGIN_DEV_MODE})

# Include BeatConnect CMake helpers
include(${CMAKE_SOURCE_DIR}/../beatconnect-sdk/cmake/BeatConnectPlugin.cmake)

# Fetch JUCE
beatconnect_fetch_juce()

# Plugin target
juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME "BeatConnect"
    PLUGIN_MANUFACTURER_CODE Beat
    PLUGIN_CODE Mypl
    FORMATS VST3 Standalone
    NEEDS_WEBVIEW2 TRUE
    # ... other options
)

# Source files
target_sources(${PROJECT_NAME} PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    # ...
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
)

# Apply BeatConnect configuration (handles WebUI, activation, etc.)
beatconnect_configure_plugin(${PROJECT_NAME})
```

The `beatconnect_configure_plugin()` function handles:
- WebView compile definitions
- Dev mode toggle
- WebUI resource copying
- Activation SDK linking (if enabled)
- Project data embedding (if present)

## CRITICAL: PluginEditor Constructor Order

**THIS IS THE #1 CAUSE OF BLACK SCREEN ISSUES.**

```cpp
MyPluginEditor::MyPluginEditor(MyPluginProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // CRITICAL ORDER - DO NOT CHANGE:
    // 1. setupWebView() - creates relays AND WebBrowserComponent
    // 2. setupAttachments() - connects relays to APVTS
    // 3. setSize() - MUST be AFTER WebView exists

    setupWebView();
    setupRelaysAndAttachments();

    setSize(800, 500);  // AFTER WebView!
    setResizable(false, false);

    startTimerHz(30);
}
```

## JUCE 8 Relay System

| Relay Type | Parameter Type | C++ Class | TypeScript |
|------------|---------------|-----------|------------|
| Slider | Float | `WebSliderRelay` | `getSliderState()` |
| Toggle | Bool | `WebToggleButtonRelay` | `getToggleState()` |
| ComboBox | Choice | `WebComboBoxRelay` | `getComboBoxState()` |

### C++ Setup (PluginEditor.cpp)

```cpp
void MyPluginEditor::setupWebView()
{
    // 1. Create relays FIRST
    gainRelay = std::make_unique<juce::WebSliderRelay>("gain");
    bypassRelay = std::make_unique<juce::WebToggleButtonRelay>("bypass");

    // 2. Build WebView options with relays
    auto options = juce::WebBrowserComponent::Options()
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withNativeIntegrationEnabled()
        .withResourceProvider([this](const juce::String& url) { /* ... */ })
        .withOptionsFrom(*gainRelay)
        .withOptionsFrom(*bypassRelay);

    webView = std::make_unique<juce::WebBrowserComponent>(options);
    addAndMakeVisible(*webView);

#if MYPLUGIN_DEV_MODE
    webView->goToURL("http://localhost:5173");
#else
    webView->goToURL(webView->getResourceProviderRoot());
#endif
}
```

### TypeScript Setup (App.tsx)

```tsx
import { useSliderParam, useToggleParam } from './hooks/useJuceParam';

function App() {
    const gain = useSliderParam('gain', { defaultValue: 0.5 });
    const bypass = useToggleParam('bypass');

    return (
        <input
            type="range"
            value={gain.value}
            onMouseDown={gain.onDragStart}
            onMouseUp={gain.onDragEnd}
            onChange={(e) => gain.setValue(parseFloat(e.target.value))}
        />
    );
}
```

## Development Workflow

```bash
# 1. Install web dependencies
cd plugin/web-ui && npm install

# 2. Start dev server (for hot reload)
npm run dev

# 3. In another terminal, build the plugin
cd plugin
cmake -B build -DMYPLUGIN_DEV_MODE=ON
cmake --build build

# 4. Run standalone
./build/MyPlugin_artefacts/Debug/Standalone/MyPlugin
```

## Reference Implementation

**Always compare your code against `beatconnect-sdk/example-plugin/`** to ensure patterns match exactly.

## Common Mistakes

### Black Screen
1. `setSize()` called before `setupWebView()`
2. Creating WebBrowserComponent before relays
3. Attachments created before WebBrowserComponent

### Parameters Not Syncing
4. Mismatched identifiers ("gain" vs "Gain")
5. Not calling `dragStart`/`dragEnd`
6. Using postMessage instead of relay system

### Build Issues
7. Forgetting `NEEDS_WEBVIEW2 TRUE`
8. Wrong path to BeatConnectPlugin.cmake

## File Locations

| What | Where |
|------|-------|
| Your plugin code | `plugin/` |
| CMake helpers | `beatconnect-sdk/cmake/` |
| Reference example | `beatconnect-sdk/example-plugin/` |
| Templates | `beatconnect-sdk/templates/` |
| Activation SDK | `beatconnect-sdk/activation/` |
| Documentation | `beatconnect-sdk/docs/` |

## More Documentation

- Patterns and troubleshooting: `beatconnect-sdk/docs/patterns.md`
- Projucer migration: `beatconnect-sdk/docs/projucer-to-cmake.md`
