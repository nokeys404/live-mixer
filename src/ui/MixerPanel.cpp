#include "MixerPanel.h"
#include <iomanip>
#include <sstream>

namespace livemixer::ui {

// =============================================================================
// Level Meter Component Implementation
// =============================================================================
LevelMeterComponent::LevelMeterComponent() {
    setOpaque(false);
}

void LevelMeterComponent::setLevel(float peakLinear) {
    m_levelL = peakLinear;
    // Ballistic peak decay
    if (m_levelL > m_displayLevelL) {
        m_displayLevelL = m_levelL;
    } else {
        m_displayLevelL = std::max(0.0f, m_displayLevelL * 0.88f - 0.005f);
    }
    repaint();
}

void LevelMeterComponent::setStereoLevels(float leftLinear, float rightLinear) {
    m_levelL = leftLinear;
    m_levelR = rightLinear;

    if (m_levelL > m_displayLevelL) {
        m_displayLevelL = m_levelL;
    } else {
        m_displayLevelL = std::max(0.0f, m_displayLevelL * 0.88f - 0.005f);
    }

    if (m_levelR > m_displayLevelR) {
        m_displayLevelR = m_levelR;
    } else {
        m_displayLevelR = std::max(0.0f, m_displayLevelR * 0.88f - 0.005f);
    }
    repaint();
}

void LevelMeterComponent::setClipped(bool clipped) {
    if (clipped != m_clipped) {
        m_clipped = clipped;
        repaint();
    }
}

void LevelMeterComponent::resetClip() {
    m_clipped = false;
    repaint();
}

void LevelMeterComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();

    // Background trough
    g.setColour(juce::Colour(0xff09090b));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

    const float clipLedHeight = 8.0f;
    auto meterArea = bounds.reduced(2.0f);
    auto clipArea = meterArea.removeFromTop(clipLedHeight);
    meterArea.removeFromTop(2.0f); // gap

    // Paint Clip Indicator LED
    if (m_clipped) {
        g.setColour(juce::Colour(0xffef4444)); // Bright red
    } else {
        g.setColour(juce::Colour(0xff3f3f46)); // Dim dark red / gray
    }
    g.fillRoundedRectangle(clipArea, 2.0f);

    auto drawMeterBar = [&](juce::Rectangle<float> barArea, float level) {
        // Trough
        g.setColour(juce::Colour(0xff18181b));
        g.fillRoundedRectangle(barArea, 2.0f);

        if (level <= 0.0001f) return;

        // Convert linear peak to normalized bar height (approximate 0 dB to -60 dB scale)
        float normalized = std::clamp(level, 0.0f, 1.5f);
        if (normalized > 1.0f) normalized = 1.0f;

        const float filledHeight = barArea.getHeight() * normalized;
        auto filledArea = barArea.removeFromBottom(filledHeight);

        // Color gradient: Green -> Yellow -> Red
        juce::ColourGradient grad(juce::Colour(0xff10b981), filledArea.getX(), filledArea.getBottom(),
                                  juce::Colour(0xfff59e0b), filledArea.getX(), filledArea.getY() + filledArea.getHeight() * 0.3f, false);
        grad.addColour(0.85, juce::Colour(0xffef4444));

        g.setGradientFill(grad);
        g.fillRoundedRectangle(filledArea, 2.0f);
    };

    if (m_isStereo) {
        const float halfWidth = (meterArea.getWidth() - 2.0f) * 0.5f;
        auto leftBar = meterArea.removeFromLeft(halfWidth);
        meterArea.removeFromLeft(2.0f);
        auto rightBar = meterArea;

        drawMeterBar(leftBar, m_displayLevelL);
        drawMeterBar(rightBar, m_displayLevelR);
    } else {
        drawMeterBar(meterArea, m_displayLevelL);
    }
}

