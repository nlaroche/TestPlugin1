/**
 * BeatConnect Asset Downloader - Implementation
 *
 * Downloads assets (samples, presets, etc.) from BeatConnect's R2 storage.
 * Uses JUCE for HTTP client and file operations.
 */

#include "beatconnect/AssetDownloader.h"

#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#if __has_include(<juce_core/juce_core.h>)
    #define BEATCONNECT_USE_JUCE 1
    #include <juce_core/juce_core.h>
#else
    #define BEATCONNECT_USE_JUCE 0
    #error "Currently requires JUCE. Standalone implementation coming soon."
#endif

namespace beatconnect {

// ==============================================================================
// Status String Conversion
// ==============================================================================

const char* downloadStatusToString(DownloadStatus status) {
    switch (status) {
        case DownloadStatus::Success:       return "Download completed";
        case DownloadStatus::NotFound:      return "Asset not found";
        case DownloadStatus::Unauthorized:  return "Not authorized";
        case DownloadStatus::NetworkError:  return "Network error";
        case DownloadStatus::DiskError:     return "Could not write file";
        case DownloadStatus::Cancelled:     return "Download cancelled";
        case DownloadStatus::AlreadyExists: return "File already exists";
        case DownloadStatus::InvalidUrl:    return "Invalid download URL";
        case DownloadStatus::Corrupted:     return "File corrupted";
    }
    return "Unknown status";
}

// ==============================================================================
// AssetDownloader Implementation
// ==============================================================================

class AssetDownloader::Impl {
public:
    Impl() = default;
    ~Impl() {
        cancelAll();
    }

    void configure(const DownloaderConfig& config) {
        std::lock_guard<std::mutex> lock(mutex);
        this->config = config;
        configured = true;

#if BEATCONNECT_USE_JUCE
        downloadDir = juce::File(config.downloadPath);
        downloadDir.createDirectory();
#endif
    }

    void setAuthToken(const std::string& token) {
        std::lock_guard<std::mutex> lock(mutex);
        config.authToken = token;
    }

    std::optional<AssetInfo> getAssetInfo(const std::string& assetId) {
        if (!configured) return std::nullopt;

#if BEATCONNECT_USE_JUCE
        juce::URL url(juce::String(config.apiBaseUrl) +
                      "/content/" + juce::String(assetId) + "/info");

        juce::URL::InputStreamOptions options(
            juce::URL::ParameterHandling::inAddress);
        options.withConnectionTimeoutMs(config.requestTimeoutMs);

        if (!config.authToken.empty()) {
            options.withExtraHeaders(
                "Authorization: Bearer " + juce::String(config.authToken) + "\r\n");
        }

        auto stream = url.createInputStream(options);
        if (!stream) return std::nullopt;

        auto response = stream->readEntireStreamAsString();
        auto json = juce::JSON::parse(response);

        if (json.isVoid() || !json.getDynamicObject()) {
            return std::nullopt;
        }

        auto* obj = json.getDynamicObject();
        if (obj->hasProperty("error")) {
            return std::nullopt;
        }

        AssetInfo info;
        info.id = assetId;
        if (obj->hasProperty("name"))
            info.name = obj->getProperty("name").toString().toStdString();
        if (obj->hasProperty("type"))
            info.type = obj->getProperty("type").toString().toStdString();
        if (obj->hasProperty("mime_type"))
            info.mimeType = obj->getProperty("mime_type").toString().toStdString();
        if (obj->hasProperty("file_size"))
            info.fileSize = (int64_t)obj->getProperty("file_size");
        if (obj->hasProperty("checksum"))
            info.checksum = obj->getProperty("checksum").toString().toStdString();

        return info;
#else
        return std::nullopt;
#endif
    }

