#include "../src/audio/core/AudioConfig.h"
#include "../src/audio/core/AudioState.h"
#include "../src/audio/core/AudioMetrics.h"
#include "../src/audio/core/AudioEngine.h"
#include "../src/audio/devices/AudioDeviceManager.h"
#include "../src/audio/mixer/MixerEngine.h"
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

    // Set discrete Left / Right panning for pure In 1 -> Out 1, In 2 -> Out 2 test
    auto mixer = engine->getMixerEngine();
    if (mixer) {
        mixer->getChannel1().setPan(-1.0f); // Hard Left
        mixer->getChannel2().setPan(1.0f);  // Hard Right
        mixer->getStereoChannel().setMute(true); // Isolate CH1 and CH2
    }

    // Execute realtime audio callback
    engine->audioDeviceIOCallback(inChannels, 2, outChannels, 4, numSamples);

    // Verify passthrough accuracy: In 1 -> Out 1, In 2 -> Out 2
    TEST_ASSERT(std::abs(outLeft[0] - 0.75f) < 0.001f && std::abs(outLeft[127] - 0.75f) < 0.001f, "Hardware In 1 successfully passed to Out 1");
    TEST_ASSERT(std::abs(outRight[0] - (-0.5f)) < 0.001f && std::abs(outRight[127] - (-0.5f)) < 0.001f, "Hardware In 2 successfully passed to Out 2");

    // Verify silence on unused extra output channels
    TEST_ASSERT(outExtra1[0] == 0.0f && outExtra1[127] == 0.0f, "Extra output channel 3 written with silence");
    TEST_ASSERT(outExtra2[0] == 0.0f && outExtra2[127] == 0.0f, "Extra output channel 4 written with silence");

    auto metrics = engine->getMetrics();
    TEST_ASSERT(metrics.processingTimeMs >= 0.0, "Processing time recorded in metrics");
    TEST_ASSERT(metrics.audioState == AudioState::Running, "Metrics state shows Running");

    engine->stop();
    TEST_ASSERT(engine->getState() == AudioState::Ready, "Engine state returns to Ready after stop");
}

void testMonoChannelProcessingAndPanning() {
    std::cout << "Running testMonoChannelProcessingAndPanning...\n";
    livemixer::mixer::MixerChannel ch(1, "CH1", "In 1", 0);

    const int numSamples = 64;
    float in[numSamples];
    float busL[numSamples];
    float busR[numSamples];

    for (int i = 0; i < numSamples; ++i) {
        in[i] = 1.0f;
        busL[i] = 0.0f;
        busR[i] = 0.0f;
    }

    // Test Center Pan (Constant Power: 0.7071 on both L and R)
    ch.setPan(0.0f);
    ch.setGainDb(0.0f);
    ch.setFaderDb(0.0f);
    ch.process(in, busL, busR, numSamples, false);

    TEST_ASSERT(std::abs(busL[0] - 0.7071f) < 0.01f, "Center pan left bus produces ~0.7071");
    TEST_ASSERT(std::abs(busR[0] - 0.7071f) < 0.01f, "Center pan right bus produces ~0.7071");
    TEST_ASSERT(ch.getPeakLevel() > 0.7f, "Peak level recorded correctly");

    // Test Hard Left
    std::memset(busL, 0, sizeof(busL));
    std::memset(busR, 0, sizeof(busR));
    ch.setPan(-1.0f);
    ch.process(in, busL, busR, numSamples, false);
    TEST_ASSERT(std::abs(busL[0] - 1.0f) < 0.01f, "Hard left pan produces 1.0 on Left");
    TEST_ASSERT(std::abs(busR[0] - 0.0f) < 0.001f, "Hard left pan produces 0.0 on Right");

    // Test Hard Right
    std::memset(busL, 0, sizeof(busL));
    std::memset(busR, 0, sizeof(busR));
    ch.setPan(1.0f);
    ch.process(in, busL, busR, numSamples, false);
    TEST_ASSERT(std::abs(busL[0] - 0.0f) < 0.001f, "Hard right pan produces 0.0 on Left");
    TEST_ASSERT(std::abs(busR[0] - 1.0f) < 0.01f, "Hard right pan produces 1.0 on Right");

    // Test Mute
    std::memset(busL, 0, sizeof(busL));
    std::memset(busR, 0, sizeof(busR));
    ch.setMute(true);
    ch.process(in, busL, busR, numSamples, false);
    TEST_ASSERT(busL[0] == 0.0f && busR[0] == 0.0f, "Muted channel produces silence");
}

