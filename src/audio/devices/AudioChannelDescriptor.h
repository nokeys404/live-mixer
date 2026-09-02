#pragma once

#include <string>

namespace livemixer::audio {

/**
 * @brief Categorization of audio source endpoints (for hardware physical/virtual inputs vs future WASAPI loopback)
 */
enum class AudioSourceType {
    HardwareInput,
    SystemAudioLoopback
};

/**
 * @brief Descriptor for a discovered physical or virtual audio channel on an AudioIODevice.
 * 
 * Maps the driver-reported device channel index and name to the realtime callback buffer index.
 */
struct AudioChannelInfo {
    int deviceChannelIndex{0};  ///< 0-based physical/driver channel index (0 .. N-1)
    int bufferIndex{0};         ///< 0-based index into audioDeviceIOCallback buffer arrays
    std::string channelName;    ///< Driver-reported name (e.g. "Maono MIC In 1", "Input 1")
    bool isInput{true};         ///< true for input channel, false for output channel
    AudioSourceType sourceType{AudioSourceType::HardwareInput}; ///< Source classification
};

} // namespace livemixer::audio
