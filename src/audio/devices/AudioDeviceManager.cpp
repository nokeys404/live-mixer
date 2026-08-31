#include "AudioDeviceManager.h"
#include <algorithm>
#include <iostream>
#include <cmath>

#if __has_include(<juce_audio_devices/juce_audio_devices.h>)
#include <juce_audio_devices/juce_audio_devices.h>
#define LIVE_MIXER_HAS_JUCE 1
#endif

namespace livemixer::audio {

#if LIVE_MIXER_HAS_JUCE

/**
 * @brief Production JUCE AudioDeviceManager implementation.
 * Directly wraps juce::AudioDeviceManager, juce::AudioIODeviceType, and juce::AudioIODevice.
 */
class JuceAudioDeviceManager : public IAudioDeviceManager,
                               private juce::AudioIODeviceCallback,
                               private juce::ChangeListener
{
public:
    JuceAudioDeviceManager()
        : m_currentDriver(DriverType::ASIO)
    {
        // Register native device backends (ASIO, WASAPI, etc.)
        m_juceManager.createAudioDeviceTypes(m_availableDeviceTypes);
        
        // Listen to device disconnect / hotplug events from JUCE
        m_juceManager.addChangeListener(this);
    }

    ~JuceAudioDeviceManager() override {
        m_juceManager.removeChangeListener(this);
        closeDevice();
    }

    std::vector<DriverType> getAvailableDriverTypes() override {
        std::vector<DriverType> drivers;
        for (auto* type : m_availableDeviceTypes) {
            if (type == nullptr) continue;
            const auto name = type->getTypeName();
            if (name.equalsIgnoreCase("ASIO")) {
                if (std::find(drivers.begin(), drivers.end(), DriverType::ASIO) == drivers.end()) {
                    drivers.push_back(DriverType::ASIO);
                }
            } else if (name.containsIgnoreCase("WASAPI") || name.containsIgnoreCase("Windows Audio")) {
                if (std::find(drivers.begin(), drivers.end(), DriverType::WASAPI) == drivers.end()) {
                    drivers.push_back(DriverType::WASAPI);
                }
            }
        }
        
        // Ensure at least standard Windows driver types are selectable if device types are registered
        if (drivers.empty()) {
            drivers.push_back(DriverType::ASIO);
            drivers.push_back(DriverType::WASAPI);
        }
        return drivers;
    }

    std::vector<AudioDeviceInfo> getDevicesForDriver(DriverType driverType) override {
        std::vector<AudioDeviceInfo> results;
        juce::AudioIODeviceType* matchingType = findDeviceTypeForDriver(driverType);
        if (matchingType == nullptr) {
            return results; // Return empty list - do NOT fabricate mock devices
        }

        // Scan actual OS hardware/drivers
        matchingType->scanForDevices();
        const auto deviceNames = matchingType->getDeviceNames();
        const int defaultInIndex = matchingType->getDefaultDeviceIndex(false);

        for (int i = 0; i < deviceNames.size(); ++i) {
            const auto devName = deviceNames[i];
            AudioDeviceInfo info;
            info.name = devName.toStdString();
            info.type = matchingType->getTypeName().toStdString();
            info.isDefault = (i == defaultInIndex);

            // Query device capabilities through temporary device creation if possible
            std::unique_ptr<juce::AudioIODevice> tempDevice(matchingType->createDevice(devName, devName));
            if (tempDevice) {
                info.maxInputChannels = static_cast<uint32_t>(tempDevice->getInputChannelNames().size());
                info.maxOutputChannels = static_cast<uint32_t>(tempDevice->getOutputChannelNames().size());
                
                auto rates = tempDevice->getAvailableSampleRates();
                for (double r : rates) {
                    info.supportedSampleRates.push_back(r);
                }
                
                auto buffers = tempDevice->getAvailableBufferSizes();
                for (int b : buffers) {
                    if (b > 0) info.supportedBufferSizes.push_back(static_cast<uint32_t>(b));
                }
            }
            results.push_back(info);
        }

        return results;
    }

    bool selectDriver(DriverType driverType) override {
        m_currentDriver = driverType;
        juce::AudioIODeviceType* type = findDeviceTypeForDriver(driverType);
        if (type != nullptr) {
            m_juceManager.setCurrentAudioDeviceType(type->getTypeName(), true);
            return true;
        }
        return false;
    }

    bool selectDevice(const std::string& deviceName) override {
        m_selectedDeviceName = deviceName;
        return true;
    }

    DriverType getCurrentDriverType() const noexcept override {
        return m_currentDriver;
    }

    std::string getCurrentDeviceName() const override {
        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev != nullptr) {
            return dev->getName().toStdString();
        }
        return m_selectedDeviceName;
    }

