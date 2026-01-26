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
        std::lock_guard<std::mutex> lock(debugMutex);
        debugCallback = callback;
    }

    // Instance-based debug logging (no global state)
    void debug(const std::string& msg) {
        // Log to instance's own log file if enabled
        debugLog("[ActivationSDK] " + msg);

        // Also call the callback if set
        Activation::DebugCallback cb;
        {
            std::lock_guard<std::mutex> lock(debugMutex);
            cb = debugCallback;
        }
        if (cb) {
            cb("[ActivationSDK] " + msg);
        }
    }

    void debugLog(const std::string& msg) {
        std::lock_guard<std::mutex> lock(debugMutex);
        if (!debugEnabled || debugLogPath.empty()) return;

#if BEATCONNECT_USE_JUCE
        std::string timestamp = juce::Time::getCurrentTime()
            .toString(false, true, true, true).toStdString();

        juce::File logFile(debugLogPath);
        logFile.appendText("[" + timestamp + "] " + msg + "\n");
        DBG(juce::String("[BeatConnect] " + msg));
#else
        std::ofstream ofs(debugLogPath, std::ios::app);
        if (ofs.is_open()) {
            ofs << msg << std::endl;
        }
#endif
    }

    std::string getDebugLogPath() const {
        std::lock_guard<std::mutex> lock(debugMutex);
        return debugLogPath;
    }

    void revealDebugLog() {
#if BEATCONNECT_USE_JUCE
        std::lock_guard<std::mutex> lock(debugMutex);
        if (!debugLogPath.empty()) {
            juce::File(debugLogPath).revealToUser();
        }
#endif
    }

    bool isDebugEnabled() const {
        std::lock_guard<std::mutex> lock(debugMutex);
        return debugEnabled;
    }

    // Simple init-time logging (before debug is fully configured)
    void initLog(const std::string& msg) {
#if BEATCONNECT_USE_JUCE
        auto logFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("BeatConnect")
            .getChildFile("init.log");
        logFile.getParentDirectory().createDirectory();
        logFile.appendText(msg + "\n");
        DBG(juce::String(msg));
#endif
    }

    void configure(const ActivationConfig& config) {
        // No mutex needed - this runs on main thread during plugin init
        initLog("[Activation] configure() called");

        this->config = config;
        configured = true;
        initLog("[Activation] config set, configured=true");

        // Setup instance-based debug logging
        {
            std::lock_guard<std::mutex> lock(debugMutex);
            debugEnabled = config.enableDebugLogging;

            // Determine plugin name for log path
            std::string pluginNameForLog = config.pluginName;
            if (pluginNameForLog.empty()) {
                pluginNameForLog = config.pluginId;  // Fallback to pluginId
            }

#if BEATCONNECT_USE_JUCE
            if (!pluginNameForLog.empty()) {
                auto appData = juce::File::getSpecialLocation(
                    juce::File::userApplicationDataDirectory);
                auto logDir = appData.getChildFile("BeatConnect")
                                     .getChildFile(juce::String(pluginNameForLog));
                logDir.createDirectory();
                debugLogPath = logDir.getChildFile("debug.log")
                                     .getFullPathName().toStdString();
            }
#else
            debugLogPath = pluginNameForLog + "_debug.log";
#endif

            if (debugEnabled && !debugLogPath.empty()) {
                // Clear log on init when enabled
                std::ofstream ofs(debugLogPath, std::ios::trunc);
                ofs << "=== Debug logging initialized for " << pluginNameForLog << " ===" << std::endl;
            }
        }

        // Set default state path if not provided
        if (config.statePath.empty()) {
#if BEATCONNECT_USE_JUCE
            auto appData = juce::File::getSpecialLocation(
                juce::File::userApplicationDataDirectory);
            initLog("[Activation] appData: " + appData.getFullPathName().toStdString());

            statePath = appData.getChildFile("BeatConnect")
                              .getChildFile(config.pluginId)
                              .getChildFile("activation.json")
                              .getFullPathName().toStdString();
            initLog("[Activation] statePath: " + statePath);
#else
            statePath = "activation.json";
#endif
        } else {
            statePath = config.statePath;
        }

        initLog("[Activation] about to call loadState()");
        loadState();
        initLog("[Activation] loadState() returned, configure() complete");
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
        initLog("[Activation] loadState() called");

        if (statePath.empty()) {
            initLog("[Activation] loadState: statePath is empty, returning");
            return;
        }

        initLog("[Activation] loadState: checking file at " + statePath);
        juce::File file(statePath);

        initLog("[Activation] loadState: juce::File created");
        bool exists = file.existsAsFile();
        initLog("[Activation] loadState: existsAsFile() = " + std::string(exists ? "true" : "false"));

        if (exists) {
            initLog("[Activation] loadState: setting activated=true");
            activated = true;
            activationInfo.isValid = true;
            initLog("[Activation] loadState: flags set");
        }

        initLog("[Activation] loadState() complete");
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

    // Instance-based debug state (no global statics!)
    mutable std::mutex debugMutex;
    bool debugEnabled = false;
    std::string debugLogPath;
    Activation::DebugCallback debugCallback;
};

// ==============================================================================
// Instance-based Implementation (NOT singleton)
// ==============================================================================

Activation::Activation(const ActivationConfig& config) : pImpl(std::make_unique<Impl>()) {
    pImpl->configure(config);
}

Activation::~Activation() = default;

std::unique_ptr<Activation> Activation::create(const ActivationConfig& config) {
    // Can't use make_unique because constructor is private
    return std::unique_ptr<Activation>(new Activation(config));
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

void Activation::debugLog(const std::string& message) {
    pImpl->debugLog(message);
}

std::string Activation::getDebugLogPath() const {
    return pImpl->getDebugLogPath();
}

void Activation::revealDebugLog() {
    pImpl->revealDebugLog();
}

bool Activation::isDebugEnabled() const {
    return pImpl->isDebugEnabled();
}

} // namespace beatconnect
