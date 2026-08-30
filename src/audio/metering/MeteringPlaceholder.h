#pragma once

namespace livemixer::metering {

/**
 * @brief Placeholder interface for Audio Metering & True Peak Detectors
 * Reserved for future milestone. Do NOT implement functionality in V0.1.
 */
class IMeteringEngine {
public:
    virtual ~IMeteringEngine() = default;
};

} // namespace livemixer::metering
