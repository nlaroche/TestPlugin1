# BeatConnect Plugin SDK

SDK for building VST3/AU plugins that integrate with BeatConnect's distribution platform.

## Features

- **Web/JUCE 8 Hybrid Architecture** - React UI + C++ audio processing
- **Build Pipeline Integration** - Automated compilation and code signing
- **Activation System** - License codes with machine limits
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
# {{COMPANY_NAME}} -> YourCompany
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

## Project Structure

```
my-plugin/
├── CMakeLists.txt          # Build configuration
├── src/
│   ├── PluginProcessor.cpp # Audio processing (C++)
│   ├── PluginProcessor.h
│   ├── PluginEditor.cpp    # WebView UI host
│   └── PluginEditor.h
├── web/                    # React UI
│   ├── package.json
│   ├── src/
│   │   ├── App.tsx
│   │   └── main.tsx
│   └── vite.config.ts
└── .github/workflows/      # CI/CD
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
│  │ • processBlock  │  │ • Parameter controls      │ │
│  │ • Parameters    │  │ • Visualizations          │ │
│  │ • State save    │  │ • postMessage bridge      │ │
│  └─────────────────┘  └───────────────────────────┘ │
└─────────────────────────────────────────────────────┘
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

## Requirements

- CMake 3.22+
- JUCE 8.0.4+ (fetched automatically)
- Node.js 18+ (for web UI)
- **Windows**: Visual Studio 2022
- **macOS**: Xcode 14+

## License

MIT
