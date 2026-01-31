# BeatConnect Integration Guide

This guide explains how to integrate your plugin with BeatConnect's build pipeline, activation system, and distribution platform.

## Overview

BeatConnect provides:
1. **Build Pipeline** - Automated compilation with code signing (Azure for Windows, Apple notarization for macOS)
2. **Activation System** - License codes with configurable machine limits
3. **Asset Downloads** - Presigned R2 URLs for samples, presets, and other content
4. **Distribution** - Signed download links, Stripe payment integration

## Setup Steps

### 1. Register Your Plugin

Contact BeatConnect to register your plugin as an "external project". You'll receive:
- `PROJECT_ID` - UUID for your plugin
- `WEBHOOK_SECRET` - Secret for authenticating build requests

### 2. Configure GitHub Secrets

Add these secrets to your GitHub repository:
- `BEATCONNECT_PROJECT_ID` - Your plugin's UUID
- `BEATCONNECT_WEBHOOK_SECRET` - Webhook authentication secret
- `BEATCONNECT_API_URL` - BeatConnect API URL (e.g., `https://xxx.supabase.co`)

### 3. Enable BeatConnect Build Job

In your `.github/workflows/build.yml`, uncomment the `beatconnect-build` job:

```yaml
beatconnect-build:
  if: startsWith(github.ref, 'refs/tags/v')
  runs-on: ubuntu-latest
  steps:
    - name: Get version from tag
      id: version
      run: echo "version=${GITHUB_REF#refs/tags/v}" >> $GITHUB_OUTPUT

    - name: Trigger BeatConnect build
      run: |
        curl -X POST \
          "${{ secrets.BEATCONNECT_API_URL }}/functions/v1/external-builds/${{ secrets.BEATCONNECT_PROJECT_ID }}/trigger" \
          -H "x-webhook-secret: ${{ secrets.BEATCONNECT_WEBHOOK_SECRET }}" \
          -H "Content-Type: application/json" \
          -d '{
            "plugin_version": "${{ steps.version.outputs.version }}",
            "commit_sha": "${{ github.sha }}",
            "build_platforms": ["windows", "macos"],
            "build_formats": ["vst3", "au"]
          }'
```

### 4. Create a Release

Tag your release to trigger the build:

```bash
git tag v1.0.0
git push origin v1.0.0
```

## Build Flow

```
1. You push a tag (v1.0.0)
         │
         ▼
2. GitHub Actions triggers BeatConnect build API
         │
         ▼
3. BeatConnect creates build record
         │
         ▼
4. GitHub Actions on BeatConnect's runners:
   - Checkout your repo
   - Build for Windows & macOS
   - Sign binaries (Azure/Apple)
   - Create installers
   - Upload to R2 storage
         │
         ▼
5. Build artifacts available in BeatConnect dashboard
```

---

## Activation System

### How It Works

1. Customer purchases your plugin via Stripe
2. BeatConnect generates activation code (e.g., `XXXX-XXXX-XXXX-XXXX`)
3. Customer enters code in plugin
4. Plugin validates against BeatConnect API
5. Activation recorded (machine fingerprint + timestamp)

### API Endpoints

```
POST /plugin-activation/activate
POST /plugin-activation/deactivate
POST /plugin-activation/validate
GET  /plugin-activation/status/:code
```

### Integration

Add the SDK as a submodule:

```bash
git submodule add https://github.com/beatconnect/beatconnect-plugin-sdk beatconnect-sdk
```

Update your `CMakeLists.txt`:

```cmake
# Add BeatConnect SDK
add_subdirectory(beatconnect-sdk/sdk/activation)
target_link_libraries(${PROJECT_NAME} PRIVATE beatconnect_activation)
```

### Usage in Plugin

**PluginProcessor.h:**

```cpp
#include <beatconnect/Activation.h>

class MyPluginProcessor : public juce::AudioProcessor {
public:
    bool isLicenseValid() const {
        return beatconnect::Activation::getInstance().isActivated();
    }
};
```

**PluginEditor.cpp (on startup):**

```cpp
#include <beatconnect/Activation.h>

void MyPluginEditor::checkActivation()
{
    // Configure SDK (do this once on startup)
    beatconnect::ActivationConfig config;
    config.apiBaseUrl = "https://your-supabase.supabase.co/functions/v1";
    config.pluginId = "your-project-uuid";
    config.validateOnStartup = true;

    auto& activation = beatconnect::Activation::getInstance();
    activation.configure(config);

    // Check if activated
    if (!activation.isActivated()) {
        showActivationDialog();
    }
}
```

**Activation Dialog:**

