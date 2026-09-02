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

    // Input and Output Routing (Lock-free realtime atomic indices, -1 = disabled)
    void setCh1InputRoute(int bufferIndex) noexcept { m_ch1InputRoute.store(bufferIndex, std::memory_order_relaxed); }
    [[nodiscard]] int getCh1InputRoute() const noexcept { return m_ch1InputRoute.load(std::memory_order_relaxed); }

    void setCh2InputRoute(int bufferIndex) noexcept { m_ch2InputRoute.store(bufferIndex, std::memory_order_relaxed); }
    [[nodiscard]] int getCh2InputRoute() const noexcept { return m_ch2InputRoute.load(std::memory_order_relaxed); }

    void setCh34InputRouteL(int bufferIndex) noexcept { m_ch34InputRouteL.store(bufferIndex, std::memory_order_relaxed); }
    [[nodiscard]] int getCh34InputRouteL() const noexcept { return m_ch34InputRouteL.load(std::memory_order_relaxed); }

    void setCh34InputRouteR(int bufferIndex) noexcept { m_ch34InputRouteR.store(bufferIndex, std::memory_order_relaxed); }
    [[nodiscard]] int getCh34InputRouteR() const noexcept { return m_ch34InputRouteR.load(std::memory_order_relaxed); }

    void setMasterOutputRouteL(int bufferIndex) noexcept { m_masterOutputRouteL.store(bufferIndex, std::memory_order_relaxed); }
    [[nodiscard]] int getMasterOutputRouteL() const noexcept { return m_masterOutputRouteL.load(std::memory_order_relaxed); }

    void setMasterOutputRouteR(int bufferIndex) noexcept { m_masterOutputRouteR.store(bufferIndex, std::memory_order_relaxed); }
    [[nodiscard]] int getMasterOutputRouteR() const noexcept { return m_masterOutputRouteR.load(std::memory_order_relaxed); }

    void setDefaultsForDiscoveredChannels(int numInputs, int numOutputs) noexcept;

    // Diagnostic Realtime Telemetry (calculated directly from realtime buffers)
    [[nodiscard]] float getRawInputPeakCh1() const noexcept { return m_rawInputPeakCh1.load(std::memory_order_relaxed); }
    [[nodiscard]] float getRawInputPeakCh2() const noexcept { return m_rawInputPeakCh2.load(std::memory_order_relaxed); }
    [[nodiscard]] float getCh1ProcessedPeak() const noexcept { return m_ch1.getPeakLevel(); }
    [[nodiscard]] float getCh2ProcessedPeak() const noexcept { return m_ch2.getPeakLevel(); }
    [[nodiscard]] float getMixBusPeakL() const noexcept { return m_mixBusPeakL.load(std::memory_order_relaxed); }
    [[nodiscard]] float getMixBusPeakR() const noexcept { return m_mixBusPeakR.load(std::memory_order_relaxed); }
    [[nodiscard]] float getOutputPeakL() const noexcept { return m_outputPeakL.load(std::memory_order_relaxed); }
    [[nodiscard]] float getOutputPeakR() const noexcept { return m_outputPeakR.load(std::memory_order_relaxed); }

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

    // Routing Configuration (Buffer index mapped to driver channel, -1 = disabled)
    std::atomic<int> m_ch1InputRoute{0};
    std::atomic<int> m_ch2InputRoute{1};
    std::atomic<int> m_ch34InputRouteL{2};
    std::atomic<int> m_ch34InputRouteR{3};
    std::atomic<int> m_masterOutputRouteL{0};
    std::atomic<int> m_masterOutputRouteR{1};

    // Master Section Parameters
    std::atomic<float> m_masterFaderDb{0.0f};
    std::atomic<bool> m_masterMuted{false};

    // Master Metering
    std::atomic<float> m_masterPeakL{0.0f};
    std::atomic<float> m_masterPeakR{0.0f};
    std::atomic<bool> m_masterClipOccurred{false};

    // Realtime Diagnostic Telemetry
    std::atomic<float> m_rawInputPeakCh1{0.0f};
    std::atomic<float> m_rawInputPeakCh2{0.0f};
    std::atomic<float> m_mixBusPeakL{0.0f};
    std::atomic<float> m_mixBusPeakR{0.0f};
    std::atomic<float> m_outputPeakL{0.0f};
    std::atomic<float> m_outputPeakR{0.0f};

    // Preallocated Mix Bus Buffers (No heap allocation in audio thread)
    alignas(16) float m_mixBusL[MAX_MIXER_BUFFER_SAMPLES];
    alignas(16) float m_mixBusR[MAX_MIXER_BUFFER_SAMPLES];
};

} // namespace livemixer::mixer
