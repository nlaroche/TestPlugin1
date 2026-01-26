#pragma once

/**
 * BeatConnect Activation SDK
 *
 * Provides license activation, validation, and deactivation for plugins
 * distributed through the BeatConnect platform.
 *
 * IMPORTANT: This is NOT a singleton. Each plugin processor should own its
 * own Activation instance to avoid conflicts when multiple plugin instances
 * or versions are loaded in a DAW.
 *
 * Usage:
 *   beatconnect::ActivationConfig config;
 *   config.apiBaseUrl = "https://xxx.supabase.co";
 *   config.pluginId = "your-project-uuid";
 *   config.supabaseKey = "your-publishable-key";
 *
 *   // Create instance (store as member variable in your processor)
 *   auto activation = beatconnect::Activation::create(config);
 *
 *   if (!activation->isActivated()) {
 *       // Show activation dialog
 *       auto status = activation->activate(userEnteredCode);
 *       if (status == ActivationStatus::Valid) {
 *           // Success!
 *       }
 *   }
 */

#include <string>
#include <functional>
#include <memory>
#include <optional>

namespace beatconnect {

// ==============================================================================
// Debug Logging Utility (DEPRECATED - use Activation instance methods instead)
// ==============================================================================

/**
 * @deprecated Use Activation::debugLog() and related instance methods instead.
 *
 * WARNING: This static class uses global state which causes issues when multiple
 * plugin instances or versions are loaded in a DAW. Each plugin instance should
 * use the debug methods on their own Activation instance instead.
 *
 * Old usage (DEPRECATED):
 *   beatconnect::Debug::init("MyPlugin", true);
 *   beatconnect::Debug::log("message");
 *
 * New usage (PREFERRED):
 *   config.pluginName = "MyPlugin";
 *   config.enableDebugLogging = true;
 *   auto activation = Activation::create(config);
 *   activation->debugLog("message");
 */
class [[deprecated("Use Activation instance debug methods instead")]] Debug {
public:
    /**
     * Initialize debug logging for a plugin.
     * @param pluginName Name used for the log folder (e.g., "MyPlugin")
     * @param enabled Whether debug logging is enabled
     */
    static void init(const std::string& pluginName, bool enabled = false);

    /**
     * Check if debug logging is enabled.
     */
    static bool isEnabled();

    /**
     * Enable or disable debug logging at runtime.
     */
    static void setEnabled(bool enabled);

    /**
     * Log a debug message (only if enabled).
     * Thread-safe.
     */
    static void log(const std::string& message);

    /**
     * Clear the debug log file.
     */
    static void clearLog();

    /**
     * Get the path to the debug log file.
     */
    static std::string getLogFilePath();

    /**
     * Open the folder containing the log file in the system file manager.
     * Useful for helping users find logs for troubleshooting.
     */
    static void revealLogFile();

private:
    Debug() = delete;
};

// ==============================================================================
// Activation Status
// ==============================================================================

enum class ActivationStatus {
    Valid,           // Activation successful or already activated
    Invalid,         // Invalid activation code
    Revoked,         // Code has been revoked
    MaxReached,      // Maximum activations reached
    NetworkError,    // Could not reach server
    ServerError,     // Server returned an error
    NotConfigured,   // SDK not configured
    AlreadyActive,   // Already activated on this machine
    NotActivated     // Not currently activated
};

// Convert status to human-readable string
const char* activationStatusToString(ActivationStatus status);

// ==============================================================================
// Activation Info
// ==============================================================================

struct ActivationInfo {
    std::string activationCode;
    std::string machineId;
    std::string activatedAt;      // ISO 8601 timestamp
    std::string expiresAt;        // ISO 8601 timestamp (empty if no expiry)
    int currentActivations = 0;
    int maxActivations = 0;
    bool isValid = false;
};

// ==============================================================================
// Activation Configuration
// ==============================================================================

struct ActivationConfig {
    // BeatConnect API base URL (e.g., "https://xxx.supabase.co/functions/v1")
    std::string apiBaseUrl;

    // Your plugin's project ID (UUID from BeatConnect dashboard)
    std::string pluginId;

    // Supabase publishable key for API authentication
    std::string supabaseKey;

    // Optional: Path to store activation state (defaults to app data folder)
    std::string statePath;

    // Optional: Timeout for API requests in milliseconds (default: 10000)
    int requestTimeoutMs = 10000;

    // Optional: Whether to validate on startup (default: true)
    bool validateOnStartup = true;

