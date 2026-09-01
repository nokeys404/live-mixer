#pragma once

#include "MixerChannel.h"
#include "StereoMixerChannel.h"
#include <memory>
#include <atomic>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace livemixer::mixer {

/**
 * @brief 4-Channel Live Mixer Engine
 * 
 * Orchestrates:
 * - CH1 (Mono Mic / Input 1)
 * - CH2 (Mono Inst / Input 2)
 * - CH3/4 (Stereo Media / Inputs 3-4 or 1-2)
 * - Mix Bus (Stereo Summing Bus)
 * - Master Section (Master Fader, Master Mute, Master Metering)
 * 
 * Guarantees:
 * - Zero memory allocation in realtime process()
 * - Zero locks in realtime process()
 * - Realtime parameter update safety via atomics
 */
class MixerEngine {
public:
    MixerEngine();
    ~MixerEngine() = default;

    // Non-copyable, non-movable
    MixerEngine(const MixerEngine&) = delete;
    MixerEngine& operator=(const MixerEngine&) = delete;

    // Channel Accessors
    [[nodiscard]] MixerChannel& getChannel1() noexcept { return m_ch1; }
    [[nodiscard]] const MixerChannel& getChannel1() const noexcept { return m_ch1; }

    [[nodiscard]] MixerChannel& getChannel2() noexcept { return m_ch2; }
    [[nodiscard]] const MixerChannel& getChannel2() const noexcept { return m_ch2; }

    [[nodiscard]] StereoMixerChannel& getStereoChannel() noexcept { return m_ch34; }
    [[nodiscard]] const StereoMixerChannel& getStereoChannel() const noexcept { return m_ch34; }

    // Master Controls
    [[nodiscard]] float getMasterFaderDb() const noexcept { return m_masterFaderDb.load(std::memory_order_relaxed); }
    void setMasterFaderDb(float faderDb) noexcept { m_masterFaderDb.store(std::clamp(faderDb, -60.0f, 10.0f), std::memory_order_relaxed); }

    [[nodiscard]] bool isMasterMuted() const noexcept { return m_masterMuted.load(std::memory_order_relaxed); }
    void setMasterMute(bool muted) noexcept { m_masterMuted.store(muted, std::memory_order_relaxed); }

    // Master Metering
    [[nodiscard]] float getMasterPeakL() const noexcept { return m_masterPeakL.load(std::memory_order_relaxed); }
    [[nodiscard]] float getMasterPeakR() const noexcept { return m_masterPeakR.load(std::memory_order_relaxed); }
    [[nodiscard]] bool hasMasterClipped() const noexcept { return m_masterClipOccurred.load(std::memory_order_relaxed); }
    void resetMasterClip() noexcept { m_masterClipOccurred.store(false, std::memory_order_relaxed); }

    // Solo System Query
    [[nodiscard]] bool isAnyChannelSoloed() const noexcept {
        return m_ch1.isSolo() || m_ch2.isSolo() || m_ch34.isSolo();
    }

    // Hard Real-Time Mixer Process (Invoked directly from AudioEngine callback)
    void process(const float* const* inputChannelData,
                 int numInputChannels,
                 float* const* outputChannelData,
                 int numOutputChannels,
                 int numSamples) noexcept;

    static constexpr size_t MAX_MIXER_BUFFER_SAMPLES = 8192;

private:
    MixerChannel m_ch1;
    MixerChannel m_ch2;
    StereoMixerChannel m_ch34;

    // Master Section Parameters
    std::atomic<float> m_masterFaderDb{0.0f};
    std::atomic<bool> m_masterMuted{false};

    // Master Metering
    std::atomic<float> m_masterPeakL{0.0f};
    std::atomic<float> m_masterPeakR{0.0f};
    std::atomic<bool> m_masterClipOccurred{false};

    // Preallocated Mix Bus Buffers (No heap allocation in audio thread)
    alignas(16) float m_mixBusL[MAX_MIXER_BUFFER_SAMPLES];
    alignas(16) float m_mixBusR[MAX_MIXER_BUFFER_SAMPLES];
};

} // namespace livemixer::mixer
