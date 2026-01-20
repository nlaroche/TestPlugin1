/**
 * BeatConnect Activation SDK - Implementation
 *
 * Uses JUCE for HTTP client and JSON parsing since plugins already have it.
 * Falls back to system APIs if JUCE is not available.
 */

#include "beatconnect/Activation.h"
#include "beatconnect/MachineId.h"

#include <mutex>
#include <thread>
#include <fstream>
#include <sstream>

#if __has_include(<juce_core/juce_core.h>)
    #define BEATCONNECT_USE_JUCE 1
    #include <juce_core/juce_core.h>
#else
    #define BEATCONNECT_USE_JUCE 0
    // TODO: Implement standalone HTTP client
    #error "Currently requires JUCE. Standalone implementation coming soon."
#endif

namespace beatconnect {

// ==============================================================================
// Status String Conversion
// ==============================================================================

const char* activationStatusToString(ActivationStatus status) {
    switch (status) {
        case ActivationStatus::Valid:         return "Valid";
        case ActivationStatus::Invalid:       return "Invalid activation code";
        case ActivationStatus::Revoked:       return "License has been revoked";
        case ActivationStatus::MaxReached:    return "Maximum activations reached";
        case ActivationStatus::NetworkError:  return "Network error - check connection";
        case ActivationStatus::ServerError:   return "Server error - try again later";
        case ActivationStatus::NotConfigured: return "SDK not configured";
        case ActivationStatus::AlreadyActive: return "Already activated";
        case ActivationStatus::NotActivated:  return "Not activated";
    }
    return "Unknown status";
}

// ==============================================================================
// Debug Logging Implementation
// ==============================================================================

namespace {
    std::mutex g_debugMutex;
    std::string g_pluginName;
    bool g_debugEnabled = false;
    std::string g_logFilePath;

    std::string getTimestamp() {
#if BEATCONNECT_USE_JUCE
        return juce::Time::getCurrentTime().toString(false, true, true, true).toStdString();
#else
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&time));
        return buf;
#endif
    }
}

void Debug::init(const std::string& pluginName, bool enabled) {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    g_pluginName = pluginName;
    g_debugEnabled = enabled;

#if BEATCONNECT_USE_JUCE
    // Build log file path: AppData/BeatConnect/<pluginName>/debug.log
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto logDir = appData.getChildFile("BeatConnect").getChildFile(juce::String(pluginName));
    logDir.createDirectory();
    g_logFilePath = logDir.getChildFile("debug.log").getFullPathName().toStdString();
#else
    g_logFilePath = pluginName + "_debug.log";
#endif

    if (enabled) {
        // Clear log on init when enabled
        std::ofstream ofs(g_logFilePath, std::ios::trunc);
        ofs << "[" << getTimestamp() << "] === Debug logging initialized for " << pluginName << " ===" << std::endl;
    }
}

bool Debug::isEnabled() {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    return g_debugEnabled;
}

void Debug::setEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    g_debugEnabled = enabled;
}

void Debug::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    if (!g_debugEnabled || g_logFilePath.empty()) return;

    std::ofstream ofs(g_logFilePath, std::ios::app);
    if (ofs.is_open()) {
        ofs << "[" << getTimestamp() << "] " << message << std::endl;
    }

#if BEATCONNECT_USE_JUCE
    DBG(juce::String("[BeatConnect] " + message));
#endif
}

void Debug::clearLog() {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    if (g_logFilePath.empty()) return;

#if BEATCONNECT_USE_JUCE
    juce::File(g_logFilePath).deleteFile();
#else
    std::ofstream ofs(g_logFilePath, std::ios::trunc);
#endif
}

std::string Debug::getLogFilePath() {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    return g_logFilePath;
}

void Debug::revealLogFile() {
#if BEATCONNECT_USE_JUCE
    std::lock_guard<std::mutex> lock(g_debugMutex);
    if (!g_logFilePath.empty()) {
        juce::File(g_logFilePath).revealToUser();
    }
#endif
}

// ==============================================================================
// Implementation Class
// ==============================================================================

