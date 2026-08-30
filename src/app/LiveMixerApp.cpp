#include "LiveMixerApp.h"

namespace livemixer::app {

void LiveMixerApplication::initialise(const juce::String& /*commandLine*/) {
    // 1. Instantiate Device Manager
    m_deviceManager = audio::createAudioDeviceManager();

    // 2. Instantiate Audio Engine
    m_audioEngine = std::make_shared<audio::AudioEngine>(m_deviceManager);

    // 3. Initialize with standard default configuration (48000 Hz, 128 samples, stereo)
    audio::AudioConfig defaultConfig;
    defaultConfig.driverType = audio::DriverType::ASIO;
    defaultConfig.sampleRate = 48000.0;
    defaultConfig.bufferSize = 128;
    defaultConfig.inputChannelCount = 2;
    defaultConfig.outputChannelCount = 2;

    m_audioEngine->initialize(defaultConfig);

    // 4. Create and show main window
    m_mainWindow = std::make_unique<MainWindow>(getApplicationName(), m_audioEngine);
}

void LiveMixerApplication::shutdown() {
    if (m_audioEngine) {
        m_audioEngine->shutdown();
    }
    m_mainWindow = nullptr;
    m_audioEngine = nullptr;
    m_deviceManager = nullptr;
}

void LiveMixerApplication::systemRequestedQuit() {
    quit();
}

void LiveMixerApplication::anotherInstanceStarted(const juce::String& /*commandLine*/) {
    // Single instance enforcement
}

} // namespace livemixer::app

// Main application startup macro for JUCE
START_JUCE_APPLICATION(livemixer::app::LiveMixerApplication)
