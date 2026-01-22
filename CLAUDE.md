# CLAUDE.md - BeatConnect Plugin SDK

This file provides instructions for Claude Code when creating or working with VST3/AU plugins that integrate with the BeatConnect platform.

## Architecture Overview

BeatConnect plugins use a **Web/JUCE 8 Hybrid Architecture**:
- **C++ (JUCE 8)** - Audio processing, parameter management, state persistence
- **React/TypeScript** - User interface rendered in WebView2 (Windows) / WKWebView (macOS)
- **JUCE 8 Relay System** - Native bidirectional parameter sync (NOT postMessage!)

This enables:
- Hot reload during development (edit React, see changes instantly)
- Modern UI frameworks (React, Tailwind, etc.)
- Cross-platform consistency
- Proper undo/redo, automation, preset support

## Critical: JUCE 8 Relay System

**DO NOT use the old postMessage/evaluateJavascript approach.** JUCE 8 provides native relay classes that handle parameter sync properly:

| Relay Type | Parameter Type | C++ Class | TypeScript |
|------------|---------------|-----------|------------|
| Slider | Float | `WebSliderRelay` | `getSliderState()` |
| Toggle | Bool | `WebToggleButtonRelay` | `getToggleState()` |
| ComboBox | Choice | `WebComboBoxRelay` | `getComboBoxState()` |

### How It Works

1. **C++ creates relays** before WebBrowserComponent
2. **Relays are registered** with WebView options
3. **Attachments connect** relays to APVTS parameters
4. **TypeScript accesses** `window.__JUCE__.backend` for sync
5. **Events flow bidirectionally** - changes from either side sync automatically

## Project Structure

```
my-plugin/
├── CMakeLists.txt                 # Build config with NEEDS_WEBVIEW2
├── beatconnect-sdk/               # SDK submodule (git submodule add)
├── Source/                        # C++ source (capital S!)
│   ├── PluginProcessor.cpp        # Audio processing + APVTS
│   ├── PluginProcessor.h
│   ├── PluginEditor.cpp           # WebView + Relay setup
│   ├── PluginEditor.h
│   └── ParameterIDs.h             # Parameter ID constants
├── Resources/                     # Build-time resources
│   └── WebUI/                     # CI copies built web assets here
│       └── .gitkeep
├── resources/                     # CI injects project_data.json here
│   └── (created by CI)
├── web-ui/                        # React/TypeScript UI source
│   ├── package.json
│   ├── vite.config.ts
│   ├── tsconfig.json
│   ├── index.html
│   └── src/
│       ├── main.tsx
│       ├── App.tsx
│       ├── index.css
│       ├── lib/
│       │   └── juce-bridge.ts     # JUCE 8 frontend library
│       └── hooks/
│           └── useJuceParam.ts    # React parameter hooks
└── .github/workflows/build.yml
```

**Important folder conventions:**
- `Source/` (capital S) - C++ source files
- `Resources/WebUI/` - Where CI places built web assets
- `resources/` (lowercase) - Where CI places project_data.json
- `web-ui/` - React/TypeScript source (built by CI into Resources/WebUI/)

## Creating a New Plugin

### Step 1: Copy Templates

Copy all files from `templates/` to your new plugin directory.

### Step 2: Replace Placeholders

Replace these in ALL files:
- `{{PLUGIN_NAME}}` → PascalCase name (e.g., "SuperDelay")
- `{{PLUGIN_NAME_UPPER}}` → SCREAMING_SNAKE (e.g., "SUPER_DELAY")
- `{{COMPANY_NAME}}` → Company name (e.g., "BeatConnect")
- `{{COMPANY_NAME_LOWER}}` → lowercase (e.g., "beatconnect")
- `{{PLUGIN_CODE}}` → 4 chars (e.g., "Sdly")
- `{{MANUFACTURER_CODE}}` → 4 chars (e.g., "Beat")

### Step 3: Define Parameters

**ParameterIDs.h:**
```cpp
namespace ParamIDs {
    inline constexpr const char* gain = "gain";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* bypass = "bypass";
    inline constexpr const char* mode = "mode";
}
```

