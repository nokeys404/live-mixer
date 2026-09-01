#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/core/AudioEngine.h"
#include "../audio/mixer/MixerEngine.h"
#include <memory>
#include <array>

namespace livemixer::ui {

/**
 * @brief Custom Level Meter Component with Peak Decay and Clip LED
 */
class LevelMeterComponent : public juce::Component {
public:
    LevelMeterComponent();
    ~LevelMeterComponent() override = default;

    void setLevel(float peakLinear);
    void setStereoLevels(float leftLinear, float rightLinear);
    void setClipped(bool clipped);
    void resetClip();

    void paint(juce::Graphics& g) override;

    void setStereo(bool isStereo) { m_isStereo = isStereo; repaint(); }

private:
    bool m_isStereo{false};
    float m_levelL{0.0f};
    float m_levelR{0.0f};
    float m_displayLevelL{0.0f};
    float m_displayLevelR{0.0f};
    bool m_clipped{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeterComponent)
};

/**
 * @brief UI Channel Strip for Mono Channels (CH1, CH2)
 */
class MonoChannelStrip : public juce::Component, public juce::Slider::Listener, public juce::Button::Listener {
public:
    MonoChannelStrip(mixer::MixerChannel& channel, juce::String title, juce::String sourceName);
    ~MonoChannelStrip() override;

    void updateTelemetry();
    void resized() override;
    void paint(juce::Graphics& g) override;

    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;

private:
    mixer::MixerChannel& m_channel;

    juce::Label m_nameLabel;
    juce::Label m_sourceLabel;

    juce::Label m_gainTitle;
    juce::Slider m_gainSlider;
    juce::Label m_gainValue;

    LevelMeterComponent m_meter;

    juce::Label m_panTitle;
    juce::Slider m_panSlider;
    juce::Label m_panValue;

    juce::Slider m_faderSlider;
    juce::Label m_faderValue;

    juce::TextButton m_muteButton;
    juce::TextButton m_soloButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MonoChannelStrip)
};

/**
 * @brief UI Channel Strip for Stereo Media Channel (CH3/4)
 */
class StereoChannelStrip : public juce::Component, public juce::Slider::Listener, public juce::Button::Listener {
public:
    StereoChannelStrip(mixer::StereoMixerChannel& channel, juce::String title, juce::String sourceName);
    ~StereoChannelStrip() override;

    void updateTelemetry();
    void resized() override;
    void paint(juce::Graphics& g) override;

    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;

private:
    mixer::StereoMixerChannel& m_channel;

    juce::Label m_nameLabel;
    juce::Label m_sourceLabel;

    juce::Label m_gainTitle;
    juce::Slider m_gainSlider;
    juce::Label m_gainValue;

    LevelMeterComponent m_meter;

    juce::Label m_balanceTitle;
    juce::Slider m_balanceSlider;
    juce::Label m_balanceValue;

    juce::Slider m_faderSlider;
    juce::Label m_faderValue;

    juce::TextButton m_muteButton;
    juce::TextButton m_soloButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoChannelStrip)
};

/**
 * @brief UI Master Channel Strip
 */
class MasterStrip : public juce::Component, public juce::Slider::Listener, public juce::Button::Listener {
public:
    explicit MasterStrip(mixer::MixerEngine& engine);
    ~MasterStrip() override;

    void updateTelemetry();
    void resized() override;
    void paint(juce::Graphics& g) override;

    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;

private:
    mixer::MixerEngine& m_engine;

    juce::Label m_nameLabel;
    juce::Label m_sourceLabel;

    LevelMeterComponent m_meter;

    juce::Slider m_faderSlider;
    juce::Label m_faderValue;

    juce::TextButton m_muteButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterStrip)
};

/**
 * @brief 4-Channel Mixer Console Panel Component
 */
class MixerPanel : public juce::Component, public juce::Timer, public juce::Button::Listener {
public:
    explicit MixerPanel(std::shared_ptr<audio::AudioEngine> engine);
    ~MixerPanel() override;

    void timerCallback() override;
    void buttonClicked(juce::Button* button) override;
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    std::shared_ptr<audio::AudioEngine> m_audioEngine;
    std::shared_ptr<mixer::MixerEngine> m_mixerEngine;

    // Header Components
    juce::Label m_titleLabel;
    juce::Label m_statusBadge;
    juce::TextButton m_audioToggleButton;

    // 4 Channel Strips
    std::unique_ptr<MonoChannelStrip> m_ch1Strip;
    std::unique_ptr<MonoChannelStrip> m_ch2Strip;
    std::unique_ptr<StereoChannelStrip> m_ch34Strip;
    std::unique_ptr<MasterStrip> m_masterStrip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerPanel)
};

} // namespace livemixer::ui
