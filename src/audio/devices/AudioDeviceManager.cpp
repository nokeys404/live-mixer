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

        juce::AudioIODeviceType* type = findDeviceTypeForDriver(requestedConfig.driverType);
        if (type != nullptr) {
            m_juceManager.setCurrentAudioDeviceType(type->getTypeName(), true);
        }

        juce::AudioDeviceManager::AudioDeviceSetup setup = m_juceManager.getAudioDeviceSetup();
        if (!requestedConfig.deviceName.empty()) {
            setup.outputDeviceName = requestedConfig.deviceName;
            setup.inputDeviceName = requestedConfig.deviceName;
        }
        
        setup.sampleRate = requestedConfig.sampleRate;
        setup.bufferSize = static_cast<int>(requestedConfig.bufferSize);
        setup.useDefaultInputChannels = true;
        setup.useDefaultOutputChannels = true;

        const juce::String error = m_juceManager.setAudioDeviceSetup(setup, true);
        if (error.isNotEmpty()) {
            std::cerr << "JUCE Audio Device setup failed: " << error.toStdString() << "\n";
            for (auto* l : m_listeners) {
                if (l) l->onAudioDeviceError(error.toStdString());
            }
            return false;
        }

        m_lastOpenedDeviceName = requestedConfig.deviceName;
        return true;
    }

    void closeDevice() override {
        stopAudio();
        m_juceManager.closeAudioDevice();
    }

    bool startAudio(IAudioCallback* callback) override {
        m_activeCallback = callback;
        if (callback != nullptr) {
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
        auto* dev = m_juceManager.getCurrentAudioDevice();
        return dev != nullptr && dev->hasControlPanel();
    }

    bool openControlPanel() override {
        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev != nullptr && dev->hasControlPanel()) {
            dev->showControlPanel();
            return true;
        }
        return false;
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
