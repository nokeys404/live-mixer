#pragma once

#include <string>
#include <atomic>
#include <algorithm>
#include <cmath>

namespace livemixer::mixer {

/**
 * @brief Mono Mixer Channel model and lock-free realtime audio processing.
 * 
 * Supports:
 * - Input Gain (-24 dB to +24 dB, default 0 dB)
 * - Fader (-60 dB to +10 dB, default 0 dB)
 * - Pan (-1.0 = Left, 0.0 = Center, +1.0 = Right) with constant-power panning law
 * - Mute & Solo states
 * - Lock-free peak metering with clipping detection
 */
class MixerChannel {
public:
    MixerChannel(int channelId, std::string name, std::string sourceName, int defaultInputIndex = 0);
    ~MixerChannel() = default;

    // Non-copyable, non-movable for atomic safety
    MixerChannel(const MixerChannel&) = delete;
    MixerChannel& operator=(const MixerChannel&) = delete;

    // Identification
    [[nodiscard]] int getChannelId() const noexcept { return m_channelId; }
    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    [[nodiscard]] const std::string& getSourceName() const noexcept { return m_sourceName; }
    void setSourceName(const std::string& sourceName) { m_sourceName = sourceName; }

    [[nodiscard]] int getInputSourceIndex() const noexcept { return m_inputSourceIndex.load(std::memory_order_relaxed); }
    void setInputSourceIndex(int index) noexcept { m_inputSourceIndex.store(index, std::memory_order_relaxed); }

    // Parameter Controls (Lock-free atomic access from UI / Audio threads)
    [[nodiscard]] float getGainDb() const noexcept { return m_gainDb.load(std::memory_order_relaxed); }
    void setGainDb(float gainDb) noexcept { m_gainDb.store(std::clamp(gainDb, -24.0f, 24.0f), std::memory_order_relaxed); }

    [[nodiscard]] float getFaderDb() const noexcept { return m_faderDb.load(std::memory_order_relaxed); }
    void setFaderDb(float faderDb) noexcept { m_faderDb.store(std::clamp(faderDb, -60.0f, 10.0f), std::memory_order_relaxed); }

    [[nodiscard]] float getPan() const noexcept { return m_pan.load(std::memory_order_relaxed); }
    void setPan(float pan) noexcept { m_pan.store(std::clamp(pan, -1.0f, 1.0f), std::memory_order_relaxed); }

    [[nodiscard]] bool isMuted() const noexcept { return m_muted.load(std::memory_order_relaxed); }
    void setMute(bool muted) noexcept { m_muted.store(muted, std::memory_order_relaxed); }

    [[nodiscard]] bool isSolo() const noexcept { return m_solo.load(std::memory_order_relaxed); }
    void setSolo(bool solo) noexcept { m_solo.store(solo, std::memory_order_relaxed); }

    [[nodiscard]] bool isEnabled() const noexcept { return m_enabled.load(std::memory_order_relaxed); }
    void setEnabled(bool enabled) noexcept { m_enabled.store(enabled, std::memory_order_relaxed); }

    // Metering (Read by UI, written by Audio Thread)
    [[nodiscard]] float getPeakLevel() const noexcept { return m_peakLevel.load(std::memory_order_relaxed); }
    [[nodiscard]] bool hasClipped() const noexcept { return m_clipOccurred.load(std::memory_order_relaxed); }
    void resetClip() noexcept { m_clipOccurred.store(false, std::memory_order_relaxed); }

    // Hard Real-Time Audio Processing (Zero allocation, zero locks, zero blocking)
    void process(const float* inputBuffer,
                 float* mixBusL,
                 float* mixBusR,
                 int numSamples,
                 bool isAudiblyMutedBySolo) noexcept;

private:
    int m_channelId{1};
    std::string m_name;
    std::string m_sourceName;
    std::atomic<int> m_inputSourceIndex{0};

    // Audio Parameters
    std::atomic<float> m_gainDb{0.0f};
    std::atomic<float> m_faderDb{0.0f};
    std::atomic<float> m_pan{0.0f}; // -1.0 Left, 0.0 Center, +1.0 Right
    std::atomic<bool> m_muted{false};
    std::atomic<bool> m_solo{false};
    std::atomic<bool> m_enabled{true};

    // Telemetry / Metering
    std::atomic<float> m_peakLevel{0.0f};
    std::atomic<bool> m_clipOccurred{false};
};

} // namespace livemixer::mixer