    AudioDeviceInfo getCurrentDeviceInfo() const override {
        AudioDeviceInfo info;
        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev != nullptr) {
            info.name = dev->getName().toStdString();
            info.type = dev->getTypeName().toStdString();
            info.maxInputChannels = static_cast<uint32_t>(dev->getInputChannelNames().size());
            info.maxOutputChannels = static_cast<uint32_t>(dev->getOutputChannelNames().size());
            for (double r : dev->getAvailableSampleRates()) {
                info.supportedSampleRates.push_back(r);
            }
            for (int b : dev->getAvailableBufferSizes()) {
                if (b > 0) info.supportedBufferSizes.push_back(static_cast<uint32_t>(b));
            }
        }
        return info;
    }

    std::vector<double> getSupportedSampleRates() const override {
        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev != nullptr) {
            std::vector<double> rates;
            for (double r : dev->getAvailableSampleRates()) {
                rates.push_back(r);
            }
            if (!rates.empty()) return rates;
        }
        return {};
    }

    std::vector<uint32_t> getSupportedBufferSizes() const override {
        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev != nullptr) {
            std::vector<uint32_t> sizes;
            for (int b : dev->getAvailableBufferSizes()) {
                if (b > 0) sizes.push_back(static_cast<uint32_t>(b));
            }
            if (!sizes.empty()) return sizes;
        }
        return {};
    }

    double getCurrentSampleRate() const noexcept override {
        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev != nullptr) {
            return dev->getCurrentSampleRate();
        }
        return m_activeConfig.sampleRate;
    }

    uint32_t getCurrentBufferSize() const noexcept override {
        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev != nullptr) {
            return static_cast<uint32_t>(dev->getCurrentBufferSizeSamples());
        }
        return m_activeConfig.bufferSize;
    }

    uint32_t getInputChannelCount() const noexcept override {
        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev != nullptr) {
            return static_cast<uint32_t>(dev->getActiveInputChannels().countNumberOfSetBits());
        }
        return 0;
    }

    uint32_t getOutputChannelCount() const noexcept override {
        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev != nullptr) {
            return static_cast<uint32_t>(dev->getActiveOutputChannels().countNumberOfSetBits());
        }
        return 0;
    }

    double getBufferDurationMs() const noexcept override {
        const double rate = getCurrentSampleRate();
        const uint32_t buffer = getCurrentBufferSize();
        if (rate > 0.0) {
            return (static_cast<double>(buffer) / rate) * 1000.0;
        }
        return 0.0;
    }

    double getInputLatencyMs() const noexcept override {
        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev != nullptr && dev->getCurrentSampleRate() > 0.0) {
            return (static_cast<double>(dev->getInputLatencyInSamples()) / dev->getCurrentSampleRate()) * 1000.0;
        }
        return 0.0;
    }

    double getOutputLatencyMs() const noexcept override {
        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev != nullptr && dev->getCurrentSampleRate() > 0.0) {
            return (static_cast<double>(dev->getOutputLatencyInSamples()) / dev->getCurrentSampleRate()) * 1000.0;
        }
        return 0.0;
    }

    bool openDevice(const AudioConfig& requestedConfig) override {
        m_activeConfig = requestedConfig;
        m_lastError.clear();

        juce::AudioIODeviceType* type = findDeviceTypeForDriver(requestedConfig.driverType);
        if (type == nullptr) {
            m_lastError = "Selected driver type (" + driverTypeToString(requestedConfig.driverType) + ") is not supported or available on this system.";
            DBG("ASIO/Audio initialization error: " + juce::String(m_lastError));
            juce::Logger::writeToLog("ASIO/Audio initialization error: " + juce::String(m_lastError));
            std::cerr << "[LiveMixer DeviceManager] " << m_lastError << "\n";
            notifyError(m_lastError);
            return false;
        }

        // Rescan devices for this driver type
        type->scanForDevices();
        const auto deviceNames = type->getDeviceNames();
        if (deviceNames.isEmpty()) {
            m_lastError = "No devices detected for driver type '" + type->getTypeName().toStdString() + "'.";
            DBG("ASIO/Audio initialization error: " + juce::String(m_lastError));
            juce::Logger::writeToLog("ASIO/Audio initialization error: " + juce::String(m_lastError));
            std::cerr << "[LiveMixer DeviceManager] " << m_lastError << "\n";
            notifyError(m_lastError);
            return false;
        }

        // Determine target device name
        juce::String targetDeviceName = requestedConfig.deviceName;
        if (targetDeviceName.isEmpty() || !deviceNames.contains(targetDeviceName)) {
            const int defaultIdx = type->getDefaultDeviceIndex(false);
            if (defaultIdx >= 0 && defaultIdx < deviceNames.size()) {
                targetDeviceName = deviceNames[defaultIdx];
            } else {
                targetDeviceName = deviceNames[0];
            }
        }
        m_selectedDeviceName = targetDeviceName.toStdString();

        // Switch JUCE device manager to target type
        m_juceManager.setCurrentAudioDeviceType(type->getTypeName(), true);

        // Setup audio device configuration structure
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        m_juceManager.getAudioDeviceSetup(setup);
        setup.outputDeviceName = targetDeviceName;
        setup.inputDeviceName = targetDeviceName;
        setup.sampleRate = requestedConfig.sampleRate;
        setup.bufferSize = static_cast<int>(requestedConfig.bufferSize);
        setup.useDefaultInputChannels = true;
        setup.useDefaultOutputChannels = true;

        // Perform real device initialization via JUCE and capture exact error string
        const juce::String juceError = m_juceManager.setAudioDeviceSetup(setup, true);
        if (juceError.isNotEmpty()) {
            m_lastError = juceError.toStdString();
            DBG("ASIO initialization error: " + juceError);
            juce::Logger::writeToLog("ASIO initialization error: " + juceError);
            std::cerr << "[LiveMixer DeviceManager] Audio Device setup error on '" << targetDeviceName.toStdString() << "': " << m_lastError << "\n";
            notifyError(m_lastError);
            return false;
        }

        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev == nullptr || !dev->isOpen()) {
            juce::String devErr;
            if (dev != nullptr) {
                devErr = dev->getLastError();
            }
            if (devErr.isNotEmpty()) {
                m_lastError = devErr.toStdString();
            } else {
                m_lastError = "Device '" + targetDeviceName.toStdString() + "' failed to open: device handle is null or closed.";
            }
            DBG("ASIO initialization error: " + juce::String(m_lastError));
            juce::Logger::writeToLog("ASIO initialization error: " + juce::String(m_lastError));
            std::cerr << "[LiveMixer DeviceManager] " << m_lastError << "\n";
            notifyError(m_lastError);
            return false;
        }

        m_lastOpenedDeviceName = targetDeviceName.toStdString();
        m_lastError.clear();
        DBG("ASIO device '" + targetDeviceName + "' successfully opened (" + juce::String(dev->getCurrentSampleRate()) + " Hz, " + juce::String(dev->getCurrentBufferSizeSamples()) + " samples).");
        return true;
    }

    void closeDevice() override {
        stopAudio();
        m_juceManager.closeAudioDevice();
    }

    bool startAudio(IAudioCallback* callback) override {
        m_activeCallback = callback;
        if (callback != nullptr) {
            auto* dev = m_juceManager.getCurrentAudioDevice();
            if (dev == nullptr || !dev->isOpen()) {
                m_lastError = "Cannot start audio: active device is not open.";
                notifyError(m_lastError);
                return false;
            }

            m_juceManager.addAudioCallback(this);
            m_isAudioRunning = true;
            return true;
        }
        return false;
    }

    void stopAudio() override {
        if (m_isAudioRunning) {
            m_juceManager.removeAudioCallback(this);
            m_isAudioRunning = false;
            if (m_activeCallback) {
                m_activeCallback->audioDeviceStopped();
                m_activeCallback = nullptr;
            }
        }
    }

    bool isAudioRunning() const noexcept override {
        return m_isAudioRunning;
    }

    bool hasControlPanel() const noexcept override {
        auto* dev = const_cast<juce::AudioDeviceManager&>(m_juceManager).getCurrentAudioDevice();
        if (dev != nullptr && dev->hasControlPanel()) {
            return true;
        }
        // ASIO devices generally support control panels even if unopened
        return (m_currentDriver == DriverType::ASIO);
    }

    bool openControlPanel() override {
        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev != nullptr && dev->hasControlPanel()) {
            dev->showControlPanel();
            return true;
        }

        // If not opened in AudioDeviceManager, create a direct device instance to show control panel
        auto* type = findDeviceTypeForDriver(m_currentDriver);
        if (type != nullptr) {
            std::string devName = m_selectedDeviceName;
            if (devName.empty()) {
                type->scanForDevices();
                auto names = type->getDeviceNames();
                if (!names.isEmpty()) {
                    devName = names[0].toStdString();
                }
            }
            if (!devName.empty()) {
                std::unique_ptr<juce::AudioIODevice> tempDev(type->createDevice(devName, devName));
                if (tempDev && tempDev->hasControlPanel()) {
                    tempDev->showControlPanel();
                    return true;
                }
            }
        }
        return false;
    }

    std::string getLastError() const override {
        return m_lastError;
    }

    void addListener(IAudioDeviceListener* listener) override {
        if (listener != nullptr) {
            m_listeners.push_back(listener);
        }
    }

    void removeListener(IAudioDeviceListener* listener) override {
        m_listeners.erase(std::remove(m_listeners.begin(), m_listeners.end(), listener), m_listeners.end());
    }