**PluginProcessor.cpp (createParameterLayout):**
```cpp
params.push_back(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID { ParamIDs::gain, 1 },
    "Gain",
    juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
    0.5f
));

params.push_back(std::make_unique<juce::AudioParameterBool>(
    juce::ParameterID { ParamIDs::bypass, 1 },
    "Bypass",
    false
));

params.push_back(std::make_unique<juce::AudioParameterChoice>(
    juce::ParameterID { ParamIDs::mode, 1 },
    "Mode",
    juce::StringArray { "Clean", "Warm", "Hot" },
    0
));
```

### Step 4: Create Relays (PluginEditor.cpp)

```cpp
void MyPluginEditor::setupWebView()
{
    // 1. Create relays FIRST (before WebBrowserComponent)
    gainRelay = std::make_unique<juce::WebSliderRelay>("gain");
    mixRelay = std::make_unique<juce::WebSliderRelay>("mix");
    bypassRelay = std::make_unique<juce::WebToggleButtonRelay>("bypass");
    modeRelay = std::make_unique<juce::WebComboBoxRelay>("mode");

    // 2. Build WebView options with relays
    auto options = juce::WebBrowserComponent::Options()
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withNativeIntegrationEnabled()
        .withResourceProvider([this](const juce::String& url) {
            // Serve bundled web files
            auto path = url.startsWith("/") ? url.substring(1) : url;
            if (path.isEmpty()) path = "index.html";
            auto file = resourcesDir.getChildFile(path);
            if (!file.existsAsFile()) return std::nullopt;
            // ... return file contents with MIME type
        })
        .withOptionsFrom(*gainRelay)
        .withOptionsFrom(*mixRelay)
        .withOptionsFrom(*bypassRelay)
        .withOptionsFrom(*modeRelay)
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2()
                .withBackgroundColour(juce::Colour(0xff1a1a1a))
                .withUserDataFolder(/* temp folder */));

    webView = std::make_unique<juce::WebBrowserComponent>(options);

    // 3. Load URL (dev server or bundled)
#if MYPLUGIN_DEV_MODE
    webView->goToURL("http://localhost:5173");
#else
    webView->goToURL(webView->getResourceProviderRoot());
#endif
}

void MyPluginEditor::setupRelaysAndAttachments()
{
    auto& apvts = processorRef.getAPVTS();

    gainAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::gain), *gainRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::mix), *mixRelay, nullptr);
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *apvts.getParameter(ParamIDs::bypass), *bypassRelay, nullptr);
    modeAttachment = std::make_unique<juce::WebComboBoxParameterAttachment>(
        *apvts.getParameter(ParamIDs::mode), *modeRelay, nullptr);
}
```

### Step 5: Use Parameters in React

**App.tsx:**
```tsx
import { useSliderParam, useToggleParam, useComboParam } from './hooks/useJuceParam';

function App() {
    // Hooks automatically sync with C++ relays
    const gain = useSliderParam('gain', { defaultValue: 0.5 });
    const bypass = useToggleParam('bypass');
    const mode = useComboParam('mode');

    return (
        <div>
            <input
                type="range"
                value={gain.value}
                onMouseDown={gain.onDragStart}
                onMouseUp={gain.onDragEnd}
                onChange={(e) => gain.setValue(parseFloat(e.target.value))}
            />
            <button onClick={bypass.toggle}>
                {bypass.value ? 'BYPASSED' : 'ACTIVE'}
            </button>
            <select
                value={mode.index}
                onChange={(e) => mode.setIndex(parseInt(e.target.value))}
            >
                {mode.choices.map((c, i) => <option key={i} value={i}>{c}</option>)}
            </select>
        </div>
    );
}
```

## CMakeLists.txt Requirements

### Critical Order of Operations

The CMakeLists.txt MUST follow this order:

1. **JUCE setup** (FetchContent or add_subdirectory)
2. **SDK setup** - `add_subdirectory(beatconnect-sdk/sdk/activation)` + cryptography link
3. **Plugin target** - `juce_add_plugin()`
4. **Sources/definitions/libraries** - Including `beatconnect_activation` in link libraries
5. **WebUI copy commands** - Copy from `Resources/WebUI`
6. **SDK integration** - project_data.json embedding and compile definitions

**Why order matters:** The SDK must be added BEFORE the plugin target because CMake needs the `beatconnect_activation` target to exist when linking.

### Required Compile Definitions