void testStereoChannelProcessingAndBalance() {
    std::cout << "Running testStereoChannelProcessingAndBalance...\n";
    livemixer::mixer::StereoMixerChannel ch(3, "CH3/4", "In 3/4", 2, 3);

    const int numSamples = 64;
    float inL[numSamples];
    float inR[numSamples];
    float busL[numSamples];
    float busR[numSamples];

    for (int i = 0; i < numSamples; ++i) {
        inL[i] = 1.0f;
        inR[i] = 0.5f;
        busL[i] = 0.0f;
        busR[i] = 0.0f;
    }

    // Center Balance
    ch.setBalance(0.0f);
    ch.process(inL, inR, busL, busR, numSamples, false);
    TEST_ASSERT(std::abs(busL[0] - 1.0f) < 0.001f, "Stereo center balance preserves left channel");
    TEST_ASSERT(std::abs(busR[0] - 0.5f) < 0.001f, "Stereo center balance preserves right channel");
    TEST_ASSERT(ch.getPeakLevelL() == 1.0f, "Left peak is 1.0");
    TEST_ASSERT(ch.getPeakLevelR() == 0.5f, "Right peak is 0.5");

    // Balance Hard Left (L at 1.0, R attenuated to 0)
    std::memset(busL, 0, sizeof(busL));
    std::memset(busR, 0, sizeof(busR));
    ch.setBalance(-1.0f);
    ch.process(inL, inR, busL, busR, numSamples, false);
    TEST_ASSERT(std::abs(busL[0] - 1.0f) < 0.001f, "Stereo hard left retains left channel");
    TEST_ASSERT(std::abs(busR[0] - 0.0f) < 0.001f, "Stereo hard left silences right channel");
}