private:
    void notifyError(const std::string& err) {
        for (auto* l : m_listeners) {
            if (l) l->onAudioDeviceError(err);
        }
    }

private:
    // =========================================================================
    // juce::AudioIODeviceCallback Realtime Implementation
    // =========================================================================
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                         int numInputChannels,
                                         float* const* outputChannelData,
                                         int numOutputChannels,
                                         int numSamples,
                                         const juce::AudioIODeviceCallbackContext& /*context*/) override
    {
        if (m_activeCallback != nullptr) {
            m_activeCallback->audioDeviceIOCallback(inputChannelData,
                                                    numInputChannels,
                                                    outputChannelData,
                                                    numOutputChannels,
                                                    numSamples);
        }
    }

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override {
        if (device != nullptr && m_activeCallback != nullptr) {
            m_activeCallback->audioDeviceAboutToStart(device->getCurrentSampleRate(),
                                                      device->getCurrentBufferSizeSamples());
        }
    }

    void audioDeviceStopped() override {
        if (m_activeCallback != nullptr) {
            m_activeCallback->audioDeviceStopped();
        }
    }

    void audioDeviceError(const juce::String& errorMessage) override {
        if (m_activeCallback != nullptr) {
            m_activeCallback->audioDeviceError(errorMessage.toStdString());
        }
        for (auto* l : m_listeners) {
            if (l) l->onAudioDeviceError(errorMessage.toStdString());
        }
    }

    // =========================================================================
    // juce::ChangeListener Implementation (Device Disconnect / Hotplug)
    // =========================================================================
    void changeListenerCallback(juce::ChangeBroadcaster* /*source*/) override {
        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev == nullptr || !dev->isOpen()) {
            // Hardware device disconnected or disappeared
            m_isAudioRunning = false;
            for (auto* l : m_listeners) {
                if (l) l->onDeviceDisconnected(m_lastOpenedDeviceName);
            }
        } else {
            for (auto* l : m_listeners) {
                if (l) l->onDeviceListChanged();
            }
        }
    }

    juce::AudioIODeviceType* findDeviceTypeForDriver(DriverType driverType) {
        for (auto* type : m_availableDeviceTypes) {
            if (type == nullptr) continue;
            const auto name = type->getTypeName();
            if (driverType == DriverType::ASIO && name.equalsIgnoreCase("ASIO")) {
                return type;
            }
            if (driverType == DriverType::WASAPI && (name.containsIgnoreCase("WASAPI") || name.containsIgnoreCase("Windows Audio"))) {
                return type;
            }
        }
        return nullptr;
    }

    juce::AudioDeviceManager m_juceManager;
    juce::OwnedArray<juce::AudioIODeviceType> m_availableDeviceTypes;
    DriverType m_currentDriver{DriverType::ASIO};
    std::string m_selectedDeviceName;
    std::string m_lastOpenedDeviceName;
    AudioConfig m_activeConfig;
    IAudioCallback* m_activeCallback{nullptr};
    bool m_isAudioRunning{false};
    std::string m_lastError;
    std::vector<IAudioDeviceListener*> m_listeners;
};

