#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MainWindow.h"
#include "../audio/core/AudioEngine.h"
#include "../audio/devices/AudioDeviceManager.h"
#include <memory>

namespace livemixer::app {

class LiveMixerApplication : public juce::JUCEApplication {
public:
    LiveMixerApplication() = default;
    ~LiveMixerApplication() override = default;

    const juce::String getApplicationName() override { return "Live Mixer"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& commandLine) override;
    void shutdown() override;
    void systemRequestedQuit() override;
    void anotherInstanceStarted(const juce::String& commandLine) override;

private:
    std::shared_ptr<audio::IAudioDeviceManager> m_deviceManager;
    std::shared_ptr<audio::AudioEngine> m_audioEngine;
    std::unique_ptr<MainWindow> m_mainWindow;
};

} // namespace livemixer::app