class Activation::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    void setDebugCallback(Activation::DebugCallback callback) {
        std::lock_guard<std::mutex> lock(mutex);
        debugCallback = callback;
    }

    void debug(const std::string& msg) {
        // Always log to the Debug class (if enabled)
        Debug::log("[ActivationSDK] " + msg);

        // Also call the callback if set (for backward compatibility)
        Activation::DebugCallback cb;
        {
            std::lock_guard<std::mutex> lock(mutex);
            cb = debugCallback;
        }
        if (cb) {
            cb("[ActivationSDK] " + msg);
        }
    }

    void configure(const ActivationConfig& config) {
        std::lock_guard<std::mutex> lock(mutex);
        this->config = config;
        configured = true;

        // Set default state path if not provided
        if (config.statePath.empty()) {
#if BEATCONNECT_USE_JUCE
            auto appData = juce::File::getSpecialLocation(
                juce::File::userApplicationDataDirectory);
            statePath = appData.getChildFile("BeatConnect")
                              .getChildFile(config.pluginId)
                              .getChildFile("activation.json")
                              .getFullPathName().toStdString();
#else
            // Fallback
            statePath = "activation.json";
#endif
        } else {
            statePath = config.statePath;
        }

        // Load existing state
        loadState();

        // NOTE: validateOnStartup is intentionally NOT performed here during plugin load.
        // Many DAWs (Ableton, Logic) don't tolerate network activity during plugin initialization.
        // Validation should be triggered by the UI layer after the editor is fully constructed.
        // The saved activation state is trusted until explicitly validated.
    }

    bool isConfigured() const {
        return configured;
    }

    bool isActivated() const {
        std::lock_guard<std::mutex> lock(mutex);
        return activated && activationInfo.isValid;
    }

    std::optional<ActivationInfo> getActivationInfo() const {
        std::lock_guard<std::mutex> lock(mutex);
        if (activated) {
            return activationInfo;
        }
        return std::nullopt;
    }

    ActivationStatus activate(const std::string& code) {
        if (!configured) {
            debug("activate: Not configured");
            return ActivationStatus::NotConfigured;
        }

        auto machineId = MachineId::generate();
        debug("activate: machineId = " + machineId);

        // Build request
#if BEATCONNECT_USE_JUCE
        // apiBaseUrl is just the Supabase URL (e.g., https://xxx.supabase.co)
        // We need to add /functions/v1/ to reach edge functions
        juce::String urlStr = juce::String(config.apiBaseUrl) + "/functions/v1/plugin-activation/activate";
        debug("activate: URL = " + urlStr.toStdString());
        juce::URL url(urlStr);

        juce::DynamicObject::Ptr body = new juce::DynamicObject();
        body->setProperty("code", juce::String(code));
        body->setProperty("plugin_id", juce::String(config.pluginId));
        body->setProperty("machine_id", juce::String(machineId));

        auto jsonBody = juce::JSON::toString(juce::var(body.get()));
        debug("activate: Request body = " + jsonBody.toStdString());

        juce::URL::InputStreamOptions options(
            juce::URL::ParameterHandling::inPostData);

        // Build headers with Supabase authentication
        juce::String headers = "Content-Type: application/json\r\n";
        if (!config.supabaseKey.empty()) {
            headers += "apikey: " + juce::String(config.supabaseKey) + "\r\n";
            headers += "Authorization: Bearer " + juce::String(config.supabaseKey) + "\r\n";
            debug("activate: Using supabaseKey (length=" + std::to_string(config.supabaseKey.length()) + ")");
        } else {
            debug("activate: WARNING - No supabaseKey configured!");
        }
        options.withExtraHeaders(headers);
        options.withConnectionTimeoutMs(config.requestTimeoutMs);
        options.withNumRedirectsToFollow(0);

        url = url.withPOSTData(jsonBody);

        debug("activate: Creating input stream...");
        auto stream = url.createInputStream(options);
        if (!stream) {
            debug("activate: FAILED - createInputStream returned null (NetworkError)");
            return ActivationStatus::NetworkError;
        }

        debug("activate: Reading response...");
        auto response = stream->readEntireStreamAsString();
        debug("activate: Response (length=" + std::to_string(response.length()) + "): " + response.toStdString());

        auto json = juce::JSON::parse(response);

        if (json.isVoid()) {
            debug("activate: FAILED - JSON parse returned void (ServerError)");
            return ActivationStatus::ServerError;
        }

        auto* obj = json.getDynamicObject();
        if (!obj) {
            debug("activate: FAILED - JSON is not an object (ServerError)");
            return ActivationStatus::ServerError;
        }

        // Check for error
        if (obj->hasProperty("error")) {
            auto error = obj->getProperty("error").toString().toStdString();
            debug("activate: Server returned error: " + error);
            if (error.find("Invalid") != std::string::npos) {
                return ActivationStatus::Invalid;
            }
            if (error.find("revoked") != std::string::npos) {
                return ActivationStatus::Revoked;
            }
            if (error.find("maximum") != std::string::npos ||
                error.find("limit") != std::string::npos) {
                return ActivationStatus::MaxReached;
            }
            return ActivationStatus::ServerError;
        }

        // Success - update state
        {
            std::lock_guard<std::mutex> lock(mutex);
            activationInfo.activationCode = code;
            activationInfo.machineId = machineId;
            activationInfo.isValid = true;

            // Server returns "activated_at" or we generate it locally
            if (obj->hasProperty("activated_at")) {
                activationInfo.activatedAt = obj->getProperty("activated_at")
                    .toString().toStdString();
            } else {
                // Generate timestamp locally (server doesn't always return it)
                activationInfo.activatedAt = juce::Time::getCurrentTime()
                    .toISO8601(true).toStdString();
            }

            // Server returns "activations" (not "current_activations")
            if (obj->hasProperty("activations")) {
                activationInfo.currentActivations = static_cast<int>(
                    obj->getProperty("activations"));
            } else if (obj->hasProperty("current_activations")) {
                activationInfo.currentActivations = static_cast<int>(
                    obj->getProperty("current_activations"));
            }

            if (obj->hasProperty("max_activations")) {
                activationInfo.maxActivations = static_cast<int>(
                    obj->getProperty("max_activations"));
            }

            activated = true;
        }

        saveState();
        return ActivationStatus::Valid;
#else
        return ActivationStatus::NotConfigured;
#endif
    }

    ActivationStatus deactivate() {
        if (!configured) {
            return ActivationStatus::NotConfigured;
        }

        std::string code;
        std::string machineId;

        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!activated) {
                return ActivationStatus::NotActivated;
            }
            code = activationInfo.activationCode;
            machineId = activationInfo.machineId;
        }

