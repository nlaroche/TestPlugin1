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
| `/bc-create-plugin` | Interactive guide to build your first plugin - no C++ knowledge required! |

### Example Prompts

Here are some ways to work with Claude on your plugin:

**Starting a new project:**
```
/bc-create-plugin
```
Then just follow the conversation - Claude will ask what kind of plugin you want to build.

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

### 1. Create your plugin repository

**Option A: Use GitHub Template (Recommended)**
1. Go to the [BeatConnect Plugin SDK](https://github.com/BeatConnect/beatconnect_plugin_sdk) repository
2. Click **"Use this template"** → **"Create a new repository"**
3. Name your repo and create it (this gives you a clean single-commit history)

**Option B: Fork the SDK**
1. Fork the repository on GitHub
2. Clone your fork locally

### 2. Configure your repository for BeatConnect

**Add the required topic** so BeatConnect can discover your plugin:
1. Go to your repository on GitHub
2. Click the ⚙️ gear icon next to "About" (top right of repo page)
3. Under "Topics", add: `beatconnect-plugin`
4. Click "Save changes"

### 3. Set up your plugin

Your repo has this structure:
```
your-repo/
├── beatconnect-sdk/    # SDK (don't modify)
├── plugin/             # YOUR PLUGIN CODE GOES HERE
│   ├── CMakeLists.txt
│   ├── Source/
│   └── web-ui/
└── CLAUDE.md
```

To customize your plugin, edit files in `plugin/` and replace placeholders:
- `{{PLUGIN_NAME}}` → YourPluginName
- `{{PLUGIN_NAME_UPPER}}` → YOUR_PLUGIN_NAME
- `{{COMPANY_NAME}}` → YourCompany
- `{{PLUGIN_CODE}}` → Ypln (4 chars)
- `{{MANUFACTURER_CODE}}` → Ycom (4 chars)

Or use Claude Code with `/bc-create-plugin` to scaffold everything automatically!

### 4. Build the web UI

```bash
cd plugin/web-ui
npm install
npm run build
cd ../..
```

### 5. Build the plugin

```bash
cd plugin
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### 6. Find your plugin

- **Windows**: `plugin/build/YourPluginName_artefacts/Release/VST3/`
- **macOS**: `plugin/build/YourPluginName_artefacts/Release/VST3/` and `AU/`

### 7. Updating the SDK (when new versions are released)

If you need to pull updates from the upstream SDK, **always use `--squash`** to keep your commit history clean:

```bash
# Add upstream remote (only once)
git remote add upstream https://github.com/BeatConnect/beatconnect_plugin_sdk.git

# Pull updates with --squash (keeps your history clean)
git fetch upstream
git merge --squash upstream/main
git commit -m "chore: update SDK to latest"
```

**Important:** Never use a regular `git merge upstream/main` - it will pollute your repo with the entire SDK development history.

## SDK Components

### Activation SDK (`beatconnect-sdk/activation/`)

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

### Asset Downloader (`beatconnect-sdk/activation/`)

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

### Preset Manager (`beatconnect-sdk/templates/Source/`)

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

The SDK is already included in `beatconnect-sdk/`. Your plugin's CMakeLists.txt should include:

```cmake
# Include BeatConnect CMake helpers
include(${CMAKE_SOURCE_DIR}/../beatconnect-sdk/cmake/BeatConnectPlugin.cmake)

# Fetch JUCE automatically
beatconnect_fetch_juce()

# After defining your plugin target:
beatconnect_configure_plugin(${PROJECT_NAME})
```

This automatically handles JUCE setup, WebView configuration, and platform-specific settings.

## Project Structure

```
your-repo/
├── beatconnect-sdk/              # SDK (don't modify these files)
│   ├── cmake/                    # CMake helpers
│   ├── activation/               # License activation SDK
│   ├── templates/                # File templates for reference
│   ├── example-plugin/           # Reference implementation
│   └── docs/                     # Documentation
├── plugin/                       # YOUR PLUGIN (edit these files)
│   ├── CMakeLists.txt            # Build configuration
│   ├── Source/
│   │   ├── PluginProcessor.cpp   # Audio processing (C++)
│   │   ├── PluginProcessor.h
│   │   ├── PluginEditor.cpp      # WebView UI host + JUCE 8 relays
│   │   ├── PluginEditor.h
│   │   └── ParameterIDs.h        # Parameter ID constants
│   ├── web-ui/                   # React UI
│   │   ├── package.json
│   │   ├── src/
│   │   │   ├── App.tsx
│   │   │   ├── main.tsx
│   │   │   ├── lib/
│   │   │   │   └── juce-bridge.ts    # JUCE 8 frontend API
│   │   │   └── hooks/
│   │   │       └── useJuceParam.ts   # React hooks for params
│   │   └── vite.config.ts
│   └── Resources/WebUI/          # Built web assets (auto-generated)
├── CLAUDE.md                     # AI assistant instructions
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
   cd plugin/web-ui && npm run dev
   ```

2. Build plugin in dev mode:
   ```bash
   cd plugin
   cmake -B build -DYOUR_PLUGIN_NAME_DEV_MODE=ON
   cmake --build build
   ```

3. Run Standalone target - connects to localhost:5173 for hot reload

### Production Build

1. Build web UI:
   ```bash
   cd plugin/web-ui && npm run build
   ```

2. Build plugin:
   ```bash
   cd plugin
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

### Example Plugin (`beatconnect-sdk/example-plugin/`)

A complete, buildable example demonstrating all correct patterns:
- JUCE 8 WebView parameter binding
- Level metering and visualizers
- Activation system integration
- State persistence

```bash
cd beatconnect-sdk/example-plugin

# Build web UI
cd web-ui && npm install && npm run build && cd ..

# Build plugin
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Documentation

- [CLAUDE.md](CLAUDE.md) - Instructions for Claude Code to scaffold new plugins
- [beatconnect-sdk/docs/architecture.md](beatconnect-sdk/docs/architecture.md) - Detailed architecture guide
- [beatconnect-sdk/docs/integration.md](beatconnect-sdk/docs/integration.md) - BeatConnect integration guide
- [beatconnect-sdk/docs/best-practices.md](beatconnect-sdk/docs/best-practices.md) - **Best practices & validation checklist** (common issues and fixes)

## Requirements

- CMake 3.22+
- JUCE 8.0.4+ (fetched automatically)
- Node.js 18+ (for web UI)
- **Windows**: Visual Studio 2022
- **macOS**: Xcode 14+

## License

MIT