// =============================================================================
// Mono Channel Strip Implementation (CH1, CH2)
// =============================================================================
MonoChannelStrip::MonoChannelStrip(mixer::MixerChannel& channel, juce::String title, juce::String sourceName)
    : m_channel(channel)
{
    // Name & Source Header
    m_nameLabel.setText(title, juce::dontSendNotification);
    m_nameLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    m_nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff4f4f5));
    m_nameLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_nameLabel);

    m_sourceLabel.setText(sourceName, juce::dontSendNotification);
    m_sourceLabel.setFont(juce::Font(10.0f, juce::Font::plain));
    m_sourceLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));
    m_sourceLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_sourceLabel);

    // Gain Control
    m_gainTitle.setText("GAIN", juce::dontSendNotification);
    m_gainTitle.setFont(juce::Font(9.0f, juce::Font::bold));
    m_gainTitle.setColour(juce::Label::textColourId, juce::Colour(0xff71717a));
    m_gainTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_gainTitle);

    m_gainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    m_gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_gainSlider.setRange(-24.0, 24.0, 0.5);
    m_gainSlider.setValue(m_channel.getGainDb(), juce::dontSendNotification);
    m_gainSlider.addListener(this);
    m_gainSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff10b981));
    m_gainSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe4e4e7));
    addAndMakeVisible(m_gainSlider);

    m_gainValue.setText(juce::String(m_channel.getGainDb(), 1) + " dB", juce::dontSendNotification);
    m_gainValue.setFont(juce::Font(10.0f, juce::Font::plain));
    m_gainValue.setColour(juce::Label::textColourId, juce::Colour(0xffd4d4d8));
    m_gainValue.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_gainValue);

    // Level Meter
    m_meter.setStereo(false);
    addAndMakeVisible(m_meter);

    // Pan Control
    m_panTitle.setText("PAN", juce::dontSendNotification);
    m_panTitle.setFont(juce::Font(9.0f, juce::Font::bold));
    m_panTitle.setColour(juce::Label::textColourId, juce::Colour(0xff71717a));
    m_panTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_panTitle);

    m_panSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    m_panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_panSlider.setRange(-1.0, 1.0, 0.05);
    m_panSlider.setValue(m_channel.getPan(), juce::dontSendNotification);
    m_panSlider.addListener(this);
    m_panSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff3b82f6));
    m_panSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe4e4e7));
    addAndMakeVisible(m_panSlider);

    m_panValue.setText("C", juce::dontSendNotification);
    m_panValue.setFont(juce::Font(10.0f, juce::Font::plain));
    m_panValue.setColour(juce::Label::textColourId, juce::Colour(0xffd4d4d8));
    m_panValue.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_panValue);

    // Fader
    m_faderSlider.setSliderStyle(juce::Slider::LinearVertical);
    m_faderSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_faderSlider.setRange(-60.0, 10.0, 0.1);
    m_faderSlider.setSkewFactorFromMidPoint(0.0);
    m_faderSlider.setValue(m_channel.getFaderDb(), juce::dontSendNotification);
    m_faderSlider.addListener(this);
    m_faderSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff10b981));
    m_faderSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xfffafafa));
    addAndMakeVisible(m_faderSlider);

    m_faderValue.setText("0.0 dB", juce::dontSendNotification);
    m_faderValue.setFont(juce::Font(11.0f, juce::Font::bold));
    m_faderValue.setColour(juce::Label::textColourId, juce::Colour(0xffe4e4e7));
    m_faderValue.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_faderValue);

    // Mute Button
    m_muteButton.setButtonText("MUTE");
    m_muteButton.setClickingTogglesState(true);
    m_muteButton.setToggleState(m_channel.isMuted(), juce::dontSendNotification);
    m_muteButton.addListener(this);
    m_muteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    m_muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffef4444));
    m_muteButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffffff));
    addAndMakeVisible(m_muteButton);

    // Solo Button
    m_soloButton.setButtonText("SOLO");
    m_soloButton.setClickingTogglesState(true);
    m_soloButton.setToggleState(m_channel.isSolo(), juce::dontSendNotification);
    m_soloButton.addListener(this);
    m_soloButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    m_soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff59e0b));
    m_soloButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff09090b));
    addAndMakeVisible(m_soloButton);
}

MonoChannelStrip::~MonoChannelStrip() {
    m_gainSlider.removeListener(this);
    m_panSlider.removeListener(this);
    m_faderSlider.removeListener(this);
    m_muteButton.removeListener(this);
    m_soloButton.removeListener(this);
}

void MonoChannelStrip::updateTelemetry() {
    m_meter.setLevel(m_channel.getPeakLevel());
    m_meter.setClipped(m_channel.hasClipped());

    if (m_muteButton.getToggleState() != m_channel.isMuted()) {
        m_muteButton.setToggleState(m_channel.isMuted(), juce::dontSendNotification);
    }
    if (m_soloButton.getToggleState() != m_channel.isSolo()) {
        m_soloButton.setToggleState(m_channel.isSolo(), juce::dontSendNotification);
    }
}

