#include "MainComponent.h"

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow()
        : DocumentWindow("VSTMIC",
            juce::Colours::lightgrey,
            DocumentWindow::allButtons)
    {
        setContentOwned(new MainComponent(), true);
        setResizable(true, true);
        setUsingNativeTitleBar(true);

        // Set minimum window size
        setResizeLimits(400, 300, 10000, 10000);

        // Center the window and set initial size
        centreWithSize(800, 600);

        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

class Application : public juce::JUCEApplication
{
public:
    Application() = default;

    const juce::String getApplicationName() override { return "VST Host"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>();
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(Application)