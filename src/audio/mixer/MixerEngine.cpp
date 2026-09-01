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

    // 3. Resolve Input Pointers
    const float* in0 = (numInputChannels > 0 && inputChannelData != nullptr) ? inputChannelData[0] : nullptr;
    const float* in1 = (numInputChannels > 1 && inputChannelData != nullptr) ? inputChannelData[1] : nullptr;
    const float* in2 = (numInputChannels > 2 && inputChannelData != nullptr) ? inputChannelData[2] : in0;
    const float* in3 = (numInputChannels > 3 && inputChannelData != nullptr) ? inputChannelData[3] : in1;

    // 4. Process Channels into Mix Bus
    m_ch1.process(in0, m_mixBusL, m_mixBusR, safeSamples, ch1AudibleMute);
    m_ch2.process(in1, m_mixBusL, m_mixBusR, safeSamples, ch2AudibleMute);
    m_ch34.process(in2, in3, m_mixBusL, m_mixBusR, safeSamples, ch34AudibleMute);

    // 5. Master Section Processing
    const float masterFaderDb = m_masterFaderDb.load(std::memory_order_relaxed);
    const bool masterMuted = m_masterMuted.load(std::memory_order_relaxed);

    const float masterGainLin = (masterMuted || masterFaderDb <= -60.0f) 
                                ? 0.0f 
                                : std::pow(10.0f, masterFaderDb / 20.0f);

    float* outL = (numOutputChannels > 0) ? outputChannelData[0] : nullptr;
    float* outR = (numOutputChannels > 1) ? outputChannelData[1] : nullptr;

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

    // 6. Clear any unused extra output channels
    for (int ch = 2; ch < numOutputChannels; ++ch) {
        float* extraOut = outputChannelData[ch];
        if (extraOut != nullptr) {
            std::memset(extraOut, 0, static_cast<size_t>(safeSamples) * sizeof(float));
        }
    }

    // 7. Publish Master Metering
    m_masterPeakL.store(masterPeakL, std::memory_order_relaxed);
    m_masterPeakR.store(masterPeakR, std::memory_order_relaxed);

    if (masterPeakL >= 1.0f || masterPeakR >= 1.0f) {
        m_masterClipOccurred.store(true, std::memory_order_relaxed);
    }
}

} // namespace livemixer::mixer
