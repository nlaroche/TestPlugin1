#pragma once

/**
 * Machine ID Generation
 *
 * Generates a stable, unique fingerprint for the current machine.
 * Used to track activation slots.
 *
 * The machine ID is derived from:
 * - Windows: Volume serial number + machine GUID from registry
 * - macOS: IOPlatformSerialNumber + hardware UUID
 *
 * The ID is hashed (SHA-256) so actual hardware info isn't exposed.
 */

#include <string>

namespace beatconnect {

class MachineId {
public:
    /**
     * Generate the machine ID for the current system.
     * Returns a hex-encoded SHA-256 hash (64 characters).
     *
     * This is deterministic - same machine always returns same ID.
     * Format changes (reinstalls, etc.) don't affect the ID.
     */
    static std::string generate();

    /**
     * Get a shorter version of the machine ID (first 16 chars).
     * Useful for display purposes.
     */
    static std::string generateShort();

private:
    // Platform-specific implementations
    static std::string getWindowsMachineInfo();
    static std::string getMacOSMachineInfo();
    static std::string getLinuxMachineInfo();

    // Hash the raw machine info
    static std::string hashMachineInfo(const std::string& info);
};

} // namespace beatconnect
