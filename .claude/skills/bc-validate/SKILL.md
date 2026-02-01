# /bc-validate Skill

Validate a BeatConnect plugin project before building. Catches common issues early and saves build credits.

## Usage

```
/bc-validate
```

Run from your plugin repository root.

## What It Checks

### 1. Project Structure
- [ ] Root CMakeLists.txt exists (wrapper for subdirectory builds)
- [ ] plugin/CMakeLists.txt exists (or CMakeLists.txt at root for flat structure)
- [ ] Source/ directory exists
- [ ] Required source files present (PluginProcessor.cpp/h, PluginEditor.cpp/h)
- [ ] ParameterIDs.h exists
- [ ] beatconnect-sdk/ directory exists and is initialized

### 2. CMake Configuration - Plugin Metadata
- [ ] `cmake_minimum_required(VERSION 3.22)` or higher
- [ ] `project()` defined with name and VERSION
- [ ] `juce_add_plugin()` target defined with all required fields:
  - COMPANY_NAME (your company/brand)
  - PLUGIN_MANUFACTURER_CODE (exactly 4 characters)
  - PLUGIN_CODE (exactly 4 characters, unique per plugin)
  - FORMATS (VST3, AU, Standalone, etc.)
  - IS_SYNTH (TRUE for instruments, FALSE for effects)
  - PRODUCT_NAME (display name)
  - NEEDS_WEBVIEW2 (TRUE if using WebUI)
- [ ] BeatConnectPlugin.cmake included correctly
- [ ] `beatconnect_configure_plugin()` called

### 3. Frontend Validation
- [ ] Detect Web UI (`web-ui/` or `web/` directory) or Native JUCE UI
- [ ] For Web UI: package.json, TypeScript compilation, Vite build
- [ ] For Web UI: Resources/WebUI/ or dist/ with index.html
- [ ] Parameter IDs match between C++ and TypeScript

### 4. Build System
- [ ] Can configure from repo root (`cmake -B build -S .`)
- [ ] .gitignore excludes build/, node_modules/, dist/
- [ ] Uses CMAKE_CURRENT_SOURCE_DIR for paths (not CMAKE_SOURCE_DIR)

---

## Output Format

### Success Output
```
============================================================
 BEATCONNECT PLUGIN VALIDATION REPORT
============================================================

PLUGIN INFORMATION
  Name:              DelayWave
  Version:           1.0.0
  Company:           BeatConnect
  Manufacturer Code: Beat
  Plugin Code:       Dwav
  Category:          Effect (IS_SYNTH=FALSE)
  Formats:           VST3, Standalone
  Frontend:          Web UI

PROJECT STRUCTURE                                      [PASS]
  ✓ Root CMakeLists.txt found (wrapper)
  ✓ plugin/CMakeLists.txt found
  ✓ plugin/Source/ directory exists
  ✓ PluginProcessor.cpp/h present
  ✓ PluginEditor.cpp/h present
  ✓ ParameterIDs.h present
  ✓ beatconnect-sdk/ found

CMAKE CONFIGURATION                                    [PASS]
  ✓ CMake 3.22+ required
  ✓ Project version: 1.0.0
  ✓ COMPANY_NAME set
  ✓ PLUGIN_MANUFACTURER_CODE: Beat (4 chars)
  ✓ PLUGIN_CODE: Dwav (4 chars)
  ✓ NEEDS_WEBVIEW2: TRUE
  ✓ BeatConnectPlugin.cmake included
  ✓ beatconnect_configure_plugin() called

FRONTEND: Web UI                                       [PASS]
  ✓ web-ui/package.json found
  ✓ TypeScript compiles
  ✓ Vite build succeeded
  ✓ Resources/WebUI/index.html exists

PARAMETER SYNC                                         [PASS]
  C++ Parameters (7):
    time, feedback, mix, modRate, modDepth, tone, bypass
  TypeScript Parameters (7):
    time, feedback, mix, modRate, modDepth, tone, bypass
  ✓ All parameters match

BUILD SYSTEM                                           [PASS]
  ✓ cmake -S . -B build configures successfully
  ✓ .gitignore properly configured

============================================================
 RESULT: ALL CHECKS PASSED
 Your plugin is ready for BeatConnect build!
============================================================
```