void MonoChannelStrip::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    // Channel Card Frame
    g.setColour(juce::Colour(0xff18181b));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    // Subtle header separator
    g.setColour(juce::Colour(0xff3f3f46));
    g.fillRect(bounds.getX() + 8.0f, bounds.getY() + 44.0f, bounds.getWidth() - 16.0f, 1.0f);
}

void MonoChannelStrip::resized() {
    auto bounds = getLocalBounds().reduced(6);

    m_nameLabel.setBounds(bounds.removeFromTop(20));
    m_sourceLabel.setBounds(bounds.removeFromTop(16));
    bounds.removeFromTop(10); // separator gap

    // Gain Section
    m_gainTitle.setBounds(bounds.removeFromTop(12));
    m_gainSlider.setBounds(bounds.removeFromTop(36));
    m_gainValue.setBounds(bounds.removeFromTop(16));
    bounds.removeFromTop(6);

    // Pan Section
    m_panTitle.setBounds(bounds.removeFromTop(12));
    m_panSlider.setBounds(bounds.removeFromTop(36));
    m_panValue.setBounds(bounds.removeFromTop(16));
    bounds.removeFromTop(6);

    // Buttons at bottom
    auto soloRow = bounds.removeFromBottom(24);
    m_soloButton.setBounds(soloRow);
    bounds.removeFromBottom(4);

    auto muteRow = bounds.removeFromBottom(24);
    m_muteButton.setBounds(muteRow);
    bounds.removeFromBottom(6);

    m_faderValue.setBounds(bounds.removeFromBottom(18));
    bounds.removeFromBottom(4);

    // Middle area: Meter on left, Fader on right
    auto midArea = bounds;
    const int meterWidth = 14;
    m_meter.setBounds(midArea.removeFromLeft(meterWidth));
    midArea.removeFromLeft(4);
    m_faderSlider.setBounds(midArea);
}

void MonoChannelStrip::sliderValueChanged(juce::Slider* slider) {
    if (slider == &m_gainSlider) {
        const float val = static_cast<float>(slider->getValue());
        m_channel.setGainDb(val);
        juce::String prefix = (val > 0.0f) ? "+" : "";
        m_gainValue.setText(prefix + juce::String(val, 1) + " dB", juce::dontSendNotification);
    } else if (slider == &m_panSlider) {
        const float val = static_cast<float>(slider->getValue());
        m_channel.setPan(val);
        if (std::abs(val) < 0.05f) {
            m_panValue.setText("C", juce::dontSendNotification);
        } else if (val < 0.0f) {
            m_panValue.setText("L" + juce::String(static_cast<int>(std::abs(val) * 100.0f)), juce::dontSendNotification);
        } else {
            m_panValue.setText("R" + juce::String(static_cast<int>(val * 100.0f)), juce::dontSendNotification);
        }
    } else if (slider == &m_faderSlider) {
        const float val = static_cast<float>(slider->getValue());
        m_channel.setFaderDb(val);
        if (val <= -60.0f) {
            m_faderValue.setText("-INF dB", juce::dontSendNotification);
        } else {
            juce::String prefix = (val > 0.0f) ? "+" : "";
            m_faderValue.setText(prefix + juce::String(val, 1) + " dB", juce::dontSendNotification);
        }
    }
}

void MonoChannelStrip::buttonClicked(juce::Button* button) {
    if (button == &m_muteButton) {
        m_channel.setMute(button->getToggleState());
    } else if (button == &m_soloButton) {
        m_channel.setSolo(button->getToggleState());
    }
}