```cpp
void ActivationDialog::onActivateClicked()
{
    auto& activation = beatconnect::Activation::getInstance();

    // Activate asynchronously
    activation.activateAsync(codeInput.getText().toStdString(),
        [this](beatconnect::ActivationStatus status) {
            juce::MessageManager::callAsync([this, status]() {
                if (status == beatconnect::ActivationStatus::Valid) {
                    showSuccess("Activated successfully!");
                    closeDialog();
                } else {
                    showError(beatconnect::activationStatusToString(status));
                }
            });
        });
}
```

### Activation Status Codes

| Status | Meaning |
|--------|---------|
| `Valid` | Activation successful |
| `Invalid` | Invalid activation code |
| `Revoked` | License has been revoked |
| `MaxReached` | Maximum activations reached |
| `NetworkError` | Could not reach server |
| `ServerError` | Server returned an error |
| `NotConfigured` | SDK not configured |
| `AlreadyActive` | Already activated on this machine |
| `NotActivated` | Not currently activated |

---

## Asset Downloads

For plugins that include downloadable content (samples, presets, etc.), use the Asset Downloader SDK.

### Configuration

```cpp
#include <beatconnect/AssetDownloader.h>

void setupDownloader()
{
    beatconnect::DownloaderConfig config;
    config.apiBaseUrl = "https://your-supabase.supabase.co/functions/v1";
    config.downloadPath = getAssetDirectory().getFullPathName().toStdString();
    config.authToken = getUserAuthToken();  // If authenticated
    config.pluginId = "your-project-uuid";

    downloader.configure(config);
}
```

### Download Single Asset

```cpp
// Synchronous
auto [status, filePath] = downloader.download(assetId,
    [](const beatconnect::DownloadProgress& p) {
        updateProgressBar(p.percent);
    });

if (status == beatconnect::DownloadStatus::Success) {
    loadSample(filePath);
}

// Asynchronous
downloader.downloadAsync(assetId,
    [](const beatconnect::DownloadProgress& p) {
        // Update UI on progress
    },
    [](beatconnect::DownloadStatus status, const std::string& path) {
        // Handle completion
    });
```

### Download Package (All Assets)

```cpp
beatconnect::PackageDownloader packageDownloader;
packageDownloader.configure(config);

// Download all assets from a purchased package
packageDownloader.downloadPackage(packageId,
    [](const beatconnect::DownloadProgress& p) {
        updateProgress(p.percent, p.currentFile, p.totalFiles);
    },
    [](int succeeded, int failed) {
        showComplete(succeeded, failed);
    });
```

### Download Status Codes

| Status | Meaning |
|--------|---------|
| `Success` | Download completed |
| `NotFound` | Asset not found |
| `Unauthorized` | Not authorized to download |
| `NetworkError` | Network connectivity issue |
| `DiskError` | Could not write to disk |
| `Cancelled` | Download was cancelled |
| `AlreadyExists` | File already exists (skipped) |
| `Corrupted` | Checksum verification failed |

---

## Distribution

### Download Links

After a successful build, signed download links are available:
- Presigned R2 URLs (1-hour expiry)
- Increment download count automatically
- Track per-platform downloads

### Creating Products

1. Build completes successfully
2. Create product in BeatConnect dashboard
3. Link to build artifacts
4. Set pricing (Stripe integration)
5. Product page shows download buttons for purchasers

---

## Monitoring

### Build Status

Check build status via API:

```bash
curl "$BEATCONNECT_API_URL/functions/v1/external-builds/$PROJECT_ID/status" \
  -H "x-webhook-secret: $WEBHOOK_SECRET"
```

Response:
```json
{
  "project_id": "uuid",
  "builds": [
    {
      "id": "build-uuid",
      "version_number": "1.0.0",
      "status": "success",
      "created_at": "2024-01-14T...",
      "artifacts": [
        {"platform": "windows", "format": "installer", "file_size": 12345678},
        {"platform": "macos", "format": "installer", "file_size": 23456789}
      ]
    }
  ]
}
```

---

## Troubleshooting

### Build Fails

1. Check GitHub Actions logs on BeatConnect's repo
2. Verify your CMakeLists.txt builds locally
3. Ensure web UI is built before plugin

### Activation Issues

1. Verify PROJECT_ID matches
2. Check API URL is correct
3. Ensure machine ID is stable (not changing on each launch)
4. Check network connectivity

### Download Issues

1. Check build status is "success"
2. Verify artifacts were uploaded
3. Check presigned URL hasn't expired
4. Verify auth token is valid for authenticated downloads

---

## Complete Example

See `examples/basic-plugin/` for a complete plugin demonstrating:
- Activation flow with UI
- Asset downloading
- Sample loading
- Progress indicators
