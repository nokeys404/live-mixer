#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/core/AudioEngine.h"
#include <memory>

namespace livemixer::ui {

/**
 * @brief JUCE-based Desktop Audio Settings Panel (V0.1 Milestone)
 * 
 * Provides full control over driver selection, device enumeration, sample rate,
 * buffer size, hardware channel reporting, audio start/stop, ASIO control panel,
 * and lock-free telemetry polling (buffer duration, input/output latency, processing time, XRuns, audio state).
 */
class AudioSettingsPanel : public juce::Component,
                           public juce::Timer,
                           private juce::ComboBox::Listener,
                           private juce::Button::Listener
{
public:
    explicit AudioSettingsPanel(std::shared_ptr<audio::AudioEngine> engine);
    ~AudioSettingsPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Timer callback for lock-free UI polling (30 Hz)
    void timerCallback() override;

private:
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    void buttonClicked(juce::Button* buttonThatWasClicked) override;

    void refreshDriverList();
    void refreshDeviceList();
    void refreshSampleRatesAndBuffers();
    void updateTelemetryUI();

    std::shared_ptr<audio::AudioEngine> m_engine;

    // Controls
    juce::Label m_titleLabel;
    
    juce::Label m_driverLabel;
    juce::ComboBox m_driverComboBox;

    juce::Label m_deviceLabel;
    juce::ComboBox m_deviceComboBox;

    juce::Label m_sampleRateLabel;
    juce::ComboBox m_sampleRateComboBox;

    juce::Label m_bufferSizeLabel;
    juce::ComboBox m_bufferSizeComboBox;

    juce::Label m_inputChannelsTitle;
    juce::Label m_inputChannelsValue;

    juce::Label m_outputChannelsTitle;
    juce::Label m_outputChannelsValue;

    juce::Label m_audioStatusTitle;
    juce::Label m_audioStatusValue;

    juce::Label m_diagTitle;
    juce::TextEditor m_diagTextEditor;

    juce::Label m_bufferDurationTitle;
    juce::Label m_bufferDurationValue;

    juce::Label m_inputLatencyTitle;
    juce::Label m_inputLatencyValue;

    juce::Label m_outputLatencyTitle;
    juce::Label m_outputLatencyValue;

    juce::Label m_processingTitle;
    juce::Label m_processingValue;

    juce::Label m_xrunsTitle;
    juce::Label m_xrunsValue;

    juce::TextButton m_startAudioButton;
    juce::TextButton m_stopAudioButton;
    juce::TextButton m_asioControlPanelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsPanel)
};

} // namespace livemixer::ui
