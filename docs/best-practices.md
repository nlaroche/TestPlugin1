# Best Practices & Validation Checklist

This document covers common issues encountered during plugin development and their solutions. Use the validation checklist at the end before releasing any plugin.

---

## Table of Contents

1. [Display Scaling](#display-scaling)
2. [State Management](#state-management)
3. [User Preset System](#user-preset-system)
4. [Activation SDK Usage](#activation-sdk-usage)
5. [DSP Best Practices](#dsp-best-practices)
6. [WebView UI Best Practices](#webview-ui-best-practices)
7. [Validation Checklist](#validation-checklist)

---

## Display Scaling

### Problem

On Windows with display scaling (125%, 150%, etc.), the plugin UI can appear incorrectly sized or clipped. The WebView may render at the wrong scale, causing UI bugs.

### Solution

Force consistent scaling in the `PluginEditor` constructor by calling `setScaleFactor(1.0f)` **before** `setSize()`:

```cpp
// PluginEditor.cpp - Constructor
MyPluginEditor::MyPluginEditor(MyPluginProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setupWebView();
    setupRelaysAndAttachments();

    // CRITICAL: Force consistent scaling regardless of OS display scaling
    // This prevents UI bugs on monitors with 125%, 150%, etc. scaling
    setScaleFactor(1.0f);

    // Now set the size
    setSize(900, 500);
    setResizable(false, false);
}
```

### Why This Works

`setScaleFactor(1.0f)` tells JUCE to handle all DPI scaling internally rather than relying on the OS. This ensures your plugin renders at the exact pixel dimensions you specify, regardless of the user's display settings.

---

## State Management

### Problem

Plugin state (parameters, settings) must persist correctly when:
- The DAW saves/loads a project
- The DAW duplicates a track
- The user saves/loads presets

### Solution

Implement `getStateInformation` and `setStateInformation` with versioning:

```cpp
// PluginProcessor.cpp

static constexpr int CURRENT_STATE_VERSION = 1;

void MyPluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Get APVTS state
    auto state = apvts.copyState();

    // Add version for future compatibility
    state.setProperty("stateVersion", CURRENT_STATE_VERSION, nullptr);

    // Optional: Add any non-parameter state
    state.setProperty("pedalOrder", pedalOrderToString(), nullptr);

    // Serialize to XML
    auto xml = state.createXml();
    copyXmlToBinary(*xml, destData);
}

void MyPluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);

    if (xml && xml->hasTagName(apvts.state.getType()))
    {
        auto state = juce::ValueTree::fromXml(*xml);

        // Check version for migrations
        int version = state.getProperty("stateVersion", 0);

        if (version < CURRENT_STATE_VERSION)
        {
            migrateState(state, version);
        }

        // Restore APVTS state
        apvts.replaceState(state);

        // Restore non-parameter state
        if (state.hasProperty("pedalOrder"))
        {
            restorePedalOrder(state.getProperty("pedalOrder").toString());
        }
    }
}
```

### Key Points

- Always include a version number for future migrations
- Use `replaceState()` for atomic state restoration
- Store non-parameter state (like UI layout) alongside APVTS state
- Test state save/load thoroughly with different DAWs

---

## User Preset System

### Overview

User presets allow users to save, load, rename, and delete their own parameter configurations. This is separate from DAW state management and factory presets.

### Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  C++ (PluginEditor)                                             │
│  ┌───────────────┐    ┌──────────────────────────────────────┐  │
│  │ PresetManager │    │ WebView Event Handlers               │  │
│  │               │◄───┤ • getPresetList                      │  │
│  │ • Save        │    │ • saveUserPreset                     │  │
│  │ • Load        │    │ • loadUserPreset                     │  │
│  │ • Rename      │    │ • renameUserPreset                   │  │
│  │ • Delete      │    │ • deleteUserPreset                   │  │
│  └───────────────┘    └──────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼ WebView Events
┌─────────────────────────────────────────────────────────────────┐
│  React UI                                                        │
│  ┌────────────────┐    ┌───────────────────────────────────────┐│
│  │ usePresets()   │    │ PresetSelector                        ││
│  │                │───►│ • Factory Presets (read-only)         ││
│  │ • factory[]    │    │ • User Presets (editable)             ││
│  │ • user[]       │    │ • Save/Rename/Delete actions          ││
│  │ • actions      │    └───────────────────────────────────────┘│
│  └────────────────┘                                              │
└─────────────────────────────────────────────────────────────────┘
```

### Storage Location

User presets are stored as XML files:
- **Windows**: `%APPDATA%/YourPluginName/UserPresets/`
- **macOS**: `~/Library/Application Support/YourPluginName/UserPresets/`

### Implementation

**PresetManager.h:**

```cpp
class PresetManager
{
public:
    PresetManager(juce::AudioProcessorValueTreeState& apvts);

    juce::StringArray getFactoryPresetNames() const;
    juce::StringArray getUserPresetNames() const;

    bool saveUserPreset(const juce::String& name);
    bool loadPreset(const juce::String& name, bool isFactory);
    bool renameUserPreset(const juce::String& oldName, const juce::String& newName);
    bool deleteUserPreset(const juce::String& name);

    juce::String getPresetListAsJson() const;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::File userPresetsDir;

    juce::File getPresetFile(const juce::String& name) const;
    juce::String sanitizeFileName(const juce::String& name) const;
};
```

**Loading User Presets - Critical Detail:**

When loading a user preset, do NOT restore the factory preset index parameter, or it will override your user preset values:

```cpp
bool PresetManager::loadPreset(const juce::String& name, bool isFactory)
{
    if (isFactory)
    {
        // Factory presets: Just set the preset parameter
        if (auto* param = apvts.getParameter("preset"))
        {
            int index = factoryPresetNames.indexOf(name);
            param->setValueNotifyingHost(param->convertTo0to1((float)index));
        }
        return true;
    }

    // User presets: Load from file
    auto xml = juce::XmlDocument::parse(getPresetFile(name));
    if (!xml) return false;

    auto state = juce::ValueTree::fromXml(*xml);

    // CRITICAL: Skip the "preset" parameter to avoid triggering factory preset
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild(i);
        if (child.hasType("PARAM"))
        {
            auto paramId = child.getProperty("id").toString();

            // Skip the factory preset selector
            if (paramId == "preset")
                continue;

            auto value = (float)child.getProperty("value");
            if (auto* param = apvts.getParameter(paramId))
            {
                param->setValueNotifyingHost(value);
            }
        }
    }

    return true;
}
```

**React Hook (usePresets.ts):**

```typescript
export function usePresets() {
  const [factoryPresets, setFactoryPresets] = useState<Preset[]>([]);
  const [userPresets, setUserPresets] = useState<Preset[]>([]);
  const [currentPreset, setCurrentPreset] = useState<string>('');
  const [isCurrentFactory, setIsCurrentFactory] = useState(true);

  useEffect(() => {
    // Request preset list on mount
    window.__JUCE__?.backend.emitEvent('getPresetList', {});

    // Listen for preset list updates
    const cleanup = window.__JUCE__?.backend.addEventListener(
      'presetList',
      (data: PresetListData) => {
        setFactoryPresets(data.factory || []);
        setUserPresets(data.user || []);
      }
    );

    return cleanup;
  }, []);

  const savePreset = (name: string) => {
    window.__JUCE__?.backend.emitEvent('saveUserPreset', { name });
  };

  const loadPreset = (name: string, isFactory: boolean) => {
    window.__JUCE__?.backend.emitEvent('loadUserPreset', { name, isFactory });
    setCurrentPreset(name);
    setIsCurrentFactory(isFactory);
  };

  // ... rename, delete similarly

  return {
    factoryPresets,
    userPresets,
    currentPreset,
    isCurrentFactory,
    savePreset,
    loadPreset,
    renamePreset,
    deletePreset,
  };
}
```

---

## Activation SDK Usage

### Problem: Static Singletons

The old singleton pattern for Activation causes issues when multiple plugin instances exist:

```cpp
// BAD - Don't do this!
auto& activation = beatconnect::Activation::getInstance();  // Static singleton
```

Problems:
- Multiple plugin instances share state incorrectly
- Destructor order issues on plugin unload
- Memory leaks

### Solution: Instance-Based Activation

Each processor should own its own Activation instance:

**PluginProcessor.h:**

```cpp
#if BEATCONNECT_ACTIVATION_ENABLED
#include <beatconnect/Activation.h>
#endif

class MyPluginProcessor : public juce::AudioProcessor
{
public:
    // ...

#if BEATCONNECT_ACTIVATION_ENABLED
    beatconnect::Activation* getActivation() { return activation_.get(); }
    bool hasActivation() const { return activation_ != nullptr; }
#endif

private:
#if BEATCONNECT_ACTIVATION_ENABLED
    std::unique_ptr<beatconnect::Activation> activation_;
#endif
};
```

**PluginProcessor.cpp:**

```cpp
MyPluginProcessor::MyPluginProcessor()
{
#if BEATCONNECT_ACTIVATION_ENABLED
    activation_ = std::make_unique<beatconnect::Activation>();

    beatconnect::ActivationConfig config;
    config.apiBaseUrl = getApiBaseUrl();
    config.pluginId = getPluginId();
    config.supabaseKey = getSupabaseKey();

    activation_->configure(config);
#endif
}
```

**PluginEditor.cpp:**

```cpp
void MyPluginEditor::checkActivation()
{
#if BEATCONNECT_ACTIVATION_ENABLED
    if (processorRef.hasActivation())
    {
        auto* activation = processorRef.getActivation();
        if (!activation->isActivated())
        {
            showActivationDialog();
        }
    }
#endif
}
```

---

## DSP Best Practices

### Filter State Reset on Effect Enable

**Problem:** When toggling effects on/off, IIR filters retain stale state that causes audible pops.

**Solution:** Reset filter state when enabling an effect:

```cpp
void MyPluginProcessor::processOverdrive(juce::AudioBuffer<float>& buffer)
{
    bool isEnabled = apvts.getRawParameterValue("overdrive_enabled")->load() >= 0.5f;

    // Reset filters when enabling to prevent pops from stale state
    if (isEnabled && !wasOverdriveEnabled)
    {
        overdriveInputHP.reset();
        overdriveDCBlock.reset();
        overdriveToneFilter.reset();
    }
    wasOverdriveEnabled = isEnabled;

    if (!isEnabled)
        return;

    // ... process audio
}
```

**Header:**

```cpp
private:
    bool wasOverdriveEnabled = false;
    bool wasFuzzEnabled = false;
    // ... for each toggleable effect
```

### Parameter Smoothing

Avoid clicks when parameters change rapidly:

```cpp
// Header
juce::SmoothedValue<float> smoothedGain;

// prepareToPlay
smoothedGain.reset(sampleRate, 0.02);  // 20ms smoothing

// processBlock
smoothedGain.setTargetValue(targetGain);
for (int i = 0; i < numSamples; ++i)
{
    float gain = smoothedGain.getNextValue();
    // ... apply gain
}
```

### DC Blocking

Always apply DC blocking after waveshapers/saturators:

```cpp
// prepareToPlay
auto dcBlockCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(
    sampleRate, 20.0f);  // 20 Hz high-pass
dcBlockFilter.coefficients = dcBlockCoeffs;

// After saturation
output = dcBlockFilter.processSample(saturatedSample);
```

---

## WebView UI Best Practices

### Section Height Consistency

**Problem:** When switching between views (e.g., pedals vs. visualizer), height differences cause jarring UI shifts.

**Solution:** Set consistent min-height on both sections:

```css
.pedals-section {
  min-height: 195px;
  /* ... other styles */
}

.visualizer-wrapper {
  min-height: 195px;  /* Must match pedals-section */
  /* ... other styles */
}
```

### Non-Interactive Displays

**Problem:** Users try to click on visualizer displays thinking they're interactive controls.

**Solution:** Use clear labeling and disable pointer events:

```tsx
// Component
<div className="viz-panel viz-panel-eq">
  <div className="viz-panel-label">Tone Stack · Display</div>
  <div className="viz-panel-content">
    <canvas ref={canvasRef} className="eq-canvas" />
  </div>
</div>
```

```css
/* Styles */
.eq-canvas {
  pointer-events: none;
  cursor: default;
}

.viz-panel-eq .viz-panel-content {
  background: rgba(0, 0, 0, 0.03);  /* Subtle screen effect */
}
```

### Dropdown Indicators

Always include visual indicators for dropdowns:

```tsx
<button className="preset-button" onClick={() => setIsOpen(!isOpen)}>
  <span className="preset-button-name">{currentPreset}</span>
  <motion.span
    className="preset-button-arrow"
    animate={{ rotate: isOpen ? 180 : 0 }}
  >
    ▼
  </motion.span>
</button>
```

---

## Validation Checklist

Use this checklist before releasing any plugin:

### Display & Scaling

- [ ] `setScaleFactor(1.0f)` is called before `setSize()` in PluginEditor constructor
- [ ] UI renders correctly at 100%, 125%, and 150% Windows display scaling
- [ ] UI renders correctly on Retina (2x) macOS displays

### State Management

- [ ] `getStateInformation()` saves all parameters
- [ ] `setStateInformation()` restores all parameters correctly
- [ ] State includes a version number for future migrations
- [ ] Non-parameter state (UI layout, etc.) is saved/restored
- [ ] DAW project save/load works (test with Ableton, Logic, FL Studio)
- [ ] Track duplication preserves state correctly
- [ ] Plugin bypass doesn't lose state

### Preset System

- [ ] Factory presets load correctly
- [ ] User presets save to correct directory
- [ ] User presets load without triggering factory presets
- [ ] Preset rename works and updates file on disk
- [ ] Preset delete removes file from disk
- [ ] Preset names are sanitized (no invalid filesystem characters)
- [ ] Preset selector shows dropdown arrow indicator

### Activation (if enabled)

- [ ] Activation uses instance-based pattern (not static singleton)
- [ ] Each plugin instance has its own Activation object
- [ ] Activation status persists across plugin reload
- [ ] Deactivation works correctly
- [ ] Offline mode handles gracefully

### DSP & Audio

- [ ] No pops/clicks when enabling/disabling effects
- [ ] Filter states reset on effect enable
- [ ] Parameters are smoothed to prevent zipper noise
- [ ] DC blocking applied after saturation/waveshaping
- [ ] No denormals (flush-to-zero enabled)
- [ ] Sample rate changes handled in `prepareToPlay()`
- [ ] Buffer size changes handled correctly

### WebView UI

- [ ] Section heights are consistent (no UI jumping)
- [ ] All dropdowns have visual indicators
- [ ] Non-interactive displays are clearly labeled
- [ ] Modals/dialogs are custom-styled (no browser alerts)
- [ ] Loading states handled gracefully

### Cross-Platform

- [ ] Builds successfully on Windows (VS 2022)
- [ ] Builds successfully on macOS (Xcode 14+)
- [ ] VST3 format works on Windows
- [ ] VST3 and AU formats work on macOS
- [ ] WebView2 resources bundled correctly (Windows)

### Memory & Performance

- [ ] No memory leaks (test with track creation/deletion)
- [ ] CPU usage reasonable (< 5% idle)
- [ ] WebView user data cleaned up appropriately
- [ ] Resources released in `releaseResources()`

---

## Quick Reference

### Common Fixes

| Issue | Solution |
|-------|----------|
| UI wrong size on scaled displays | Add `setScaleFactor(1.0f)` before `setSize()` |
| Pop when enabling effect | Reset filter states on enable transition |
| User preset loads factory preset | Skip "preset" parameter when loading user preset |
| Multiple instances share activation | Use instance-based Activation, not singleton |
| UI shifts when changing views | Set matching `min-height` on all views |
| Users click on visualizer | Add "Display" label and `pointer-events: none` |
| Dropdown missing indicator | Add arrow character (▼) to dropdown button |
