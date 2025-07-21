#pragma once
#include <JuceHeader.h>

class PluginInstance
{
public:
    std::unique_ptr<juce::AudioPluginInstance> processor;
    bool isEditorVisible = false;

    ~PluginInstance()
    {
        processor = nullptr;
    }
};