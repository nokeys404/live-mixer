#include "../src/audio/core/AudioConfig.h"
#include "../src/audio/core/AudioState.h"
#include "../src/audio/core/AudioMetrics.h"
#include "../src/audio/core/AudioEngine.h"
#include "../src/audio/devices/AudioDeviceManager.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>

using namespace livemixer::audio;

static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST_ASSERT(expr, msg) \
    do { \
        if (expr) { \
            std::cout << "  [PASS] " << msg << "\n"; \
            g_testsPassed++; \
        } else { \
            std::cerr << "  [FAIL] " << msg << " (line " << __LINE__ << ")\n"; \
            g_testsFailed++; \
        } \
    } while (0)

void testAudioConfigDefaults() {
    std::cout << "Running testAudioConfigDefaults...\n";
    AudioConfig config;

    TEST_ASSERT(config.driverType == DriverType::ASIO, "Default driver type should be ASIO");
    TEST_ASSERT(config.sampleRate == 48000.0, "Default sample rate should be 48000 Hz");
    TEST_ASSERT(config.bufferSize == 128, "Default buffer size should be 128 samples");
    TEST_ASSERT(config.inputChannelCount == 2, "Default input channel count should be 2");
    TEST_ASSERT(config.outputChannelCount == 2, "Default output channel count should be 2");
    TEST_ASSERT(config.isValid(), "Default config should be valid");
}

void testBufferDurationCalculation() {
    std::cout << "Running testBufferDurationCalculation...\n";
    AudioConfig config;
    config.sampleRate = 48000.0;
    config.bufferSize = 128;

    const double durationMs = config.getBufferDurationMs();
    // 128 / 48000 * 1000 = 2.6666666667 ms
    const double expected = 128.0 / 48000.0 * 1000.0;
    TEST_ASSERT(std::abs(durationMs - expected) < 0.0001, "Buffer duration for 48kHz/128 samples is ~2.6667 ms");
    TEST_ASSERT(std::abs(durationMs - 2.6666667) < 0.001, "Buffer duration matches approximately 2.6667 ms");

    // Test other sample rates
    AudioConfig config96k;
    config96k.sampleRate = 96000.0;
    config96k.bufferSize = 64;
    TEST_ASSERT(std::abs(config96k.getBufferDurationMs() - 0.6666667) < 0.001, "64 samples @ 96kHz = ~0.6667 ms");
}

void testAudioStateTransitions() {
    std::cout << "Running testAudioStateTransitions...\n";

    // Valid transitions
    TEST_ASSERT(isValidStateTransition(AudioState::Offline, AudioState::Initializing), "Offline -> Initializing is valid");
    TEST_ASSERT(isValidStateTransition(AudioState::Initializing, AudioState::Ready), "Initializing -> Ready is valid");
    TEST_ASSERT(isValidStateTransition(AudioState::Ready, AudioState::Running), "Ready -> Running is valid");
    TEST_ASSERT(isValidStateTransition(AudioState::Running, AudioState::Stopping), "Running -> Stopping is valid");
    TEST_ASSERT(isValidStateTransition(AudioState::Stopping, AudioState::Ready), "Stopping -> Ready is valid");
    TEST_ASSERT(isValidStateTransition(AudioState::Running, AudioState::Error), "Running -> Error is valid");
    TEST_ASSERT(isValidStateTransition(AudioState::Error, AudioState::Recovering), "Error -> Recovering is valid");
    TEST_ASSERT(isValidStateTransition(AudioState::Recovering, AudioState::Ready), "Recovering -> Ready is valid");

    // Invalid transitions
    TEST_ASSERT(!isValidStateTransition(AudioState::Offline, AudioState::Running), "Offline -> Running is invalid");
    TEST_ASSERT(!isValidStateTransition(AudioState::Initializing, AudioState::Stopping), "Initializing -> Stopping is invalid");
}

