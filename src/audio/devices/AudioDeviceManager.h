#pragma once

#include "../core/AudioConfig.h"
#include "../core/AudioState.h"
#include "AudioChannelDescriptor.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace livemixer::audio {

/**
 * @brief Detailed diagnostic snapshot of JUCE AudioDeviceManager and AudioIODevice initialization state
 */
struct AudioDeviceDiagnostic {
    std::string driverTypeName;
    std::string deviceName;
    double requestedSampleRate = 0.0;
    uint32_t requestedBufferSize = 0;
    int requestedInputChannels = 2;
    int requestedOutputChannels = 2;

    std::string initialiseResult;
    std::string setAudioDeviceSetupResult;
    std::string deviceLastError;
    bool hasDevicePointer = false;
    bool isDeviceOpen = false;
    double actualSampleRate = 0.0;
    uint32_t actualBufferSize = 0;
    std::vector<std::string> inputChannelNames;
    std::vector<std::string> outputChannelNames;
    uint32_t activeInputChannels = 0;
    uint32_t activeOutputChannels = 0;
    std::string secondaryDiagnostic;
};

/**
 * @brief Information about an audio device discovered directly from JUCE AudioIODeviceType
 */
struct AudioDeviceInfo {
    std::string name;
    std::string type; // "ASIO", "Windows Audio", "Windows Audio (Exclusive Mode)", etc.
    uint32_t maxInputChannels = 0;
    uint32_t maxOutputChannels = 0;
    std::vector<double> supportedSampleRates;
    std::vector<uint32_t> supportedBufferSizes;
    bool isDefault = false;
};

/**
 * @brief Listener interface for audio device lifecycle events and disconnects
 */
class IAudioDeviceListener {
public:
    virtual ~IAudioDeviceListener() = default;
    virtual void onDeviceListChanged() = 0;
    virtual void onDeviceDisconnected(const std::string& deviceName) = 0;
    virtual void onAudioDeviceError(const std::string& errorMessage) = 0;
};

/**
 * @brief Audio callback interface passed to the device manager
 */
class IAudioCallback {
public:
    virtual ~IAudioCallback() = default;
    virtual void audioDeviceIOCallback(const float* const* inputChannelData,
                                       int numInputChannels,
                                       float* const* outputChannelData,
                                       int numOutputChannels,
                                       int numSamples) noexcept = 0;
    virtual void audioDeviceAboutToStart(double sampleRate, int samplesPerBlock) = 0;
    virtual void audioDeviceStopped() = 0;
    virtual void audioDeviceError(const std::string& errorMessage) = 0;
};

/**
 * @brief Thin application-level abstraction over JUCE's native AudioDeviceManager.
 * 
 * Interacts directly with juce::AudioDeviceManager and juce::AudioIODeviceType.
 * Supports ASIO and WASAPI on Windows, while allowing macOS CoreAudio addition
 * without architecture modifications.
 */
class IAudioDeviceManager {
public:
    virtual ~IAudioDeviceManager() = default;

    // Driver & Device enumeration (Discovered natively via JUCE AudioIODeviceType)
    virtual std::vector<DriverType> getAvailableDriverTypes() = 0;
    virtual std::vector<AudioDeviceInfo> getDevicesForDriver(DriverType driverType) = 0;

    // Selection
    virtual bool selectDriver(DriverType driverType) = 0;
    virtual bool selectDevice(const std::string& deviceName) = 0;
    virtual DriverType getCurrentDriverType() const noexcept = 0;
    virtual std::string getCurrentDeviceName() const = 0;

    // Capabilities & active state queried from active juce::AudioIODevice
    virtual AudioDeviceInfo getCurrentDeviceInfo() const = 0;
    virtual std::vector<double> getSupportedSampleRates() const = 0;
    virtual std::vector<uint32_t> getSupportedBufferSizes() const = 0;
    virtual double getCurrentSampleRate() const noexcept = 0;
    virtual uint32_t getCurrentBufferSize() const noexcept = 0;
    virtual uint32_t getInputChannelCount() const noexcept = 0;
    virtual uint32_t getOutputChannelCount() const noexcept = 0;
    virtual std::vector<AudioChannelInfo> getDiscoveredInputChannels() const = 0;
    virtual std::vector<AudioChannelInfo> getDiscoveredOutputChannels() const = 0;

    // Latency & buffer specs
    virtual double getBufferDurationMs() const noexcept = 0;
    virtual double getInputLatencyMs() const noexcept = 0;
    virtual double getOutputLatencyMs() const noexcept = 0;

    // Device control
    virtual bool openDevice(const AudioConfig& requestedConfig) = 0;
    virtual void closeDevice() = 0;
    virtual bool startAudio(IAudioCallback* callback) = 0;
    virtual void stopAudio() = 0;
    virtual bool isAudioRunning() const noexcept = 0;

    // ASIO Control Panel (Calls real juce::AudioIODevice::showControlPanel)
    virtual bool hasControlPanel() const noexcept = 0;
    virtual bool openControlPanel() = 0;

    // Errors & Diagnostics
    virtual std::string getLastError() const = 0;
    virtual AudioDeviceDiagnostic getDiagnosticInfo() const = 0;

    // Listeners for hotplug/disconnect notifications
    virtual void addListener(IAudioDeviceListener* listener) = 0;
    virtual void removeListener(IAudioDeviceListener* listener) = 0;
};

/**
 * @brief Factory function to create the platform JUCE AudioDeviceManager
 */
std::unique_ptr<IAudioDeviceManager> createAudioDeviceManager();

} // namespace livemixer::audio
