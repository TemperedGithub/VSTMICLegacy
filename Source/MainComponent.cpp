#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    setSize(800, 600);

    // Color scheme
    const auto darkGrey = juce::Colour(40, 40, 40);
    const auto darkerGrey = juce::Colour(30, 30, 30);
    const auto lighterGrey = juce::Colour(60, 60, 60);
    const auto whitish = juce::Colour(230, 230, 230);
    const auto highlightGrey = juce::Colour(70, 70, 70);

    // Style buttons
    for (auto* button : { &loadPluginButton, &settingsButton, &saveButton })
    {
        addAndMakeVisible(button);
        button->setColour(juce::TextButton::buttonColourId, lighterGrey);
        button->setColour(juce::TextButton::buttonOnColourId, highlightGrey);
        button->setColour(juce::TextButton::textColourOffId, whitish);
        button->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }

    // Initialize the monitor device manager
    monitorDeviceManager.initialiseWithDefaultDevices(/*numInput*/ 0, /*numOutput*/ 2);

    // Create a MonitorAudioSource, attach it to a player, and add callback
    monitorAudioSource = std::make_unique<MonitorAudioSource>();
    monitorSourcePlayer.setSource(monitorAudioSource.get());
    monitorDeviceManager.addAudioCallback(&monitorSourcePlayer);

    loadPluginButton.setButtonText("Add VST3 Plugin");
    loadPluginButton.onClick = [this] { loadPlugin(); };

    settingsButton.setButtonText("Audio Settings");
    settingsButton.onClick = [this] { showAudioSettings(); };

    saveButton.setButtonText("Save Plugin State");
    saveButton.onClick = [this]
    {
        DBG("Save button clicked, saving plugin state...");
        if (settings.savePluginState(plugins))
        {
            DBG("Successfully saved plugin state");
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Save Successful",
                "Plugin state has been saved.",
                "OK");
        }
        else
        {
            DBG("Failed to save plugin state");
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Save Failed",
                "Failed to save plugin state.",
                "OK");
        }
    };

    // Plugin list
    addAndMakeVisible(pluginList);
    pluginList.setModel(this);
    pluginList.setRowHeight(30);
    pluginList.setMultipleSelectionEnabled(false);
    pluginList.setColour(juce::ListBox::backgroundColourId, darkerGrey);
    pluginList.setColour(juce::ListBox::outlineColourId, lighterGrey);
    pluginList.setColour(juce::ListBox::textColourId, whitish);
    pluginList.setOutlineThickness(1);

    formatManager.addDefaultFormats();

    // Main device manager
    auto result = deviceManager.initialiseWithDefaultDevices(2, 2);
    if (result.isEmpty())
    {
        DBG("Successfully initialized main device with default devices");
    }
    else
    {
        DBG("Failed to initialize main device: " << result);
    }

    if (!settings.loadState(deviceManager))
        DBG("Failed to load main device settings, using defaults");

    // Load saved plugins
    DBG("Loading saved plugins");
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        DBG("Device available for plugin loading");
        DBG("Sample rate: " << device->getCurrentSampleRate());
        DBG("Buffer size: " << device->getCurrentBufferSizeSamples());

        if (!settings.loadPluginState(plugins, formatManager,
            device->getCurrentSampleRate(),
            device->getCurrentBufferSizeSamples()))
        {
            DBG("Failed to load plugin state");
        }
        else
        {
            DBG("Successfully loaded plugin state");
            DBG("Number of plugins loaded: " << plugins.size());
        }
        pluginList.updateContent();
    }
    else
    {
        DBG("No audio device available for plugin loading");
    }

    deviceManager.addAudioCallback(this);
    DBG("MainComponent constructor completed");
}

MainComponent::~MainComponent()
{
    deviceManager.removeAudioCallback(this);

    // Stop monitor player
    monitorSourcePlayer.setSource(nullptr);
    monitorDeviceManager.removeAudioCallback(&monitorSourcePlayer);

    shutdownAudio();
    settings.saveState(deviceManager);
    plugins.clear();
    DBG("MainComponent destructor completed");
}

//==============================================================================
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    DBG("prepareToPlay called: " << samplesPerBlockExpected << " samples at " << sampleRate << " Hz");
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    // This might not get called if you're using manual callbacks, but
    // AudioAppComponent requires us to implement it.
    DBG("getNextAudioBlock called");
}