// =============================================================================
// Stereo Channel Strip Implementation (CH3/4 Media)
// =============================================================================
StereoChannelStrip::StereoChannelStrip(mixer::StereoMixerChannel& channel, juce::String title, juce::String sourceName)
    : m_channel(channel)
{
    // Name & Source Header
    m_nameLabel.setText(title, juce::dontSendNotification);
    m_nameLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    m_nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff4f4f5));
    m_nameLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_nameLabel);

    m_sourceLabel.setText(sourceName, juce::dontSendNotification);
    m_sourceLabel.setFont(juce::Font(10.0f, juce::Font::plain));
    m_sourceLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));
    m_sourceLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_sourceLabel);

    // Gain Control
    m_gainTitle.setText("GAIN", juce::dontSendNotification);
    m_gainTitle.setFont(juce::Font(9.0f, juce::Font::bold));
    m_gainTitle.setColour(juce::Label::textColourId, juce::Colour(0xff71717a));
    m_gainTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_gainTitle);

    m_gainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    m_gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_gainSlider.setRange(-24.0, 24.0, 0.5);
    m_gainSlider.setValue(m_channel.getGainDb(), juce::dontSendNotification);
    m_gainSlider.addListener(this);
    m_gainSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff10b981));
    m_gainSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe4e4e7));
    addAndMakeVisible(m_gainSlider);

    m_gainValue.setText(juce::String(m_channel.getGainDb(), 1) + " dB", juce::dontSendNotification);
    m_gainValue.setFont(juce::Font(10.0f, juce::Font::plain));
    m_gainValue.setColour(juce::Label::textColourId, juce::Colour(0xffd4d4d8));
    m_gainValue.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_gainValue);

    // Dual Stereo Meter
    m_meter.setStereo(true);
    addAndMakeVisible(m_meter);

    // Balance Control
    m_balanceTitle.setText("BAL", juce::dontSendNotification);
    m_balanceTitle.setFont(juce::Font(9.0f, juce::Font::bold));
    m_balanceTitle.setColour(juce::Label::textColourId, juce::Colour(0xff71717a));
    m_balanceTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_balanceTitle);

    m_balanceSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    m_balanceSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_balanceSlider.setRange(-1.0, 1.0, 0.05);
    m_balanceSlider.setValue(m_channel.getBalance(), juce::dontSendNotification);
    m_balanceSlider.addListener(this);
    m_balanceSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff8b5cf6)); // Violet
    m_balanceSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe4e4e7));
    addAndMakeVisible(m_balanceSlider);

    m_balanceValue.setText("BAL C", juce::dontSendNotification);
    m_balanceValue.setFont(juce::Font(10.0f, juce::Font::plain));
    m_balanceValue.setColour(juce::Label::textColourId, juce::Colour(0xffd4d4d8));
    m_balanceValue.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_balanceValue);

    // Fader
    m_faderSlider.setSliderStyle(juce::Slider::LinearVertical);
    m_faderSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_faderSlider.setRange(-60.0, 10.0, 0.1);
    m_faderSlider.setSkewFactorFromMidPoint(0.0);
    m_faderSlider.setValue(m_channel.getFaderDb(), juce::dontSendNotification);
    m_faderSlider.addListener(this);
    m_faderSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff8b5cf6));
    m_faderSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xfffafafa));
    addAndMakeVisible(m_faderSlider);

    m_faderValue.setText("0.0 dB", juce::dontSendNotification);
    m_faderValue.setFont(juce::Font(11.0f, juce::Font::bold));
    m_faderValue.setColour(juce::Label::textColourId, juce::Colour(0xffe4e4e7));
    m_faderValue.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_faderValue);

    // Mute Button
    m_muteButton.setButtonText("MUTE");
    m_muteButton.setClickingTogglesState(true);
    m_muteButton.setToggleState(m_channel.isMuted(), juce::dontSendNotification);
    m_muteButton.addListener(this);
    m_muteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    m_muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffef4444));
    m_muteButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffffff));
    addAndMakeVisible(m_muteButton);

    // Solo Button
    m_soloButton.setButtonText("SOLO");
    m_soloButton.setClickingTogglesState(true);
    m_soloButton.setToggleState(m_channel.isSolo(), juce::dontSendNotification);
    m_soloButton.addListener(this);
    m_soloButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    m_soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff59e0b));
    m_soloButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff09090b));
    addAndMakeVisible(m_soloButton);
}

StereoChannelStrip::~StereoChannelStrip() {
    m_gainSlider.removeListener(this);
    m_balanceSlider.removeListener(this);
    m_faderSlider.removeListener(this);
    m_muteButton.removeListener(this);
    m_soloButton.removeListener(this);
}

void StereoChannelStrip::updateTelemetry() {
    m_meter.setStereoLevels(m_channel.getPeakLevelL(), m_channel.getPeakLevelR());
    m_meter.setClipped(m_channel.hasClipped());

    if (m_muteButton.getToggleState() != m_channel.isMuted()) {
        m_muteButton.setToggleState(m_channel.isMuted(), juce::dontSendNotification);
    }
    if (m_soloButton.getToggleState() != m_channel.isSolo()) {
        m_soloButton.setToggleState(m_channel.isSolo(), juce::dontSendNotification);
    }
}