#else

/**
 * @brief Clean non-JUCE reference implementation for standalone unit testing environments.
 * Strictly avoids hardcoding fake hardware devices.
 */
class StandaloneTestAudioDeviceManager : public IAudioDeviceManager {
public:
    StandaloneTestAudioDeviceManager() = default;
    ~StandaloneTestAudioDeviceManager() override = default;

    std::vector<DriverType> getAvailableDriverTypes() override {
        return { DriverType::ASIO, DriverType::WASAPI };
    }

    std::vector<AudioDeviceInfo> getDevicesForDriver(DriverType /*driverType*/) override {
        // Return empty list - do NOT fabricate mock hardware devices
        return {};
    }

    std::string getLastError() const override {
        return m_lastError;
    }

    bool selectDriver(DriverType driverType) override {
        m_driver = driverType;
        return true;
    }

    bool selectDevice(const std::string& deviceName) override {
        m_device = deviceName;
        return true;
    }

    DriverType getCurrentDriverType() const noexcept override { return m_driver; }
    std::string getCurrentDeviceName() const override { return m_device; }
    AudioDeviceInfo getCurrentDeviceInfo() const override { return AudioDeviceInfo{}; }

    std::vector<double> getSupportedSampleRates() const override { return { 44100.0, 48000.0, 96000.0 }; }
    std::vector<uint32_t> getSupportedBufferSizes() const override { return { 64, 128, 256, 512 }; }

