#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MixerPanel.h"
#include "AudioSettingsPanel.h"
#include "../audio/core/AudioEngine.h"
#include <memory>

namespace livemixer::ui {

/**
 * @brief Top-level application container hosting Mixer and Audio Settings views
 */
class MainContainerComponent : public juce::Component, public juce::Button::Listener {
public:
    explicit MainContainerComponent(std::shared_ptr<audio::AudioEngine> engine);
    ~MainContainerComponent() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void buttonClicked(juce::Button* button) override;

    void showMixerView();
    void showSettingsView();

private:
    std::shared_ptr<audio::AudioEngine> m_engine;

    juce::TextButton m_mixerTabButton;
    juce::TextButton m_settingsTabButton;

    std::unique_ptr<MixerPanel> m_mixerPanel;
    std::unique_ptr<AudioSettingsPanel> m_settingsPanel;

    bool m_isMixerView{true};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContainerComponent)
};

} // namespace livemixer::ui
