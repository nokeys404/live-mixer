#include "MainWindow.h"

namespace livemixer::app {

MainWindow::MainWindow(juce::String name, std::shared_ptr<audio::AudioEngine> engine)
    : DocumentWindow(name,
                     juce::Colour(0xff18181b),
                     DocumentWindow::allButtons)
{
    setUsingNativeTitleBar(true);
    m_mainContainer = std::make_unique<ui::MainContainerComponent>(std::move(engine));
    setContentNonOwned(m_mainContainer.get(), false);

    setResizable(true, true);
    setResizeLimits(680, 460, 1920, 1200);
    centreWithSize(860, 560);
    setVisible(true);
}

MainWindow::~MainWindow() {
    clearContentComponent();
}

void MainWindow::closeButtonPressed() {
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

} // namespace livemixer::app