void MainComponent::releaseResources()
{
    DBG("releaseResources called");
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(40, 40, 40));
    g.setColour(juce::Colour(60, 60, 60));
    g.drawRect(getLocalBounds(), 1);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    auto buttonHeight = 30;
    auto margin = 10;

    auto buttonArea = area.removeFromTop(buttonHeight);
    loadPluginButton.setBounds(buttonArea.removeFromLeft(200).reduced(margin, 0));
    settingsButton.setBounds(buttonArea.removeFromLeft(200).reduced(margin, 0));
    saveButton.setBounds(buttonArea.removeFromLeft(200).reduced(margin, 0));

    pluginList.setBounds(area.reduced(margin));
}

//==============================================================================
int MainComponent::getNumRows()
{
    return (int)plugins.size();
}

void MainComponent::paintListBoxItem(int rowNumber, juce::Graphics& g,
    int width, int height, bool rowIsSelected)
{
    if (rowNumber >= 0 && rowNumber < (int)plugins.size())
    {
        if (rowIsSelected)
            g.fillAll(juce::Colour(70, 70, 70));
        else
            g.fillAll(juce::Colour(45, 45, 45));

        g.setColour(juce::Colour(35, 35, 35));
        g.drawLine(0, height, width, height, 1.0f);

        auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(8, 0);
        auto plugin = plugins[rowNumber]->processor.get();

        g.setColour(juce::Colour(230, 230, 230));
        g.drawText(plugin->getName(), bounds, juce::Justification::centredLeft);

        if (plugins[rowNumber]->isEditorVisible)
        {
            g.setColour(juce::Colour(200, 200, 200));
            g.drawEllipse(width - 20, height / 2 - 5, 10, 10, 1.0f);
            g.setColour(juce::Colour(230, 230, 230));
            g.fillEllipse(width - 19, height / 2 - 4, 8, 8);
        }
    }
}

void MainComponent::deleteSelectedPlugin()
{
    int selectedRow = pluginList.getSelectedRow();
    if (selectedRow >= 0 && selectedRow < (int)plugins.size())
    {
        plugins.erase(plugins.begin() + selectedRow);
        pluginList.updateContent();
        settings.savePluginState(plugins);
        DBG("Removed plugin at index " << selectedRow);
    }
}

void MainComponent::listBoxItemDoubleClicked(int row, const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Remove Plugin");

        menu.showMenuAsync(juce::PopupMenu::Options(),
            [this](int result)
            {
                if (result == 1)
                    deleteSelectedPlugin();
            });
    }
    else
    {
        togglePluginWindow(row);
    }
}

//==============================================================================
void MainComponent::audioDeviceIOCallback(const float** inputChannelData,
    int numInputChannels,
    float** outputChannelData,
    int numOutputChannels,
    int numSamples)
{
    // Make sure tempBuffer is large enough
    if (tempBuffer.getNumChannels() < numOutputChannels || tempBuffer.getNumSamples() < numSamples)
        tempBuffer.setSize(numOutputChannels, numSamples, false, false, true);

    // If monitoring is enabled, feed input to the monitor AudioSource
    if (monitoringEnabled && monitorAudioSource)
        monitorAudioSource->writeToFifo(inputChannelData, numInputChannels, numSamples);

    // Copy input to tempBuffer for plugin processing
    for (int channel = 0; channel < numInputChannels && channel < tempBuffer.getNumChannels(); ++channel)
    {
        if (inputChannelData[channel] != nullptr)
            tempBuffer.copyFrom(channel, 0, inputChannelData[channel], numSamples);
    }

    // Process through plugins
    if (!plugins.empty())
    {
        juce::MidiBuffer midiBuffer;
        for (auto& plugin : plugins)
        {
            if (plugin && plugin->processor)
                plugin->processor->processBlock(tempBuffer, midiBuffer);
        }
    }

    // Output processed audio
    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        if (outputChannelData[channel] != nullptr)
        {
            if (channel < tempBuffer.getNumChannels())
            {
                memcpy(outputChannelData[channel],
                    tempBuffer.getReadPointer(channel),
                    sizeof(float) * numSamples);
            }
            else
            {
                juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);
            }
        }
    }
}

void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    DBG("Main device about to start: " << device->getName());
    DBG("Sample rate: " << device->getCurrentSampleRate());
    DBG("Buffer size: " << device->getCurrentBufferSizeSamples());

    tempBuffer.setSize(2, device->getCurrentBufferSizeSamples());

    for (auto& plugin : plugins)
    {
        if (plugin && plugin->processor)
        {
            plugin->processor->prepareToPlay(device->getCurrentSampleRate(),
                device->getCurrentBufferSizeSamples());
            DBG("Prepared plugin: " << plugin->processor->getName());
        }
    }
}