    std::string getDownloadUrl(const std::string& assetId) {
        if (!configured) return "";

#if BEATCONNECT_USE_JUCE
        juce::URL url(juce::String(config.apiBaseUrl) +
                      "/content/" + juce::String(assetId) + "/download-url");

        juce::URL::InputStreamOptions options(
            juce::URL::ParameterHandling::inAddress);
        options.withConnectionTimeoutMs(config.requestTimeoutMs);

        if (!config.authToken.empty()) {
            options.withExtraHeaders(
                "Authorization: Bearer " + juce::String(config.authToken) + "\r\n");
        }

        if (!config.pluginId.empty()) {
            url = url.withParameter("plugin_id", config.pluginId);
        }

        auto stream = url.createInputStream(options);
        if (!stream) return "";

        auto response = stream->readEntireStreamAsString();
        auto json = juce::JSON::parse(response);

        if (json.isVoid() || !json.getDynamicObject()) {
            return "";
        }

        auto* obj = json.getDynamicObject();
        if (obj->hasProperty("url")) {
            return obj->getProperty("url").toString().toStdString();
        }
#endif
        return "";
    }

    std::pair<DownloadStatus, std::string> download(
        const std::string& assetId,
        ProgressCallback progressCallback
    ) {
        if (!configured) {
            return {DownloadStatus::NetworkError, ""};
        }

        // Check if already downloading
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (activeDownloads.find(assetId) != activeDownloads.end()) {
                return {DownloadStatus::Success, ""}; // Already in progress
            }
            activeDownloads.insert(assetId);
        }

        // Get asset info for filename
        auto info = getAssetInfo(assetId);
        std::string fileName = info ? info->name : assetId;

#if BEATCONNECT_USE_JUCE
        auto localPath = downloadDir.getChildFile(fileName).getFullPathName()
            .toStdString();

        // Check if already exists
        if (config.skipExisting && juce::File(localPath).existsAsFile()) {
            std::lock_guard<std::mutex> lock(mutex);
            activeDownloads.erase(assetId);
            downloadedAssets[assetId] = localPath;
            return {DownloadStatus::AlreadyExists, localPath};
        }

        // Get download URL
        auto downloadUrl = getDownloadUrl(assetId);
        if (downloadUrl.empty()) {
            std::lock_guard<std::mutex> lock(mutex);
            activeDownloads.erase(assetId);
            return {DownloadStatus::NotFound, ""};
        }

        // Download the file
        auto result = downloadFromUrlInternal(downloadUrl, fileName,
                                               assetId, progressCallback);

        {
            std::lock_guard<std::mutex> lock(mutex);
            activeDownloads.erase(assetId);
            if (result.first == DownloadStatus::Success) {
                downloadedAssets[assetId] = result.second;
            }
        }

        return result;
#else
        std::lock_guard<std::mutex> lock(mutex);
        activeDownloads.erase(assetId);
        return {DownloadStatus::NetworkError, ""};
#endif
    }

    void downloadAsync(
        const std::string& assetId,
        ProgressCallback progressCallback,
        CompletionCallback completionCallback
    ) {
        std::thread([this, assetId, progressCallback, completionCallback]() {
            auto result = download(assetId, progressCallback);
            if (completionCallback) {
                completionCallback(result.first, result.second);
            }
        }).detach();
    }

    void downloadBatch(
        const std::vector<std::string>& assetIds,
        ProgressCallback progressCallback,
        BatchCompletionCallback completionCallback
    ) {
        std::thread([this, assetIds, progressCallback, completionCallback]() {
            std::atomic<int> succeeded{0};
            std::atomic<int> failed{0};
            std::atomic<int> current{0};

            // Simple sequential for now
            // TODO: Implement concurrent downloads
            for (const auto& assetId : assetIds) {
                if (cancelRequested) break;

                int idx = ++current;

                auto wrappedProgress = [&, idx](const DownloadProgress& p) {
                    if (progressCallback) {
                        DownloadProgress bp = p;
                        bp.currentFile = idx;
                        bp.totalFiles = (int)assetIds.size();
                        progressCallback(bp);
                    }
                };

                auto result = download(assetId, wrappedProgress);
                if (result.first == DownloadStatus::Success ||
                    result.first == DownloadStatus::AlreadyExists) {
                    ++succeeded;
                } else {
                    ++failed;
                }
            }

            if (completionCallback) {
                completionCallback(succeeded, failed);
            }
        }).detach();
    }

