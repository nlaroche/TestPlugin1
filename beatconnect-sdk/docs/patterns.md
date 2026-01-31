# Plugin Development Patterns & Best Practices

This document captures battle-tested patterns from real plugin development. Follow these to avoid common pitfalls.

## Table of Contents

1. [State Management](#state-management)
2. [Parameter Patterns](#parameter-patterns)
3. [DSP Patterns](#dsp-patterns)
4. [Web/JUCE Communication](#webjuce-communication)
5. [Variable Buffer Size Handling](#variable-buffer-size-handling)
6. [Common Pitfalls & Solutions](#common-pitfalls--solutions)

---

## State Management

### State Versioning Pattern

**Problem:** When plugin parameters change between versions, old saved states can cause crashes or undefined behavior.

**Solution:** Add a state version number that resets to defaults on mismatch.

```cpp
// In PluginProcessor.cpp

// Increment this when making breaking changes to parameter structure
static constexpr int kStateVersion = 2;

void MyPluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    // Add state version
    xml->setAttribute("stateVersion", kStateVersion);

    copyXmlToBinary(*xml, destData);
}

void MyPluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
    {
        // Check state version
        int loadedVersion = xmlState->getIntAttribute("stateVersion", 0);
        bool needsReset = (loadedVersion != kStateVersion);

        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

        // Reset specific parameters on version mismatch
        if (needsReset)
        {
            DBG("State version mismatch - resetting to defaults");
            resetToDefaults();
        }
    }
}
```

### Non-Automatable Parameters

**Problem:** Some parameters (like module routing, slot assignments) shouldn't be automated by the DAW but still need to persist with the project.

**Solution:** Use `withAutomatable(false)` attribute.

```cpp
// In createParameterLayout()
auto slotAttributes = juce::AudioParameterChoiceAttributes()
    .withAutomatable(false);

params.push_back(std::make_unique<juce::AudioParameterChoice>(
    juce::ParameterID { ParamIDs::moduleSlot1, 1 },
    "Module Slot 1",
    moduleOptions,
    0,  // default
    slotAttributes  // Non-automatable but still persists
));
```

### UI-Only State Sync Pattern

**Problem:** Some UI state (like which modules are visible) needs to persist but isn't a simple parameter.

**Solution:** Use APVTS choice parameters with relay sync to TypeScript stores.

```typescript
// TypeScript store that syncs with APVTS via relays
function createModuleSlotsStore() {
  const { subscribe, set: rawSet, update } = writable<ModuleId[]>(DEFAULT_SLOTS);

  const syncToNative = (slots: ModuleId[]) => {
    const states = [
      getComboBoxState('module_slot_1'),
      getComboBoxState('module_slot_2'),
      // ...
    ];
    slots.forEach((moduleId, i) => {
      const index = moduleIdToIndex(moduleId);
      states[i].setChoiceIndex(index);
    });
  };

  return {
    subscribe,
    set: (slots) => { rawSet(slots); syncToNative(slots); },
    initFromNative: () => {
      // Read current values from relays on startup
      const states = getSlotStates();
      const slots = states.map(s => indexToModuleId(s.getChoiceIndex()));
      rawSet(slots);

      // Listen for external changes
      states.forEach((state, i) => {
        state.valueChangedEvent.addListener(() => {
          update(currentSlots => {
            const newSlots = [...currentSlots];
            newSlots[i] = indexToModuleId(state.getChoiceIndex());
            return newSlots;
          });
        });
      });
    }
  };
}
```

---

## Parameter Patterns

### Relay Setup Order (Critical!)

**Problem:** WebBrowserComponent initialization fails if relays don't exist.

**Solution:** Always create relays BEFORE WebBrowserComponent.

```cpp
void PluginEditor::setupWebView()
{
    // 1. CREATE RELAYS FIRST
    gainRelay = std::make_unique<juce::WebSliderRelay>("gain");
    modeRelay = std::make_unique<juce::WebComboBoxRelay>("mode");

    // 2. BUILD OPTIONS WITH RELAYS
    auto options = juce::WebBrowserComponent::Options()
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withNativeIntegrationEnabled()
        .withOptionsFrom(*gainRelay)
        .withOptionsFrom(*modeRelay)
        // ...

    // 3. CREATE WEBVIEW LAST
    webView = std::make_unique<juce::WebBrowserComponent>(options);
}

void PluginEditor::setupRelaysAndAttachments()
{
    // 4. CREATE ATTACHMENTS AFTER WEBVIEW
    auto& apvts = processor.getAPVTS();
    gainAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter("gain"), *gainRelay, nullptr);
}
```

### Smoothed Parameter Values in DSP

**Problem:** Abrupt parameter changes cause clicks and pops.

**Solution:** Use `juce::SmoothedValue` with proper reset on bypass.

```cpp
class MyProcessor
{
    juce::SmoothedValue<float> smoothedGain;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        smoothedGain.reset(spec.sampleRate, 0.02);  // 20ms smoothing
        smoothedGain.setCurrentAndTargetValue(gain);
    }

    void process(juce::dsp::ProcessContextReplacing<float>& context)
    {
        if (!enabled)
        {
            // Reset smoothed value when disabled to prevent clicks on re-enable
            smoothedGain.setCurrentAndTargetValue(gain);
            return;
        }

        smoothedGain.setTargetValue(gain);

        for (size_t i = 0; i < numSamples; ++i)
        {
            float currentGain = smoothedGain.getNextValue();
            // Use currentGain for processing
        }
    }
};
```

---

## DSP Patterns

### Proper Bypass Handling

**Problem:** Effects that don't properly bypass still run filters/delays, causing issues.

**Solution:** Early return with state reset.

```cpp
void MyEffect::process(juce::dsp::ProcessContextReplacing<float>& context)
{
    if (!enabled)
    {
        // Reset any smoothed values to prevent clicks
        smoothedAmount.setCurrentAndTargetValue(amount);
        return;  // Audio passes through unchanged
    }

    // Normal processing...
}
```

### Filter Coefficient Updates

**Problem:** Updating filter coefficients every sample is expensive and unnecessary.

**Solution:** Only update when parameters actually change.

```cpp
void MyFilter::process(...)
{
    // Check if update needed (use atomics or compare values)
    float newCutoff = cutoffParam;
    if (std::abs(newCutoff - lastCutoff) > 0.001f)
    {
        updateCoefficients(newCutoff);
        lastCutoff = newCutoff;
    }

    // Process with current coefficients
}
```

### Waveshaper Patterns (Avoiding Crackling)

**Problem:** Hard clipping or discontinuous waveshapers cause harsh artifacts.

**Solution:** Use soft clipping with smooth transitions.

```cpp
// BAD - hard discontinuity
float badClip(float x) {
    return x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x);
}

// GOOD - smooth tanh saturation
float goodSaturate(float x, float drive) {
    return std::tanh(x * drive);
}

// GOOD - polynomial waveshaper (odd harmonics)
float polyShape(float x) {
    return x - (x * x * x) / 3.0f + (x * x * x * x * x) / 5.0f;
}

// GOOD - asymmetric for even harmonics (tube-like)
float asymmetricSaturate(float x, float asymmetry) {
    if (x > 0.0f)
        return std::tanh(x * (1.0f + asymmetry));
    else
        return std::tanh(x * (1.0f - asymmetry * 0.5f));
}
```

---

## Web/JUCE Communication

### Custom Events Pattern

For data that isn't parameter-based (presets, visualizers, activation):

**C++ Side:**
```cpp
// In timerCallback() or when data changes
void PluginEditor::sendVisualizerData()
{
    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    data->setProperty("inputLevel", processor.getInputLevel());
    data->setProperty("outputLevel", processor.getOutputLevel());

    webView->emitEventIfBrowserIsVisible("visualizerData", juce::var(data.get()));
}

// Setup listener for events FROM web
webView->addListener("loadPreset", [this](const juce::var& data) {
    handleLoadPreset(data);
});
```

**TypeScript Side:**
```typescript
// Listen for events from C++
useEffect(() => {
  if (window.__JUCE__?.backend) {
    window.__JUCE__.backend.addEventListener('visualizerData', (data) => {
      setLevels(data);
    });
  }
}, []);

// Send events to C++
function loadPreset(presetId: string) {
  window.__JUCE__?.backend?.emitEvent('loadPreset', { id: presetId });
}
```

---

## Variable Buffer Size Handling

### Problem

Some DAWs (notably FL Studio) use variable buffer sizes during playback. Fixed-size internal buffers can cause issues.

### Solution

Allocate buffers with extra headroom and handle variable sizes gracefully.

```cpp
void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Allocate with 2x headroom for variable buffer sizes
    visualizerBuffer.setSize(2, samplesPerBlock * 2);

    // Also pass max size to DSP modules
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock * 2);
    spec.numChannels = 2;

    myEffect.prepare(spec);
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, ...)
{
    // Safe copy that handles any buffer size
    int samplesToCopy = juce::jmin(buffer.getNumSamples(),
                                    visualizerBuffer.getNumSamples());
    int channelsToCopy = juce::jmin(buffer.getNumChannels(),
                                     visualizerBuffer.getNumChannels());

    for (int ch = 0; ch < channelsToCopy; ++ch)
        visualizerBuffer.copyFrom(ch, 0, buffer, ch, 0, samplesToCopy);
}
```

---

## Common Pitfalls & Solutions

### 1. UI Shows Wrong Module/Effect

**Symptom:** UI displays one thing but audio is different.

**Cause:** State version mismatch from development changes.

**Fix:** Implement state versioning (see above). Increment version when parameters change.

### 2. Crackling with All Effects Off

**Symptom:** Audio crackles even when plugin should be bypassing.

**Causes:**
- Buffer size mismatch
- Visualizer buffer too small
- Filters still processing when "bypassed"

**Fix:**
- Allocate larger internal buffers
- Ensure all processors properly early-return when disabled
- Reset smoothed values on disable

### 3. Parameters Don't Restore on Reload

**Symptom:** Closing and reopening plugin loses settings.

**Cause:** UI state not connected to APVTS parameters.

**Fix:** All persistent state must be APVTS parameters. Use relay system to sync with UI stores.

### 4. Clicks When Toggling Effects

**Symptom:** Audible click when enabling/disabling effects.

**Cause:** Smoothed values jump when re-enabled.

**Fix:** Reset `SmoothedValue` to current target when disabled:
```cpp
if (!enabled) {
    smoothedValue.setCurrentAndTargetValue(targetValue);
    return;
}
```

### 5. WebView Crashes on Load

**Symptom:** Plugin crashes immediately when editor opens.

**Cause:** Relays created after WebBrowserComponent.

**Fix:** Create ALL relays BEFORE WebBrowserComponent constructor.

### 6. Preset Selection Shows "None" After Applying

**Symptom:** Selecting a preset works but UI shows "None selected".

**Cause:** Preset store clears selection after applying (to allow free editing).

**Fix:** Keep the preset ID in store but remove restrictions on editing:
```typescript
// Don't clear selection after applying
set(presetId);

// But don't restrict module interaction
export const presetActiveModules = derived(currentPreset, () => null);
```

### 7. Module Slots Don't Persist

**Symptom:** Module arrangement resets on reload.

**Cause:** Slots stored only in JS state, not APVTS.

**Fix:** Create APVTS choice parameters for slots (non-automatable) and sync via relays.

---

## Checklist: Before Release

- [ ] State version is set and incremented since last release
- [ ] All persistent state is in APVTS parameters
- [ ] All effects properly bypass (early return + state reset)
- [ ] Variable buffer sizes handled (2x headroom)
- [ ] No hardcoded plugin-specific values in SDK code
- [ ] Smoothed values reset on effect disable
- [ ] WebView user data folder uses unique name
- [ ] Production build tested (not just dev mode)
- [ ] Tested in multiple DAWs (especially FL Studio for buffer sizes)
- [ ] Memory profiled for leaks during extended use
