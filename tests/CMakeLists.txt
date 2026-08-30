add_executable(LiveMixerTests
    AudioEngineTests.cpp
    ../src/audio/core/AudioState.cpp
    ../src/audio/core/AudioEngine.cpp
    ../src/audio/devices/AudioDeviceManager.cpp
)

target_include_directories(LiveMixerTests PRIVATE
    ../src
)

target_compile_definitions(LiveMixerTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_ASIO=1
    JUCE_WASAPI=1
)

add_test(NAME LiveMixerCoreTests COMMAND LiveMixerTests)