void StereoChannelStrip::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff18181b));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    g.setColour(juce::Colour(0xff3f3f46));
    g.fillRect(bounds.getX() + 8.0f, bounds.getY() + 44.0f, bounds.getWidth() - 16.0f, 1.0f);
}

void StereoChannelStrip::resized() {
    auto bounds = getLocalBounds().reduced(6);

    m_nameLabel.setBounds(bounds.removeFromTop(20));
    m_sourceLabel.setBounds(bounds.removeFromTop(16));
    bounds.removeFromTop(10);

    // Gain Section
    m_gainTitle.setBounds(bounds.removeFromTop(12));
    m_gainSlider.setBounds(bounds.removeFromTop(36));
    m_gainValue.setBounds(bounds.removeFromTop(16));
    bounds.removeFromTop(6);

    // Balance Section
    m_balanceTitle.setBounds(bounds.removeFromTop(12));
    m_balanceSlider.setBounds(bounds.removeFromTop(36));
    m_balanceValue.setBounds(bounds.removeFromTop(16));
    bounds.removeFromTop(6);

    // Buttons at bottom
    auto soloRow = bounds.removeFromBottom(24);
    m_soloButton.setBounds(soloRow);
    bounds.removeFromBottom(4);

    auto muteRow = bounds.removeFromBottom(24);
    m_muteButton.setBounds(muteRow);
    bounds.removeFromBottom(6);

    m_faderValue.setBounds(bounds.removeFromBottom(18));
    bounds.removeFromBottom(4);

    // Middle area: Dual Meter on left, Fader on right
    auto midArea = bounds;
    const int meterWidth = 24;
    m_meter.setBounds(midArea.removeFromLeft(meterWidth));
    midArea.removeFromLeft(4);
    m_faderSlider.setBounds(midArea);
}

void StereoChannelStrip::sliderValueChanged(juce::Slider* slider) {
    if (slider == &m_gainSlider) {
        const float val = static_cast<float>(slider->getValue());
        m_channel.setGainDb(val);
        juce::String prefix = (val > 0.0f) ? "+" : "";
        m_gainValue.setText(prefix + juce::String(val, 1) + " dB", juce::dontSendNotification);
    } else if (slider == &m_balanceSlider) {
        const float val = static_cast<float>(slider->getValue());
        m_channel.setBalance(val);
        if (std::abs(val) < 0.05f) {
            m_balanceValue.setText("BAL C", juce::dontSendNotification);
        } else if (val < 0.0f) {
            m_balanceValue.setText("L" + juce::String(static_cast<int>(std::abs(val) * 100.0f)), juce::dontSendNotification);
        } else {
            m_balanceValue.setText("R" + juce::String(static_cast<int>(val * 100.0f)), juce::dontSendNotification);
        }
    } else if (slider == &m_faderSlider) {
        const float val = static_cast<float>(slider->getValue());
        m_channel.setFaderDb(val);
        if (val <= -60.0f) {
            m_faderValue.setText("-INF dB", juce::dontSendNotification);
        } else {
            juce::String prefix = (val > 0.0f) ? "+" : "";
            m_faderValue.setText(prefix + juce::String(val, 1) + " dB", juce::dontSendNotification);
        }
    }
}

void StereoChannelStrip::buttonClicked(juce::Button* button) {
    if (button == &m_muteButton) {
        m_channel.setMute(button->getToggleState());
    } else if (button == &m_soloButton) {
        m_channel.setSolo(button->getToggleState());
    }
}

