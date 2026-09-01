#include "MixerChannel.h"

namespace livemixer::mixer {

MixerChannel::MixerChannel(int channelId, std::string name, std::string sourceName, int defaultInputIndex)
    : m_channelId(channelId),
      m_name(std::move(name)),
      m_sourceName(std::move(sourceName)),
      m_inputSourceIndex(defaultInputIndex)
{
}

void MixerChannel::process(const float* inputBuffer,
                           float* mixBusL,
                           float* mixBusR,
                           int numSamples,
                           bool isAudiblyMutedBySolo) noexcept
{
    if (numSamples <= 0) {
        return;
    }

    const bool enabled = m_enabled.load(std::memory_order_relaxed);
    if (!enabled || inputBuffer == nullptr) {
        m_peakLevel.store(0.0f, std::memory_order_relaxed);
        return;
    }

    const float gainDb = m_gainDb.load(std::memory_order_relaxed);
    const float faderDb = m_faderDb.load(std::memory_order_relaxed);
    const float pan = m_pan.load(std::memory_order_relaxed);
    const bool muted = m_muted.load(std::memory_order_relaxed);

    // Compute linear gain from input gain and fader values
    const float inGainLin = std::pow(10.0f, gainDb / 20.0f);
    const float faderGainLin = (faderDb <= -60.0f) ? 0.0f : std::pow(10.0f, faderDb / 20.0f);
    const float totalGain = inGainLin * faderGainLin;

    // Constant-power panning law
    // Pan angle theta in [0, pi/2]
    // pan in [-1.0, 1.0] -> normalized 0 to 1
    constexpr float kPiOver4 = 0.7853981633974483f; // pi / 4
    const float panNorm = (pan + 1.0f) * 0.5f; // 0.0 (left) to 1.0 (right)
    const float panAngle = panNorm * (kPiOver4 * 2.0f);
    const float panGainL = std::cos(panAngle);
    const float panGainR = std::sin(panAngle);

    const bool audible = (!muted && !isAudiblyMutedBySolo && totalGain > 0.0f);
    const float effectiveGainL = audible ? (totalGain * panGainL) : 0.0f;
    const float effectiveGainR = audible ? (totalGain * panGainR) : 0.0f;

    float maxPeak = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        const float inSample = inputBuffer[i];
        const float processedSample = inSample * totalGain;
        const float absSample = std::abs(processedSample);

        if (absSample > maxPeak) {
            maxPeak = absSample;
        }

        if (audible) {
            if (mixBusL != nullptr) {
                mixBusL[i] += inSample * effectiveGainL;
            }
            if (mixBusR != nullptr) {
                mixBusR[i] += inSample * effectiveGainR;
            }
        }
    }

    // Publish peak level to UI lock-free
    m_peakLevel.store(maxPeak, std::memory_order_relaxed);
    if (maxPeak >= 1.0f) {
        m_clipOccurred.store(true, std::memory_order_relaxed);
    }
}

} // namespace livemixer::mixer