void MainComponent::audioDeviceStopped()
{
    DBG("Main device stopped");
    for (auto& plugin : plugins)
    {
        if (plugin && plugin->processor)
            plugin->processor->releaseResources();
    }
}

//==============================================================================
void MainComponent::loadPlugin()
{
    chooser = std::make_unique<juce::FileChooser>("Select a VST3 plugin",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.vst3");

    auto flags = juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (result == juce::File{})
            {
                DBG("No file selected");
                return;
            }

            DBG("Selected plugin file: " << result.getFullPathName());

            auto format = formatManager.getFormat(0);
            if (format == nullptr)
            {
                DBG("No plugin format found");
                return;
            }

            juce::OwnedArray<juce::PluginDescription> descriptions;
            format->findAllTypesForFile(descriptions, result.getFullPathName());

            if (descriptions.isEmpty())
            {
                DBG("No plugin descriptions found");
                return;
            }

            DBG("Found plugin: " << descriptions[0]->name);

            auto* device = deviceManager.getCurrentAudioDevice();
            if (device == nullptr)
            {
                DBG("No main audio device available");
                return;
            }

            auto instance = std::make_unique<PluginInstance>();

            double sampleRate = device->getCurrentSampleRate();
            int bufferSize = device->getCurrentBufferSizeSamples();

            DBG("Creating plugin instance with sample rate: " << sampleRate
                << " and buffer size: " << bufferSize);

            auto pluginInstance = format->createInstanceFromDescription(*descriptions[0], sampleRate, bufferSize);
            if (pluginInstance == nullptr)
            {
                DBG("Failed to create plugin instance");
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Error", "Failed to create plugin instance");
                return;
            }

            DBG("Plugin instance created successfully");
            instance->processor = std::move(pluginInstance);

            // Configure the plugin
            DBG("Configuring plugin with sample rate: " << sampleRate
                << " and buffer size: " << bufferSize);
            instance->processor->setRateAndBufferSizeDetails(sampleRate, bufferSize);

            if (auto* bus = instance->processor->getBus(true, 0))
            {
                bus->enable();
                DBG("Enabled input bus");
            }
            if (auto* bus = instance->processor->getBus(false, 0))
            {
                bus->enable();
                DBG("Enabled output bus");
            }

            auto layout = instance->processor->getBusesLayout();
            if (!instance->processor->setBusesLayout(layout))
            {
                DBG("Failed to set plugin bus layout");
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Error", "Failed to set plugin bus layout");
                return;
            }

            instance->processor->prepareToPlay(sampleRate, bufferSize);
            DBG("Plugin prepared to play");

            // Add plugin to chain
            plugins.push_back(std::move(instance));
            pluginList.updateContent();
            DBG("Plugin added successfully to chain");

            auto* lastPlugin = plugins.back()->processor.get();
            DBG("Final plugin state:");
            DBG("Name: " << lastPlugin->getName());
            DBG("Input channels: " << lastPlugin->getTotalNumInputChannels());
            DBG("Output channels: " << lastPlugin->getTotalNumOutputChannels());
            DBG("Latency samples: " << lastPlugin->getLatencySamples());

            // Save plugin state
            settings.savePluginState(plugins);
        });
}

//==============================================================================
void MainComponent::showAudioSettings()
{
    if (settingsWindow == nullptr)
    {
        const auto darkGrey = juce::Colour(40, 40, 40);
        const auto whitish = juce::Colour(230, 230, 230);

        settingsWindow = std::make_unique<SettingsWindow>();
        auto container = new juce::Component();
        container->setColour(juce::ResizableWindow::backgroundColourId, darkGrey);

        audioSettings.reset(new juce::AudioDeviceSelectorComponent(
            deviceManager, 0, 256, 0, 256, true, true, true, true
        ));
        styleAudioSettings(*audioSettings);

        monitorSettings.reset(new juce::AudioDeviceSelectorComponent(
            monitorDeviceManager, 0, 0, 2, 2, false, true, false, false
        ));
        styleAudioSettings(*monitorSettings);

        monitorButton.setButtonText("Enable Monitoring Output");
        monitorButton.setToggleState(monitoringEnabled, juce::dontSendNotification);
        monitorButton.setColour(juce::ToggleButton::textColourId, whitish);
        monitorButton.setColour(juce::ToggleButton::tickColourId, whitish);
        monitorButton.onClick = [this]
        {
            monitoringEnabled = monitorButton.getToggleState();
            monitorSettings->setVisible(monitoringEnabled);
            settingsWindow->centreWithSize(500, monitoringEnabled ? 800 : 500);
        };

        container->addAndMakeVisible(audioSettings.get());
        container->addAndMakeVisible(monitorButton);
        container->addAndMakeVisible(monitorSettings.get());

        container->setSize(500, 800);
        audioSettings->setBounds(0, 0, 500, 450);
        monitorButton.setBounds(10, 460, 200, 25);
        monitorSettings->setBounds(0, 500, 500, 280);
        monitorSettings->setVisible(monitoringEnabled);

        settingsWindow->setContentOwned(container, true);
        settingsWindow->setBackgroundColour(darkGrey);
        settingsWindow->centreWithSize(500, monitoringEnabled ? 800 : 500);
    }

    settingsWindow->setVisible(true);
}