void testDeviceConfigurationValidation() {
    std::cout << "Running testDeviceConfigurationValidation...\n";

    AudioConfig validConfig;
    validConfig.driverType = DriverType::ASIO;
    validConfig.sampleRate = 48000.0;
    validConfig.bufferSize = 256;
    validConfig.inputChannelCount = 2;
    validConfig.outputChannelCount = 2;
    TEST_ASSERT(validConfig.isValid(), "Standard config is valid");

    AudioConfig invalidRate;
    invalidRate.sampleRate = 0.0;
    TEST_ASSERT(!invalidRate.isValid(), "0 Hz sample rate is invalid");

    AudioConfig invalidBuffer;
    invalidBuffer.bufferSize = 4; // Below minimum
    TEST_ASSERT(!invalidBuffer.isValid(), "4 sample buffer is invalid");

    AudioConfig invalidChannels;
    invalidChannels.inputChannelCount = 0;
    invalidChannels.outputChannelCount = 0;
    TEST_ASSERT(!invalidChannels.isValid(), "0 in / 0 out channels is invalid");
}

void testRealtimePassthroughAndSilence() {
    std::cout << "Running testRealtimePassthroughAndSilence...\n";
    std::shared_ptr<IAudioDeviceManager> devMgr = createAudioDeviceManager();
    auto engine = std::make_unique<AudioEngine>(devMgr);

    AudioConfig cfg;
    cfg.driverType = DriverType::ASIO;
    cfg.sampleRate = 48000.0;
    cfg.bufferSize = 128;
    cfg.inputChannelCount = 2;
    cfg.outputChannelCount = 4; // 4 outputs to test silence on extra channels

    bool initOk = engine->initialize(cfg);
    TEST_ASSERT(initOk, "AudioEngine successfully initializes");
    TEST_ASSERT(engine->getState() == AudioState::Ready, "Engine state is Ready after initialization");

    bool startOk = engine->start();
    TEST_ASSERT(startOk, "AudioEngine starts successfully");
    TEST_ASSERT(engine->getState() == AudioState::Running, "Engine state is Running after start");

    // Simulate realtime block
    const int numSamples = 128;
    float inLeft[numSamples];
    float inRight[numSamples];
    float outLeft[numSamples];
    float outRight[numSamples];
    float outExtra1[numSamples];
    float outExtra2[numSamples];

    for (int i = 0; i < numSamples; ++i) {
        inLeft[i] = 0.75f;
        inRight[i] = -0.5f;
        outLeft[i] = 0.0f;
        outRight[i] = 0.0f;
        outExtra1[i] = 1.0f; // Dirty buffer
        outExtra2[i] = 1.0f; // Dirty buffer
    }

    const float* inChannels[2] = { inLeft, inRight };
    float* outChannels[4] = { outLeft, outRight, outExtra1, outExtra2 };

    // Execute realtime audio callback
    engine->audioDeviceIOCallback(inChannels, 2, outChannels, 4, numSamples);

    // Verify passthrough accuracy: In 1 -> Out 1, In 2 -> Out 2
    TEST_ASSERT(outLeft[0] == 0.75f && outLeft[127] == 0.75f, "Hardware In 1 successfully passed to Out 1");
    TEST_ASSERT(outRight[0] == -0.5f && outRight[127] == -0.5f, "Hardware In 2 successfully passed to Out 2");

    // Verify silence on unused extra output channels
    TEST_ASSERT(outExtra1[0] == 0.0f && outExtra1[127] == 0.0f, "Extra output channel 3 written with silence");
    TEST_ASSERT(outExtra2[0] == 0.0f && outExtra2[127] == 0.0f, "Extra output channel 4 written with silence");

    auto metrics = engine->getMetrics();
    TEST_ASSERT(metrics.processingTimeMs >= 0.0, "Processing time recorded in metrics");
    TEST_ASSERT(metrics.audioState == AudioState::Running, "Metrics state shows Running");

    engine->stop();
    TEST_ASSERT(engine->getState() == AudioState::Ready, "Engine state returns to Ready after stop");
}

void testDeviceDisconnectHandling() {
    std::cout << "Running testDeviceDisconnectHandling...\n";
    std::shared_ptr<IAudioDeviceManager> devMgr = createAudioDeviceManager();
    auto engine = std::make_unique<AudioEngine>(devMgr);

    AudioConfig cfg;
    cfg.driverType = DriverType::ASIO;
    cfg.deviceName = "TestAudioDevice";
    engine->initialize(cfg);
    engine->start();

    TEST_ASSERT(engine->getState() == AudioState::Running, "Engine is running before disconnect");

    // Simulate device disconnect
    engine->simulateDeviceDisconnect();

    TEST_ASSERT(engine->getState() == AudioState::Error, "Engine safely transitions to Error upon device disconnect without crashing");
}

