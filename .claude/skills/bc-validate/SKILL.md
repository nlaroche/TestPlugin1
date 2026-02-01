# /bc-validate Skill

Validate a BeatConnect plugin project before building. Catches common issues early and saves build credits.

**CRITICAL: This validation MUST actually run cmake and web builds, not just check if files exist.**

## Usage

```
/bc-validate
```

Run from your plugin repository root.

---

## MANDATORY VALIDATION STEPS

You MUST execute these steps in order. Do NOT skip any step.

### Step 1: Check for Pre-compiled Binaries in Git

**Run this command:**
```bash
git ls-files | grep -E "\.(exe|dll|lib|obj|o|a|so|dylib|vst3|component|aaxplugin)$"
```

If ANY files are returned, **FAIL immediately** with error:
```
CRITICAL ERROR: Pre-compiled binaries found in repository!
Files: [list the files]
These must be removed with: git rm <file> && git commit
```

### Step 2: Check Project Structure

Check these files/directories exist:
- `CMakeLists.txt` (at repo root - REQUIRED for CI)
- `plugin/CMakeLists.txt` (plugin code)
- `plugin/Source/` directory
- `plugin/Source/PluginProcessor.cpp` and `.h`
- `plugin/Source/PluginEditor.cpp` and `.h`
- `plugin/Source/ParameterIDs.h`
- `beatconnect-sdk/` directory

### Step 3: Check for CMAKE_SOURCE_DIR Bug

**Run this command:**
```bash
grep -r "CMAKE_SOURCE_DIR" plugin/CMakeLists.txt
```

If found WITHOUT `CMAKE_CURRENT_SOURCE_DIR`, **WARN**:
```
WARNING: Using CMAKE_SOURCE_DIR instead of CMAKE_CURRENT_SOURCE_DIR
This will break when built from repo root. Change to CMAKE_CURRENT_SOURCE_DIR.
```

### Step 4: Check Root CMakeLists.txt

The root `CMakeLists.txt` MUST:
- Include `LANGUAGES C CXX` (not just CXX)
- Call `add_subdirectory(plugin)` or similar

**Read the file and verify these are present.**

### Step 5: Extract and Display Plugin Metadata

Parse `plugin/CMakeLists.txt` and extract:
- Project name and version from `project(NAME VERSION X.X.X)`
- COMPANY_NAME from juce_add_plugin()
- PLUGIN_MANUFACTURER_CODE (must be exactly 4 chars)
- PLUGIN_CODE (must be exactly 4 chars)
- FORMATS
- IS_SYNTH
- NEEDS_WEBVIEW2

Display in report header.

### Step 6: Build Web UI (if present)

If `plugin/web-ui/` exists:

```bash
cd plugin/web-ui
npm ci  # or npm install
npm run build
```

Check that `plugin/Resources/WebUI/index.html` exists after build.

**If build fails, capture and display the error.**

### Step 7: Run CMake Configure WITH ACTIVATION ENABLED

**This is the most critical test.** CI builds with activation enabled, so we MUST test that way:

```bash
cd <repo-root>
rm -rf build-validate
cmake -B build-validate -S . -DBEATCONNECT_ENABLE_ACTIVATION=ON 2>&1
```

Check output for:
- ✓ Configuration completes without error
- ✓ "[BeatConnect] Detected SDK-at-root structure" message appears
- ✓ "[BeatConnect] WebUI directory found" (if web UI)
- ✓ "[BeatConnect] Activation SDK enabled" message appears
- ✓ "BeatConnect SDK: Using JUCE cryptography for SHA256" message appears
- ✗ Any CMake errors or warnings

**If cmake fails, this is a CRITICAL FAILURE.**

### Step 7b: Run CMake BUILD (Full Compile + Link Test)

**Don't just configure - actually build to catch linker errors:**

```bash
cmake --build build-validate --config Release 2>&1
```

Check for:
- ✓ Build completes without error
- ✓ VST3 and/or Standalone artefacts created
- ✗ Any linker errors (LNK2001, undefined symbols, etc.)

**If build fails, this is a CRITICAL FAILURE.**

### Step 8: Parameter Sync Check (Web UI only)

Compare parameter IDs between:
- `plugin/Source/ParameterIDs.h`
- `plugin/web-ui/src/App.tsx`

List any mismatches.

### Step 9: Check .gitignore

Verify `.gitignore` includes:
- `build/`
- `node_modules/`
- `**/Resources/WebUI/assets/`
- `**/Resources/WebUI/index.html`

