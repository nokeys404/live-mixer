#pragma once

#include <string>
#include <atomic>
#include <algorithm>
#include <cmath>

namespace livemixer::mixer {

/**
 * @brief Stereo Mixer Channel model (CH3/4 Media) with stereo balance behavior.
 * 
 * Supports:
 * - Input Gain (-24 dB to +24 dB, default 0 dB)
 * - Fader (-60 dB to +10 dB, default 0 dB)
 * - Stereo Balance (-1.0 = Left, 0.0 = Center/Both, +1.0 = Right)
 * - Linked Left/Right gain attenuation
 * - Mute & Solo states
 * - Independent Left and Right peak metering with clip detection
 */
class StereoMixerChannel {
public:
    StereoMixerChannel(int channelId, std::string name, std::string sourceName, int defaultInputL = 2, int defaultInputR = 3);
    ~StereoMixerChannel() = default;

    // Non-copyable, non-movable for atomic safety
    StereoMixerChannel(const StereoMixerChannel&) = delete;
    StereoMixerChannel& operator=(const StereoMixerChannel&) = delete;

    // Identification
    [[nodiscard]] int getChannelId() const noexcept { return m_channelId; }
    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    [[nodiscard]] const std::string& getSourceName() const noexcept { return m_sourceName; }
    void setSourceName(const std::string& sourceName) { m_sourceName = sourceName; }

    [[nodiscard]] int getInputSourceIndexL() const noexcept { return m_inputSourceIndexL.load(std::memory_order_relaxed); }
    [[nodiscard]] int getInputSourceIndexR() const noexcept { return m_inputSourceIndexR.load(std::memory_order_relaxed); }
    void setInputSourceIndices(int inL, int inR) noexcept {
        m_inputSourceIndexL.store(inL, std::memory_order_relaxed);
        m_inputSourceIndexR.store(inR, std::memory_order_relaxed);
    }

    // Parameter Controls (Lock-free atomic access from UI / Audio threads)
    [[nodiscard]] float getGainDb() const noexcept { return m_gainDb.load(std::memory_order_relaxed); }
    void setGainDb(float gainDb) noexcept { m_gainDb.store(std::clamp(gainDb, -24.0f, 24.0f), std::memory_order_relaxed); }

    [[nodiscard]] float getFaderDb() const noexcept { return m_faderDb.load(std::memory_order_relaxed); }
    void setFaderDb(float faderDb) noexcept { m_faderDb.store(std::clamp(faderDb, -60.0f, 10.0f), std::memory_order_relaxed); }

    [[nodiscard]] float getBalance() const noexcept { return m_balance.load(std::memory_order_relaxed); }
    void setBalance(float balance) noexcept { m_balance.store(std::clamp(balance, -1.0f, 1.0f), std::memory_order_relaxed); }

    [[nodiscard]] bool isMuted() const noexcept { return m_muted.load(std::memory_order_relaxed); }
    void setMute(bool muted) noexcept { m_muted.store(muted, std::memory_order_relaxed); }

    [[nodiscard]] bool isSolo() const noexcept { return m_solo.load(std::memory_order_relaxed); }
    void setSolo(bool solo) noexcept { m_solo.store(solo, std::memory_order_relaxed); }

    [[nodiscard]] bool isEnabled() const noexcept { return m_enabled.load(std::memory_order_relaxed); }
    void setEnabled(bool enabled) noexcept { m_enabled.store(enabled, std::memory_order_relaxed); }

    // Metering (Read by UI, written by Audio Thread)
    [[nodiscard]] float getPeakLevelL() const noexcept { return m_peakLevelL.load(std::memory_order_relaxed); }
    [[nodiscard]] float getPeakLevelR() const noexcept { return m_peakLevelR.load(std::memory_order_relaxed); }
    [[nodiscard]] bool hasClipped() const noexcept { return m_clipOccurred.load(std::memory_order_relaxed); }
    void resetClip() noexcept { m_clipOccurred.store(false, std::memory_order_relaxed); }

    // Hard Real-Time Audio Processing (Zero allocation, zero locks, zero blocking)
    void process(const float* inputBufferL,
                 const float* inputBufferR,
                 float* mixBusL,
                 float* mixBusR,
                 int numSamples,
                 bool isAudiblyMutedBySolo) noexcept;

private:
    int m_channelId{3};
    std::string m_name;
    std::string m_sourceName;
    std::atomic<int> m_inputSourceIndexL{2};
    std::atomic<int> m_inputSourceIndexR{3};

    // Audio Parameters
    std::atomic<float> m_gainDb{0.0f};
    std::atomic<float> m_faderDb{0.0f};
    std::atomic<float> m_balance{0.0f}; // -1.0 Full Left, 0.0 Center, +1.0 Full Right
    std::atomic<bool> m_muted{false};
    std::atomic<bool> m_solo{false};
    std::atomic<bool> m_enabled{true};

    // Telemetry / Metering
    std::atomic<float> m_peakLevelL{0.0f};
    std::atomic<float> m_peakLevelR{0.0f};
    std::atomic<bool> m_clipOccurred{false};
};

} // namespace livemixer::mixer
