#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace livemixer::audio {

/**
 * @brief Driver backend types supported by Live Mixer
 */
enum class DriverType {
    ASIO,
    WASAPI,
    Unknown
};

inline std::string driverTypeToString(DriverType type) {
    switch (type) {
        case DriverType::ASIO:   return "ASIO";
        case DriverType::WASAPI: return "WASAPI";
        default:                 return "Unknown";
    }
}

inline DriverType stringToDriverType(const std::string& str) {
    if (str == "ASIO")   return DriverType::ASIO;
    if (str == "WASAPI") return DriverType::WASAPI;
    return DriverType::Unknown;
}

/**
 * @brief Audio configuration structure
 */
struct AudioConfig {
    DriverType driverType = DriverType::ASIO;
    std::string deviceName = "";
    double sampleRate = 48000.0;           // Default requested: 48000 Hz
    uint32_t bufferSize = 128;             // Default requested: 128 samples
    uint32_t inputChannelCount = 2;        // Default stereo in
    uint32_t outputChannelCount = 2;       // Default stereo out

    /**
     * @brief Calculate buffer duration in milliseconds
     * Example: 128 samples @ 48000 Hz = 2.6667 ms
     */
    [[nodiscard]] double getBufferDurationMs() const noexcept {
        if (sampleRate <= 0.0) return 0.0;
        return (static_cast<double>(bufferSize) / sampleRate) * 1000.0;
    }

    /**
     * @brief Validate configuration parameters
     */
    [[nodiscard]] bool isValid() const noexcept {
        if (driverType == DriverType::Unknown) return false;
        if (sampleRate < 8000.0 || sampleRate > 384000.0) return false;
        if (bufferSize < 16 || bufferSize > 8192) return false;
        if (inputChannelCount == 0 && outputChannelCount == 0) return false;
        return true;
    }
};

} // namespace livemixer::audio