### Failure Output
```
============================================================
 BEATCONNECT PLUGIN VALIDATION REPORT
============================================================

PLUGIN INFORMATION
  Name:              MyPlugin
  Version:           1.0.0
  Company:           [NOT SET]
  Manufacturer Code: [NOT SET]
  Plugin Code:       [NOT SET]
  Category:          Unknown
  Formats:           VST3
  Frontend:          Web UI

PROJECT STRUCTURE                                      [FAIL]
  ✓ Root CMakeLists.txt found
  ✓ plugin/CMakeLists.txt found
  ✓ plugin/Source/ directory exists
  ✓ PluginProcessor.cpp/h present
  ✓ PluginEditor.cpp/h present
  ✗ ParameterIDs.h MISSING
  ✓ beatconnect-sdk/ found

CMAKE CONFIGURATION                                    [FAIL]
  ✓ CMake 3.22+ required
  ✓ Project version: 1.0.0
  ✗ COMPANY_NAME not set
  ✗ PLUGIN_MANUFACTURER_CODE not set (required, 4 chars)
  ✗ PLUGIN_CODE not set (required, 4 chars)
  ✓ NEEDS_WEBVIEW2: TRUE
  ✓ BeatConnectPlugin.cmake included

FRONTEND: Web UI                                       [FAIL]
  ✓ web-ui/package.json found
  ✗ npm install failed
  ○ Build skipped (dependencies failed)

============================================================
 RESULT: 5 ISSUES FOUND
============================================================

REQUIRED FIXES:
1. Create plugin/Source/ParameterIDs.h with parameter ID constants
2. Set COMPANY_NAME in juce_add_plugin() - e.g., "MyCompany"
3. Set PLUGIN_MANUFACTURER_CODE in juce_add_plugin() - exactly 4 characters, e.g., Myco
4. Set PLUGIN_CODE in juce_add_plugin() - exactly 4 characters, unique, e.g., Mypl
5. Fix npm: cd plugin/web-ui && rm -rf node_modules package-lock.json && npm install
```

---

## Implementation Notes

### Extracting Plugin Info from CMakeLists.txt

Parse juce_add_plugin() call:
```cmake
juce_add_plugin(PluginName
    COMPANY_NAME "CompanyName"
    PLUGIN_MANUFACTURER_CODE Abcd
    PLUGIN_CODE Efgh
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    PRODUCT_NAME "Product Name"
    NEEDS_WEBVIEW2 TRUE
)
```

Extract from project():
```cmake
project(PluginName VERSION 1.0.0 LANGUAGES CXX)
```

### Key Validations

1. **PLUGIN_MANUFACTURER_CODE** - Must be exactly 4 alphanumeric characters
2. **PLUGIN_CODE** - Must be exactly 4 alphanumeric characters, unique per plugin
3. **NEEDS_WEBVIEW2** - Must be TRUE if web-ui/ directory exists
4. **Root CMakeLists.txt** - Required for CI builds that run `cmake -S . -B build` from repo root

### Testing Build Configuration

Run cmake configure test:
```bash
cmake -B build-test -S . 2>&1
```

Check for:
- Configuration succeeds without errors
- "[BeatConnect]" status messages appear
- No "file not found" errors

---

## Integration

This validation catches issues that would fail BeatConnect CI builds:
- Missing root CMakeLists.txt wrapper
- Incorrect SDK paths (CMAKE_SOURCE_DIR vs CMAKE_CURRENT_SOURCE_DIR)
- Missing or invalid plugin metadata
- Web UI build failures
- Parameter ID mismatches between C++ and TypeScript
