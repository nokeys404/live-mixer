#include "AudioSettingsPanel.h"
#include <iomanip>
#include <sstream>

namespace livemixer::ui {

AudioSettingsPanel::AudioSettingsPanel(std::shared_ptr<audio::AudioEngine> engine)
    : m_engine(std::move(engine))
{
    // Configure Title
    m_titleLabel.setText("LIVE MIXER", juce::dontSendNotification);
    m_titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Driver selection
    m_driverLabel.setText("Driver:", juce::dontSendNotification);
    addAndMakeVisible(m_driverLabel);
    addAndMakeVisible(m_driverComboBox);
    m_driverComboBox.addListener(this);

    // Device selection
    m_deviceLabel.setText("Device:", juce::dontSendNotification);
    addAndMakeVisible(m_deviceLabel);
    addAndMakeVisible(m_deviceComboBox);
    m_deviceComboBox.addListener(this);

    // Sample Rate selection
    m_sampleRateLabel.setText("Sample Rate:", juce::dontSendNotification);
    addAndMakeVisible(m_sampleRateLabel);
    addAndMakeVisible(m_sampleRateComboBox);
    m_sampleRateComboBox.addListener(this);

    // Buffer size selection
    m_bufferSizeLabel.setText("Buffer:", juce::dontSendNotification);
    addAndMakeVisible(m_bufferSizeLabel);
    addAndMakeVisible(m_bufferSizeComboBox);
    m_bufferSizeComboBox.addListener(this);

    // Input / Output Channels
    m_inputChannelsTitle.setText("Input Channels:", juce::dontSendNotification);
    addAndMakeVisible(m_inputChannelsTitle);
    m_inputChannelsValue.setText("0", juce::dontSendNotification);
    addAndMakeVisible(m_inputChannelsValue);

    m_outputChannelsTitle.setText("Output Channels:", juce::dontSendNotification);
    addAndMakeVisible(m_outputChannelsTitle);
    m_outputChannelsValue.setText("0", juce::dontSendNotification);
    addAndMakeVisible(m_outputChannelsValue);

    // Audio Status
    m_audioStatusTitle.setText("Audio Status:", juce::dontSendNotification);
    addAndMakeVisible(m_audioStatusTitle);
    m_audioStatusValue.setText(juce::CharPointer_UTF8("\xe2\x97\x8f READY"), juce::dontSendNotification);
    addAndMakeVisible(m_audioStatusValue);

    // Diagnostic Box - Always visible permanent section
    m_diagTitle.setText("Device Diagnostics:", juce::dontSendNotification);
    m_diagTitle.setFont(juce::Font(13.0f, juce::Font::bold));
    addAndMakeVisible(m_diagTitle);

    m_diagTextEditor.setMultiLine(true);
    m_diagTextEditor.setReadOnly(true);
    m_diagTextEditor.setCaretVisible(false);
    m_diagTextEditor.setScrollbarsShown(true);
    m_diagTextEditor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.5f, juce::Font::plain));
    m_diagTextEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff121214));
    m_diagTextEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff27272a));
    m_diagTextEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe4e4e7));
    addAndMakeVisible(m_diagTextEditor);

    // Buffer Duration vs Latency Telemetry
    m_bufferDurationTitle.setText("Buffer Duration:", juce::dontSendNotification);
    addAndMakeVisible(m_bufferDurationTitle);
    m_bufferDurationValue.setText("0.00 ms", juce::dontSendNotification);
    addAndMakeVisible(m_bufferDurationValue);

    m_inputLatencyTitle.setText("Input Latency:", juce::dontSendNotification);
    addAndMakeVisible(m_inputLatencyTitle);
    m_inputLatencyValue.setText("0.00 ms", juce::dontSendNotification);
    addAndMakeVisible(m_inputLatencyValue);

    m_outputLatencyTitle.setText("Output Latency:", juce::dontSendNotification);
    addAndMakeVisible(m_outputLatencyTitle);
    m_outputLatencyValue.setText("0.00 ms", juce::dontSendNotification);
    addAndMakeVisible(m_outputLatencyValue);

    m_processingTitle.setText("Processing:", juce::dontSendNotification);
    addAndMakeVisible(m_processingTitle);
    m_processingValue.setText("0.00 ms", juce::dontSendNotification);
    addAndMakeVisible(m_processingValue);

    m_xrunsTitle.setText("XRuns:", juce::dontSendNotification);
    addAndMakeVisible(m_xrunsTitle);
    m_xrunsValue.setText("0", juce::dontSendNotification);
    addAndMakeVisible(m_xrunsValue);

    // Buttons
    m_startAudioButton.setButtonText("START AUDIO");
    m_startAudioButton.addListener(this);
    addAndMakeVisible(m_startAudioButton);

    m_stopAudioButton.setButtonText("STOP AUDIO");
    m_stopAudioButton.addListener(this);
    addAndMakeVisible(m_stopAudioButton);

    m_asioControlPanelButton.setButtonText("Open ASIO Control Panel");
    m_asioControlPanelButton.addListener(this);
    addAndMakeVisible(m_asioControlPanelButton);

    refreshDriverList();
    refreshDeviceList();
    refreshSampleRatesAndBuffers();

    // Start UI poll timer at 30 FPS
    startTimerHz(30);
}