void MainComponent::styleAudioSettings(juce::AudioDeviceSelectorComponent& selector)
{
    const auto darkGrey = juce::Colour(40, 40, 40);
    const auto lighterGrey = juce::Colour(60, 60, 60);
    const auto whitish = juce::Colour(230, 230, 230);

    std::function<void(juce::Component*)> styleComponent = [&](juce::Component* comp)
    {
        if (auto* listBox = dynamic_cast<juce::ListBox*>(comp))
        {
            listBox->setColour(juce::ListBox::backgroundColourId, darkGrey);
            listBox->setColour(juce::ListBox::textColourId, whitish);
            listBox->setColour(juce::ListBox::outlineColourId, lighterGrey);
        }
        if (auto* comboBox = dynamic_cast<juce::ComboBox*>(comp))
        {
            comboBox->setColour(juce::ComboBox::backgroundColourId, darkGrey);
            comboBox->setColour(juce::ComboBox::textColourId, whitish);
            comboBox->setColour(juce::ComboBox::arrowColourId, whitish);
            comboBox->setColour(juce::ComboBox::outlineColourId, lighterGrey);
        }
        if (auto* label = dynamic_cast<juce::Label*>(comp))
        {
            label->setColour(juce::Label::textColourId, whitish);
            label->setColour(juce::Label::backgroundColourId, darkGrey);
        }
        if (auto* textEditor = dynamic_cast<juce::TextEditor*>(comp))
        {
            textEditor->setColour(juce::TextEditor::backgroundColourId, darkGrey);
            textEditor->setColour(juce::TextEditor::textColourId, whitish);
            textEditor->setColour(juce::TextEditor::outlineColourId, lighterGrey);
        }

        for (auto* child : comp->getChildren())
            styleComponent(child);
    };

    styleComponent(&selector);
}

//==============================================================================
void MainComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    DBG("Audio settings changed, saving...");
    settings.saveState(deviceManager);
}

void MainComponent::removePlugin(int index)
{
    if (index >= 0 && index < (int)plugins.size())
    {
        plugins.erase(plugins.begin() + index);
        pluginList.updateContent();
    }
}

void MainComponent::togglePluginWindow(int index)
{
    if (index >= 0 && index < (int)plugins.size())
    {
        auto& plugin = plugins[index];

        if (!plugin->isEditorVisible)
        {
            juce::MessageManager::callAsync([this, &plugin]()
                {
                    auto* window = new PluginEditorWindow(*plugin->processor, *plugin);
                    plugin->isEditorVisible = true;
                    window->setVisible(true);
                    pluginList.repaint();
                });
        }

        pluginList.repaint();
    }
}

//==============================================================================
MainComponent::PluginEditorWindow::PluginEditorWindow(juce::AudioProcessor& processor, PluginInstance& plugin)
    : DocumentWindow(processor.getName(),
        juce::Colours::lightgrey,
        DocumentWindow::minimiseButton | DocumentWindow::closeButton),
    ownerPlugin(plugin)
{
    if (auto* editor = processor.createEditor())
    {
        setContentOwned(editor, true);
        setResizable(true, true);
        setUsingNativeTitleBar(true);
        centreWithSize(editor->getWidth(), editor->getHeight());
        addComponentListener(this);
    }
}

void MainComponent::PluginEditorWindow::closeButtonPressed()
{
    setVisible(false);
    juce::MessageManager::callAsync([this]()
        {
            ownerPlugin.isEditorVisible = false;
            delete this;
        });
}

void MainComponent::PluginEditorWindow::componentVisibilityChanged(juce::Component& component)
{
    if (!component.isVisible())
        ownerPlugin.isEditorVisible = false;
}

MainComponent::SettingsWindow::SettingsWindow()
    : DocumentWindow("Audio Settings",
        juce::Colours::lightgrey,
        DocumentWindow::closeButton)
{
    setUsingNativeTitleBar(true);
    setResizable(true, true);
}

void MainComponent::SettingsWindow::closeButtonPressed()
{
    setVisible(false);
}