// =============================================================================
// Master Strip Implementation
// =============================================================================
MasterStrip::MasterStrip(mixer::MixerEngine& engine)
    : m_engine(engine)
{
    m_nameLabel.setText("MASTER", juce::dontSendNotification);
    m_nameLabel.setFont(juce::Font(15.0f, juce::Font::bold));
    m_nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff4f4f5));
    m_nameLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_nameLabel);

    m_sourceLabel.setText("Out 1/2 (Main)", juce::dontSendNotification);
    m_sourceLabel.setFont(juce::Font(10.0f, juce::Font::plain));
    m_sourceLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));
    m_sourceLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_sourceLabel);

    m_meter.setStereo(true);
    addAndMakeVisible(m_meter);

    m_faderSlider.setSliderStyle(juce::Slider::LinearVertical);
    m_faderSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_faderSlider.setRange(-60.0, 10.0, 0.1);
    m_faderSlider.setSkewFactorFromMidPoint(0.0);
    m_faderSlider.setValue(m_engine.getMasterFaderDb(), juce::dontSendNotification);
    m_faderSlider.addListener(this);
    m_faderSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xffe11d48)); // Crimson / Red
    m_faderSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffffffff));
    addAndMakeVisible(m_faderSlider);

    m_faderValue.setText("0.0 dB", juce::dontSendNotification);
    m_faderValue.setFont(juce::Font(12.0f, juce::Font::bold));
    m_faderValue.setColour(juce::Label::textColourId, juce::Colour(0xffe4e4e7));
    m_faderValue.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_faderValue);

    m_muteButton.setButtonText("MASTER MUTE");
    m_muteButton.setClickingTogglesState(true);
    m_muteButton.setToggleState(m_engine.isMasterMuted(), juce::dontSendNotification);
    m_muteButton.addListener(this);
    m_muteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    m_muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffef4444));
    m_muteButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffffff));
    addAndMakeVisible(m_muteButton);
}

MasterStrip::~MasterStrip() {
    m_faderSlider.removeListener(this);
    m_muteButton.removeListener(this);
}

void MasterStrip::updateTelemetry() {
    m_meter.setStereoLevels(m_engine.getMasterPeakL(), m_engine.getMasterPeakR());
    m_meter.setClipped(m_engine.hasMasterClipped());

    if (m_muteButton.getToggleState() != m_engine.isMasterMuted()) {
        m_muteButton.setToggleState(m_engine.isMasterMuted(), juce::dontSendNotification);
    }
}

void MasterStrip::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    // Master Section distinct styling with subtle accent border
    g.setColour(juce::Colour(0xff1c1917));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(juce::Colour(0xff44403c));
    g.drawRoundedRectangle(bounds, 6.0f, 1.5f);

    g.setColour(juce::Colour(0xff57534e));
    g.fillRect(bounds.getX() + 8.0f, bounds.getY() + 44.0f, bounds.getWidth() - 16.0f, 1.0f);
}

void MasterStrip::resized() {
    auto bounds = getLocalBounds().reduced(6);

    m_nameLabel.setBounds(bounds.removeFromTop(20));
    m_sourceLabel.setBounds(bounds.removeFromTop(16));
    bounds.removeFromTop(14);

    // Mute button at bottom
    auto muteRow = bounds.removeFromBottom(28);
    m_muteButton.setBounds(muteRow);
    bounds.removeFromBottom(6);

    m_faderValue.setBounds(bounds.removeFromBottom(20));
    bounds.removeFromBottom(6);

    // Middle area: Dual Meter on left, Master Fader on right
    auto midArea = bounds;
    const int meterWidth = 28;
    m_meter.setBounds(midArea.removeFromLeft(meterWidth));
    midArea.removeFromLeft(6);
    m_faderSlider.setBounds(midArea);
}

void MasterStrip::sliderValueChanged(juce::Slider* slider) {
    if (slider == &m_faderSlider) {
        const float val = static_cast<float>(slider->getValue());
        m_engine.setMasterFaderDb(val);
        if (val <= -60.0f) {
            m_faderValue.setText("-INF dB", juce::dontSendNotification);
        } else {
            juce::String prefix = (val > 0.0f) ? "+" : "";
            m_faderValue.setText(prefix + juce::String(val, 1) + " dB", juce::dontSendNotification);
        }
    }
}

void MasterStrip::buttonClicked(juce::Button* button) {
    if (button == &m_muteButton) {
        m_engine.setMasterMute(button->getToggleState());
    }
}