AudioSettingsPanel::~AudioSettingsPanel() {
    stopTimer();
    m_driverComboBox.removeListener(this);
    m_deviceComboBox.removeListener(this);
    m_sampleRateComboBox.removeListener(this);
    m_bufferSizeComboBox.removeListener(this);
    m_startAudioButton.removeListener(this);
    m_stopAudioButton.removeListener(this);
    m_asioControlPanelButton.removeListener(this);
}

void AudioSettingsPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff18181b)); // Dark charcoal background
    g.setColour(juce::Colour(0xff27272a));
    g.drawRect(getLocalBounds(), 1);
}

void AudioSettingsPanel::resized() {
    auto bounds = getLocalBounds().reduced(16);
    
    // Title at the top
    m_titleLabel.setBounds(bounds.removeFromTop(32));
    bounds.removeFromTop(10);

    if (bounds.getWidth() >= 520) {
        // Two-column responsive desktop layout
        const int colGap = 20;
        const int totalWidth = bounds.getWidth();
        const int leftColWidth = (totalWidth - colGap) / 2;
        
        auto leftCol = bounds.removeFromLeft(leftColWidth);
        bounds.removeFromLeft(colGap);
        auto rightCol = bounds;

        const int itemHeight = 28;
        const int rowSpacing = 6;

        auto layoutLeftRow = [&](juce::Label& label, juce::Component& control, int labelWidth = 100) {
            auto row = leftCol.removeFromTop(itemHeight);
            label.setBounds(row.removeFromLeft(labelWidth));
            control.setBounds(row);
            leftCol.removeFromTop(rowSpacing);
        };

        // Left Column: Audio Device Configuration
        layoutLeftRow(m_driverLabel, m_driverComboBox, 100);
        layoutLeftRow(m_deviceLabel, m_deviceComboBox, 100);
        layoutLeftRow(m_sampleRateLabel, m_sampleRateComboBox, 100);
        layoutLeftRow(m_bufferSizeLabel, m_bufferSizeComboBox, 100);

        leftCol.removeFromTop(6);

        // Start / Stop Buttons side-by-side
        auto buttonRow = leftCol.removeFromTop(34);
        const int btnGap = 8;
        const int btnWidth = (buttonRow.getWidth() - btnGap) / 2;
        m_startAudioButton.setBounds(buttonRow.removeFromLeft(btnWidth));
        buttonRow.removeFromLeft(btnGap);
        m_stopAudioButton.setBounds(buttonRow);
        leftCol.removeFromTop(rowSpacing);

        // ASIO Control Panel Button
        m_asioControlPanelButton.setBounds(leftCol.removeFromTop(30));
        leftCol.removeFromTop(10);

        // Telemetry Metrics in Left Column
        const int metricHeight = 22;
        const int metricSpacing = 4;
        auto layoutMetric = [&](juce::Label& title, juce::Label& value) {
            auto row = leftCol.removeFromTop(metricHeight);
            title.setBounds(row.removeFromLeft(120));
            value.setBounds(row);
            leftCol.removeFromTop(metricSpacing);
        };

        layoutMetric(m_bufferDurationTitle, m_bufferDurationValue);
        layoutMetric(m_inputLatencyTitle, m_inputLatencyValue);
        layoutMetric(m_outputLatencyTitle, m_outputLatencyValue);
        layoutMetric(m_processingTitle, m_processingValue);
        layoutMetric(m_xrunsTitle, m_xrunsValue);

        // Right Column: Audio Status, Channel Topologies & Full Height Diagnostics
        auto layoutRightRow = [&](juce::Label& label, juce::Component& control, int labelWidth = 120) {
            auto row = rightCol.removeFromTop(itemHeight);
            label.setBounds(row.removeFromLeft(labelWidth));
            control.setBounds(row);
            rightCol.removeFromTop(rowSpacing);
        };

        layoutRightRow(m_audioStatusTitle, m_audioStatusValue, 120);
        layoutRightRow(m_inputChannelsTitle, m_inputChannelsValue, 120);
        layoutRightRow(m_outputChannelsTitle, m_outputChannelsValue, 120);

        rightCol.removeFromTop(6);
        m_diagTitle.setBounds(rightCol.removeFromTop(20));
        rightCol.removeFromTop(4);
        m_diagTextEditor.setBounds(rightCol);
    } else {
        // Single column fallback for narrow sizes
        const int itemHeight = 26;
        const int spacing = 4;

        auto layoutRow = [&](juce::Label& label, juce::Component& control) {
            auto row = bounds.removeFromTop(itemHeight);
            label.setBounds(row.removeFromLeft(120));
            control.setBounds(row);
            bounds.removeFromTop(spacing);
        };

        layoutRow(m_driverLabel, m_driverComboBox);
        layoutRow(m_deviceLabel, m_deviceComboBox);
        layoutRow(m_sampleRateLabel, m_sampleRateComboBox);
        layoutRow(m_bufferSizeLabel, m_bufferSizeComboBox);

        layoutRow(m_inputChannelsTitle, m_inputChannelsValue);
        layoutRow(m_outputChannelsTitle, m_outputChannelsValue);
        layoutRow(m_audioStatusTitle, m_audioStatusValue);

        auto buttonRow = bounds.removeFromTop(32);
        const int btnWidth = (buttonRow.getWidth() - 8) / 2;
        m_startAudioButton.setBounds(buttonRow.removeFromLeft(btnWidth));
        buttonRow.removeFromLeft(8);
        m_stopAudioButton.setBounds(buttonRow);
        bounds.removeFromTop(spacing);

        m_asioControlPanelButton.setBounds(bounds.removeFromTop(28));
        bounds.removeFromTop(spacing);

        m_diagTitle.setBounds(bounds.removeFromTop(18));
        bounds.removeFromTop(2);
        m_diagTextEditor.setBounds(bounds.removeFromTop(120));
        bounds.removeFromTop(spacing);

        layoutRow(m_bufferDurationTitle, m_bufferDurationValue);
        layoutRow(m_inputLatencyTitle, m_inputLatencyValue);
        layoutRow(m_outputLatencyTitle, m_outputLatencyValue);
        layoutRow(m_processingTitle, m_processingValue);
        layoutRow(m_xrunsTitle, m_xrunsValue);
    }
}

