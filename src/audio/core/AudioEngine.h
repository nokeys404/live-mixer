#pragma once

#include "AudioConfig.h"
#include "AudioState.h"
#include "AudioMetrics.h"
#include "../devices/AudioDeviceManager.h"
#include "../mixer/MixerEngine.h"
#include <memory>
#include <vector>
#include <chrono>
#include <atomic>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

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

    // Auto-reconnection trigger / check
    void checkAutoReconnect();

    // Configuration & Device querying
    [[nodiscard]] AudioConfig getCurrentConfig() const;
    [[nodiscard]] AudioState getState() const noexcept;
    [[nodiscard]] AudioMetricsSnapshot getMetrics() const noexcept;
    [[nodiscard]] std::string getLastError() const;
    [[nodiscard]] AudioDeviceDiagnostic getDiagnosticInfo() const;
    [[nodiscard]] std::shared_ptr<IAudioDeviceManager> getDeviceManager() const noexcept { return m_deviceManager; }
    [[nodiscard]] std::shared_ptr<mixer::MixerEngine> getMixerEngine() const noexcept { return m_mixerEngine; }

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

    // Reconnect background thread management (non-realtime)
    void startReconnectThread();
    void stopReconnectThread();
    void reconnectThreadLoop();

    std::shared_ptr<IAudioDeviceManager> m_deviceManager;
    std::shared_ptr<mixer::MixerEngine> m_mixerEngine;
    AudioConfig m_config;
    AudioMetrics m_metrics;
    std::string m_lastErrorMessage;

    // Auto-reconnection state & guards
    bool m_wasRunningBeforeDisconnect{false};
    std::atomic<bool> m_isReconnecting{false};
    std::string m_disconnectedDeviceName;

    // Background reconnect thread members
    std::thread m_reconnectThread;
    std::mutex m_reconnectMutex;
    std::condition_variable m_reconnectCv;
    std::atomic<bool> m_reconnectThreadActive{false};
    std::atomic<bool> m_reconnectRequested{false};

    // Preallocated scratch / passthrough buffers (Never allocate in audio thread!)
    static constexpr size_t MAX_SUPPORTED_CHANNELS = 8;
    static constexpr size_t MAX_BUFFER_SAMPLES = 8192;
    float m_scratchBuffer[MAX_SUPPORTED_CHANNELS][MAX_BUFFER_SAMPLES];

    // High resolution clock tracking for real-time processing duration
    std::atomic<bool> m_isProcessing{false};
    std::atomic<bool> m_simulateXRunNextBlock{false};
};

} // namespace livemixer::audio
