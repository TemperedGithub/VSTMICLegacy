#pragma once
#include <JuceHeader.h>
#include "PluginInstance.h"

class Settings
{
public:
    Settings() = default;
    bool saveState(juce::AudioDeviceManager& deviceManager);
    bool loadState(juce::AudioDeviceManager& deviceManager);

    // New methods for plugin state
    bool savePluginState(const std::vector<std::unique_ptr<PluginInstance>>& plugins);
    bool loadPluginState(std::vector<std::unique_ptr<PluginInstance>>& plugins,
        juce::AudioPluginFormatManager& formatManager,
        double sampleRate,
        int bufferSize);

private:
    juce::File getSettingsFile()
    {
        auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("VSTMIC");
        appDataDir.createDirectory();
        return appDataDir.getChildFile("settings.xml");
    }

    juce::File getPluginStateFile()
    {
        auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("VSTMIC");
        appDataDir.createDirectory();
        return appDataDir.getChildFile("pluginstate.xml");
    }
};