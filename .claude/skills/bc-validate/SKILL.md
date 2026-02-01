# /bc-validate Skill

Validate a BeatConnect plugin project before building. Catches common issues early and saves build credits.

## Usage

```
/bc-validate
```

Run from your plugin repository root.

## What It Checks

### 1. Project Structure
- [ ] CMakeLists.txt exists (at root or in `plugin/`)
- [ ] Source/ directory exists
- [ ] Required source files present (PluginProcessor.cpp/h, PluginEditor.cpp/h)
- [ ] ParameterIDs.h exists
- [ ] beatconnect-sdk submodule initialized (if using SDK structure)

### 2. CMake Configuration
- [ ] CMakeLists.txt parses without errors
- [ ] `cmake_minimum_required(VERSION 3.22)` or higher
- [ ] `project()` defined with version
- [ ] `juce_add_plugin()` target defined
- [ ] Required JUCE modules linked

### 3. Frontend Type Detection
- [ ] Detect Web UI (`web-ui/` or `web/` directory)
- [ ] Detect Native JUCE UI (no web directory, JUCE UI code in PluginEditor)
- [ ] Validate frontend-specific requirements

### 4. Web UI Validation (if applicable)
- [ ] package.json exists
- [ ] `npm install` succeeds
- [ ] `npm run build` succeeds
- [ ] dist/ folder generated with index.html
- [ ] Parameter IDs match between C++ and TypeScript

### 5. Native UI Validation (if applicable)
- [ ] PluginEditor creates JUCE components (not WebBrowserComponent)
- [ ] UI components properly initialized

### 6. SDK Integration
- [ ] BeatConnectPlugin.cmake included (or equivalent config)
- [ ] BEATCONNECT_ENABLE_ACTIVATION option defined
- [ ] Activation SDK path correct

### 7. Build Readiness
- [ ] .gitignore excludes build artifacts
- [ ] No pre-compiled binaries in repo
- [ ] Repository can be cloned cleanly

---

## Validation Process

### Phase 1: Structure Check (No Build Required)

```bash
# Run these checks first - they're fast
```

1. **Find project root**
   ```
   Check for CMakeLists.txt at:
   - ./ (current directory)
   - ./plugin/
   ```

2. **Check required files**
   ```
   Required:
   - CMakeLists.txt
   - Source/PluginProcessor.cpp
   - Source/PluginProcessor.h
   - Source/PluginEditor.cpp
   - Source/PluginEditor.h
   - Source/ParameterIDs.h
   ```

3. **Detect frontend type**
   ```
   Web UI: web-ui/package.json OR web/package.json exists
   Native: No web directory found
   ```

4. **Check SDK**
   ```
   Look for beatconnect-sdk/ at:
   - ../beatconnect-sdk (SDK at repo root)
   - ./beatconnect-sdk (SDK in plugin dir)
   ```

### Phase 2: CMake Validation

1. **Parse CMakeLists.txt**
   - Check for required cmake commands
   - Verify juce_add_plugin() call
   - Check target_link_libraries

2. **Dry-run configure** (optional, if cmake available)
   ```bash
   cmake -B build-validate -S . --dry-run 2>&1
   ```

### Phase 3: Frontend Validation

**For Web UI:**
```bash
cd web-ui  # or web/
npm install
npm run build
# Check dist/index.html exists
```

**For Native UI:**
- Verify PluginEditor.cpp doesn't use WebBrowserComponent
- Check for proper JUCE component setup

### Phase 4: Parameter Sync Check (Web UI only)

Compare parameter IDs between:
- `Source/ParameterIDs.h` (C++ definitions)
- `web-ui/src/**/*.ts` (TypeScript usage)

Report any mismatches.

---

## Output Format

### Success Output
```
🔍 BeatConnect Plugin Validation

📁 Structure
  ✓ CMakeLists.txt found at plugin/
  ✓ Source/ directory exists
  ✓ All required source files present
  ✓ beatconnect-sdk found at ../beatconnect-sdk

🔧 CMake
  ✓ CMake version 3.22+ required
  ✓ Project: DelayWave v1.0.0
  ✓ juce_add_plugin() target defined
  ✓ BeatConnect SDK integration configured

🎨 Frontend: Web UI
  ✓ web-ui/package.json found
  ✓ npm install succeeded
  ✓ npm run build succeeded
  ✓ dist/index.html generated

🔗 Parameters
  ✓ 3 parameters defined in C++
  ✓ All parameters referenced in TypeScript
  ✓ No mismatches found

✅ All checks passed! Ready for BeatConnect build.

🎋 Haiku Summary:
  Code structure clean
  Web and native paths aligned
  Build will succeed now
```

### Failure Output
```
🔍 BeatConnect Plugin Validation

📁 Structure
  ✓ CMakeLists.txt found at plugin/
  ✓ Source/ directory exists
  ✗ Missing: Source/ParameterIDs.h
  ✓ beatconnect-sdk found

🔧 CMake
  ✓ CMake version 3.22+ required
  ✗ juce_add_plugin() not found - check CMakeLists.txt

🎨 Frontend: Web UI
  ✓ web-ui/package.json found
  ✗ npm install failed: ERESOLVE unable to resolve dependency tree
  ○ npm run build skipped (install failed)

❌ 3 issues found

🔧 Fixes:
1. Create Source/ParameterIDs.h with your parameter ID constants
2. Add juce_add_plugin() call to CMakeLists.txt (see example-plugin)
3. Run: cd web-ui && rm -rf node_modules package-lock.json && npm install

🎋 Haiku Summary:
  Parameters missing
  CMake config incomplete
  Fix before you build
```

---

## Implementation Notes

### File Reading Strategy

1. Read CMakeLists.txt and parse for:
   - `cmake_minimum_required`
   - `project(NAME VERSION X.Y.Z)`
   - `juce_add_plugin(TARGET_NAME ...)`
   - `target_link_libraries`
   - `include(...BeatConnectPlugin.cmake)`

2. Read ParameterIDs.h and extract:
   - `constexpr` or `const` string definitions
   - `inline constexpr auto` patterns
   - `#define` macros for IDs

3. For Web UI, grep TypeScript files for:
   - `useSliderParam("id")`
   - `useToggleParam("id")`
   - Parameter ID string literals

### Running Commands

Only run npm commands if:
- package.json exists
- Node.js is available (`which node`)

Use `--prefer-offline` for faster npm install.

### Haiku Generation

Generate a haiku (5-7-5 syllables) summarizing:
- Overall status (pass/fail)
- Main issues if any
- Encouraging if passing

---

## Integration with /bc-create-plugin

After scaffolding a new plugin, `/bc-create-plugin` should automatically run `/bc-validate` to ensure the generated project is valid.

```
# At end of /bc-create-plugin:
"Let me validate the generated project..."
[Run /bc-validate checks]
"All checks passed! Your plugin is ready."
```