void testErrorDiagnostics() {
    std::cout << "Running testErrorDiagnostics...\n";
    std::shared_ptr<IAudioDeviceManager> devMgr = createAudioDeviceManager();
    auto engine = std::make_unique<AudioEngine>(devMgr);

    AudioConfig invalidCfg;
    invalidCfg.driverType = DriverType::ASIO;
    invalidCfg.sampleRate = 0.0; // Invalid configuration

    bool initResult = engine->initialize(invalidCfg);
    TEST_ASSERT(!initResult, "Initialization fails on invalid config");
    TEST_ASSERT(engine->getState() == AudioState::Error, "Engine state is Error on initialization failure");
    TEST_ASSERT(!engine->getLastError().empty(), "Error message is captured and non-empty");
}

void testExactErrorStringPreservation() {
    std::cout << "Running testExactErrorStringPreservation...\n";
    std::shared_ptr<IAudioDeviceManager> devMgr = createAudioDeviceManager();
    auto engine = std::make_unique<AudioEngine>(devMgr);

    AudioConfig validCfg;
    validCfg.driverType = DriverType::ASIO;
    validCfg.sampleRate = 48000.0;
    validCfg.bufferSize = 128;
    validCfg.inputChannelCount = 2;
    validCfg.outputChannelCount = 2;

    bool initResult = engine->initialize(validCfg);
    TEST_ASSERT(initResult, "Engine successfully initializes with valid config");
    TEST_ASSERT(engine->getLastError().empty(), "Error string is empty on clean initialization");

    // Simulate an explicit error notification from hardware listener
    const std::string simulatedDriverError = "ASIO initialization failed: ASIO TCH DICE USB Platform driver not responding";
    engine->onAudioDeviceError(simulatedDriverError);

    TEST_ASSERT(engine->getState() == AudioState::Error, "Engine transitioned to Error state upon device error");
    TEST_ASSERT(engine->getLastError() == simulatedDriverError, "Exact device driver error string preserved in AudioEngine");
}

void testDiagnosticInfoRetrieval() {
    std::cout << "Running testDiagnosticInfoRetrieval...\n";
    std::shared_ptr<IAudioDeviceManager> devMgr = createAudioDeviceManager();
    auto engine = std::make_unique<AudioEngine>(devMgr);

    AudioConfig validCfg;
    validCfg.driverType = DriverType::ASIO;
    validCfg.sampleRate = 48000.0;
    validCfg.bufferSize = 128;
    validCfg.inputChannelCount = 2;
    validCfg.outputChannelCount = 2;

    engine->initialize(validCfg);
    auto diag = engine->getDiagnosticInfo();

    TEST_ASSERT(diag.hasDevicePointer, "Diagnostic info reports valid device pointer when opened");
    TEST_ASSERT(diag.isDeviceOpen, "Diagnostic info reports device is open");
    TEST_ASSERT(diag.actualSampleRate == 48000.0, "Diagnostic reports correct actual sample rate");
    TEST_ASSERT(diag.actualBufferSize == 128, "Diagnostic reports correct actual buffer size");
    TEST_ASSERT(diag.activeInputChannels == 2, "Diagnostic reports 2 input channels");
    TEST_ASSERT(diag.activeOutputChannels == 2, "Diagnostic reports 2 output channels");
}

int main() {
    std::cout << "========================================\n";
    std::cout << "LIVE MIXER V0.1 FOUNDATION TEST SUITE\n";
    std::cout << "========================================\n\n";

    testAudioConfigDefaults();
    testBufferDurationCalculation();
    testAudioStateTransitions();
    testDeviceConfigurationValidation();
    testRealtimePassthroughAndSilence();
    testDeviceDisconnectHandling();
    testErrorDiagnostics();
    testExactErrorStringPreservation();
    testDiagnosticInfoRetrieval();

    std::cout << "\n========================================\n";
    std::cout << "TEST RESULTS: " << g_testsPassed << " passed, " << g_testsFailed << " failed.\n";
    std::cout << "========================================\n";

    return (g_testsFailed == 0) ? 0 : 1;
}
