#include "MixerEngine.h"

namespace livemixer::mixer {

MixerEngine::MixerEngine()
    : m_ch1(1, "CH1 Mic", "Input 1", 0),
      m_ch2(2, "CH2 Inst", "Input 2", 1),
      m_ch34(3, "CH3/4 Media", "Inputs 3/4", 2, 3)
{
    std::memset(m_mixBusL, 0, sizeof(m_mixBusL));
    std::memset(m_mixBusR, 0, sizeof(m_mixBusR));
}

void MixerEngine::setDefaultsForDiscoveredChannels(int numInputs, int numOutputs) noexcept
{
    // Default Input Routing based strictly on discovered channel counts (never hardcoded vendors)
    m_ch1InputRoute.store(numInputs >= 1 ? 0 : -1, std::memory_order_relaxed);
    m_ch2InputRoute.store(numInputs >= 2 ? 1 : -1, std::memory_order_relaxed);
    m_ch34InputRouteL.store(numInputs >= 4 ? 2 : (numInputs >= 3 ? 2 : -1), std::memory_order_relaxed);
    m_ch34InputRouteR.store(numInputs >= 4 ? 3 : -1, std::memory_order_relaxed);

    // Default Output Routing
    m_masterOutputRouteL.store(numOutputs >= 1 ? 0 : -1, std::memory_order_relaxed);
    m_masterOutputRouteR.store(numOutputs >= 2 ? 1 : -1, std::memory_order_relaxed);
}

void MixerEngine::process(const float* const* inputChannelData,
                          int numInputChannels,
                          float* const* outputChannelData,
                          int numOutputChannels,
                          int numSamples) noexcept
{
    if (numSamples <= 0 || outputChannelData == nullptr) {
        return;
    }

    const int safeSamples = std::min(numSamples, static_cast<int>(MAX_MIXER_BUFFER_SAMPLES));

    // 1. Clear mix buses
    std::memset(m_mixBusL, 0, static_cast<size_t>(safeSamples) * sizeof(float));
    std::memset(m_mixBusR, 0, static_cast<size_t>(safeSamples) * sizeof(float));

    // 2. Global Solo Evaluation
    const bool ch1Solo = m_ch1.isSolo();
    const bool ch2Solo = m_ch2.isSolo();
    const bool ch34Solo = m_ch34.isSolo();
    const bool hasAnySolo = (ch1Solo || ch2Solo || ch34Solo);

    const bool ch1AudibleMute = hasAnySolo && !ch1Solo;
    const bool ch2AudibleMute = hasAnySolo && !ch2Solo;
    const bool ch34AudibleMute = hasAnySolo && !ch34Solo;

    // 3. Resolve Input Pointers via Explicit Routing Configuration (Zero crossfeed guarantee)
    const int rCh1 = m_ch1InputRoute.load(std::memory_order_relaxed);
    const int rCh2 = m_ch2InputRoute.load(std::memory_order_relaxed);
    const int rCh34L = m_ch34InputRouteL.load(std::memory_order_relaxed);
    const int rCh34R = m_ch34InputRouteR.load(std::memory_order_relaxed);

    const float* in0 = (rCh1 >= 0 && rCh1 < numInputChannels && inputChannelData != nullptr) ? inputChannelData[rCh1] : nullptr;
    const float* in1 = (rCh2 >= 0 && rCh2 < numInputChannels && inputChannelData != nullptr) ? inputChannelData[rCh2] : nullptr;
    const float* in2 = (rCh34L >= 0 && rCh34L < numInputChannels && inputChannelData != nullptr) ? inputChannelData[rCh34L] : nullptr;
    const float* in3 = (rCh34R >= 0 && rCh34R < numInputChannels && inputChannelData != nullptr) ? inputChannelData[rCh34R] : nullptr;

    float rawPeakCh1 = 0.0f;
    float rawPeakCh2 = 0.0f;
    if (in0 != nullptr) {
        for (int i = 0; i < safeSamples; ++i) {
            const float absVal = std::abs(in0[i]);
            if (absVal > rawPeakCh1) rawPeakCh1 = absVal;
        }
    }
    if (in1 != nullptr) {
        for (int i = 0; i < safeSamples; ++i) {
            const float absVal = std::abs(in1[i]);
            if (absVal > rawPeakCh2) rawPeakCh2 = absVal;
        }
    }
    m_rawInputPeakCh1.store(rawPeakCh1, std::memory_order_relaxed);
    m_rawInputPeakCh2.store(rawPeakCh2, std::memory_order_relaxed);

    // 4. Process Channels into Mix Bus
    m_ch1.process(in0, m_mixBusL, m_mixBusR, safeSamples, ch1AudibleMute);
    m_ch2.process(in1, m_mixBusL, m_mixBusR, safeSamples, ch2AudibleMute);
    m_ch34.process(in2, in3, m_mixBusL, m_mixBusR, safeSamples, ch34AudibleMute);

    // Measure Mix Bus Summing Peaks
    float mixPeakL = 0.0f;
    float mixPeakR = 0.0f;
    for (int i = 0; i < safeSamples; ++i) {
        const float valL = std::abs(m_mixBusL[i]);
        const float valR = std::abs(m_mixBusR[i]);
        if (valL > mixPeakL) mixPeakL = valL;
        if (valR > mixPeakR) mixPeakR = valR;
    }
    m_mixBusPeakL.store(mixPeakL, std::memory_order_relaxed);
    m_mixBusPeakR.store(mixPeakR, std::memory_order_relaxed);

    // 5. Master Section Processing & Output Routing
    const float masterFaderDb = m_masterFaderDb.load(std::memory_order_relaxed);
    const bool masterMuted = m_masterMuted.load(std::memory_order_relaxed);

    const float masterGainLin = (masterMuted || masterFaderDb <= -60.0f) 
                                ? 0.0f 
                                : std::pow(10.0f, masterFaderDb / 20.0f);

    const int rOutL = m_masterOutputRouteL.load(std::memory_order_relaxed);
    const int rOutR = m_masterOutputRouteR.load(std::memory_order_relaxed);

    // Clear all available output buffers to prevent floating noise
    for (int ch = 0; ch < numOutputChannels; ++ch) {
        float* outBuffer = (outputChannelData != nullptr) ? outputChannelData[ch] : nullptr;
        if (outBuffer != nullptr) {
            std::memset(outBuffer, 0, static_cast<size_t>(safeSamples) * sizeof(float));
        }
    }

    float* outL = (rOutL >= 0 && rOutL < numOutputChannels && outputChannelData != nullptr) ? outputChannelData[rOutL] : nullptr;
    float* outR = (rOutR >= 0 && rOutR < numOutputChannels && outputChannelData != nullptr) ? outputChannelData[rOutR] : nullptr;

    float masterPeakL = 0.0f;
    float masterPeakR = 0.0f;

    for (int i = 0; i < safeSamples; ++i) {
        const float sampleL = m_mixBusL[i] * masterGainLin;
        const float sampleR = m_mixBusR[i] * masterGainLin;

        const float absL = std::abs(sampleL);
        const float absR = std::abs(sampleR);

        if (absL > masterPeakL) masterPeakL = absL;
        if (absR > masterPeakR) masterPeakR = absR;

        if (outL != nullptr) {
            outL[i] = sampleL;
        }
        if (outR != nullptr) {
            outR[i] = sampleR;
        }
    }

    // 6. Publish Master and Output Metering
    m_masterPeakL.store(masterPeakL, std::memory_order_relaxed);
    m_masterPeakR.store(masterPeakR, std::memory_order_relaxed);
    m_outputPeakL.store(masterPeakL, std::memory_order_relaxed);
    m_outputPeakR.store(masterPeakR, std::memory_order_relaxed);

    if (masterPeakL >= 1.0f || masterPeakR >= 1.0f) {
        m_masterClipOccurred.store(true, std::memory_order_relaxed);
    }
}

} // namespace livemixer::mixer
