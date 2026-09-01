#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../ui/MainContainerComponent.h"
#include "../audio/core/AudioEngine.h"
#include <memory>

namespace livemixer::app {

class MainWindow : public juce::DocumentWindow {
public:
    explicit MainWindow(juce::String name, std::shared_ptr<audio::AudioEngine> engine);
    ~MainWindow() override;

    void closeButtonPressed() override;

private:
    std::unique_ptr<ui::MainContainerComponent> m_mainContainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

} // namespace livemixer::app