    std::pair<DownloadStatus, std::string> downloadFromUrl(
        const std::string& url,
        const std::string& fileName,
        ProgressCallback progressCallback
    ) {
        return downloadFromUrlInternal(url, fileName, "", progressCallback);
    }

    void cancel(const std::string& assetId) {
        std::lock_guard<std::mutex> lock(mutex);
        cancelledDownloads.insert(assetId);
    }

    void cancelAll() {
        cancelRequested = true;
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& id : activeDownloads) {
            cancelledDownloads.insert(id);
        }
    }

    bool isDownloading() const {
        std::lock_guard<std::mutex> lock(mutex);
        return !activeDownloads.empty();
    }

    bool isDownloaded(const std::string& assetId) const {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = downloadedAssets.find(assetId);
        if (it != downloadedAssets.end()) {
#if BEATCONNECT_USE_JUCE
            return juce::File(it->second).existsAsFile();
#endif
        }
        return false;
    }

    std::string getLocalPath(const std::string& assetId) const {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = downloadedAssets.find(assetId);
        if (it != downloadedAssets.end()) {
            return it->second;
        }
        return "";
    }

    bool deleteDownload(const std::string& assetId) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = downloadedAssets.find(assetId);
        if (it != downloadedAssets.end()) {
#if BEATCONNECT_USE_JUCE
            juce::File file(it->second);
            if (file.deleteFile()) {
                downloadedAssets.erase(it);
                return true;
            }
#endif
        }
        return false;
    }

    int64_t getTotalDownloadedSize() const {
        int64_t total = 0;
#if BEATCONNECT_USE_JUCE
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& pair : downloadedAssets) {
            juce::File file(pair.second);
            if (file.existsAsFile()) {
                total += file.getSize();
            }
        }
#endif
        return total;
    }

private:
    std::pair<DownloadStatus, std::string> downloadFromUrlInternal(
        const std::string& url,
        const std::string& fileName,
        const std::string& assetId,
        ProgressCallback progressCallback
    ) {
#if BEATCONNECT_USE_JUCE
        juce::URL downloadUrl(url);

        auto targetFile = downloadDir.getChildFile(fileName);
        auto tempFile = targetFile.getSiblingFile(fileName + ".download");

        // Create input stream
        int statusCode = 0;
        auto stream = downloadUrl.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(config.requestTimeoutMs)
                .withStatusCode(&statusCode));

        if (!stream) {
            if (statusCode == 401 || statusCode == 403) {
                return {DownloadStatus::Unauthorized, ""};
            }
            if (statusCode == 404) {
                return {DownloadStatus::NotFound, ""};
            }
            return {DownloadStatus::NetworkError, ""};
        }

        // Get content length
        int64_t contentLength = stream->getTotalLength();

        // Create output file
        auto output = tempFile.createOutputStream();
        if (!output) {
            return {DownloadStatus::DiskError, ""};
        }

        // Download with progress
        const int bufferSize = 65536;
        juce::HeapBlock<char> buffer(bufferSize);
        int64_t bytesRead = 0;
        auto startTime = juce::Time::getMillisecondCounter();

        while (!stream->isExhausted()) {
            // Check for cancellation
            if (!assetId.empty()) {
                std::lock_guard<std::mutex> lock(mutex);
                if (cancelledDownloads.find(assetId) != cancelledDownloads.end()) {
                    cancelledDownloads.erase(assetId);
                    output.reset();
                    tempFile.deleteFile();
                    return {DownloadStatus::Cancelled, ""};
                }
            }

            int read = stream->read(buffer, bufferSize);
            if (read <= 0) break;

            if (!output->write(buffer, read)) {
                output.reset();
                tempFile.deleteFile();
                return {DownloadStatus::DiskError, ""};
            }

            bytesRead += read;

            // Report progress
            if (progressCallback) {
                DownloadProgress progress;
                progress.assetId = assetId;
                progress.fileName = fileName;
                progress.bytesDownloaded = bytesRead;
                progress.totalBytes = contentLength;
                progress.percent = contentLength > 0
                    ? (float)bytesRead / contentLength * 100.0f
                    : 0.0f;

                auto elapsed = juce::Time::getMillisecondCounter() - startTime;
                if (elapsed > 0) {
                    progress.speedBytesPerSec = (float)bytesRead / elapsed * 1000.0f;
                }

                progressCallback(progress);
            }
        }

        output.reset();

        // Move temp file to final location
        if (!tempFile.moveFileTo(targetFile)) {
            tempFile.deleteFile();
            return {DownloadStatus::DiskError, ""};
        }

        return {DownloadStatus::Success, targetFile.getFullPathName().toStdString()};
