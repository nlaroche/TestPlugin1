# Plugin Architecture Guide

This document explains the Web/JUCE 8 hybrid architecture used by BeatConnect plugins.

## Overview

BeatConnect plugins use a hybrid architecture:
- **C++ (JUCE)** - Audio processing, parameter management, state persistence
- **Web (React)** - User interface, visualizations, controls

This approach enables:
- Rapid UI iteration (hot reload during development)
- Modern UI frameworks (React, Tailwind, etc.)
- Cross-platform consistency
- Easier designer collaboration

## Architecture Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│  Host DAW                                                         │
│  (Ableton, Logic, FL Studio, Cubase, etc.)                       │
└──────────────────────────────────────────────────────────────────┘
                              │
                              │ Audio/MIDI buffers
                              │ Parameter automation
                              ▼
┌──────────────────────────────────────────────────────────────────┐
│  Plugin Process                                                   │
│                                                                   │
│  ┌────────────────────────┐    ┌────────────────────────────────┐│
│  │  PluginProcessor       │    │  PluginEditor                  ││
│  │  (Audio Thread)        │    │  (UI Thread)                   ││
│  │                        │    │                                ││
│  │  • processBlock()      │    │  ┌──────────────────────────┐  ││
│  │  • Parameters (APVTS)  │◄───┼──│  WebBrowserComponent     │  ││
│  │  • State save/load     │    │  │  (WebView2 / WKWebView)  │  ││
│  │                        │    │  │                          │  ││
│  │  DSP:                  │    │  │  React App               │  ││
│  │  • Filters             │    │  │  • Knobs, sliders        │  ││
│  │  • Effects             │    │  │  • Visualizers           │  ││
│  │  • Oscillators         │    │  │  • Metering              │  ││
│  └────────────────────────┘    │  └──────────────────────────┘  ││
│                                └────────────────────────────────┘│
└──────────────────────────────────────────────────────────────────┘
```

## Communication Bridge

### Web → JUCE

The web UI sends messages to JUCE via the `juce` channel:

```typescript
// Web side
function sendToJuce(type: string, data: any) {
  const message = JSON.stringify({ type, data })

  // WebView2 (Windows)
  window.chrome?.webview?.postMessage(message)

  // WKWebView (macOS)
  window.webkit?.messageHandlers?.juce?.postMessage(message)
}

// Example: Update parameter
sendToJuce('setParameter', { id: 'gain', value: 0.75 })
```

### JUCE → Web

JUCE sends messages to the web via `evaluateJavascript`:

```cpp
// JUCE side
void sendToWeb(const juce::String& type, const juce::var& data)
{
    juce::DynamicObject::Ptr message = new juce::DynamicObject();
    message->setProperty("type", type);
    message->setProperty("data", data);

    auto json = juce::JSON::toString(message.get());
    webView->evaluateJavascript(
        "window.postMessage('" + json + "', '*');",
        nullptr
    );
}
```

### Message Types

| Type | Direction | Purpose |
|------|-----------|---------|
| `setParameter` | Web → JUCE | User changed a control |
| `getParameterState` | Web → JUCE | Request current values |
| `ready` | Web → JUCE | UI finished loading |
| `parameterState` | JUCE → Web | Full parameter snapshot |
| `parameterChanged` | JUCE → Web | Single parameter update |

## Parameter Management

### Definition (C++)

Parameters are defined in `createParameterLayout()`:

```cpp
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Float parameter with range and default
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("gain", 1),    // ID and version
        "Gain",                           // Display name
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f                              // Default value
    ));

    // Choice parameter
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("mode", 1),
        "Mode",
        juce::StringArray{"Clean", "Warm", "Hot"},
        0
    ));

    return { params.begin(), params.end() };
}
```

### Reading Values (Audio Thread)

```cpp
void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Atomic read - safe from audio thread
    auto gain = apvts.getRawParameterValue("gain")->load();

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            data[i] *= gain;
    }
}
```

### Handling Changes (UI Thread)

```cpp
// Listen for parameter changes
class MyEditor : public juce::AudioProcessorValueTreeState::Listener
{
    void parameterChanged(const juce::String& paramId, float newValue) override
    {
        // Called on audio thread - dispatch to UI thread
        juce::MessageManager::callAsync([this, paramId, newValue]() {
            sendSingleParameter(paramId, newValue);
        });
    }
};
```

## State Persistence

### Saving State

```cpp
void getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}
```

### Loading State

```cpp
void setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
```

## WebView Setup

### Windows (WebView2)

```cpp
juce::WebBrowserComponent::Options options;
options = options
    .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
    .withWinWebView2Options(
        juce::WebBrowserComponent::Options::WinWebView2{}
            .withUserDataFolder(tempDir.getChildFile("MyPlugin_WebView"))
    );

webView = std::make_unique<juce::WebBrowserComponent>(options);
```

### macOS (WKWebView)

WebKit is used automatically on macOS. No special configuration needed.

## Development Workflow

### Hot Reload (Development)

1. Run web dev server: `cd web && npm run dev`
2. Point WebView to localhost:
   ```cpp
   webView->goToURL("http://localhost:5173");
   ```
3. Edit React code → see changes instantly

### Production Build

1. Build web UI: `npm run build`
2. Bundle with plugin (CMake copies `web/dist/` to plugin bundle)
3. Load from bundled files:
   ```cpp
   auto webDir = pluginBundle.getChildFile("web");
   webView->goToURL(juce::URL(webDir.getChildFile("index.html")));
   ```

## Best Practices

### Thread Safety

- **Never** access APVTS directly from web message handlers
- Use `getRawParameterValue()->load()` for audio thread reads
- Use `setValueNotifyingHost()` from UI thread only
- Use `callAsync` to dispatch to message thread

### Performance

- Minimize message frequency (debounce rapid UI changes)
- Use `requestAnimationFrame` for visualizations
- Cache DOM references in React
- Use CSS transforms for animations (GPU accelerated)

### Memory

- Clean up WebView user data folder on uninstall
- Don't leak JavaScript closures
- Release audio resources in `releaseResources()`
