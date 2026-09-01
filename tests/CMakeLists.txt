add_executable(LiveMixerTests
    AudioEngineTests.cpp
    ../src/audio/core/AudioState.cpp
    ../src/audio/core/AudioEngine.cpp
    ../src/audio/devices/AudioDeviceManager.cpp
    ../src/audio/mixer/MixerChannel.cpp
    ../src/audio/mixer/StereoMixerChannel.cpp
    ../src/audio/mixer/MixerEngine.cpp
)

target_include_directories(LiveMixerTests PRIVATE
    ../src
    ${LIVE_MIXER_ASIO_INCLUDE_DIRS}
)

target_compile_definitions(LiveMixerTests PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_ASIO=1
    JUCE_ASIO_USE_EXTERNAL_SDK=1
    JUCE_WASAPI=1
)

target_compile_options(LiveMixerTests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive-;/EHsc>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic>
)

set_target_properties(LiveMixerTests PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

target_link_libraries(LiveMixerTests PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_core
    juce::juce_events
)

add_test(NAME LiveMixerCoreTests COMMAND LiveMixerTests)