void AudioSettingsPanel::refreshDriverList() {
    m_driverComboBox.clear(juce::dontSendNotification);
    m_driverComboBox.addItem("ASIO", 1);
    m_driverComboBox.addItem("WASAPI", 2);

    if (m_engine) {
        const auto driver = m_engine->getCurrentConfig().driverType;
        m_driverComboBox.setSelectedId(driver == audio::DriverType::ASIO ? 1 : 2, juce::dontSendNotification);
    }
}

void AudioSettingsPanel::refreshDeviceList() {
    m_deviceComboBox.clear(juce::dontSendNotification);
    if (!m_engine || !m_engine->getDeviceManager()) return;

    auto devMgr = m_engine->getDeviceManager();
    const auto currentDriver = devMgr->getCurrentDriverType();
    auto devices = devMgr->getDevicesForDriver(currentDriver);

    if (devices.empty()) {
        const juce::String emptyMsg = (currentDriver == audio::DriverType::ASIO)
            ? "No ASIO devices available"
            : "No WASAPI devices available";
        m_deviceComboBox.setTextWhenNoChoicesAvailable(emptyMsg);
        m_deviceComboBox.setTextWhenNothingSelected(emptyMsg);
        m_deviceComboBox.setEnabled(false);
        return;
    }

    m_deviceComboBox.setEnabled(true);
    int id = 1;
    int selectedId = 1;
    const std::string currentDevice = devMgr->getCurrentDeviceName();

    for (const auto& dev : devices) {
        m_deviceComboBox.addItem(dev.name, id);
        if (dev.name == currentDevice) {
            selectedId = id;
        }
        id++;
    }
    m_deviceComboBox.setSelectedId(selectedId, juce::dontSendNotification);
}