void testMixerEngineSoloLogic() {
    std::cout << "Running testMixerEngineSoloLogic...\n";
    livemixer::mixer::MixerEngine engine;

    const int numSamples = 64;
    float in0[numSamples], in1[numSamples], in2[numSamples], in3[numSamples];
    float out0[numSamples], out1[numSamples];

    for (int i = 0; i < numSamples; ++i) {
        in0[i] = 0.6f;
        in1[i] = 0.4f;
        in2[i] = 0.2f;
        in3[i] = 0.2f;
        out0[i] = 0.0f;
        out1[i] = 0.0f;
    }

    const float* inChannels[4] = { in0, in1, in2, in3 };
    float* outChannels[2] = { out0, out1 };

    // Solo CH1 only
    engine.getChannel1().setSolo(true);
    engine.getChannel1().setPan(-1.0f); // Hard Left

    engine.process(inChannels, 4, outChannels, 2, numSamples);

    // CH1 (0.6f) should be heard on Out 0; CH2 and CH3/4 must be muted
    TEST_ASSERT(std::abs(out0[0] - 0.6f) < 0.01f, "Soloed CH1 is audible on Out 0");
    TEST_ASSERT(std::abs(out1[0] - 0.0f) < 0.001f, "Un-soloed channels are silent on Out 1");

    // Release Solo
    engine.getChannel1().setSolo(false);
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

void testRealtimeTelemetryAndSignalPath() {
    std::cout << "Running testRealtimeTelemetryAndSignalPath...\n";
    livemixer::mixer::MixerEngine engine;

    const int numSamples = 128;
    float in0[numSamples];
    float in1[numSamples];
    float out0[numSamples];
    float out1[numSamples];

    for (int i = 0; i < numSamples; ++i) {
        in0[i] = 0.8f;
        in1[i] = -0.5f;
        out0[i] = 0.0f;
        out1[i] = 0.0f;
    }

    const float* inChannels[2] = { in0, in1 };
    float* outChannels[2] = { out0, out1 };

    // Process block
    engine.process(inChannels, 2, outChannels, 2, numSamples);

    // Verify diagnostic peak telemetry calculation
    TEST_ASSERT(std::abs(engine.getRawInputPeakCh1() - 0.8f) < 0.001f, "Raw Input CH1 peak correctly tracked as 0.8");
    TEST_ASSERT(std::abs(engine.getRawInputPeakCh2() - 0.5f) < 0.001f, "Raw Input CH2 peak correctly tracked as 0.5");
    TEST_ASSERT(engine.getCh1ProcessedPeak() > 0.5f, "CH1 processed peak correctly tracked");
    TEST_ASSERT(engine.getCh2ProcessedPeak() > 0.3f, "CH2 processed peak correctly tracked");
    TEST_ASSERT(engine.getMixBusPeakL() > 0.0f && engine.getMixBusPeakR() > 0.0f, "Mix bus peaks non-zero");
    TEST_ASSERT(engine.getMasterPeakL() > 0.0f && engine.getMasterPeakR() > 0.0f, "Master output peaks non-zero");
    TEST_ASSERT(engine.getOutputPeakL() > 0.0f && engine.getOutputPeakR() > 0.0f, "Output peaks non-zero");
}

void testDriverDiscoveredChannelEnumeration() {
    std::cout << "Running testDriverDiscoveredChannelEnumeration...\n";
    std::shared_ptr<IAudioDeviceManager> devMgr = createAudioDeviceManager();
    auto engine = std::make_unique<AudioEngine>(devMgr);

    AudioConfig cfg;
    cfg.driverType = DriverType::ASIO;
    cfg.inputChannelCount = 8;
    cfg.outputChannelCount = 8;

    bool initOk = engine->initialize(cfg);
    TEST_ASSERT(initOk, "AudioEngine successfully initializes with multi-channel configuration");

    const auto inChannels = engine->getDiscoveredInputChannels();
    const auto outChannels = engine->getDiscoveredOutputChannels();

    TEST_ASSERT(inChannels.size() == 8, "8 input channels discovered");
    TEST_ASSERT(outChannels.size() == 8, "8 output channels discovered");
    TEST_ASSERT(inChannels[0].channelIndex == 0, "Input 0 index is 0");
    TEST_ASSERT(inChannels[7].channelIndex == 7, "Input 7 index is 7");
}

void testExplicitRoutingAndZeroChannelBleed() {
    std::cout << "Running testExplicitRoutingAndZeroChannelBleed...\n";
    livemixer::mixer::MixerEngine mixer;

    const int numSamples = 64;
    // Simulate 8 ASIO input channels (like Maono ProStudio 2x2 ASIO driver)
    std::vector<float> buffers[8];
    for (int ch = 0; ch < 8; ++ch) {
        buffers[ch].assign(numSamples, 0.0f);
    }

    // Signal ONLY on Channel 0 (e.g. Maono MIC In 1)
    for (int i = 0; i < numSamples; ++i) {
        buffers[0][i] = 0.9f;
    }

    const float* inPointers[8];
    for (int ch = 0; ch < 8; ++ch) {
        inPointers[ch] = buffers[ch].data();
    }

    float out0[numSamples];
    float out1[numSamples];
    std::memset(out0, 0, sizeof(out0));
    std::memset(out1, 0, sizeof(out1));
    float* outPointers[2] = { out0, out1 };

    // Explicitly configure routing:
    // CH1 -> Input 0
    // CH2 -> Input 1
    // CH3/4 -> Input 2 and 3
    mixer.setCh1InputRoute(0);
    mixer.setCh2InputRoute(1);
    mixer.setCh34InputRouteL(2);
    mixer.setCh34InputRouteR(3);
    mixer.setMasterOutputRouteL(0);
    mixer.setMasterOutputRouteR(1);

    // Process block
    mixer.process(inPointers, 8, outPointers, 2, numSamples);

    // Verify complete channel isolation and ZERO cross-bleed
    TEST_ASSERT(mixer.getChannel1().getPeakLevel() > 0.8f, "CH1 meter active for routed Input 0");
    TEST_ASSERT(mixer.getChannel2().getPeakLevel() == 0.0f, "CH2 meter is STRICTLY 0.0 (NO BLEED from Input 0)");
    TEST_ASSERT(mixer.getStereoChannel().getPeakLevelL() == 0.0f, "CH3/4 Left meter is STRICTLY 0.0 (NO BLEED from Input 0)");
    TEST_ASSERT(mixer.getStereoChannel().getPeakLevelR() == 0.0f, "CH3/4 Right meter is STRICTLY 0.0 (NO BLEED from Input 0)");
    TEST_ASSERT(std::abs(mixer.getRawInputPeakCh1() - 0.9f) < 0.001f, "Raw Input CH1 peak is 0.9");
    TEST_ASSERT(mixer.getRawInputPeakCh2() == 0.0f, "Raw Input CH2 peak is 0.0");

    // Dynamic Route Switch: Re-route CH1 to Input 1 (which has silence)
    mixer.setCh1InputRoute(1);
    mixer.process(inPointers, 8, outPointers, 2, numSamples);
    TEST_ASSERT(mixer.getChannel1().getPeakLevel() == 0.0f, "CH1 dynamically switched to Input 1 and is now 0.0");
}

void testDisconnectedRouteGeneratesSilence() {
    std::cout << "Running testDisconnectedRouteGeneratesSilence...\n";
    livemixer::mixer::MixerEngine mixer;

    const int numSamples = 64;
    float in0[numSamples];
    for (int i = 0; i < numSamples; ++i) in0[i] = 1.0f;
    const float* inPointers[1] = { in0 };

    float out0[numSamples], out1[numSamples];
    float* outPointers[2] = { out0, out1 };

    // Set CH1 to disconnected (-1)
    mixer.setCh1InputRoute(-1);
    mixer.process(inPointers, 1, outPointers, 2, numSamples);

    TEST_ASSERT(mixer.getChannel1().getPeakLevel() == 0.0f, "Disconnected CH1 route results in 0.0 meter");
    TEST_ASSERT(out0[0] == 0.0f && out1[0] == 0.0f, "Disconnected routes output pure silence");
}

void testIsolationTestA_SignalOnlyOnInput0() {
    std::cout << "Running testIsolationTestA_SignalOnlyOnInput0...\n";
    livemixer::mixer::MixerEngine mixer;
    const int numSamples = 64;
    float in0[numSamples], in1[numSamples], in2[numSamples], in3[numSamples];
    for (int i = 0; i < numSamples; ++i) {
        in0[i] = 0.8f;
        in1[i] = 0.0f;
        in2[i] = 0.0f;
        in3[i] = 0.0f;
    }
    const float* inPointers[4] = { in0, in1, in2, in3 };
    float out0[numSamples], out1[numSamples];
    float* outPointers[2] = { out0, out1 };

    mixer.setCh1InputRoute(0);
    mixer.setCh2InputRoute(1);
    mixer.setCh34InputRouteL(2);
    mixer.setCh34InputRouteR(3);

    mixer.process(inPointers, 4, outPointers, 2, numSamples);

    TEST_ASSERT(mixer.getChannel1().getPeakLevel() > 0.5f, "TEST A: CH1 is active");
    TEST_ASSERT(mixer.getChannel2().getPeakLevel() == 0.0f, "TEST A: CH2 is silent");
    TEST_ASSERT(mixer.getStereoChannel().getPeakLevelL() == 0.0f, "TEST A: CH3/4 L is silent");
    TEST_ASSERT(mixer.getStereoChannel().getPeakLevelR() == 0.0f, "TEST A: CH3/4 R is silent");
}

void testIsolationTestB_SignalOnlyOnInput1() {
    std::cout << "Running testIsolationTestB_SignalOnlyOnInput1...\n";
    livemixer::mixer::MixerEngine mixer;
    const int numSamples = 64;
    float in0[numSamples], in1[numSamples], in2[numSamples], in3[numSamples];
    for (int i = 0; i < numSamples; ++i) {
        in0[i] = 0.0f;
        in1[i] = 0.8f;
        in2[i] = 0.0f;
        in3[i] = 0.0f;
    }
    const float* inPointers[4] = { in0, in1, in2, in3 };
    float out0[numSamples], out1[numSamples];
    float* outPointers[2] = { out0, out1 };

    mixer.setCh1InputRoute(0);
    mixer.setCh2InputRoute(1);
    mixer.setCh34InputRouteL(2);
    mixer.setCh34InputRouteR(3);

    mixer.process(inPointers, 4, outPointers, 2, numSamples);

    TEST_ASSERT(mixer.getChannel1().getPeakLevel() == 0.0f, "TEST B: CH1 is silent");
    TEST_ASSERT(mixer.getChannel2().getPeakLevel() > 0.5f, "TEST B: CH2 is active");
    TEST_ASSERT(mixer.getStereoChannel().getPeakLevelL() == 0.0f, "TEST B: CH3/4 L is silent");
    TEST_ASSERT(mixer.getStereoChannel().getPeakLevelR() == 0.0f, "TEST B: CH3/4 R is silent");
}

void testIsolationTestC_SignalOnlyOnInput2() {
    std::cout << "Running testIsolationTestC_SignalOnlyOnInput2...\n";
    livemixer::mixer::MixerEngine mixer;
    const int numSamples = 64;
    float in0[numSamples], in1[numSamples], in2[numSamples], in3[numSamples];
    for (int i = 0; i < numSamples; ++i) {
        in0[i] = 0.0f;
        in1[i] = 0.0f;
        in2[i] = 0.8f;
        in3[i] = 0.0f;
    }
    const float* inPointers[4] = { in0, in1, in2, in3 };
    float out0[numSamples], out1[numSamples];
    float* outPointers[2] = { out0, out1 };

    mixer.setCh1InputRoute(0);
    mixer.setCh2InputRoute(1);
    mixer.setCh34InputRouteL(2);
    mixer.setCh34InputRouteR(3);

    mixer.process(inPointers, 4, outPointers, 2, numSamples);

    TEST_ASSERT(mixer.getChannel1().getPeakLevel() == 0.0f, "TEST C: CH1 is silent");
    TEST_ASSERT(mixer.getChannel2().getPeakLevel() == 0.0f, "TEST C: CH2 is silent");
    TEST_ASSERT(mixer.getStereoChannel().getPeakLevelL() > 0.5f, "TEST C: CH3/4 L is active");
    TEST_ASSERT(mixer.getStereoChannel().getPeakLevelR() == 0.0f, "TEST C: CH3/4 R is silent");
}

void testIsolationTestD_SignalOnlyOnInput3() {
    std::cout << "Running testIsolationTestD_SignalOnlyOnInput3...\n";
    livemixer::mixer::MixerEngine mixer;
    const int numSamples = 64;
    float in0[numSamples], in1[numSamples], in2[numSamples], in3[numSamples];
    for (int i = 0; i < numSamples; ++i) {
        in0[i] = 0.0f;
        in1[i] = 0.0f;
        in2[i] = 0.0f;
        in3[i] = 0.8f;
    }
    const float* inPointers[4] = { in0, in1, in2, in3 };
    float out0[numSamples], out1[numSamples];
    float* outPointers[2] = { out0, out1 };

    mixer.setCh1InputRoute(0);
    mixer.setCh2InputRoute(1);
    mixer.setCh34InputRouteL(2);
    mixer.setCh34InputRouteR(3);

    mixer.process(inPointers, 4, outPointers, 2, numSamples);

    TEST_ASSERT(mixer.getChannel1().getPeakLevel() == 0.0f, "TEST D: CH1 is silent");
    TEST_ASSERT(mixer.getChannel2().getPeakLevel() == 0.0f, "TEST D: CH2 is silent");
    TEST_ASSERT(mixer.getStereoChannel().getPeakLevelL() == 0.0f, "TEST D: CH3/4 L is silent");
    TEST_ASSERT(mixer.getStereoChannel().getPeakLevelR() > 0.5f, "TEST D: CH3/4 R is active");
}

void testIsolationTestE_NoInputSignals() {
    std::cout << "Running testIsolationTestE_NoInputSignals...\n";
    livemixer::mixer::MixerEngine mixer;
    const int numSamples = 64;
    float in0[numSamples], in1[numSamples], in2[numSamples], in3[numSamples];
    for (int i = 0; i < numSamples; ++i) {
        in0[i] = 0.0f;
        in1[i] = 0.0f;
        in2[i] = 0.0f;
        in3[i] = 0.0f;
    }
    const float* inPointers[4] = { in0, in1, in2, in3 };
    float out0[numSamples], out1[numSamples];
    float* outPointers[2] = { out0, out1 };

    mixer.setCh1InputRoute(0);
    mixer.setCh2InputRoute(1);
    mixer.setCh34InputRouteL(2);
    mixer.setCh34InputRouteR(3);

    mixer.process(inPointers, 4, outPointers, 2, numSamples);

    TEST_ASSERT(mixer.getChannel1().getPeakLevel() == 0.0f, "TEST E: CH1 is silent");
    TEST_ASSERT(mixer.getChannel2().getPeakLevel() == 0.0f, "TEST E: CH2 is silent");
    TEST_ASSERT(mixer.getStereoChannel().getPeakLevelL() == 0.0f, "TEST E: CH3/4 L is silent");
    TEST_ASSERT(mixer.getStereoChannel().getPeakLevelR() == 0.0f, "TEST E: CH3/4 R is silent");
    TEST_ASSERT(mixer.getMasterPeakL() == 0.0f && mixer.getMasterPeakR() == 0.0f, "TEST E: MASTER is silent");
    TEST_ASSERT(out0[0] == 0.0f && out1[0] == 0.0f, "TEST E: Output buffers contain pure silence");
}

void testIsolationTestF_CustomRouting() {
    std::cout << "Running testIsolationTestF_CustomRouting...\n";
    livemixer::mixer::MixerEngine mixer;
    const int numSamples = 64;
    std::vector<float> inBuffers[8];
    for (int ch = 0; ch < 8; ++ch) {
        inBuffers[ch].assign(numSamples, 0.0f);
    }

    // Assign unique distinct signal amplitudes to specific channels
    for (int i = 0; i < numSamples; ++i) {
        inBuffers[5][i] = 0.5f; // Destined for CH1
        inBuffers[2][i] = 0.6f; // Destined for CH2
        inBuffers[7][i] = 0.7f; // Destined for CH3/4 L
        inBuffers[6][i] = 0.8f; // Destined for CH3/4 R
    }

    const float* inPointers[8];
    for (int ch = 0; ch < 8; ++ch) {
        inPointers[ch] = inBuffers[ch].data();
    }

    float out0[numSamples], out1[numSamples];
    float* outPointers[2] = { out0, out1 };

    // Set custom mapping:
    // CH1 -> input 5
    // CH2 -> input 2
    // CH3/4 L -> input 7
    // CH3/4 R -> input 6
    mixer.setCh1InputRoute(5);
    mixer.setCh2InputRoute(2);
    mixer.setCh34InputRouteL(7);
    mixer.setCh34InputRouteR(6);

    mixer.process(inPointers, 8, outPointers, 2, numSamples);

    TEST_ASSERT(std::abs(mixer.getChannel1().getPeakLevel() - 0.5f) < 0.01f, "TEST F: CH1 received ONLY input 5 (0.5f)");
    TEST_ASSERT(std::abs(mixer.getChannel2().getPeakLevel() - 0.6f) < 0.01f, "TEST F: CH2 received ONLY input 2 (0.6f)");
    TEST_ASSERT(std::abs(mixer.getStereoChannel().getPeakLevelL() - 0.7f) < 0.01f, "TEST F: CH3/4 L received ONLY input 7 (0.7f)");
    TEST_ASSERT(std::abs(mixer.getStereoChannel().getPeakLevelR() - 0.8f) < 0.01f, "TEST F: CH3/4 R received ONLY input 6 (0.8f)");
}

void testMasterOutputIsolation() {
    std::cout << "Running testMasterOutputIsolation...\n";
    livemixer::mixer::MixerEngine mixer;
    const int numSamples = 64;

    float in0[numSamples];
    for (int i = 0; i < numSamples; ++i) in0[i] = 0.85f;
    const float* inPointers[1] = { in0 };

    // 4 output channels
    float out0[numSamples], out1[numSamples], out2[numSamples], out3[numSamples];
    for (int i = 0; i < numSamples; ++i) {
        out0[i] = -999.0f; // Dirty memory
        out1[i] = -999.0f;
        out2[i] = -999.0f;
        out3[i] = -999.0f;
    }
    float* outPointers[4] = { out0, out1, out2, out3 };

    // Route CH1 -> 0
    mixer.setCh1InputRoute(0);
    // Route Master L -> output 2, Master R -> output 3
    mixer.setMasterOutputRouteL(2);
    mixer.setMasterOutputRouteR(3);

    mixer.process(inPointers, 1, outPointers, 4, numSamples);

    // Verify output 0 and 1 are strictly ZEROED (no leak/duplication to unselected outputs)
    bool out0IsZero = true;
    bool out1IsZero = true;
    for (int i = 0; i < numSamples; ++i) {
        if (out0[i] != 0.0f) out0IsZero = false;
        if (out1[i] != 0.0f) out1IsZero = false;
    }
    TEST_ASSERT(out0IsZero, "Output 0 remains strictly 0.0 (no leak from Master)");
    TEST_ASSERT(out1IsZero, "Output 1 remains strictly 0.0 (no leak from Master)");

    // Verify outputs 2 and 3 received the master audio
    TEST_ASSERT(std::abs(out2[0]) > 0.1f, "Output 2 received Master Left signal");
    TEST_ASSERT(std::abs(out3[0]) > 0.1f, "Output 3 received Master Right signal");
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
    testMonoChannelProcessingAndPanning();
    testStereoChannelProcessingAndBalance();
    testMixerEngineSoloLogic();
    testRealtimeTelemetryAndSignalPath();
    testDeviceDisconnectHandling();
    testErrorDiagnostics();
    testExactErrorStringPreservation();
    testDiagnosticInfoRetrieval();
    testDriverDiscoveredChannelEnumeration();
    testExplicitRoutingAndZeroChannelBleed();
    testDisconnectedRouteGeneratesSilence();
    testIsolationTestA_SignalOnlyOnInput0();
    testIsolationTestB_SignalOnlyOnInput1();
    testIsolationTestC_SignalOnlyOnInput2();
    testIsolationTestD_SignalOnlyOnInput3();
    testIsolationTestE_NoInputSignals();
    testIsolationTestF_CustomRouting();
    testMasterOutputIsolation();

    std::cout << "\n========================================\n";
    std::cout << "TEST RESULTS: " << g_testsPassed << " passed, " << g_testsFailed << " failed.\n";
    std::cout << "========================================\n";

    return (g_testsFailed == 0) ? 0 : 1;
}
