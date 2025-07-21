#pragma once

#include <JuceHeader.h>
#include "PluginInstance.h"
#include "Settings.h"
#include "MonitorAudioSource.h" // <--- Include the new audio source

class MainComponent : public juce::AudioAppComponent,
    public juce::ListBoxModel,
    public juce::AudioIODeviceCallback,
    public juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    // AudioAppComponent overrides
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    // Component methods
    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // ListBoxModel methods
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g,
        int width, int height, bool rowIsSelected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

    //==============================================================================
    // AudioIODeviceCallback for the main device
    void audioDeviceIOCallback(const float** inputChannelData,
        int numInputChannels,
        float** outputChannelData,
        int numOutputChannels,
        int numSamples) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    //==============================================================================
    // ChangeListener
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

private:
    //==============================================================================
    // Plugin Editor Window (unchanged from your code)
    class PluginEditorWindow : public juce::DocumentWindow,
        public juce::ComponentListener
    {
    public:
        PluginEditorWindow(juce::AudioProcessor& processor, PluginInstance& plugin);
        void closeButtonPressed() override;
        void componentVisibilityChanged(juce::Component& component) override;
    private:
        PluginInstance& ownerPlugin;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditorWindow)
    };

    //==============================================================================
    // Settings Window (unchanged)
    class SettingsWindow : public juce::DocumentWindow
    {
    public:
        SettingsWindow();
        void closeButtonPressed() override;
    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsWindow)
    };

    //==============================================================================
    // Private methods
    void loadPlugin();
    void showAudioSettings();
    void removePlugin(int index);
    void togglePluginWindow(int index);
    void deleteSelectedPlugin();
    void styleAudioSettings(juce::AudioDeviceSelectorComponent& selector);

    //==============================================================================
    // Data members

    // Audio + plugin stuff
    Settings settings;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::AudioPluginFormatManager formatManager;
    juce::AudioBuffer<float> tempBuffer;
    std::vector<std::unique_ptr<PluginInstance>> plugins;

    // UI
    juce::TextButton loadPluginButton;
    juce::TextButton settingsButton;
    juce::TextButton saveButton;
    juce::ListBox pluginList;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> audioSettings;
    std::unique_ptr<SettingsWindow> settingsWindow;

    //==============================================================================
    // Monitoring via AudioSource
    juce::AudioDeviceManager monitorDeviceManager;
    std::unique_ptr<MonitorAudioSource> monitorAudioSource; // <--- NEW
    juce::AudioSourcePlayer monitorSourcePlayer;            // <--- NEW

    bool monitoringEnabled = false;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> monitorSettings;
    juce::ToggleButton monitorButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