```cmake
target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1  # No DLL needed at runtime!
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        # Dev mode toggle
        $<IF:$<BOOL:${MYPLUGIN_DEV_MODE}>,MYPLUGIN_DEV_MODE=1,MYPLUGIN_DEV_MODE=0>
)
```

### Build Flags (Set by CI)

| Flag | Purpose |
|------|---------|
| `BEATCONNECT_ENABLE_ACTIVATION=ON` | Enable activation key validation |
| `JUCE_PATH=/path/to/juce` | Use local JUCE instead of fetching |
| `MYPLUGIN_DEV_MODE=ON` | Use Vite dev server instead of bundled assets |
| `HAS_PROJECT_DATA=1` | Auto-set when project_data.json exists |
| `BEATCONNECT_ACTIVATION_ENABLED=1` | Auto-set when activation is enabled |

### Plugin Target Configuration

```cmake
juce_add_plugin(${PROJECT_NAME}
    NEEDS_WEBVIEW2 TRUE              # Critical for Windows WebView2
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE     # Copy to system plugin folder
)
```

## Development Workflow

1. **Start web dev server:**
   ```bash
   cd web && npm install && npm run dev
   ```

2. **Build plugin in dev mode:**
   ```bash
   cmake -B build -DMYPLUGIN_DEV_MODE=ON
   cmake --build build
   ```

3. **Run Standalone** - it connects to localhost:5173 for hot reload

4. **Production build:**
   ```bash
   cd web && npm run build
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```

## Sending Non-Parameter Data (Visualizers)

For level meters, waveforms, and other real-time data:

**C++ (in timer callback):**
```cpp
void sendVisualizerData()
{
    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    data->setProperty("inputLevel", getInputLevel());
    data->setProperty("outputLevel", getOutputLevel());
    webView->emitEventIfBrowserIsVisible("visualizerData", juce::var(data.get()));
}
```

**TypeScript:**
```tsx
const viz = useVisualizerData<{ inputLevel: number }>('visualizerData', { inputLevel: 0 });
// Use viz.inputLevel in your meter component
```

## Common Mistakes to Avoid

1. **Creating WebBrowserComponent before relays** - Relays MUST exist first
2. **Mismatched identifiers** - "gain" in C++ must match "gain" in TypeScript exactly
3. **Using postMessage for parameters** - Use the relay system instead
4. **Forgetting NEEDS_WEBVIEW2** - Required for Windows WebView2 support
5. **Not calling dragStart/dragEnd** - Needed for proper undo/redo grouping

## State Management

### State Versioning (Required)

**Always implement state versioning to handle breaking parameter changes between versions.**

```cpp
// In PluginProcessor.cpp - Increment when parameters change
static constexpr int kStateVersion = 1;

void MyPluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    xml->setAttribute("stateVersion", kStateVersion);
    copyXmlToBinary(*xml, destData);
}

void MyPluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName(apvts.state.getType()))
    {
        int loadedVersion = xmlState->getIntAttribute("stateVersion", 0);
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

        if (loadedVersion != kStateVersion)
        {
            DBG("State version mismatch - resetting parameters");
            resetToDefaults();
        }
    }
}
```

### Non-Automatable Parameters

For parameters that should persist but NOT be automated by the DAW (e.g., module slots, routing):

```cpp
auto slotAttributes = juce::AudioParameterChoiceAttributes()
    .withAutomatable(false);

params.push_back(std::make_unique<juce::AudioParameterChoice>(
    juce::ParameterID { ParamIDs::moduleSlot1, 1 },
    "Module Slot 1",
    moduleOptions,
    0,
    slotAttributes  // Persists with project, not automatable
));
```

## DSP Best Practices

### Proper Bypass Handling

Always reset smoothed values when disabled to prevent clicks on re-enable:

```cpp
void MyEffect::process(juce::dsp::ProcessContextReplacing<float>& context)
{
    if (!enabled)
    {
        smoothedValue.setCurrentAndTargetValue(targetValue);
        return;  // Audio passes through unchanged
    }
    // Normal processing...
}
```

### Variable Buffer Size Handling

Some DAWs (FL Studio) use variable buffer sizes. Allocate with headroom:

```cpp
void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // 2x headroom for variable buffer sizes
    visualizerBuffer.setSize(2, samplesPerBlock * 2);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock * 2);
    spec.numChannels = 2;

    myEffect.prepare(spec);
}
```

