/**
 * Machine ID Generation - Implementation
 *
 * Generates a stable machine fingerprint using platform-specific APIs.
 * The result is hashed so hardware info isn't exposed.
 */

#include "beatconnect/MachineId.h"

#include <sstream>
#include <iomanip>
#include <array>

#if _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <wincrypt.h>
    #pragma comment(lib, "advapi32.lib")
#elif __APPLE__
    #include <IOKit/IOKitLib.h>
    #include <CoreFoundation/CoreFoundation.h>
#elif __linux__
    #include <fstream>
    #include <unistd.h>
#endif

#if __has_include(<juce_core/juce_core.h>)
    #define BEATCONNECT_USE_JUCE 1
    #include <juce_core/juce_core.h>
    #if __has_include(<juce_cryptography/juce_cryptography.h>)
        #include <juce_cryptography/juce_cryptography.h>
    #endif
#else
    #define BEATCONNECT_USE_JUCE 0
#endif

namespace beatconnect {

// ==============================================================================
// SHA-256 Helper (using JUCE or system)
// ==============================================================================

#if BEATCONNECT_USE_JUCE

std::string MachineId::hashMachineInfo(const std::string& info) {
    juce::SHA256 hash(info.data(), info.size());
    return hash.toHexString().toStdString();
}

#elif _WIN32

// Windows CryptoAPI SHA-256
std::string MachineId::hashMachineInfo(const std::string& info) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string result;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES,
                             CRYPT_VERIFYCONTEXT)) {
        return "";
    }

    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }

    if (!CryptHashData(hHash, (BYTE*)info.c_str(), (DWORD)info.size(), 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    DWORD hashLen = 32;
    std::array<BYTE, 32> hashData;

    if (CryptGetHashParam(hHash, HP_HASHVAL, hashData.data(), &hashLen, 0)) {
        std::stringstream ss;
        for (auto byte : hashData) {
            ss << std::hex << std::setfill('0') << std::setw(2) << (int)byte;
        }
        result = ss.str();
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    return result;
}

#else

// Fallback: Simple hash (not cryptographic - for build testing only)
std::string MachineId::hashMachineInfo(const std::string& info) {
    // TODO: Implement proper SHA-256 for non-JUCE builds
    unsigned long hash = 5381;
    for (char c : info) {
        hash = ((hash << 5) + hash) + c;
    }

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << hash;
    // Pad to 64 chars to match SHA-256 format
    auto h = ss.str();
    while (h.size() < 64) h += h;
    return h.substr(0, 64);
}

#endif

// ==============================================================================
// Platform-Specific Machine Info
// ==============================================================================

#if _WIN32

std::string MachineId::getWindowsMachineInfo() {
    std::string info;

    // Get volume serial number of system drive
    char systemDrive[4] = "C:\\";
    DWORD volumeSerialNumber = 0;

    if (GetVolumeInformationA(systemDrive, NULL, 0, &volumeSerialNumber,
                               NULL, NULL, NULL, 0)) {
        info += "VOL:" + std::to_string(volumeSerialNumber) + ";";
    }

    // Get MachineGuid from registry
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "SOFTWARE\\Microsoft\\Cryptography",
                      0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        char buffer[256];
        DWORD bufferSize = sizeof(buffer);
        DWORD type = REG_SZ;

        if (RegQueryValueExA(hKey, "MachineGuid", NULL, &type,
                             (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
            info += "GUID:" + std::string(buffer) + ";";
        }
        RegCloseKey(hKey);
    }

    // Get computer name as fallback identifier
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD nameSize = sizeof(computerName);
    if (GetComputerNameA(computerName, &nameSize)) {
        info += "NAME:" + std::string(computerName) + ";";
    }

    return info;
}

#elif __APPLE__

std::string MachineId::getMacOSMachineInfo() {
    std::string info;

    io_service_t platformExpert = IOServiceGetMatchingService(
        kIOMasterPortDefault,
        IOServiceMatching("IOPlatformExpertDevice"));

    if (platformExpert) {
        // Get serial number
        CFTypeRef serialRef = IORegistryEntryCreateCFProperty(
            platformExpert,
            CFSTR(kIOPlatformSerialNumberKey),
            kCFAllocatorDefault, 0);

        if (serialRef) {
            char serial[256];
            if (CFStringGetCString((CFStringRef)serialRef, serial,
                                   sizeof(serial), kCFStringEncodingUTF8)) {
                info += "SERIAL:" + std::string(serial) + ";";
            }
            CFRelease(serialRef);
        }

        // Get platform UUID
        CFTypeRef uuidRef = IORegistryEntryCreateCFProperty(
            platformExpert,
            CFSTR(kIOPlatformUUIDKey),
            kCFAllocatorDefault, 0);

        if (uuidRef) {
            char uuid[256];
            if (CFStringGetCString((CFStringRef)uuidRef, uuid,
                                   sizeof(uuid), kCFStringEncodingUTF8)) {
                info += "UUID:" + std::string(uuid) + ";";
            }
            CFRelease(uuidRef);
        }

        IOObjectRelease(platformExpert);
    }

    return info;
}

#elif __linux__

std::string MachineId::getLinuxMachineInfo() {
    std::string info;

    // Read machine-id
    std::ifstream machineIdFile("/etc/machine-id");
    if (machineIdFile.good()) {
        std::string machineId;
        std::getline(machineIdFile, machineId);
        info += "MID:" + machineId + ";";
    }

    // Read product_uuid (requires root, may not be available)
    std::ifstream productUuid("/sys/class/dmi/id/product_uuid");
    if (productUuid.good()) {
        std::string uuid;
        std::getline(productUuid, uuid);
        info += "UUID:" + uuid + ";";
    }

    // Hostname as fallback
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        info += "HOST:" + std::string(hostname) + ";";
    }

    return info;
}

#endif

// ==============================================================================
// Public API
// ==============================================================================

std::string MachineId::generate() {
    std::string machineInfo;

#if _WIN32
    machineInfo = getWindowsMachineInfo();
#elif __APPLE__
    machineInfo = getMacOSMachineInfo();
#elif __linux__
    machineInfo = getLinuxMachineInfo();
#else
    machineInfo = "UNKNOWN_PLATFORM";
#endif

    if (machineInfo.empty()) {
        machineInfo = "FALLBACK_ID";
    }

    return hashMachineInfo(machineInfo);
}

std::string MachineId::generateShort() {
    auto full = generate();
    return full.substr(0, 16);
}

} // namespace beatconnect
