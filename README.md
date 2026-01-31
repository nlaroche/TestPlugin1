# BeatConnect Plugin SDK

Build and distribute professional VST3/AU audio plugins with BeatConnect's complete development platform.

## Getting Started

Before diving into the code, you'll need a BeatConnect developer account:

1. **Sign up at [beatconnect.com](https://beatconnect.com)**
2. **Request Developer Access** from your account settings
3. **Create a new project** in the developer dashboard

Once approved, you'll have access to:
- **Cloud Build Pipeline** - Automated compilation for Windows & macOS
- **Code Signing** - Automatic signing for trusted installs
- **License Activation** - Built-in copy protection with machine fingerprinting
- **Distribution** - Hosted downloads with Stripe payment integration

Check your dashboard at [beatconnect.com](https://beatconnect.com) for current build quotas and plan limits.

## AI-Assisted Development

This SDK is designed to work seamlessly with AI coding assistants like [Claude Code](https://claude.ai/code). Whether you're new to plugin development or just want to move faster, AI can help you scaffold, debug, and iterate on your plugins.

### Getting Set Up

1. **Clone this SDK** to your machine
2. **Open in Claude Code** or your preferred AI-enabled IDE
3. **Use the built-in skills** to scaffold and modify your plugin

### Available Skills

| Skill | What it does |
|-------|--------------|
| `/new-plugin` | Scaffold a complete new plugin from templates with your branding |

### Example Prompts

Here are some ways to work with Claude on your plugin:

**Starting a new project:**
```
/new-plugin "LoFi Tape" by "AudioCraft" - a vintage tape saturation plugin
```

**Adding features:**
```
Add a wet/dry mix parameter to my plugin with a range of 0-100%
```

**Debugging:**
```
My plugin crashes when I load it in Ableton - help me debug the initialization
```

**UI work:**
```
Create a vintage-style knob component for the saturation control
```

**Learning:**
```
Explain how the JUCE 8 WebSliderRelay system works
```

The SDK includes a `CLAUDE.md` file with detailed context about the codebase, so AI assistants can understand the architecture and patterns automatically.

## Features

- **Web/JUCE 8 Hybrid Architecture** - React UI + C++ audio processing with native relay system
- **Activation System** - License validation with machine fingerprinting
- **Preset Management** - Factory and user preset save/load system
- **Asset Downloads** - Download samples, presets, and content from BeatConnect
- **Build Pipeline Integration** - Automated compilation and code signing
- **Distribution** - Signed downloads, Stripe payments

## Quick Start

Once you have developer access, you can start building locally:

### 1. Create a new plugin from template

```bash
# Clone this SDK
git clone https://github.com/BeatConnect/beatconnect_plugin_sdk.git

# Copy templates to your new plugin directory
cp -r beatconnect_plugin_sdk/templates my-plugin
cd my-plugin

# Replace placeholders in all files:
# {{PLUGIN_NAME}} -> YourPluginName
# {{PLUGIN_NAME_UPPER}} -> YOUR_PLUGIN_NAME
# {{COMPANY_NAME}} -> YourCompany
# {{COMPANY_NAME_LOWER}} -> yourcompany
# {{PLUGIN_CODE}} -> Ypln (4 chars)
# {{MANUFACTURER_CODE}} -> Ycom (4 chars)
```

### 2. Build the web UI

```bash
cd web
npm install
npm run build
cd ..
```

### 3. Build the plugin

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### 4. Find your plugin

- **Windows**: `build/YourPluginName_artefacts/Release/VST3/`
- **macOS**: `build/YourPluginName_artefacts/Release/VST3/` and `AU/`

## SDK Components

### Activation SDK (`sdk/activation/`)

License activation with machine fingerprinting. **No manual configuration required** - credentials are automatically injected by the BeatConnect build system.

#### Recommended Usage (Auto-configured)

When you build through BeatConnect, the build system injects `project_data.json` into your plugin's resources folder with all necessary credentials. Your code just needs to call `createFromBuildData()`:

```cpp
#include <beatconnect/Activation.h>

// In your PluginProcessor constructor or loadProjectData():
#if BEATCONNECT_ACTIVATION_ENABLED
    // Auto-configured from project_data.json injected by BeatConnect build
    activation_ = beatconnect::Activation::createFromBuildData(
        JucePlugin_Name,  // Plugin name for debug logging
        false             // Disable debug logging in production
    );

    if (activation_) {
        DBG("BeatConnect Activation SDK initialized");
    } else {
        DBG("Running in development mode (no build data)");
    }
#endif

// In your UI layer, check activation status:
if (activation_ && !activation_->isActivated()) {
    showActivationDialog();
}

// Activate with user-entered code (UUID format supported)
auto status = activation_->activate(userEnteredCode);
if (status == beatconnect::ActivationStatus::Valid) {
    // Success! State is automatically saved.
}
```

#### Manual Configuration (Testing Only)

For local testing without the BeatConnect build pipeline:

```cpp
beatconnect::ActivationConfig config;
config.apiBaseUrl = "https://xxx.supabase.co";  // SDK adds /functions/v1 internally
config.pluginId = "your-project-uuid";
config.supabaseKey = "your-publishable-key";
config.pluginName = "MyPlugin";
config.enableDebugLogging = true;

auto activation = beatconnect::Activation::create(config);
```

**Important Notes**:
- The SDK does NOT perform network validation during creation. This is intentional - many DAWs (Ableton, Logic) reject plugins that make network requests during initialization.
- Each plugin instance should own its own `Activation` instance (NOT a singleton) to avoid conflicts when multiple plugin instances are loaded.
- `createFromBuildData()` returns `nullptr` when running in development without build data.

### Debug Logging (Instance-based)

Each Activation instance has its own debug logging:

```cpp
// Enable via createFromBuildData second parameter
activation_ = beatconnect::Activation::createFromBuildData("MyPlugin", true);

// Or via config for manual setup
config.enableDebugLogging = true;

// Log messages (thread-safe)
activation_->debugLog("Starting activation...");
activation_->debugLog("Result: " + statusString);

// Show user where logs are (opens folder in file manager)
if (activation_->isDebugEnabled()) {
    activation_->revealDebugLog();
}

// Get log path
std::string logPath = activation_->getDebugLogPath();
```

Log file locations:
- **macOS**: `~/Library/Application Support/BeatConnect/<pluginName>/debug.log`
- **Windows**: `%APPDATA%/BeatConnect/<pluginName>/debug.log`
- **Linux**: `~/.local/share/BeatConnect/<pluginName>/debug.log`

### Asset Downloader (`sdk/activation/`)

Download samples, presets, and other content:

```cpp
#include <beatconnect/AssetDownloader.h>

beatconnect::DownloaderConfig config;
config.apiBaseUrl = "https://xxx.supabase.co/functions/v1";
config.downloadPath = "/path/to/assets";
config.authToken = userToken;

beatconnect::AssetDownloader downloader;
downloader.configure(config);

// Download with progress
auto [status, filePath] = downloader.download(assetId,
    [](const beatconnect::DownloadProgress& p) {
        updateProgressBar(p.percent);
    });
```

### Preset Manager (`templates/Source/`)

User and factory preset management with C++/React integration:

```cpp
#include "PresetManager.h"

// In your processor
PresetManager presetManager(apvts, JucePlugin_Name);

// Define factory presets
presetManager.setFactoryPresets({
    "Default", "Clean", "Warm", "Bright"
});

// Save user preset
presetManager.saveUserPreset("My Custom Sound");

// Load preset
presetManager.loadPreset("My Custom Sound", false);  // false = user preset
presetManager.loadPreset("Default", true);           // true = factory preset

// Get JSON for UI
juce::String json = presetManager.getPresetListAsJson();
```

```tsx
// React: Use the preset hook
import { usePresets } from './hooks/usePresets';

function PresetSelector() {
  const {
    factoryPresets,
    userPresets,
    currentPreset,
    savePreset,
    loadPreset,
    deletePreset
  } = usePresets();

  return (
    <select onChange={(e) => loadPreset(e.target.value, false)}>
      {userPresets.map(p => (
        <option key={p.name} value={p.name}>{p.name}</option>
      ))}
    </select>
  );
}
```

User presets are stored as XML files in:
- **macOS**: `~/Library/Application Support/<PluginName>/UserPresets/`
- **Windows**: `%APPDATA%/<PluginName>/UserPresets/`
- **Linux**: `~/.local/share/<PluginName>/UserPresets/`

### Using the SDK in your plugin

Add as a submodule:

```bash
git submodule add https://github.com/beatconnect/beatconnect-plugin-sdk beatconnect-sdk
```

In your CMakeLists.txt:

```cmake
add_subdirectory(beatconnect-sdk/sdk/activation)
target_link_libraries(${PROJECT_NAME} PRIVATE beatconnect_activation)
```

## Project Structure

```
my-plugin/
├── CMakeLists.txt          # Build configuration
├── src/
│   ├── PluginProcessor.cpp # Audio processing (C++)
│   ├── PluginProcessor.h
│   ├── PluginEditor.cpp    # WebView UI host + JUCE 8 relays
│   ├── PluginEditor.h
│   ├── ParameterIDs.h      # Parameter ID constants
│   ├── PresetManager.h     # Preset save/load system
│   └── PresetManager.cpp
├── web/                    # React UI
│   ├── package.json
│   ├── src/
│   │   ├── App.tsx
│   │   ├── main.tsx
│   │   ├── lib/
│   │   │   └── juce-bridge.ts   # JUCE 8 frontend API
│   │   └── hooks/
│   │       ├── useJuceParam.ts  # React hooks for params
│   │       └── usePresets.ts    # Preset management hook
│   └── vite.config.ts
├── beatconnect-sdk/        # SDK submodule (optional)
└── .github/workflows/
    └── build.yml
```

## Architecture

```
┌─────────────────────────────────────────────────────┐
│  DAW (Ableton, Logic, FL Studio, etc.)              │
└─────────────────────────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────┐
│  Plugin (VST3/AU)                                   │
│  ┌─────────────────┐  ┌───────────────────────────┐ │
│  │ PluginProcessor │  │ PluginEditor              │ │
│  │ (C++ DSP)       │◄─┤ (WebView + React UI)      │ │
│  │                 │  │                           │ │
│  │ • processBlock  │  │ • JUCE 8 Relay System     │ │
│  │ • Parameters    │  │ • useSliderParam hooks    │ │
│  │ • State save    │  │ • Visualizations          │ │
│  └─────────────────┘  └───────────────────────────┘ │
└─────────────────────────────────────────────────────┘
```

### JUCE 8 Relay System

Unlike postMessage-based approaches, JUCE 8 provides native relay classes for bidirectional parameter sync:

| C++ Class | TypeScript | Use Case |
|-----------|------------|----------|
| `WebSliderRelay` | `useSliderParam()` | Float parameters |
| `WebToggleButtonRelay` | `useToggleParam()` | Bool parameters |
| `WebComboBoxRelay` | `useComboParam()` | Choice parameters |

```cpp
// C++: Create relays BEFORE WebBrowserComponent
gainRelay = std::make_unique<juce::WebSliderRelay>("gain");

// Register with WebView options
auto options = juce::WebBrowserComponent::Options()
    .withNativeIntegrationEnabled()
    .withOptionsFrom(*gainRelay);

// Connect to APVTS
gainAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
    *apvts.getParameter("gain"), *gainRelay, nullptr);
```

```tsx
// TypeScript: Hook syncs automatically
const gain = useSliderParam('gain', { defaultValue: 0.5 });

<input
  type="range"
  value={gain.value}
  onMouseDown={gain.onDragStart}
  onMouseUp={gain.onDragEnd}
  onChange={(e) => gain.setValue(parseFloat(e.target.value))}
/>
```

## Development Workflow

### Hot Reload (Development)

1. Start web dev server:
   ```bash
   cd web && npm run dev
   ```

2. Build plugin in dev mode:
   ```bash
   cmake -B build -DYOUR_PLUGIN_NAME_DEV_MODE=ON
   cmake --build build
   ```

3. Run Standalone target - connects to localhost:5173 for hot reload

### Production Build

1. Build web UI:
   ```bash
   cd web && npm run build
   ```

2. Build plugin:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```

## BeatConnect Integration

Once your plugin builds locally, connect it to BeatConnect's cloud pipeline:

1. **Create a project** in your BeatConnect developer dashboard
2. **Link your GitHub repo** - BeatConnect will provide `PROJECT_ID` and `WEBHOOK_SECRET`
3. **Add secrets** to your GitHub repo settings
4. **Push to trigger builds** - BeatConnect compiles, signs, and hosts your plugin

Your dashboard shows build status, download stats, and license activations in real-time.

See [docs/integration.md](docs/integration.md) for detailed setup instructions.

## Examples

### Example Plugin (`examples/example-plugin/`)

A complete, buildable example demonstrating all correct patterns:
- JUCE 8 WebView parameter binding
- Level metering and visualizers
- Activation system integration
- State persistence

```bash
cd examples/example-plugin

# Build web UI
cd web-ui && npm install && npm run build && cd ..

# Build plugin
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Documentation

- [CLAUDE.md](CLAUDE.md) - Instructions for Claude Code to scaffold new plugins
- [docs/architecture.md](docs/architecture.md) - Detailed architecture guide
- [docs/integration.md](docs/integration.md) - BeatConnect integration guide
- [docs/best-practices.md](docs/best-practices.md) - **Best practices & validation checklist** (common issues and fixes)

## Requirements

- CMake 3.22+
- JUCE 8.0.4+ (fetched automatically)
- Node.js 18+ (for web UI)
- **Windows**: Visual Studio 2022
- **macOS**: Xcode 14+

## License

MIT