    double getCurrentSampleRate() const noexcept override { return m_config.sampleRate; }
    uint32_t getCurrentBufferSize() const noexcept override { return m_config.bufferSize; }
    uint32_t getInputChannelCount() const noexcept override { return m_config.inputChannelCount; }
    uint32_t getOutputChannelCount() const noexcept override { return m_config.outputChannelCount; }

    double getBufferDurationMs() const noexcept override {
        return (m_config.sampleRate > 0.0) ? (static_cast<double>(m_config.bufferSize) / m_config.sampleRate) * 1000.0 : 0.0;
    }

    double getInputLatencyMs() const noexcept override { return getBufferDurationMs(); }
    double getOutputLatencyMs() const noexcept override { return getBufferDurationMs(); }

    bool openDevice(const AudioConfig& requestedConfig) override {
        m_config = requestedConfig;
        m_lastError.clear();
        if (requestedConfig.sampleRate <= 0.0 || requestedConfig.bufferSize == 0) {
            m_lastError = "Invalid sample rate or buffer size.";
            for (auto* l : m_listeners) {
                if (l) l->onAudioDeviceError(m_lastError);
            }
            m_isOpen = false;
            return false;
        }
        m_isOpen = true;
        return true;
    }

    void closeDevice() override {
        stopAudio();
        m_isOpen = false;
    }

    bool startAudio(IAudioCallback* callback) override {
        m_callback = callback;
        m_isRunning = true;
        if (m_callback) {
            m_callback->audioDeviceAboutToStart(m_config.sampleRate, static_cast<int>(m_config.bufferSize));
        }
        return true;
    }

    void stopAudio() override {
        if (m_isRunning) {
            m_isRunning = false;
            if (m_callback) {
                m_callback->audioDeviceStopped();
                m_callback = nullptr;
            }
        }
    }

    bool isAudioRunning() const noexcept override { return m_isRunning; }
    bool hasControlPanel() const noexcept override { return m_driver == DriverType::ASIO; }
    bool openControlPanel() override { return hasControlPanel(); }

    void addListener(IAudioDeviceListener* listener) override {
        if (listener) m_listeners.push_back(listener);
    }

    void removeListener(IAudioDeviceListener* listener) override {
        m_listeners.erase(std::remove(m_listeners.begin(), m_listeners.end(), listener), m_listeners.end());
    }

    void simulateDisconnect() {
        for (auto* l : m_listeners) {
            if (l) l->onDeviceDisconnected(m_device);
        }
    }

private:
    DriverType m_driver{DriverType::ASIO};
    std::string m_device;
    AudioConfig m_config;
    bool m_isOpen{false};
    bool m_isRunning{false};
    std::string m_lastError;
    IAudioCallback* m_callback{nullptr};
    std::vector<IAudioDeviceListener*> m_listeners;
};

#endif

std::unique_ptr<IAudioDeviceManager> createAudioDeviceManager() {
#if LIVE_MIXER_HAS_JUCE
    return std::make_unique<JuceAudioDeviceManager>();
#else
    return std::make_unique<StandaloneTestAudioDeviceManager>();
#endif
}

} // namespace livemixer::audio