#if BEATCONNECT_USE_JUCE
        juce::URL url(juce::String(config.apiBaseUrl) + "/functions/v1/plugin-activation/deactivate");

        juce::DynamicObject::Ptr body = new juce::DynamicObject();
        body->setProperty("code", juce::String(code));
        body->setProperty("plugin_id", juce::String(config.pluginId));
        body->setProperty("machine_id", juce::String(machineId));

        auto jsonBody = juce::JSON::toString(juce::var(body.get()));

        juce::URL::InputStreamOptions options(
            juce::URL::ParameterHandling::inPostData);

        // Build headers with Supabase authentication
        juce::String headers = "Content-Type: application/json\r\n";
        if (!config.supabaseKey.empty()) {
            headers += "apikey: " + juce::String(config.supabaseKey) + "\r\n";
            headers += "Authorization: Bearer " + juce::String(config.supabaseKey) + "\r\n";
        }
        options.withExtraHeaders(headers);
        options.withConnectionTimeoutMs(config.requestTimeoutMs);

        url = url.withPOSTData(jsonBody);

        auto stream = url.createInputStream(options);
        if (!stream) {
            return ActivationStatus::NetworkError;
        }

        auto response = stream->readEntireStreamAsString();
        auto json = juce::JSON::parse(response);

        if (json.isVoid() || !json.getDynamicObject()) {
            return ActivationStatus::ServerError;
        }

        auto* obj = json.getDynamicObject();
        if (obj->hasProperty("error")) {
            return ActivationStatus::ServerError;
        }

        // Success - clear local state
        {
            std::lock_guard<std::mutex> lock(mutex);
            activated = false;
            activationInfo = ActivationInfo{};
        }

        clearState();
        return ActivationStatus::Valid;
#else
        return ActivationStatus::NotConfigured;
#endif
    }

    ActivationStatus validate() {
        if (!configured) {
            return ActivationStatus::NotConfigured;
        }

        std::string code;
        std::string machineId;

        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!activated) {
                return ActivationStatus::NotActivated;
            }
            code = activationInfo.activationCode;
            machineId = activationInfo.machineId;
        }

#if BEATCONNECT_USE_JUCE
        juce::URL url(juce::String(config.apiBaseUrl) + "/functions/v1/plugin-activation/validate");

        juce::DynamicObject::Ptr body = new juce::DynamicObject();
        body->setProperty("code", juce::String(code));
        body->setProperty("plugin_id", juce::String(config.pluginId));
        body->setProperty("machine_id", juce::String(machineId));

        auto jsonBody = juce::JSON::toString(juce::var(body.get()));

        juce::URL::InputStreamOptions options(
            juce::URL::ParameterHandling::inPostData);

        // Build headers with Supabase authentication
        juce::String headers = "Content-Type: application/json\r\n";
        if (!config.supabaseKey.empty()) {
            headers += "apikey: " + juce::String(config.supabaseKey) + "\r\n";
            headers += "Authorization: Bearer " + juce::String(config.supabaseKey) + "\r\n";
        }
        options.withExtraHeaders(headers);
        options.withConnectionTimeoutMs(config.requestTimeoutMs);

        url = url.withPOSTData(jsonBody);

        auto stream = url.createInputStream(options);
        if (!stream) {
            return ActivationStatus::NetworkError;
        }

        auto response = stream->readEntireStreamAsString();
        auto json = juce::JSON::parse(response);

        if (json.isVoid() || !json.getDynamicObject()) {
            return ActivationStatus::ServerError;
        }

        auto* obj = json.getDynamicObject();

        // Check for specific errors
        if (obj->hasProperty("error")) {
            auto error = obj->getProperty("error").toString().toStdString();
            if (error.find("revoked") != std::string::npos) {
                std::lock_guard<std::mutex> lock(mutex);
                activationInfo.isValid = false;
                return ActivationStatus::Revoked;
            }
            if (error.find("Invalid") != std::string::npos) {
                std::lock_guard<std::mutex> lock(mutex);
                activationInfo.isValid = false;
                return ActivationStatus::Invalid;
            }
            return ActivationStatus::ServerError;
        }

        // Check valid flag
        bool isValid = obj->getProperty("valid");
        {
            std::lock_guard<std::mutex> lock(mutex);
            activationInfo.isValid = isValid;
        }

        return isValid ? ActivationStatus::Valid : ActivationStatus::Invalid;
