# BeatConnect Plugin SDK

SDK for building VST3/AU plugins that integrate with BeatConnect's distribution platform.

## Features

- **Web/JUCE 8 Hybrid Architecture** - React UI + C++ audio processing with native relay system
- **Activation System** - License validation with machine fingerprinting
- **Asset Downloads** - Download samples, presets, and content from BeatConnect
- **Build Pipeline Integration** - Automated compilation and code signing
- **Distribution** - Signed downloads, Stripe payments

## Quick Start

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

License activation with machine fingerprinting:

```cpp
#include <beatconnect/Activation.h>

// Configure (apiBaseUrl is the Supabase URL WITHOUT /functions/v1)
beatconnect::ActivationConfig config;
config.apiBaseUrl = "https://xxx.supabase.co";  // SDK adds /functions/v1 internally
config.pluginId = "your-project-uuid";
config.supabaseKey = "your-publishable-key";    // For API auth headers

auto& activation = beatconnect::Activation::getInstance();
activation.configure(config);

// Check on startup
if (!activation.isActivated()) {
    showActivationDialog();
}

// Activate (supports UUID format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)
auto status = activation.activate(userEnteredCode);
if (status == beatconnect::ActivationStatus::Valid) {
    // Success! State is automatically saved.
}
```

**Important**: The SDK does NOT perform network validation during `configure()`. This is intentional - many DAWs (Ableton, Logic) reject plugins that make network requests during initialization. Trigger validation from the UI layer after the plugin is fully loaded if needed.

### Debug Logging (`beatconnect::Debug`)

Cross-platform debug logging for troubleshooting:

```cpp
#include <beatconnect/Activation.h>  // Debug is in same header

// Initialize in your PluginEditor constructor
// Logs written to: AppData/BeatConnect/<pluginName>/debug.log
bool debugEnabled = getDebugFlagFromConfig();
beatconnect::Debug::init("MyPlugin", debugEnabled);

// Log messages (thread-safe, no-op if disabled)
beatconnect::Debug::log("Starting activation...");
beatconnect::Debug::log("Result: " + statusString);

// Show user where logs are (opens folder in file manager)
if (beatconnect::Debug::isEnabled()) {
    beatconnect::Debug::revealLogFile();
}
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
│   └── ParameterIDs.h      # Parameter ID constants
├── web/                    # React UI
│   ├── package.json
│   ├── src/
│   │   ├── App.tsx
│   │   ├── main.tsx
│   │   ├── lib/
│   │   │   └── juce-bridge.ts   # JUCE 8 frontend API
│   │   └── hooks/
│   │       └── useJuceParam.ts  # React hooks for params
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

To use BeatConnect's build pipeline (code signing, activation, distribution):

1. Register your plugin as an "external project" in BeatConnect
2. Get your `PROJECT_ID` and `WEBHOOK_SECRET`
3. Add GitHub secrets and uncomment the `beatconnect-build` job in your workflow

See [docs/integration.md](docs/integration.md) for details.

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
