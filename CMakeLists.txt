cmake_minimum_required(VERSION 3.20)

# Ensure modern MSVC runtime library selection (CMP0091)
if (POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

# Configure consistent dynamic MSVC runtime (/MDd in Debug, /MD in Release)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "MSVC runtime library" FORCE)

project(LiveMixer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==============================================================================
# ASIO SDK Configuration
# Supply the root path of the Steinberg ASIO SDK via:
#   -DLIVE_MIXER_ASIO_SDK_PATH="D:/NOKI/PROJECT VIRTUAL MIXER/asio-main/asio-main"
# ==============================================================================
set(LIVE_MIXER_ASIO_SDK_PATH "" CACHE PATH "Path to the Steinberg ASIO SDK root folder containing 'common/iasiodrv.h'")

set(LIVE_MIXER_ENABLE_ASIO ON)
set(LIVE_MIXER_ASIO_INCLUDE_DIRS "")

if (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (LIVE_MIXER_ASIO_SDK_PATH)
        # Normalize path
        file(TO_CMAKE_PATH "${LIVE_MIXER_ASIO_SDK_PATH}" _ASIO_SDK_NORM_PATH)
        set(ASIO_HEADER_PROBE "${_ASIO_SDK_NORM_PATH}/common/iasiodrv.h")
        
        if (EXISTS "${ASIO_HEADER_PROBE}")
            message(STATUS "Found Steinberg ASIO SDK at: ${_ASIO_SDK_NORM_PATH}")
            set(LIVE_MIXER_ASIO_INCLUDE_DIRS
                "${_ASIO_SDK_NORM_PATH}/common"
                "${_ASIO_SDK_NORM_PATH}/host"
                "${_ASIO_SDK_NORM_PATH}/host/pc"
            )
        else()
            message(FATAL_ERROR
                "LIVE_MIXER_ASIO_SDK_PATH was set to '${LIVE_MIXER_ASIO_SDK_PATH}', but '${ASIO_HEADER_PROBE}' was not found. "
                "Please verify that LIVE_MIXER_ASIO_SDK_PATH points to the root directory containing 'common/iasiodrv.h'."
            )
        endif()
    else()
        message(STATUS "LIVE_MIXER_ASIO_SDK_PATH is not set. Looking for default system/JUCE ASIO SDK paths.")
    endif()
endif()

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
    BUNDLE_ID "com.nokeys.livemixer"
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
    ${LIVE_MIXER_ASIO_INCLUDE_DIRS}
)

target_compile_definitions(LiveMixer PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_ASIO=1
    JUCE_ASIO_USE_EXTERNAL_SDK=1
    JUCE_WASAPI=1
    JUCE_WASAPI_EXCLUSIVE=1
    JUCE_DIRECTSOUND=0
)

target_compile_options(LiveMixer PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive-;/EHsc>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic>
)

set_target_properties(LiveMixer PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

# Also ensure juce::juce_audio_devices interface includes the ASIO headers if the target is available
if (TARGET juce_audio_devices AND LIVE_MIXER_ASIO_INCLUDE_DIRS)
    target_include_directories(juce_audio_devices INTERFACE ${LIVE_MIXER_ASIO_INCLUDE_DIRS})
    target_compile_definitions(juce_audio_devices INTERFACE JUCE_ASIO=1 JUCE_ASIO_USE_EXTERNAL_SDK=1)
endif()

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