#else
        return ActivationStatus::NotConfigured;
#endif
    }

    void activateAsync(const std::string& code, StatusCallback callback) {
        std::thread([this, code, callback]() {
            auto status = activate(code);
            if (callback) {
                callback(status);
            }
        }).detach();
    }

    void validateAsync(StatusCallback callback) {
        std::thread([this, callback]() {
            auto status = validate();
            if (callback) {
                callback(status);
            }
        }).detach();
    }

    void loadState() {
#if BEATCONNECT_USE_JUCE
        if (statePath.empty()) {
            return;
        }

        juce::File file(statePath);

        // Simple check: if the activation file exists, consider activated.
        // No parsing, no verification - just existence check.
        // This avoids any potential issues during DAW plugin scanning.
        if (file.existsAsFile()) {
            std::lock_guard<std::mutex> lock(mutex);
            activated = true;
            activationInfo.isValid = true;
        }
#endif
    }

    void saveState() {
#if BEATCONNECT_USE_JUCE
        juce::File file(statePath);
        file.getParentDirectory().createDirectory();

        juce::DynamicObject::Ptr obj = new juce::DynamicObject();

        {
            std::lock_guard<std::mutex> lock(mutex);
            obj->setProperty("activation_code",
                juce::String(activationInfo.activationCode));
            obj->setProperty("machine_id",
                juce::String(activationInfo.machineId));
            obj->setProperty("activated_at",
                juce::String(activationInfo.activatedAt));
            obj->setProperty("is_valid", activationInfo.isValid);
            obj->setProperty("current_activations",
                activationInfo.currentActivations);
            obj->setProperty("max_activations",
                activationInfo.maxActivations);
        }

        auto json = juce::JSON::toString(juce::var(obj.get()));
        file.replaceWithText(json);
#endif
    }

    void clearState() {
#if BEATCONNECT_USE_JUCE
        juce::File file(statePath);
        if (file.existsAsFile()) {
            file.deleteFile();
        }
#endif
    }

    std::string getMachineId() const {
        return MachineId::generate();
    }

private:
    mutable std::mutex mutex;
    ActivationConfig config;
    std::string statePath;
    bool configured = false;
    bool activated = false;
    ActivationInfo activationInfo;
    Activation::DebugCallback debugCallback;
};

// ==============================================================================
// Singleton Implementation
// ==============================================================================

Activation::Activation() : pImpl(std::make_unique<Impl>()) {}
Activation::~Activation() = default;

Activation& Activation::getInstance() {
    static Activation instance;
    return instance;
}

void Activation::configure(const ActivationConfig& config) {
    pImpl->configure(config);
}

bool Activation::isConfigured() const {
    return pImpl->isConfigured();
}

bool Activation::isActivated() const {
    return pImpl->isActivated();
}

std::optional<ActivationInfo> Activation::getActivationInfo() const {
    return pImpl->getActivationInfo();
}

ActivationStatus Activation::activate(const std::string& code) {
    return pImpl->activate(code);
}

ActivationStatus Activation::deactivate() {
    return pImpl->deactivate();
}

ActivationStatus Activation::validate() {
    return pImpl->validate();
}

void Activation::activateAsync(const std::string& code, StatusCallback callback) {
    pImpl->activateAsync(code, callback);
}

void Activation::validateAsync(StatusCallback callback) {
    pImpl->validateAsync(callback);
}

void Activation::loadState() {
    pImpl->loadState();
}

void Activation::saveState() {
    pImpl->saveState();
}

void Activation::clearState() {
    pImpl->clearState();
}

std::string Activation::getMachineId() const {
    return pImpl->getMachineId();
}

void Activation::setDebugCallback(DebugCallback callback) {
    pImpl->setDebugCallback(callback);
}

} // namespace beatconnect
