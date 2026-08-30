#include "AudioState.h"

namespace livemixer::audio {

const char* audioStateToString(AudioState state) noexcept {
    switch (state) {
        case AudioState::Offline:      return "Offline";
        case AudioState::Initializing: return "Initializing";
        case AudioState::Ready:        return "Ready";
        case AudioState::Running:      return "Running";
        case AudioState::Stopping:     return "Stopping";
        case AudioState::Error:        return "Error";
        case AudioState::Recovering:   return "Recovering";
        default:                       return "Unknown";
    }
}

bool isValidStateTransition(AudioState from, AudioState to) noexcept {
    if (from == to) return true;

    switch (from) {
        case AudioState::Offline:
            return (to == AudioState::Initializing || to == AudioState::Error);

        case AudioState::Initializing:
            return (to == AudioState::Ready || to == AudioState::Error || to == AudioState::Offline);

        case AudioState::Ready:
            return (to == AudioState::Running || to == AudioState::Initializing || to == AudioState::Offline || to == AudioState::Error);

        case AudioState::Running:
            return (to == AudioState::Stopping || to == AudioState::Error || to == AudioState::Recovering);

        case AudioState::Stopping:
            return (to == AudioState::Ready || to == AudioState::Offline || to == AudioState::Error);

        case AudioState::Error:
            return (to == AudioState::Recovering || to == AudioState::Offline || to == AudioState::Initializing);

        case AudioState::Recovering:
            return (to == AudioState::Ready || to == AudioState::Running || to == AudioState::Error || to == AudioState::Offline);

        default:
            return false;
    }
}

} // namespace livemixer::audio