void AudioSettingsPanel::refreshSampleRatesAndBuffers() {
    m_sampleRateComboBox.clear(juce::dontSendNotification);
    m_bufferSizeComboBox.clear(juce::dontSendNotification);

    if (!m_engine || !m_engine->getDeviceManager()) return;
    auto devMgr = m_engine->getDeviceManager();

    auto sampleRates = devMgr->getSupportedSampleRates();
    if (sampleRates.empty()) {
        sampleRates = { 44100.0, 48000.0, 96000.0 };
    }
    int sId = 1;
    const double currentRate = m_engine->getCurrentConfig().sampleRate;
    int selectedSId = 1;
    for (double rate : sampleRates) {
        std::string label = std::to_string(static_cast<int>(rate)) + " Hz";
        m_sampleRateComboBox.addItem(label, sId);
        if (std::abs(rate - currentRate) < 1.0) {
            selectedSId = sId;
        }
        sId++;
    }
    m_sampleRateComboBox.setSelectedId(selectedSId, juce::dontSendNotification);

    auto bufferSizes = devMgr->getSupportedBufferSizes();
    if (bufferSizes.empty()) {
        bufferSizes = { 64, 128, 256, 512 };
    }
    int bId = 1;
    const uint32_t currentBuffer = m_engine->getCurrentConfig().bufferSize;
    int selectedBId = 1;
    for (uint32_t buf : bufferSizes) {
        std::string label = std::to_string(buf) + " samples";
        m_bufferSizeComboBox.addItem(label, bId);
        if (buf == currentBuffer) {
            selectedBId = bId;
        }
        bId++;
    }
    m_bufferSizeComboBox.setSelectedId(selectedBId, juce::dontSendNotification);
}

void AudioSettingsPanel::timerCallback() {
    updateTelemetryUI();
}

