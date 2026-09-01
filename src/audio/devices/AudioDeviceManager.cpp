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
        // 1. Initialise JUCE AudioDeviceManager with standard stereo channel request (2 in, 2 out)
        // This populates JUCE internal device types and scans for available systems.
        const juce::String initError = m_juceManager.initialise(2, 2, nullptr, true);
        m_diagnostic.initialiseResult = initError.isEmpty() ? "OK" : initError.toStdString();
        if (initError.isNotEmpty()) {
            DBG("[AudioDevice] initialise() result: " + initError);
            juce::Logger::writeToLog("[AudioDevice] initialise() result: " + initError);
        } else {
            DBG("[AudioDevice] initialise() result: OK");
            juce::Logger::writeToLog("[AudioDevice] initialise() result: OK");
        }

        // 2. Also register device types if needed
        m_juceManager.createAudioDeviceTypes(m_availableDeviceTypes);

        // 3. Listen to device disconnect / hotplug events from JUCE
        m_juceManager.addChangeListener(this);
    }

    ~JuceAudioDeviceManager() override {
        m_juceManager.removeChangeListener(this);
        closeDevice();
    }

    std::vector<DriverType> getAvailableDriverTypes() override {
        std::vector<DriverType> drivers;
        auto checkType = [&](juce::AudioIODeviceType* type) {
            if (type == nullptr) return;
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
        };

        for (auto* type : m_juceManager.getAvailableDeviceTypes()) {
            checkType(type);
        }
        for (auto* type : m_availableDeviceTypes) {
            checkType(type);
        }

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

        // Scan actual OS hardware/drivers dynamically
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
        if (dev != nullptr && dev->isOpen()) {
            return static_cast<uint32_t>(dev->getActiveInputChannels().countNumberOfSetBits());
        }
        return 0;
    }

    uint32_t getOutputChannelCount() const noexcept override {
        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev != nullptr && dev->isOpen()) {
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
        if (dev != nullptr && dev->isOpen() && dev->getCurrentSampleRate() > 0.0) {
            return (static_cast<double>(dev->getInputLatencyInSamples()) / dev->getCurrentSampleRate()) * 1000.0;
        }
        return 0.0;
    }

    double getOutputLatencyMs() const noexcept override {
        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev != nullptr && dev->isOpen() && dev->getCurrentSampleRate() > 0.0) {
            return (static_cast<double>(dev->getOutputLatencyInSamples()) / dev->getCurrentSampleRate()) * 1000.0;
        }
        return 0.0;
    }

    bool openDevice(const AudioConfig& requestedConfig) override {
        m_activeConfig = requestedConfig;
        m_lastError.clear();

        // Initialize structured diagnostic record for this attempt
        m_diagnostic.driverTypeName = driverTypeToString(requestedConfig.driverType);
        m_diagnostic.requestedSampleRate = requestedConfig.sampleRate;
        m_diagnostic.requestedBufferSize = requestedConfig.bufferSize;
        m_diagnostic.requestedInputChannels = requestedConfig.inputChannelCount > 0 ? requestedConfig.inputChannelCount : 2;
        m_diagnostic.requestedOutputChannels = requestedConfig.outputChannelCount > 0 ? requestedConfig.outputChannelCount : 2;
        m_diagnostic.inputChannelNames.clear();
        m_diagnostic.outputChannelNames.clear();
        m_diagnostic.activeInputChannels = 0;
        m_diagnostic.activeOutputChannels = 0;
        m_diagnostic.actualSampleRate = 0.0;
        m_diagnostic.actualBufferSize = 0;
        m_diagnostic.secondaryDiagnostic.clear();

        // 1. Locate device type for requested driver
        juce::AudioIODeviceType* type = findDeviceTypeForDriver(requestedConfig.driverType);
        if (type == nullptr) {
            m_lastError = "Selected driver type (" + driverTypeToString(requestedConfig.driverType) + ") is not supported or available on this system.";
            m_diagnostic.secondaryDiagnostic = m_lastError;
            logDiagnostics(requestedConfig, "", "Driver type not available", nullptr);
            notifyError(m_lastError);
            return false;
        }

        // 2. Rescan devices for this driver type dynamically
        type->scanForDevices();
        const auto deviceNames = type->getDeviceNames();
        if (deviceNames.isEmpty()) {
            m_lastError = "No devices detected for driver type '" + type->getTypeName().toStdString() + "'.";
            m_diagnostic.secondaryDiagnostic = m_lastError;
            logDiagnostics(requestedConfig, "", "No devices detected", nullptr);
            notifyError(m_lastError);
            return false;
        }

        // 3. Determine target device name (Preserve user's selected device)
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
        m_diagnostic.deviceName = m_selectedDeviceName;

        // 4. Switch JUCE device manager to target type
        m_juceManager.setCurrentAudioDeviceType(type->getTypeName(), true);

        // 5. Setup audio device configuration structure
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        m_juceManager.getAudioDeviceSetup(setup);
        setup.outputDeviceName = targetDeviceName;
        setup.inputDeviceName = targetDeviceName;
        setup.sampleRate = requestedConfig.sampleRate;
        setup.bufferSize = static_cast<int>(requestedConfig.bufferSize);
        setup.useDefaultInputChannels = true;
        setup.useDefaultOutputChannels = true;

        // 6. Perform real device initialization via JUCE and capture exact error string
        const juce::String juceError = m_juceManager.setAudioDeviceSetup(setup, true);
        m_diagnostic.setAudioDeviceSetupResult = juceError.isEmpty() ? "OK" : juceError.toStdString();

        // 7. Obtain and query the AudioIODevice
        auto* dev = m_juceManager.getCurrentAudioDevice();
        m_diagnostic.hasDevicePointer = (dev != nullptr);
        if (dev != nullptr) {
            m_diagnostic.isDeviceOpen = dev->isOpen();
            m_diagnostic.deviceLastError = dev->getLastError().toStdString();
            if (dev->isOpen()) {
                m_diagnostic.actualSampleRate = dev->getCurrentSampleRate();
                m_diagnostic.actualBufferSize = static_cast<uint32_t>(dev->getCurrentBufferSizeSamples());
                m_diagnostic.activeInputChannels = static_cast<uint32_t>(dev->getActiveInputChannels().countNumberOfSetBits());
                m_diagnostic.activeOutputChannels = static_cast<uint32_t>(dev->getActiveOutputChannels().countNumberOfSetBits());
                for (const auto& name : dev->getInputChannelNames()) {
                    m_diagnostic.inputChannelNames.push_back(name.toStdString());
                }
                for (const auto& name : dev->getOutputChannelNames()) {
                    m_diagnostic.outputChannelNames.push_back(name.toStdString());
                }
            }
        }

        // 8. Output complete diagnostic log sequence outside realtime audio callback
        logDiagnostics(requestedConfig, targetDeviceName, juceError, dev);

        // 9. Inspect failure modes without manufacturing fake errors
        if (juceError.isNotEmpty()) {
            m_lastError = juceError.toStdString();
            notifyError(m_lastError);
            return false;
        }

        if (dev == nullptr) {
            m_diagnostic.secondaryDiagnostic = "Audio device pointer is null after setAudioDeviceSetup.";
            m_lastError = !m_diagnostic.deviceLastError.empty() ? m_diagnostic.deviceLastError : "Audio device pointer is null.";
            notifyError(m_lastError);
            return false;
        }

        if (!dev->isOpen()) {
            const std::string devErr = dev->getLastError().toStdString();
            if (!devErr.empty()) {
                m_lastError = devErr;
            } else {
                m_diagnostic.secondaryDiagnostic = "Audio device handle exists but isOpen() is false.";
                m_lastError = "Audio device failed to open (driver reported failure).";
            }
            notifyError(m_lastError);
            return false;
        }

        m_lastOpenedDeviceName = targetDeviceName.toStdString();
        m_lastError.clear();
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
        // ASIO devices support control panels even if unopened
        return (m_currentDriver == DriverType::ASIO);
    }

    bool openControlPanel() override {
        auto* dev = m_juceManager.getCurrentAudioDevice();
        if (dev != nullptr && dev->hasControlPanel()) {
            dev->showControlPanel();
            return true;
        }

        // Direct device control panel probe
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

    AudioDeviceDiagnostic getDiagnosticInfo() const override {
        return m_diagnostic;
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

    void logDiagnostics(const AudioConfig& cfg, const juce::String& devName, const juce::String& setupErr, juce::AudioIODevice* dev) {
        auto logLine = [](const juce::String& line) {
            DBG(line);
            juce::Logger::writeToLog(line);
        };

        logLine("[AudioDevice]");
        logLine("Driver: " + juce::String(driverTypeToString(cfg.driverType)));
        logLine("Device: " + (devName.isNotEmpty() ? devName : juce::String(cfg.deviceName)));
        logLine("Requested sample rate: " + juce::String(static_cast<int>(cfg.sampleRate)) + " Hz");
        logLine("Requested buffer: " + juce::String(cfg.bufferSize) + " samples");
        logLine("");
        logLine("[AudioDevice]");
        logLine("initialise() result: " + (m_diagnostic.initialiseResult.empty() ? juce::String("OK") : juce::String(m_diagnostic.initialiseResult)));
        logLine("");
        logLine("[AudioDevice]");
        logLine("setAudioDeviceSetup() result: " + (setupErr.isEmpty() ? juce::String("OK") : setupErr));
        logLine("");
        logLine("[AudioDevice]");
        logLine("current device: " + (dev != nullptr ? dev->getName() : juce::String("NULL")));
        logLine("");
        logLine("[AudioDevice]");
        logLine("isOpen: " + juce::String(dev != nullptr && dev->isOpen() ? "YES" : "NO"));
        logLine("");
        logLine("[AudioDevice]");
        logLine("getLastError: " + (dev != nullptr ? dev->getLastError() : juce::String("None")));
        logLine("");
        logLine("[AudioDevice]");
        logLine("input channels: " + (dev != nullptr && dev->isOpen() ? juce::String(dev->getActiveInputChannels().countNumberOfSetBits()) : juce::String("--")));
        logLine("");
        logLine("[AudioDevice]");
        logLine("output channels: " + (dev != nullptr && dev->isOpen() ? juce::String(dev->getActiveOutputChannels().countNumberOfSetBits()) : juce::String("--")));
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
    AudioDeviceDiagnostic m_diagnostic;
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

    AudioDeviceDiagnostic getDiagnosticInfo() const override {
        AudioDeviceDiagnostic diag;
        diag.driverTypeName = driverTypeToString(m_driver);
        diag.deviceName = m_device;
        diag.requestedSampleRate = m_config.sampleRate;
        diag.requestedBufferSize = m_config.bufferSize;
        diag.requestedInputChannels = static_cast<int>(m_config.inputChannelCount);
        diag.requestedOutputChannels = static_cast<int>(m_config.outputChannelCount);
        diag.initialiseResult = "OK";
        diag.setAudioDeviceSetupResult = m_isOpen ? "OK" : m_lastError;
        diag.hasDevicePointer = m_isOpen;
        diag.isDeviceOpen = m_isOpen;
        diag.deviceLastError = m_lastError;
        if (m_isOpen) {
            diag.actualSampleRate = m_config.sampleRate;
            diag.actualBufferSize = m_config.bufferSize;
            diag.activeInputChannels = m_config.inputChannelCount;
            diag.activeOutputChannels = m_config.outputChannelCount;
            for (uint32_t i = 0; i < m_config.inputChannelCount; ++i) {
                diag.inputChannelNames.push_back("In " + std::to_string(i + 1));
            }
            for (uint32_t i = 0; i < m_config.outputChannelCount; ++i) {
                diag.outputChannelNames.push_back("Out " + std::to_string(i + 1));
            }
        }
        return diag;
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
