#include "MainWindow.h"

namespace livemixer::app {

MainWindow::MainWindow(juce::String name, std::shared_ptr<audio::AudioEngine> engine)
    : DocumentWindow(name,
                     juce::Colour(0xff18181b),
                     DocumentWindow::allButtons)
{
    setUsingNativeTitleBar(true);
    m_settingsPanel = std::make_unique<ui::AudioSettingsPanel>(std::move(engine));
    setContentNonOwned(m_settingsPanel.get(), false);

    setResizable(true, true);
    setResizeLimits(640, 440, 1920, 1200);
    centreWithSize(780, 520);
    setVisible(true);
}

MainWindow::~MainWindow() {
    clearContentComponent();
}

void MainWindow::closeButtonPressed() {
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

} // namespace livemixer::app

