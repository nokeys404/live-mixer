#pragma once

#include <string>
#include <string_view>

namespace livemixer::audio {

/**
 * @brief Formal Audio State Machine states
 */
enum class AudioState {
    Offline,
    Initializing,
    Ready,
    Running,
    Stopping,
    Error,
    Recovering
};

[[nodiscard]] const char* audioStateToString(AudioState state) noexcept;

/**
 * @brief Checks if a transition between two audio states is valid according
 *        to the state machine specification.
 */
[[nodiscard]] bool isValidStateTransition(AudioState from, AudioState to) noexcept;

} // namespace livemixer::audio