void AudioSettingsPanel::updateTelemetryUI() {
    if (!m_engine) return;

    const auto metrics = m_engine->getMetrics();
    const auto state = metrics.audioState;
    const auto diag = m_engine->getDiagnosticInfo();
    const auto config = m_engine->getCurrentConfig();

    // Format Audio Status
    juce::String statusText = juce::String(juce::CharPointer_UTF8("\xe2\x97\x8f ")) + juce::String(audio::audioStateToString(state)).toUpperCase();
    m_audioStatusValue.setText(statusText, juce::dontSendNotification);

    if (state == audio::AudioState::Running) {
        m_audioStatusValue.setColour(juce::Label::textColourId, juce::Colour(0xff22c55e)); // Green
    } else if (state == audio::AudioState::Error) {
        m_audioStatusValue.setColour(juce::Label::textColourId, juce::Colour(0xffef4444)); // Red
    } else if (state == audio::AudioState::Ready) {
        m_audioStatusValue.setColour(juce::Label::textColourId, juce::Colour(0xff3b82f6)); // Blue
    } else {
        m_audioStatusValue.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa)); // Gray
    }

    // Capture & Format Full Diagnostic String
    const std::string err = m_engine->getLastError();
    const bool isErrorState = (state == audio::AudioState::Error) || !err.empty();

    if (isErrorState) {
        m_diagTitle.setColour(juce::Label::textColourId, juce::Colour(0xffef4444));
        m_diagTextEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xffef4444));

        // When device has not opened successfully, display "--" instead of pretending 0 is actual topology
        m_inputChannelsValue.setText("--", juce::dontSendNotification);
        m_outputChannelsValue.setText("--", juce::dontSendNotification);
        m_inputLatencyValue.setText("--", juce::dontSendNotification);
        m_outputLatencyValue.setText("--", juce::dontSendNotification);
        m_processingValue.setText("--", juce::dontSendNotification);
    } else {
        m_diagTitle.setColour(juce::Label::textColourId, juce::Colour(0xff71717a));
        m_diagTextEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff27272a));

        auto devMgr = m_engine->getDeviceManager();
        if (devMgr && devMgr->isAudioRunning()) {
            m_inputChannelsValue.setText(std::to_string(devMgr->getInputChannelCount()), juce::dontSendNotification);
            m_outputChannelsValue.setText(std::to_string(devMgr->getOutputChannelCount()), juce::dontSendNotification);
        } else if (diag.isDeviceOpen) {
            m_inputChannelsValue.setText(std::to_string(diag.activeInputChannels), juce::dontSendNotification);
            m_outputChannelsValue.setText(std::to_string(diag.activeOutputChannels), juce::dontSendNotification);
        } else {
            m_inputChannelsValue.setText("--", juce::dontSendNotification);
            m_outputChannelsValue.setText("--", juce::dontSendNotification);
        }

        std::ostringstream inLatStream, outLatStream, procStream;
        inLatStream << std::fixed << std::setprecision(2) << metrics.inputLatencyMs << " ms";
        outLatStream << std::fixed << std::setprecision(2) << metrics.outputLatencyMs << " ms";
        procStream << std::fixed << std::setprecision(2) << metrics.processingTimeMs << " ms";

        m_inputLatencyValue.setText(inLatStream.str(), juce::dontSendNotification);
        m_outputLatencyValue.setText(outLatStream.str(), juce::dontSendNotification);
        m_processingValue.setText(procStream.str(), juce::dontSendNotification);
    }

    // Build Formatted Diagnostic Information
    juce::String diagText;
    diagText << "Device:\n" << (diag.deviceName.empty() ? (config.deviceName.empty() ? "None" : juce::String(config.deviceName)) : juce::String(diag.deviceName)) << "\n\n";

    diagText << "Initialization result:\n";
    if (!diag.setAudioDeviceSetupResult.empty() && diag.setAudioDeviceSetupResult != "OK") {
        diagText << diag.setAudioDeviceSetupResult << "\n\n";
    } else if (!diag.initialiseResult.empty() && diag.initialiseResult != "OK") {
        diagText << diag.initialiseResult << "\n\n";
    } else if (isErrorState) {
        diagText << (err.empty() ? "Device initialization failed" : juce::String(err)) << "\n\n";
    } else {
        diagText << "OK (Initialized successfully)\n\n";
    }

    diagText << "Device pointer:\n" << (diag.hasDevicePointer ? "VALID" : "NULL") << "\n\n";
    diagText << "Device open:\n" << (diag.isDeviceOpen ? "YES" : "NO") << "\n\n";
    diagText << "JUCE last error:\n" << (diag.deviceLastError.empty() ? "None" : juce::String(diag.deviceLastError)) << "\n\n";

    diagText << "Requested:\n" << juce::String(static_cast<int>(config.sampleRate)) << " Hz / " << juce::String(config.bufferSize) << " samples\n\n";

    diagText << "Actual:\n";
    if (diag.isDeviceOpen && diag.actualSampleRate > 0.0) {
        diagText << juce::String(static_cast<int>(diag.actualSampleRate)) << " Hz / " << juce::String(diag.actualBufferSize) << " samples\n\n";
    } else {
        diagText << "--\n\n";
    }

    diagText << "Input channels:\n";
    if (diag.isDeviceOpen) {
        diagText << juce::String(diag.activeInputChannels) << "\n\n";
    } else {
        diagText << "--\n\n";
    }

    diagText << "Output channels:\n";
    if (diag.isDeviceOpen) {
        diagText << juce::String(diag.activeOutputChannels);
    } else {
        diagText << "--";
    }

    if (!diag.secondaryDiagnostic.empty()) {
        diagText << "\n\nNote:\n" << diag.secondaryDiagnostic;
    }

    if (m_diagTextEditor.getText() != diagText) {
        m_diagTextEditor.setText(diagText, false);
    }

    // Format Metrics: Buffer Duration
    const double bufferDurationMs = m_engine->getCurrentConfig().getBufferDurationMs();
    std::ostringstream bufDurStream;
    bufDurStream << std::fixed << std::setprecision(2) << bufferDurationMs << " ms";
    m_bufferDurationValue.setText(bufDurStream.str(), juce::dontSendNotification);
    m_xrunsValue.setText(std::to_string(metrics.xrunCount), juce::dontSendNotification);

    // Format ASIO Control Panel state - Enabled for ASIO drivers even on failure
    auto devMgr = m_engine->getDeviceManager();
    m_asioControlPanelButton.setEnabled(devMgr ? devMgr->hasControlPanel() : false);

    // Update button states
    m_startAudioButton.setEnabled(state == audio::AudioState::Ready || state == audio::AudioState::Stopping);
    m_stopAudioButton.setEnabled(state == audio::AudioState::Running);
}

