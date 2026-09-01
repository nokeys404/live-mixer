#include "MainContainerComponent.h"

namespace livemixer::ui {

MainContainerComponent::MainContainerComponent(std::shared_ptr<audio::AudioEngine> engine)
    : m_engine(std::move(engine))
{
    m_mixerTabButton.setButtonText("MIXER");
    m_mixerTabButton.setClickingTogglesState(false);
    m_mixerTabButton.addListener(this);
    m_mixerTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    m_mixerTabButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfffafafa));
    addAndMakeVisible(m_mixerTabButton);

    m_settingsTabButton.setButtonText("AUDIO SETTINGS");
    m_settingsTabButton.setClickingTogglesState(false);
    m_settingsTabButton.addListener(this);
    m_settingsTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18181b));
    m_settingsTabButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffa1a1aa));
    addAndMakeVisible(m_settingsTabButton);

    m_mixerPanel = std::make_unique<MixerPanel>(m_engine);
    addAndMakeVisible(m_mixerPanel.get());

    m_settingsPanel = std::make_unique<AudioSettingsPanel>(m_engine);
    addChildComponent(m_settingsPanel.get());

    showMixerView();
}

MainContainerComponent::~MainContainerComponent() {
    m_mixerTabButton.removeListener(this);
    m_settingsTabButton.removeListener(this);
}

void MainContainerComponent::showMixerView() {
    m_isMixerView = true;
    m_mixerTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3f3f46));
    m_mixerTabButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));

    m_settingsTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18181b));
    m_settingsTabButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff71717a));

    if (m_mixerPanel) m_mixerPanel->setVisible(true);
    if (m_settingsPanel) m_settingsPanel->setVisible(false);
    resized();
}

void MainContainerComponent::showSettingsView() {
    m_isMixerView = false;
    m_settingsTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3f3f46));
    m_settingsTabButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));

    m_mixerTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18181b));
    m_mixerTabButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff71717a));

    if (m_mixerPanel) m_mixerPanel->setVisible(false);
    if (m_settingsPanel) m_settingsPanel->setVisible(true);
    resized();
}

void MainContainerComponent::buttonClicked(juce::Button* button) {
    if (button == &m_mixerTabButton) {
        showMixerView();
    } else if (button == &m_settingsTabButton) {
        showSettingsView();
    }
}

void MainContainerComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff09090b));

    // Divider line under navigation tabs
    g.setColour(juce::Colour(0xff27272a));
    g.fillRect(0, 36, getWidth(), 1);
}

void MainContainerComponent::resized() {
    auto bounds = getLocalBounds();

    // Top Tab Bar
    auto tabRow = bounds.removeFromTop(36);
    tabRow.removeFromLeft(12);
    m_mixerTabButton.setBounds(tabRow.removeFromLeft(100).reduced(0, 4));
    tabRow.removeFromLeft(8);
    m_settingsTabButton.setBounds(tabRow.removeFromLeft(140).reduced(0, 4));

    // Content view
    if (m_isMixerView && m_mixerPanel) {
        m_mixerPanel->setBounds(bounds);
    } else if (!m_isMixerView && m_settingsPanel) {
        m_settingsPanel->setBounds(bounds);
    }
}

} // namespace livemixer::ui
