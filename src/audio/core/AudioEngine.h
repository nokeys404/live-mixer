#pragma once

#include "AudioConfig.h"
#include "AudioState.h"
#include "AudioMetrics.h"
#include "../devices/AudioDeviceManager.h"
#include <memory>
#include <vector>
#include <chrono>
#include <atomic>
#include <string>

namespace livemixer::audio {

/**
 * @brief Realtime Live Audio Engine Core (V0.1 Foundation)
 * 
 * Performs lock-free hardware input-to-output passthrough in the realtime audio thread.
 * Guarantees zero memory allocation, zero file I/O, zero blocking calls in the audio callback.
 */
class AudioEngine : public IAudioCallback, public IAudioDeviceListener {
public:
    explicit AudioEngine(std::shared_ptr<IAudioDeviceManager> deviceManager);
    ~AudioEngine() override;

    // Initialization & lifecycle
    bool initialize(const AudioConfig& config);
    bool start();
    void stop();
    void shutdown();

    // Configuration & Device querying
    [[nodiscard]] AudioConfig getCurrentConfig() const;
    [[nodiscard]] AudioState getState() const noexcept;
    [[nodiscard]] AudioMetricsSnapshot getMetrics() const noexcept;
    [[nodiscard]] std::string getLastError() const;
    [[nodiscard]] AudioDeviceDiagnostic getDiagnosticInfo() const;
    [[nodiscard]] std::shared_ptr<IAudioDeviceManager> getDeviceManager() const noexcept { return m_deviceManager; }

    // Configuration updates
    bool setDriver(DriverType driverType);
    bool setDevice(const std::string& deviceName);
    bool setSampleRate(double sampleRate);
    bool setBufferSize(uint32_t bufferSize);

    // Diagnostics / testing
    void triggerSimulatedXRun() noexcept;
    void simulateDeviceDisconnect();

    // =========================================================================
    // IAudioCallback - Hard Realtime Thread Methods
    // =========================================================================
    void audioDeviceIOCallback(const float* const* inputChannelData,
                               int numInputChannels,
                               float* const* outputChannelData,
                               int numOutputChannels,
                               int numSamples) noexcept override;

    void audioDeviceAboutToStart(double sampleRate, int samplesPerBlock) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const std::string& errorMessage) override;

    // =========================================================================
    // IAudioDeviceListener - Device state notifications
    // =========================================================================
    void onDeviceListChanged() override;
    void onDeviceDisconnected(const std::string& deviceName) override;
    void onAudioDeviceError(const std::string& errorMessage) override;

private:
    void setState(AudioState newState) noexcept;
    void updateLatencies() noexcept;

    std::shared_ptr<IAudioDeviceManager> m_deviceManager;
    AudioConfig m_config;
    AudioMetrics m_metrics;
    std::string m_lastErrorMessage;

    // Preallocated scratch / passthrough buffers (Never allocate in audio thread!)
    static constexpr size_t MAX_SUPPORTED_CHANNELS = 8;
    static constexpr size_t MAX_BUFFER_SAMPLES = 8192;
    float m_scratchBuffer[MAX_SUPPORTED_CHANNELS][MAX_BUFFER_SAMPLES];

    // High resolution clock tracking for real-time processing duration
    std::atomic<bool> m_isProcessing{false};
    std::atomic<bool> m_simulateXRunNextBlock{false};
};

} // namespace livemixer::audio
