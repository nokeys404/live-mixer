#include "AudioEngine.h"
#include <cstring>
#include <chrono>
#include <algorithm>
#include <iostream>

namespace livemixer::audio {

AudioEngine::AudioEngine(std::shared_ptr<IAudioDeviceManager> deviceManager)
    : m_deviceManager(std::move(deviceManager))
{
    // Zero out preallocated scratch buffers
    std::memset(m_scratchBuffer, 0, sizeof(m_scratchBuffer));

    if (m_deviceManager) {
        m_deviceManager->addListener(this);
    }
    setState(AudioState::Offline);
}

AudioEngine::~AudioEngine() {
    shutdown();
    if (m_deviceManager) {
        m_deviceManager->removeListener(this);
    }
}

bool AudioEngine::initialize(const AudioConfig& config) {
    setState(AudioState::Initializing);
    m_config = config;
    m_lastErrorMessage.clear();

    if (!m_deviceManager) {
        m_lastErrorMessage = "Device manager pointer is null.";
        setState(AudioState::Error);
        return false;
    }

    if (!m_config.isValid()) {
        m_lastErrorMessage = "Audio configuration parameters are invalid.";
        setState(AudioState::Error);
        return false;
    }

    if (!m_deviceManager->selectDriver(m_config.driverType)) {
        m_lastErrorMessage = "Failed to select driver type: " + driverTypeToString(m_config.driverType);
        setState(AudioState::Error);
        return false;
    }

    if (!m_config.deviceName.empty()) {
        m_deviceManager->selectDevice(m_config.deviceName);
    }

    if (!m_deviceManager->openDevice(m_config)) {
        const std::string devErr = m_deviceManager->getLastError();
        if (!devErr.empty()) {
            m_lastErrorMessage = devErr;
        } else {
            m_lastErrorMessage = "Failed to open requested audio device '" + m_config.deviceName + "'.";
        }
        setState(AudioState::Error);
        return false;
    }

    // Sync actual active parameters from opened hardware device
    m_config.deviceName = m_deviceManager->getCurrentDeviceName();
    m_config.inputChannelCount = m_deviceManager->getInputChannelCount();
    m_config.outputChannelCount = m_deviceManager->getOutputChannelCount();
    m_config.sampleRate = m_deviceManager->getCurrentSampleRate();
    m_config.bufferSize = m_deviceManager->getCurrentBufferSize();

    m_metrics.setConfig(m_config.sampleRate, m_config.bufferSize);
    updateLatencies();
    m_lastErrorMessage.clear();
    setState(AudioState::Ready);
    return true;
}

bool AudioEngine::start() {
    const auto currentState = getState();
    if (currentState != AudioState::Ready && currentState != AudioState::Stopping) {
        if (currentState == AudioState::Offline || currentState == AudioState::Error) {
            if (!initialize(m_config)) {
                return false;
            }
        } else if (currentState == AudioState::Running) {
            return true;
        }
    }

    if (!m_deviceManager) {
        setState(AudioState::Error);
        return false;
    }

    if (!m_deviceManager->startAudio(this)) {
        setState(AudioState::Error);
        return false;
    }

    setState(AudioState::Running);
    return true;
}

void AudioEngine::stop() {
    if (getState() == AudioState::Running) {
        setState(AudioState::Stopping);
        if (m_deviceManager) {
            m_deviceManager->stopAudio();
        }
        setState(AudioState::Ready);
    }
}

void AudioEngine::shutdown() {
    stop();
    if (m_deviceManager) {
        m_deviceManager->closeDevice();
    }
    setState(AudioState::Offline);
}

AudioConfig AudioEngine::getCurrentConfig() const {
    return m_config;
}

AudioState AudioEngine::getState() const noexcept {
    return m_metrics.getState();
}

AudioMetricsSnapshot AudioEngine::getMetrics() const noexcept {
    return m_metrics.getSnapshot();
}

std::string AudioEngine::getLastError() const {
    if (!m_lastErrorMessage.empty()) {
        return m_lastErrorMessage;
    }
    if (m_deviceManager) {
        return m_deviceManager->getLastError();
    }
    return {};
}

AudioDeviceDiagnostic AudioEngine::getDiagnosticInfo() const {
    if (m_deviceManager) {
        return m_deviceManager->getDiagnosticInfo();
    }
    return {};
}

void AudioEngine::setState(AudioState newState) noexcept {
    const auto current = getState();
    if (isValidStateTransition(current, newState)) {
        m_metrics.setState(newState);
    } else {
        // Enforce fallback to Error on illegal state jumps
        m_metrics.setState(AudioState::Error);
    }
}

void AudioEngine::updateLatencies() noexcept {
    if (m_deviceManager) {
        m_metrics.setLatencies(m_deviceManager->getInputLatencyMs(), m_deviceManager->getOutputLatencyMs());
    }
}

bool AudioEngine::setDriver(DriverType driverType) {
    if (driverType == m_config.driverType) return true;
    const bool wasRunning = (getState() == AudioState::Running);
    
    stop();
    m_config.driverType = driverType;
    m_deviceManager->selectDriver(driverType);
    m_config.deviceName = m_deviceManager->getCurrentDeviceName();

    auto sampleRates = m_deviceManager->getSupportedSampleRates();
    if (!sampleRates.empty()) {
        auto it = std::find(sampleRates.begin(), sampleRates.end(), m_config.sampleRate);
        if (it == sampleRates.end()) {
            m_config.sampleRate = sampleRates.front();
        }
    }

    auto bufferSizes = m_deviceManager->getSupportedBufferSizes();
    if (!bufferSizes.empty()) {
        auto it = std::find(bufferSizes.begin(), bufferSizes.end(), m_config.bufferSize);
        if (it == bufferSizes.end()) {
            m_config.bufferSize = bufferSizes.front();
        }
    }

    m_config.inputChannelCount = m_deviceManager->getInputChannelCount();
    m_config.outputChannelCount = m_deviceManager->getOutputChannelCount();

    bool ok = initialize(m_config);
    if (ok && wasRunning) {
        start();
    }
    return ok;
}

bool AudioEngine::setDevice(const std::string& deviceName) {
    if (deviceName == m_config.deviceName) return true;
    const bool wasRunning = (getState() == AudioState::Running);

    stop();
    m_config.deviceName = deviceName;
    m_deviceManager->selectDevice(deviceName);

    m_config.inputChannelCount = m_deviceManager->getInputChannelCount();
    m_config.outputChannelCount = m_deviceManager->getOutputChannelCount();

    bool ok = initialize(m_config);
    if (ok && wasRunning) {
        start();
    }
    return ok;
}

bool AudioEngine::setSampleRate(double sampleRate) {
    if (sampleRate == m_config.sampleRate) return true;
    const bool wasRunning = (getState() == AudioState::Running);

    stop();
    m_config.sampleRate = sampleRate;
    bool ok = initialize(m_config);
    if (ok && wasRunning) {
        start();
    }
    return ok;
}

bool AudioEngine::setBufferSize(uint32_t bufferSize) {
    if (bufferSize == m_config.bufferSize) return true;
    const bool wasRunning = (getState() == AudioState::Running);

    stop();
    m_config.bufferSize = bufferSize;
    bool ok = initialize(m_config);
    if (ok && wasRunning) {
        start();
    }
    return ok;
}

void AudioEngine::triggerSimulatedXRun() noexcept {
    m_simulateXRunNextBlock.store(true, std::memory_order_relaxed);
}

void AudioEngine::simulateDeviceDisconnect() {
    onDeviceDisconnected(m_config.deviceName);
}

// =============================================================================
// Hard Real-Time Audio Callback (Zero allocations, zero locks, zero I/O)
// =============================================================================
void AudioEngine::audioDeviceIOCallback(const float* const* inputChannelData,
                                       int numInputChannels,
                                       float* const* outputChannelData,
                                       int numOutputChannels,
                                       int numSamples) noexcept
{
    // Fast high-resolution timer start
    const auto startTime = std::chrono::steady_clock::now();

    // Check for simulated dropout / XRun test
    if (m_simulateXRunNextBlock.exchange(false, std::memory_order_relaxed)) {
        m_metrics.incrementXRuns();
    }

    if (numSamples <= 0 || outputChannelData == nullptr) {
        return;
    }

    const int safeSamples = std::min(numSamples, static_cast<int>(MAX_BUFFER_SAMPLES));
    const int channelsToProcess = std::min({ numInputChannels, numOutputChannels, static_cast<int>(MAX_SUPPORTED_CHANNELS) });

    // Passthrough: Hardware In 1 -> Out 1, Hardware In 2 -> Out 2
    for (int ch = 0; ch < channelsToProcess; ++ch) {
        const float* in = inputChannelData ? inputChannelData[ch] : nullptr;
        float* out = outputChannelData[ch];

        if (out != nullptr) {
            if (in != nullptr) {
                // Realtime safe memory copy
                std::memcpy(out, in, static_cast<size_t>(safeSamples) * sizeof(float));
            } else {
                // Silence if no input channel data
                std::memset(out, 0, static_cast<size_t>(safeSamples) * sizeof(float));
            }
        }
    }

    // Clear any remaining output channels (e.g. if numOutputChannels > numInputChannels)
    for (int ch = channelsToProcess; ch < numOutputChannels; ++ch) {
        float* out = outputChannelData[ch];
        if (out != nullptr) {
            std::memset(out, 0, static_cast<size_t>(safeSamples) * sizeof(float));
        }
    }

    // Calculate processing time in milliseconds and publish lock-free to metrics
    const auto endTime = std::chrono::steady_clock::now();
    const double elapsedUs = std::chrono::duration<double, std::micro>(endTime - startTime).count();
    const double elapsedMs = elapsedUs / 1000.0;

    m_metrics.setProcessingTimeMs(elapsedMs);

    // If processing time exceeded buffer duration, record an XRun
    const double bufferDurationMs = m_config.getBufferDurationMs();
    if (bufferDurationMs > 0.0 && elapsedMs > bufferDurationMs) {
        m_metrics.incrementXRuns();
    }
}

void AudioEngine::audioDeviceAboutToStart(double sampleRate, int samplesPerBlock) {
    m_config.sampleRate = sampleRate;
    m_config.bufferSize = static_cast<uint32_t>(samplesPerBlock);
    m_metrics.setConfig(sampleRate, static_cast<uint32_t>(samplesPerBlock));
    updateLatencies();
}

void AudioEngine::audioDeviceStopped() {
    // Audio device stopped
}

void AudioEngine::audioDeviceError(const std::string& errorMessage) {
    m_lastErrorMessage = errorMessage;
    setState(AudioState::Error);
}

// =============================================================================
// IAudioDeviceListener Implementations
// =============================================================================
void AudioEngine::onDeviceListChanged() {
    // Device list updated on system
}

void AudioEngine::onDeviceDisconnected(const std::string& deviceName) {
    if (m_config.deviceName == deviceName || deviceName.empty()) {
        setState(AudioState::Recovering);
        stop();
        if (m_deviceManager) {
            m_deviceManager->closeDevice();
        }
        setState(AudioState::Error);
    }
}

void AudioEngine::onAudioDeviceError(const std::string& errorMessage) {
    m_lastErrorMessage = errorMessage;
    setState(AudioState::Error);
}

} // namespace livemixer::audio