---

## Output Format

```
╔══════════════════════════════════════════════════════════════╗
║  🎛️  BEATCONNECT PLUGIN VALIDATION                           ║
╚══════════════════════════════════════════════════════════════╝

┌──────────────────────────────────────────────────────────────┐
│  📦 PLUGIN INFO                                              │
├──────────────────────────────────────────────────────────────┤
│  Name              DelayWave                                 │
│  Version           1.0.0                                     │
│  Company           BeatConnect                               │
│  Manufacturer      Beat (4 chars ✓)                          │
│  Plugin Code       Dwav (4 chars ✓)                          │
│  Category          Effect                                    │
│  Formats           VST3, Standalone                          │
│  Frontend          Web UI (React)                            │
└──────────────────────────────────────────────────────────────┘

🔒 PRE-FLIGHT CHECKS ─────────────────────────────────── ✅ PASS
   ✅ No pre-compiled binaries in git
   ✅ No CMAKE_SOURCE_DIR bugs
   ✅ Root CMakeLists.txt has LANGUAGES C CXX

📁 PROJECT STRUCTURE ─────────────────────────────────── ✅ PASS
   ✅ CMakeLists.txt (root wrapper)
   ✅ plugin/CMakeLists.txt
   ✅ plugin/Source/ directory
   ✅ All required source files present
   ✅ beatconnect-sdk/ found

🌐 WEB UI BUILD ──────────────────────────────────────── ✅ PASS
   ✅ npm install succeeded
   ✅ npm run build succeeded
   ✅ Resources/WebUI/index.html generated

⚙️  CMAKE CONFIGURE ──────────────────────────────────── ✅ PASS
   ✅ cmake -B build-validate -S . succeeded
   ✅ [BeatConnect] detection messages present
   ✅ No errors or warnings

🔗 PARAMETER SYNC ────────────────────────────────────── ✅ PASS
   C++ │ time, feedback, mix, modRate, modDepth, tone, bypass
   TS  │ time, feedback, mix, modRate, modDepth, tone, bypass
   ✅ All 7 parameters match

╔══════════════════════════════════════════════════════════════╗
║  🎉 ALL CHECKS PASSED                                        ║
║  Your plugin is ready for BeatConnect build!                 ║
╚══════════════════════════════════════════════════════════════╝
```

### Failure Example

```
╔══════════════════════════════════════════════════════════════╗
║  🎛️  BEATCONNECT PLUGIN VALIDATION                           ║
╚══════════════════════════════════════════════════════════════╝

┌──────────────────────────────────────────────────────────────┐
│  📦 PLUGIN INFO                                              │
├──────────────────────────────────────────────────────────────┤
│  Name              DelayWave                                 │
│  Version           1.0.0                                     │
│  ...                                                         │
└──────────────────────────────────────────────────────────────┘

🔒 PRE-FLIGHT CHECKS ─────────────────────────────────── ❌ FAIL
   ❌ CRITICAL: Root CMakeLists.txt missing!
      └─ CI runs 'cmake -S . -B build' from repo root
      └─ Create CMakeLists.txt at repo root (see fix below)

⚙️  CMAKE CONFIGURE ──────────────────────────────────── ❌ FAIL
   ❌ cmake failed with error:
      └─ CMake Error: The source directory does not contain CMakeLists.txt

╔══════════════════════════════════════════════════════════════╗
║  ⛔ 2 CRITICAL ISSUES FOUND                                  ║
║  Build will fail! Fix the issues below.                      ║
╚══════════════════════════════════════════════════════════════╝

🔧 REQUIRED FIXES
───────────────────────────────────────────────────────────────
1. Create root CMakeLists.txt:

   cmake_minimum_required(VERSION 3.22)
   project(Wrapper LANGUAGES C CXX)
   add_subdirectory(plugin)

2. Re-run /bc-validate after fixing
───────────────────────────────────────────────────────────────
```

---

## Common Issues This Catches

1. **Missing root CMakeLists.txt** - CI builds from repo root
2. **CMAKE_SOURCE_DIR bug** - Breaks when plugin/ is a subdirectory
3. **Missing LANGUAGES C** - Some dependencies need C compiler
4. **Pre-compiled binaries** - CI rejects repos with binaries
5. **Web UI build failures** - npm errors, TypeScript errors
6. **Parameter mismatches** - C++ and TS using different IDs
