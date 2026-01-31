#pragma once

/**
 * BeatConnect Asset Downloader
 *
 * Download samples, presets, and other assets from BeatConnect's R2 storage.
 * Supports progress tracking, resume, and concurrent downloads.
 *
 * Usage:
 *   beatconnect::AssetDownloader downloader;
 *   downloader.configure({
 *       .apiBaseUrl = "https://xxx.supabase.co/functions/v1",
 *       .downloadPath = "/path/to/assets",
 *       .authToken = "user-jwt-token"  // For authenticated downloads
 *   });
 *
 *   // Download single asset
 *   downloader.download(assetId, [](const DownloadProgress& p) {
 *       updateProgressBar(p.percent);
 *   });
 *
 *   // Download multiple assets
 *   downloader.downloadBatch(assetIds, progressCallback, completionCallback);
 */

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <optional>

namespace beatconnect {

// ==============================================================================
// Download Status
// ==============================================================================

enum class DownloadStatus {
    Success,         // Download completed successfully
    NotFound,        // Asset not found
    Unauthorized,    // Not authorized to download
    NetworkError,    // Network connectivity issue
    DiskError,       // Could not write to disk
    Cancelled,       // Download was cancelled
    AlreadyExists,   // File already exists (skip)
    InvalidUrl,      // Invalid download URL
    Corrupted        // Downloaded file is corrupted (checksum mismatch)
};

const char* downloadStatusToString(DownloadStatus status);

// ==============================================================================
// Download Progress
// ==============================================================================

struct DownloadProgress {
    std::string assetId;
    std::string fileName;
    int64_t bytesDownloaded = 0;
    int64_t totalBytes = 0;
    float percent = 0.0f;          // 0.0 to 100.0
    float speedBytesPerSec = 0.0f;
    int currentFile = 0;           // For batch downloads
    int totalFiles = 0;
};

// ==============================================================================
// Asset Info
// ==============================================================================

struct AssetInfo {
    std::string id;
    std::string name;
    std::string type;              // "sample", "preset", "bundle"
    std::string mimeType;
    int64_t fileSize = 0;
    std::string checksum;          // MD5 or SHA-256
    std::string downloadUrl;       // Presigned R2 URL
    int64_t expiresAt = 0;         // URL expiry timestamp
};

// ==============================================================================
// Downloader Configuration
// ==============================================================================

struct DownloaderConfig {
    // BeatConnect API base URL
    std::string apiBaseUrl;

    // Local path to save downloaded files
    std::string downloadPath;

    // JWT token for authenticated downloads (optional for public assets)
    std::string authToken;

    // Plugin ID for tracking downloads
    std::string pluginId;

    // Request timeout in milliseconds
    int requestTimeoutMs = 30000;

    // Whether to verify checksums after download
    bool verifyChecksums = true;

    // Whether to skip existing files
    bool skipExisting = true;

    // Maximum concurrent downloads
    int maxConcurrent = 2;
};

// ==============================================================================
// Asset Downloader
// ==============================================================================

class AssetDownloader {
public:
    AssetDownloader();
    ~AssetDownloader();

    // Prevent copying
    AssetDownloader(const AssetDownloader&) = delete;
    AssetDownloader& operator=(const AssetDownloader&) = delete;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * Configure the downloader.
     */
    void configure(const DownloaderConfig& config);

    /**
     * Update auth token (e.g., after user login).
     */
    void setAuthToken(const std::string& token);

    // =========================================================================
    // Asset Discovery
    // =========================================================================

    /**
     * Get info about an asset without downloading.
     * @param assetId Asset ID from BeatConnect
     * @return Asset info or nullopt if not found
     */
    std::optional<AssetInfo> getAssetInfo(const std::string& assetId);

    /**
     * Get download URL for an asset.
     * @param assetId Asset ID
     * @return Presigned download URL or empty string on error
     */
    std::string getDownloadUrl(const std::string& assetId);

    // =========================================================================
    // Download Operations
    // =========================================================================

    using ProgressCallback = std::function<void(const DownloadProgress&)>;
    using CompletionCallback = std::function<void(DownloadStatus, const std::string& filePath)>;
    using BatchCompletionCallback = std::function<void(int succeeded, int failed)>;

    /**
     * Download a single asset synchronously.
     * @param assetId Asset ID to download
     * @param progressCallback Called periodically with progress
     * @return Status and local file path
     */
    std::pair<DownloadStatus, std::string> download(
        const std::string& assetId,
        ProgressCallback progressCallback = nullptr
    );

    /**
     * Download a single asset asynchronously.
     */
    void downloadAsync(
        const std::string& assetId,
        ProgressCallback progressCallback,
        CompletionCallback completionCallback
    );

    /**
     * Download multiple assets.
     * Downloads run concurrently up to maxConcurrent limit.
     */
    void downloadBatch(
        const std::vector<std::string>& assetIds,
        ProgressCallback progressCallback,
        BatchCompletionCallback completionCallback
    );

    /**
     * Download from a presigned URL directly.
     * Use when you already have the URL (e.g., from package purchase).
     */
    std::pair<DownloadStatus, std::string> downloadFromUrl(
        const std::string& url,
        const std::string& fileName,
        ProgressCallback progressCallback = nullptr
    );

    // =========================================================================
    // Download Control
    // =========================================================================

    /**
     * Cancel a specific download.
     */
    void cancel(const std::string& assetId);

    /**
     * Cancel all active downloads.
     */
    void cancelAll();

    /**
     * Check if any downloads are in progress.
     */
    bool isDownloading() const;

    // =========================================================================
    // Local File Management
    // =========================================================================

    /**
     * Check if an asset has been downloaded.
     */
    bool isDownloaded(const std::string& assetId) const;

    /**
     * Get local file path for a downloaded asset.
     * Returns empty string if not downloaded.
     */
    std::string getLocalPath(const std::string& assetId) const;

    /**
     * Delete a downloaded asset from disk.
     */
    bool deleteDownload(const std::string& assetId);

    /**
     * Get total size of all downloaded files.
     */
    int64_t getTotalDownloadedSize() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace beatconnect