### Smoothed Values for Parameters

Prevent clicks with parameter smoothing:

```cpp
juce::SmoothedValue<float> smoothedGain;

void prepare(const juce::dsp::ProcessSpec& spec)
{
    smoothedGain.reset(spec.sampleRate, 0.02);  // 20ms smoothing
    smoothedGain.setCurrentAndTargetValue(gain);
}
```

## Common Mistakes to Avoid

1. **Creating WebBrowserComponent before relays** - Relays MUST exist first
2. **Mismatched identifiers** - "gain" in C++ must match "gain" in TypeScript exactly
3. **Using postMessage for parameters** - Use the relay system instead
4. **Forgetting NEEDS_WEBVIEW2** - Required for Windows WebView2 support
5. **Not calling dragStart/dragEnd** - Needed for proper undo/redo grouping
6. **No state versioning** - Causes crashes when parameters change between versions
7. **Smoothed values not reset on bypass** - Causes clicks when re-enabling effects
8. **Fixed buffer sizes** - Causes crackling in DAWs with variable buffer sizes
9. **UI state not in APVTS** - All persistent state must be APVTS parameters

## Pre-Release Checklist

Before releasing a plugin, verify:

- [ ] State version is set and incremented since last release
- [ ] All persistent state is in APVTS parameters
- [ ] All effects properly bypass (early return + smoothed value reset)
- [ ] Variable buffer sizes handled (2x headroom)
- [ ] Smoothed values reset on effect disable
- [ ] WebView user data folder uses unique name per plugin
- [ ] Production build tested (not just dev mode)
- [ ] Tested in multiple DAWs (Ableton, FL Studio, Logic, Reaper)
- [ ] Memory profiled for leaks during extended use

## Patterns Documentation

For detailed code examples and battle-tested patterns, see:
**[docs/patterns.md](docs/patterns.md)**

Covers:
- State versioning pattern with full code
- UI-only state sync with TypeScript stores
- Filter coefficient update optimization
- Waveshaper patterns (avoiding crackling)
- Web/JUCE custom event patterns
- Troubleshooting common issues

## BeatConnect Integration

See `docs/integration.md` for:
- Build pipeline setup
- Activation SDK
- Distribution integration

## File Templates

All templates are in `templates/`:
- `CMakeLists.txt` - Full build configuration (with validation checklist)
- `src/` - C++ source with relay setup (PluginProcessor, PluginEditor, ParameterIDs)
- `web/` - React UI with JUCE 8 hooks
- `.github/workflows/build.yml` - CI/CD workflow

## New Plugin Validation Checklist

Before building a new plugin, verify:

### Structure
- [ ] `beatconnect-sdk/` submodule initialized (`git submodule update --init`)
- [ ] `Source/` folder contains: PluginProcessor.cpp/h, PluginEditor.cpp/h, ParameterIDs.h
- [ ] `web-ui/` folder contains: package.json, vite.config.ts, tsconfig.json, src/
- [ ] `Resources/WebUI/` folder exists (can be empty with .gitkeep)

### CMakeLists.txt
- [ ] All placeholders replaced (search for `{{` to verify none remain)
- [ ] SDK added BEFORE plugin target (`add_subdirectory(beatconnect-sdk/sdk/activation)`)
- [ ] SDK linked to cryptography (`target_link_libraries(beatconnect_activation PRIVATE juce::juce_cryptography)`)
- [ ] `beatconnect_activation` in plugin's link libraries
- [ ] `JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1` in compile definitions
- [ ] WebUI copy commands use `Resources/WebUI` as source path
- [ ] Windows installed VST3 copy command present

### C++ Source
- [ ] ParameterIDs.h uses `inline constexpr const char*` for all IDs
- [ ] State version constant defined in PluginProcessor.cpp
- [ ] All relays created BEFORE WebBrowserComponent
- [ ] Unique WebView2 user data folder name per plugin

### Web UI
- [ ] Parameter IDs in TypeScript match ParameterIDs.h exactly
- [ ] vite.config.ts has `base: './'` for relative paths
- [ ] Predictable asset naming in rollupOptions

### GitHub
- [ ] `beatconnect-plugin` topic added to repo for auto-discovery