void AudioSettingsPanel::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) {
    if (!m_engine) return;

    if (comboBoxThatHasChanged == &m_driverComboBox) {
        const int id = m_driverComboBox.getSelectedId();
        const auto driver = (id == 1) ? audio::DriverType::ASIO : audio::DriverType::WASAPI;
        m_engine->setDriver(driver);
        refreshDeviceList();
        refreshSampleRatesAndBuffers();
    } else if (comboBoxThatHasChanged == &m_deviceComboBox) {
        const auto devName = m_deviceComboBox.getText().toStdString();
        m_engine->setDevice(devName);
        refreshSampleRatesAndBuffers();
    } else if (comboBoxThatHasChanged == &m_sampleRateComboBox) {
        auto devMgr = m_engine->getDeviceManager();
        if (devMgr) {
            auto rates = devMgr->getSupportedSampleRates();
            const int idx = m_sampleRateComboBox.getSelectedId() - 1;
            if (idx >= 0 && idx < static_cast<int>(rates.size())) {
                m_engine->setSampleRate(rates[idx]);
            }
        }
    } else if (comboBoxThatHasChanged == &m_bufferSizeComboBox) {
        auto devMgr = m_engine->getDeviceManager();
        if (devMgr) {
            auto buffers = devMgr->getSupportedBufferSizes();
            const int idx = m_bufferSizeComboBox.getSelectedId() - 1;
            if (idx >= 0 && idx < static_cast<int>(buffers.size())) {
                m_engine->setBufferSize(buffers[idx]);
            }
        }
    }
}

void AudioSettingsPanel::buttonClicked(juce::Button* buttonThatWasClicked) {
    if (!m_engine) return;

    if (buttonThatWasClicked == &m_startAudioButton) {
        m_engine->start();
    } else if (buttonThatWasClicked == &m_stopAudioButton) {
        m_engine->stop();
    } else if (buttonThatWasClicked == &m_asioControlPanelButton) {
        if (m_engine->getDeviceManager()) {
            m_engine->getDeviceManager()->openControlPanel();
        }
    }
}

} // namespace livemixer::ui