#else
        return {DownloadStatus::NetworkError, ""};
#endif
    }

    mutable std::mutex mutex;
    DownloaderConfig config;
    bool configured = false;
    std::atomic<bool> cancelRequested{false};

#if BEATCONNECT_USE_JUCE
    juce::File downloadDir;
#endif

    std::unordered_set<std::string> activeDownloads;
    std::unordered_set<std::string> cancelledDownloads;
    std::unordered_map<std::string, std::string> downloadedAssets;
};

// ==============================================================================
// AssetDownloader Public API
// ==============================================================================

AssetDownloader::AssetDownloader() : pImpl(std::make_unique<Impl>()) {}
AssetDownloader::~AssetDownloader() = default;

void AssetDownloader::configure(const DownloaderConfig& config) {
    pImpl->configure(config);
}

void AssetDownloader::setAuthToken(const std::string& token) {
    pImpl->setAuthToken(token);
}

std::optional<AssetInfo> AssetDownloader::getAssetInfo(const std::string& assetId) {
    return pImpl->getAssetInfo(assetId);
}

std::string AssetDownloader::getDownloadUrl(const std::string& assetId) {
    return pImpl->getDownloadUrl(assetId);
}

std::pair<DownloadStatus, std::string> AssetDownloader::download(
    const std::string& assetId,
    ProgressCallback progressCallback
) {
    return pImpl->download(assetId, progressCallback);
}

void AssetDownloader::downloadAsync(
    const std::string& assetId,
    ProgressCallback progressCallback,
    CompletionCallback completionCallback
) {
    pImpl->downloadAsync(assetId, progressCallback, completionCallback);
}

void AssetDownloader::downloadBatch(
    const std::vector<std::string>& assetIds,
    ProgressCallback progressCallback,
    BatchCompletionCallback completionCallback
) {
    pImpl->downloadBatch(assetIds, progressCallback, completionCallback);
}

std::pair<DownloadStatus, std::string> AssetDownloader::downloadFromUrl(
    const std::string& url,
    const std::string& fileName,
    ProgressCallback progressCallback
) {
    return pImpl->downloadFromUrl(url, fileName, progressCallback);
}

void AssetDownloader::cancel(const std::string& assetId) {
    pImpl->cancel(assetId);
}

void AssetDownloader::cancelAll() {
    pImpl->cancelAll();
}

bool AssetDownloader::isDownloading() const {
    return pImpl->isDownloading();
}

bool AssetDownloader::isDownloaded(const std::string& assetId) const {
    return pImpl->isDownloaded(assetId);
}

std::string AssetDownloader::getLocalPath(const std::string& assetId) const {
    return pImpl->getLocalPath(assetId);
}

bool AssetDownloader::deleteDownload(const std::string& assetId) {
    return pImpl->deleteDownload(assetId);
}

int64_t AssetDownloader::getTotalDownloadedSize() const {
    return pImpl->getTotalDownloadedSize();
}

} // namespace beatconnect
