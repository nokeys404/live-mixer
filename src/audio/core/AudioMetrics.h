#pragma once

#include "AudioState.h"
#include <atomic>
#include <cstdint>

namespace livemixer::audio {

/**
 * @brief Plain struct snapshot of audio telemetry metrics for UI consumption
 */
struct AudioMetricsSnapshot {
    double inputLatencyMs = 0.0;
    double outputLatencyMs = 0.0;
    double sampleRate = 0.0;
    uint32_t bufferSize = 0;
    double processingTimeMs = 0.0;
    double bufferDurationMs = 0.0;
    uint64_t xrunCount = 0;
    AudioState audioState = AudioState::Offline;
};

/**
 * @brief Thread-safe, lock-free real-time metrics container
 */
class AudioMetrics {
public:
    AudioMetrics() = default;
    ~AudioMetrics() = default;

    // Non-copyable, non-movable for atomic safety
    AudioMetrics(const AudioMetrics&) = delete;
    AudioMetrics& operator=(const AudioMetrics&) = delete;

    // =========================================================================
    // Real-time audio thread setters (atomic stores, memory_order_relaxed/release)
    // =========================================================================
    void setProcessingTimeMs(double ms) noexcept {
        m_processingTimeMs.store(ms, std::memory_order_relaxed);
    }

    void incrementXRuns() noexcept {
        m_xrunCount.fetch_add(1, std::memory_order_relaxed);
    }

    void setLatencies(double inMs, double outMs) noexcept {
        m_inputLatencyMs.store(inMs, std::memory_order_relaxed);
        m_outputLatencyMs.store(outMs, std::memory_order_relaxed);
    }

    void setConfig(double sampleRate, uint32_t bufferSize) noexcept {
        m_sampleRate.store(sampleRate, std::memory_order_relaxed);
        m_bufferSize.store(bufferSize, std::memory_order_relaxed);
        if (sampleRate > 0.0) {
            const double duration = (static_cast<double>(bufferSize) / sampleRate) * 1000.0;
            m_bufferDurationMs.store(duration, std::memory_order_relaxed);
        } else {
            m_bufferDurationMs.store(0.0, std::memory_order_relaxed);
        }
    }

    void setState(AudioState state) noexcept {
        m_audioState.store(state, std::memory_order_release);
    }

    void reset() noexcept {
        m_processingTimeMs.store(0.0, std::memory_order_relaxed);
        m_xrunCount.store(0, std::memory_order_relaxed);
        m_inputLatencyMs.store(0.0, std::memory_order_relaxed);
        m_outputLatencyMs.store(0.0, std::memory_order_relaxed);
        m_bufferDurationMs.store(0.0, std::memory_order_relaxed);
        m_audioState.store(AudioState::Offline, std::memory_order_relaxed);
    }

    // =========================================================================
    // UI thread getters (atomic loads, memory_order_relaxed/acquire)
    // =========================================================================
    [[nodiscard]] AudioMetricsSnapshot getSnapshot() const noexcept {
        AudioMetricsSnapshot snapshot;
        snapshot.inputLatencyMs = m_inputLatencyMs.load(std::memory_order_relaxed);
        snapshot.outputLatencyMs = m_outputLatencyMs.load(std::memory_order_relaxed);
        snapshot.sampleRate = m_sampleRate.load(std::memory_order_relaxed);
        snapshot.bufferSize = m_bufferSize.load(std::memory_order_relaxed);
        snapshot.processingTimeMs = m_processingTimeMs.load(std::memory_order_relaxed);
        snapshot.bufferDurationMs = m_bufferDurationMs.load(std::memory_order_relaxed);
        snapshot.xrunCount = m_xrunCount.load(std::memory_order_relaxed);
        snapshot.audioState = m_audioState.load(std::memory_order_acquire);
        return snapshot;
    }

    [[nodiscard]] AudioState getState() const noexcept {
        return m_audioState.load(std::memory_order_acquire);
    }

    [[nodiscard]] uint64_t getXRunCount() const noexcept {
        return m_xrunCount.load(std::memory_order_relaxed);
    }

private:
    std::atomic<double> m_inputLatencyMs{0.0};
    std::atomic<double> m_outputLatencyMs{0.0};
    std::atomic<double> m_sampleRate{48000.0};
    std::atomic<uint32_t> m_bufferSize{128};
    std::atomic<double> m_processingTimeMs{0.0};
    std::atomic<double> m_bufferDurationMs{2.6666667};
    std::atomic<uint64_t> m_xrunCount{0};
    std::atomic<AudioState> m_audioState{AudioState::Offline};
};

} // namespace livemixer::audio
