#include "StereoMixerChannel.h"

namespace livemixer::mixer {

StereoMixerChannel::StereoMixerChannel(int channelId, std::string name, std::string sourceName, int defaultInputL, int defaultInputR)
    : m_channelId(channelId),
      m_name(std::move(name)),
      m_sourceName(std::move(sourceName)),
      m_inputSourceIndexL(defaultInputL),
      m_inputSourceIndexR(defaultInputR)
{
}

void StereoMixerChannel::process(const float* inputBufferL,
                                 const float* inputBufferR,
                                 float* mixBusL,
                                 float* mixBusR,
                                 int numSamples,
                                 bool isAudiblyMutedBySolo) noexcept
{
    if (numSamples <= 0) {
        return;
    }

    const bool enabled = m_enabled.load(std::memory_order_relaxed);
    if (!enabled || (inputBufferL == nullptr && inputBufferR == nullptr)) {
        m_peakLevelL.store(0.0f, std::memory_order_relaxed);
        m_peakLevelR.store(0.0f, std::memory_order_relaxed);
        return;
    }

    const float gainDb = m_gainDb.load(std::memory_order_relaxed);
    const float faderDb = m_faderDb.load(std::memory_order_relaxed);
    const float balance = m_balance.load(std::memory_order_relaxed);
    const bool muted = m_muted.load(std::memory_order_relaxed);

    // Compute linear gain from input gain and fader values
    const float inGainLin = std::pow(10.0f, gainDb / 20.0f);
    const float faderGainLin = (faderDb <= -60.0f) ? 0.0f : std::pow(10.0f, faderDb / 20.0f);
    const float totalGain = inGainLin * faderGainLin;

    // Stereo Balance Law
    // balance <= 0 (panning left): Left = 1.0, Right = 1.0 + balance
    // balance > 0 (panning right): Left = 1.0 - balance, Right = 1.0
    float balGainL = 1.0f;
    float balGainR = 1.0f;

    if (balance < 0.0f) {
        balGainR = std::max(0.0f, 1.0f + balance);
    } else if (balance > 0.0f) {
        balGainL = std::max(0.0f, 1.0f - balance);
    }

    const bool audible = (!muted && !isAudiblyMutedBySolo && totalGain > 0.0f);
    const float effectiveGainL = audible ? (totalGain * balGainL) : 0.0f;
    const float effectiveGainR = audible ? (totalGain * balGainR) : 0.0f;

    float maxPeakL = 0.0f;
    float maxPeakR = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        const float inSampleL = (inputBufferL != nullptr) ? inputBufferL[i] : 0.0f;
        const float inSampleR = (inputBufferR != nullptr) ? inputBufferR[i] : 0.0f;

        const float processedL = inSampleL * totalGain * balGainL;
        const float processedR = inSampleR * totalGain * balGainR;

        const float absL = std::abs(processedL);
        const float absR = std::abs(processedR);

        if (absL > maxPeakL) maxPeakL = absL;
        if (absR > maxPeakR) maxPeakR = absR;

        if (audible) {
            if (mixBusL != nullptr) {
                mixBusL[i] += inSampleL * effectiveGainL;
            }
            if (mixBusR != nullptr) {
                mixBusR[i] += inSampleR * effectiveGainR;
            }
        }
    }

    // Publish peak levels to UI lock-free
    m_peakLevelL.store(maxPeakL, std::memory_order_relaxed);
    m_peakLevelR.store(maxPeakR, std::memory_order_relaxed);

    if (maxPeakL >= 1.0f || maxPeakR >= 1.0f) {
        m_clipOccurred.store(true, std::memory_order_relaxed);
    }
}

} // namespace livemixer::mixer
