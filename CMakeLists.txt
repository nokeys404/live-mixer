cmake_minimum_required(VERSION 3.20)
project(LiveMixer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Portable compiler flags
if (MSVC)
    add_compile_options(/W4 /WX- /permissive- /Zc:preprocessor /EHsc)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ==============================================================================
# ASIO SDK Configuration Note:
# On Windows, Steinberg ASIO SDK 2.3+ can be supplied by setting:
#   cmake -B build -DJUCE_ASIO_SDK_PATH="C:/path/to/asiosdk"
# If not provided, JUCE searches standard system paths.
# ==============================================================================

# ==============================================================================
# JUCE Setup
# Fetch or integrate JUCE 7/8 for desktop application and audio backends (ASIO/WASAPI)
# ==============================================================================
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.9
    GIT_SHALLOW TRUE
)
# Fetch JUCE if available, or fall back to system/local JUCE installation
FetchContent_MakeAvailable(JUCE)

# ==============================================================================
# Main LiveMixer Executable Target
# ==============================================================================
juce_add_gui_app(LiveMixer
    PRODUCT_NAME "Live Mixer"
    COMPANY_NAME "LiveMixer Audio"
    VERSION 0.1.0
)

target_sources(LiveMixer PRIVATE
    # Application Entry
    src/app/LiveMixerApp.h
    src/app/LiveMixerApp.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp

    # Audio Core
    src/audio/core/AudioConfig.h
    src/audio/core/AudioState.h
    src/audio/core/AudioState.cpp
    src/audio/core/AudioMetrics.h
    src/audio/core/AudioEngine.h
    src/audio/core/AudioEngine.cpp

    # Audio Devices (Thin application wrapper over juce::AudioDeviceManager)
    src/audio/devices/AudioDeviceManager.h
    src/audio/devices/AudioDeviceManager.cpp

    # Architecture Placeholders for Future Milestones
    src/audio/mixer/MixerPlaceholder.h
    src/audio/routing/RoutingPlaceholder.h
    src/audio/metering/MeteringPlaceholder.h
    src/parameters/ParameterPlaceholder.h
    src/dsp/DspPlaceholder.h

    # User Interface
    src/ui/AudioSettingsPanel.h
    src/ui/AudioSettingsPanel.cpp
)

target_include_directories(LiveMixer PRIVATE
    src
)

target_compile_definitions(LiveMixer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_ASIO=1
    JUCE_WASAPI=1
    JUCE_WASAPI_EXCLUSIVE=1
    JUCE_DIRECTSOUND=0
)

target_link_libraries(LiveMixer PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# ==============================================================================
# Unit Tests Target
# ==============================================================================
enable_testing()
add_subdirectory(tests)
