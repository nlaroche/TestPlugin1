# CLAUDE.md - BeatConnect Plugin SDK

This file provides instructions for Claude Code when creating or working with VST3 plugins that integrate with the BeatConnect platform.

## Overview

This SDK enables building VST3/AU plugins that use BeatConnect's:
- **Build Pipeline** - Automated compilation and signing (Azure/Apple)
- **Activation System** - License codes with machine limits
- **Distribution** - Signed downloads, Stripe payments

## Quick Start - Creating a New Plugin

When a user asks to create a new BeatConnect-integrated plugin:

### 1. Create the Repository Structure

```
my-plugin/
├── CMakeLists.txt              # Main CMake config (see templates/)
├── CLAUDE.md                   # Copy from this repo's templates/
├── .github/
│   └── workflows/
│       └── build.yml           # CI workflow (see templates/)
├── src/
│   ├── PluginProcessor.cpp     # Audio processing
│   ├── PluginProcessor.h
│   ├── PluginEditor.cpp        # GUI (WebView-based)
│   └── PluginEditor.h
├── web/                        # Web UI (React/TypeScript)
│   ├── package.json
│   ├── vite.config.ts
│   ├── tsconfig.json
│   └── src/
│       ├── App.tsx
│       ├── main.tsx
│       └── index.css
└── beatconnect-sdk/            # Submodule (this repo)
```

### 2. Key Architecture Decisions

**Web/JUCE 8 Hybrid Pattern:**
- UI is built with React/TypeScript, served via JUCE's WebBrowserComponent
- Audio processing in C++ (PluginProcessor)
- Communication via postMessage/handleMessage bridge
- This enables rapid UI iteration without recompilation

**Parameter Binding:**
- Parameters defined in C++ (AudioProcessorValueTreeState)
- Web UI reads/writes via message bridge
- Real-time safe: UI sends changes, processor applies atomically

### 3. CMakeLists.txt Template

```cmake
cmake_minimum_required(VERSION 3.22)

project(MyPlugin VERSION 1.0.0)

# JUCE 8 (fetch or use local)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
)
FetchContent_MakeAvailable(JUCE)

# Plugin target
juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME "MyCompany"
    PLUGIN_MANUFACTURER_CODE Bcnn
    PLUGIN_CODE Mypg
    FORMATS VST3 AU
    PRODUCT_NAME "${PROJECT_NAME}"

    # Web UI
    NEEDS_WEB_BROWSER TRUE
)

target_sources(${PROJECT_NAME}
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
)

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Copy web assets to build output
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/web/dist
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/web
)
```

### 4. PluginProcessor Pattern

```cpp
// PluginProcessor.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class MyPluginProcessor : public juce::AudioProcessor
{
public:
    MyPluginProcessor();
    ~MyPluginProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // Parameters
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

private:
    juce::AudioProcessorValueTreeState apvts;

    // Add your DSP members here

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MyPluginProcessor)
};
```

### 5. PluginEditor Pattern (WebView)

```cpp
// PluginEditor.h
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class MyPluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit MyPluginEditor(MyPluginProcessor&);
    ~MyPluginEditor() override;

    void resized() override;

private:
    MyPluginProcessor& processor;
    juce::WebBrowserComponent webView;

    void handleWebMessage(const juce::String& message);
    void sendToWeb(const juce::String& type, const juce::var& data);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MyPluginEditor)
};

// PluginEditor.cpp
MyPluginEditor::MyPluginEditor(MyPluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(800, 600);

    // Set up WebView message handling
    webView.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    addAndMakeVisible(webView);
}

void MyPluginEditor::resized()
{
    webView.setBounds(getLocalBounds());
}
```

### 6. Web UI Pattern (React)