// =============================================================================
// MixerPanel Implementation
// =============================================================================
MixerPanel::MixerPanel(std::shared_ptr<audio::AudioEngine> engine)
    : m_audioEngine(std::move(engine)),
      m_mixerEngine(m_audioEngine ? m_audioEngine->getMixerEngine() : nullptr)
{
    // Header Title
    m_titleLabel.setText("LIVE MIXER", juce::dontSendNotification);
    m_titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    m_titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xfffafafa));
    addAndMakeVisible(m_titleLabel);

    // Audio Engine Start/Stop Toggle Button
    m_audioToggleButton.setButtonText("START AUDIO");
    m_audioToggleButton.addListener(this);
    m_audioToggleButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18181b));
    m_audioToggleButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfffafafa));
    addAndMakeVisible(m_audioToggleButton);

    // Status Badge
    m_statusBadge.setText(juce::CharPointer_UTF8("\xe2\x97\x8f READY"), juce::dontSendNotification);
    m_statusBadge.setFont(juce::Font(12.0f, juce::Font::bold));
    m_statusBadge.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
    m_statusBadge.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_statusBadge);

    // Instantiate 4 channel strips
    if (m_mixerEngine) {
        m_ch1Strip = std::make_unique<MonoChannelStrip>(m_mixerEngine->getChannel1(), "CH1 MIC", "Hardware In 1");
        addAndMakeVisible(m_ch1Strip.get());

        m_ch2Strip = std::make_unique<MonoChannelStrip>(m_mixerEngine->getChannel2(), "CH2 INST", "Hardware In 2");
        addAndMakeVisible(m_ch2Strip.get());

        m_ch34Strip = std::make_unique<StereoChannelStrip>(m_mixerEngine->getStereoChannel(), "CH3/4 MEDIA", "Hardware In 1-2");
        addAndMakeVisible(m_ch34Strip.get());

        m_masterStrip = std::make_unique<MasterStrip>(*m_mixerEngine);
        addAndMakeVisible(m_masterStrip.get());
    }

    // Start 30 Hz timer for smooth meter rendering without audio thread contention
    startTimerHz(30);
}

MixerPanel::~MixerPanel() {
    m_audioToggleButton.removeListener(this);
    stopTimer();
}

void MixerPanel::timerCallback() {
    if (m_audioEngine) {
        const auto state = m_audioEngine->getState();
        juce::String stateStr = juce::String(audio::audioStateToString(state)).toUpperCase();
        m_statusBadge.setText(juce::String(juce::CharPointer_UTF8("\xe2\x97\x8f ")) + stateStr, juce::dontSendNotification);

        if (state == audio::AudioState::Running) {
            m_statusBadge.setColour(juce::Label::textColourId, juce::Colour(0xff10b981)); // Green
            m_audioToggleButton.setButtonText("STOP AUDIO");
            m_audioToggleButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
        } else if (state == audio::AudioState::Error) {
            m_statusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffef4444)); // Red
            m_audioToggleButton.setButtonText("RETRY AUDIO");
            m_audioToggleButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdc2626));
        } else {
            m_statusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffe4e4e7));
            m_audioToggleButton.setButtonText("START AUDIO");
            m_audioToggleButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff059669));
        }
    }

    if (m_ch1Strip) m_ch1Strip->updateTelemetry();
    if (m_ch2Strip) m_ch2Strip->updateTelemetry();
    if (m_ch34Strip) m_ch34Strip->updateTelemetry();
    if (m_masterStrip) m_masterStrip->updateTelemetry();
}

void MixerPanel::buttonClicked(juce::Button* button) {
    if (button == &m_audioToggleButton && m_audioEngine) {
        const auto state = m_audioEngine->getState();
        if (state == audio::AudioState::Running) {
            m_audioEngine->stop();
        } else {
            m_audioEngine->start();
        }
    }
}

void MixerPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff09090b));
}

void MixerPanel::resized() {
    auto bounds = getLocalBounds().reduced(12);

    // Top Header Row
    auto headerRow = bounds.removeFromTop(28);
    m_titleLabel.setBounds(headerRow.removeFromLeft(160));
    m_audioToggleButton.setBounds(headerRow.removeFromLeft(110));
    headerRow.removeFromLeft(10);
    m_statusBadge.setBounds(headerRow);
    bounds.removeFromTop(8);

    // 4 Channel Strips side by side with responsive spacing
    const int numStrips = 4;
    const int gap = 8;
    const int totalGap = gap * (numStrips - 1);
    const int stripWidth = (bounds.getWidth() - totalGap) / numStrips;

    if (m_ch1Strip) {
        m_ch1Strip->setBounds(bounds.removeFromLeft(stripWidth));
        bounds.removeFromLeft(gap);
    }
    if (m_ch2Strip) {
        m_ch2Strip->setBounds(bounds.removeFromLeft(stripWidth));
        bounds.removeFromLeft(gap);
    }
    if (m_ch34Strip) {
        m_ch34Strip->setBounds(bounds.removeFromLeft(stripWidth));
        bounds.removeFromLeft(gap);
    }
    if (m_masterStrip) {
        m_masterStrip->setBounds(bounds);
    }
}

} // namespace livemixer::ui