    // Optional: How often to re-validate in seconds (default: 86400 = 24 hours, 0 = never)
    int revalidateIntervalSeconds = 86400;

    // Optional: Plugin name for debug logging (e.g., "MyPlugin")
    std::string pluginName;

    // Optional: Enable debug logging (default: false)
    bool enableDebugLogging = false;
};

// ==============================================================================
// Activation Class (Instance-based, NOT singleton)
// ==============================================================================

/**
 * IMPORTANT: This class is NOT a singleton. Each plugin instance should create
 * its own Activation instance using the create() factory method. This avoids
 * issues when multiple plugin instances or versions are loaded in a DAW.
 */
class Activation {
public:
    /**
     * Create a new Activation instance with the given configuration.
     * Each plugin processor should own its own instance.
     *
     * @param config Configuration options
     * @return Unique pointer to configured Activation instance
     */
    static std::unique_ptr<Activation> create(const ActivationConfig& config);

    // Destructor must be public for unique_ptr
    ~Activation();

    // Prevent copying (but allow move for unique_ptr)
    Activation(const Activation&) = delete;
    Activation& operator=(const Activation&) = delete;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * Check if SDK has been configured.
     */
    bool isConfigured() const;

    // =========================================================================
    // Activation State (Fast - Local Check)
    // =========================================================================

    /**
     * Check if currently activated (fast local check, no network).
     * Returns cached activation state.
     */
    bool isActivated() const;

    /**
     * Get current activation info (fast local check).
     * Returns nullopt if not activated.
     */
    std::optional<ActivationInfo> getActivationInfo() const;

    // =========================================================================
    // Activation Operations (Slow - Network Required)
    // =========================================================================

    /**
     * Activate with a license code.
     * Makes network request to BeatConnect API.
     * Thread-safe - can be called from any thread.
     *
     * @param code Activation code - supports UUID format (e.g., "fd5cf09b-b8f4-495c-a4b9-8404dd965b4c")
     *             or legacy format (e.g., "XXXX-XXXX-XXXX-XXXX")
     * @return Activation status
     */
    ActivationStatus activate(const std::string& code);

    /**
     * Deactivate current license on this machine.
     * Frees up an activation slot.
     *
     * @return Status indicating success or failure
     */
    ActivationStatus deactivate();

    /**
     * Validate current activation with server.
     * Updates local cache with server state.
     *
     * @return Status indicating if still valid
     */
    ActivationStatus validate();

    // =========================================================================
    // Async Operations (Non-blocking)
    // =========================================================================

    using StatusCallback = std::function<void(ActivationStatus)>;

    /**
     * Activate asynchronously.
     * @param code Activation code
     * @param callback Called on completion (may be on different thread)
     */
    void activateAsync(const std::string& code, StatusCallback callback);

    /**
     * Validate asynchronously.
     * @param callback Called on completion
     */
    void validateAsync(StatusCallback callback);

    // =========================================================================
    // State Persistence
    // =========================================================================

    /**
     * Load activation state from disk.
     * Called automatically on configure() if statePath is set.
     */
    void loadState();

    /**
     * Save activation state to disk.
     * Called automatically after successful activation.
     */
    void saveState();

    /**
     * Clear all stored activation state.
     * Does NOT deactivate on server - use deactivate() for that.
     */
    void clearState();

    // =========================================================================
    // Machine ID
    // =========================================================================

    /**
     * Get the machine ID used for activation.
     * This is a stable fingerprint of the current machine.
     */
    std::string getMachineId() const;

    // =========================================================================
    // Debug Logging (Instance-based)
    // =========================================================================

    using DebugCallback = std::function<void(const std::string&)>;

    /**
     * Set a callback for debug logging.
     * When set, the SDK will call this with debug messages during operations.
     * Pass nullptr to disable debug logging.
     */
    void setDebugCallback(DebugCallback callback);

    /**
     * Log a debug message (if debug logging is enabled in config).
     * Thread-safe. Each instance logs to its own file.
     */
    void debugLog(const std::string& message);

    /**
     * Get the path to this instance's debug log file.
     */
    std::string getDebugLogPath() const;

    /**
     * Open the folder containing the log file in the system file manager.
     */
    void revealDebugLog();

    /**
     * Check if debug logging is enabled for this instance.
     */
    bool isDebugEnabled() const;

private:
    // Private constructor - use create() factory method
    explicit Activation(const ActivationConfig& config);

    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace beatconnect