```tsx
// src/App.tsx
import { useEffect, useState } from 'react'

// Bridge to JUCE
declare global {
  interface Window {
    juce?: {
      postMessage: (msg: string) => void
    }
  }
}

function sendToJuce(type: string, data: any) {
  window.juce?.postMessage(JSON.stringify({ type, data }))
}

function App() {
  const [gain, setGain] = useState(0.5)

  // Listen for messages from JUCE
  useEffect(() => {
    const handler = (event: MessageEvent) => {
      try {
        const msg = JSON.parse(event.data)
        if (msg.type === 'parameterChanged') {
          // Handle parameter updates from JUCE
        }
      } catch {}
    }
    window.addEventListener('message', handler)
    return () => window.removeEventListener('message', handler)
  }, [])

  const handleGainChange = (value: number) => {
    setGain(value)
    sendToJuce('setParameter', { id: 'gain', value })
  }

  return (
    <div className="plugin-ui">
      <h1>My Plugin</h1>
      <input
        type="range"
        min={0}
        max={1}
        step={0.01}
        value={gain}
        onChange={(e) => handleGainChange(parseFloat(e.target.value))}
      />
    </div>
  )
}

export default App
```

## BeatConnect Integration

### Activation SDK (Coming Soon)

The SDK will include C++ code for activation:

```cpp
#include <beatconnect/Activation.h>

// In your plugin startup
beatconnect::Activation::Config config;
config.apiBaseUrl = "https://your-supabase-url.supabase.co";
config.pluginId = "your-project-uuid";

auto& activation = beatconnect::Activation::getInstance();
activation.configure(config);

// Check activation
if (!activation.isActivated()) {
    // Show activation dialog
}

// Activate with code
auto status = activation.activate("XXXX-XXXX-XXXX-XXXX");
```

### CI/CD Integration

1. Register your plugin as an "external project" in BeatConnect
2. Get your `PROJECT_ID` and `WEBHOOK_SECRET`
3. Add to GitHub secrets:
   - `BEATCONNECT_PROJECT_ID`
   - `BEATCONNECT_WEBHOOK_SECRET`
   - `BEATCONNECT_API_URL`

4. Use the workflow template from `templates/.github/workflows/build.yml`

## File Templates

All templates are in the `templates/` directory:
- `templates/CMakeLists.txt` - Full CMake configuration
- `templates/src/` - C++ source templates
- `templates/web/` - React/Vite web UI template
- `templates/.github/workflows/build.yml` - CI workflow
- `templates/CLAUDE.md` - Instructions for the new plugin repo

## Creating a New Plugin (Step by Step)

When scaffolding a new plugin, follow these steps:

1. **Create repo structure** (use `templates/` as base)
2. **Update identifiers**:
   - Plugin name in CMakeLists.txt
   - PLUGIN_CODE (4 chars, unique)
   - Company name
3. **Implement DSP** in PluginProcessor.cpp
4. **Build web UI** in web/ directory
5. **Register with BeatConnect** to get PROJECT_ID
6. **Configure CI** with secrets
7. **Tag release** to trigger build

## Common Patterns

### Parameter Definition

```cpp
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "gain",           // ID
        "Gain",           // Name
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f              // Default
    ));

    return { params.begin(), params.end() };
}
```

### DSP Processing

```cpp
void MyPluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto gain = apvts.getRawParameterValue("gain")->load();

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] *= gain;
        }
    }
}
```

### Web Message Bridge

```cpp
// Send parameter state to web
void MyPluginEditor::sendParameterState()
{
    juce::DynamicObject::Ptr params = new juce::DynamicObject();

    for (auto* param : processor.getAPVTS().processor.getParameters())
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(param))
            params->setProperty(p->getParameterID(), p->getValue());
    }

    sendToWeb("parameterState", params.get());
}
```

## Troubleshooting

### Build Issues
- Ensure JUCE 8.0.4+ is used
- Check CMake version (3.22+)
- Web assets must be built before plugin (`npm run build` in web/)

### WebView Issues
- Check browser permissions in JUCE
- Ensure web/dist exists after build
- Test web UI in regular browser first

### Activation Issues
- Verify PROJECT_ID matches BeatConnect
- Check API URL is correct
- Ensure machine ID is stable
