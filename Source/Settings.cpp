#include "Settings.h"

bool Settings::saveState(juce::AudioDeviceManager& deviceManager)
{
    auto settingsFile = getSettingsFile();
    DBG("Attempting to save settings to: " << settingsFile.getFullPathName());

    // Make sure the directory exists
    if (!settingsFile.getParentDirectory().exists())
    {
        DBG("Creating settings directory: " << settingsFile.getParentDirectory().getFullPathName());
        settingsFile.getParentDirectory().createDirectory();
    }

    if (auto xml = deviceManager.createStateXml())
    {
        // Save current setup for debugging
        auto* device = deviceManager.getCurrentAudioDevice();
        if (device != nullptr)
        {
            DBG("Saving settings:");
            DBG("Sample rate: " << device->getCurrentSampleRate());
            DBG("Buffer size: " << device->getCurrentBufferSizeSamples());
            DBG("Input: " << device->getActiveInputChannels().toString(2));
            DBG("Output: " << device->getActiveOutputChannels().toString(2));
            DBG("Device name: " << device->getName());
        }

        bool success = xml->writeTo(settingsFile);
        if (success)
        {
            DBG("Successfully saved settings file");
        }
        else
        {
            DBG("Failed to write settings file!");
            DBG("File path: " << settingsFile.getFullPathName());
            DBG("File exists: " << (settingsFile.exists() ? "Yes" : "No"));
            DBG("Directory exists: " << (settingsFile.getParentDirectory().exists() ? "Yes" : "No"));
            DBG("Directory writable: " << (settingsFile.getParentDirectory().hasWriteAccess() ? "Yes" : "No"));
        }
        return success;
    }

    DBG("Failed to create XML state");
    return false;
}

bool Settings::loadState(juce::AudioDeviceManager& deviceManager)
{
    auto settingsFile = getSettingsFile();
    DBG("Loading settings from: " << settingsFile.getFullPathName());
    if (settingsFile.existsAsFile())
    {
        if (auto xml = juce::parseXML(settingsFile))
        {
            DBG("XML parsed successfully");
            // Initialize with default setup first
            deviceManager.initialiseWithDefaultDevices(2, 2);
            // Then apply saved settings
            if (deviceManager.initialise(2, 2, xml.get(), true) == "")
            {
                // Print loaded settings
                if (auto* device = deviceManager.getCurrentAudioDevice())
                {
                    DBG("Settings loaded:");
                    DBG("Sample rate: " << device->getCurrentSampleRate());
                    DBG("Buffer size: " << device->getCurrentBufferSizeSamples());
                    DBG("Input: " << device->getActiveInputChannels().toString(2));
                    DBG("Output: " << device->getActiveOutputChannels().toString(2));
                    DBG("Device name: " << device->getName());
                }
                return true;
            }
            else
            {
                DBG("Failed to apply settings");
            }
        }
        else
        {
            DBG("Failed to parse settings XML");
        }
    }
    else
    {
        DBG("No settings file found, using defaults");
        deviceManager.initialiseWithDefaultDevices(2, 2);
    }
    return false;
}

bool Settings::savePluginState(const std::vector<std::unique_ptr<PluginInstance>>& plugins)
{
    auto stateFile = getPluginStateFile();
    DBG("Saving plugin state to: " << stateFile.getFullPathName());

    juce::XmlElement rootElement("PluginState");
    DBG("Number of plugins to save: " << plugins.size());

    for (int i = 0; i < plugins.size(); ++i)
    {
        auto& plugin = plugins[i];
        if (plugin != nullptr && plugin->processor != nullptr)
        {
            auto pluginElement = std::make_unique<juce::XmlElement>("Plugin");

            // Save basic plugin info
            pluginElement->setAttribute("index", i);
            pluginElement->setAttribute("name", plugin->processor->getName());

            // Save the plugin description
            juce::PluginDescription desc = plugin->processor->getPluginDescription();
            DBG("Saving plugin " << i << ":");
            DBG(" - Name: " << desc.name);
            DBG(" - Format: " << desc.pluginFormatName);
            DBG(" - File: " << desc.fileOrIdentifier);
            DBG(" - Manufacturer: " << desc.manufacturerName);
            DBG(" - Version: " << desc.version);

            if (std::unique_ptr<juce::XmlElement> descXml = desc.createXml())
            {
                auto descElement = std::make_unique<juce::XmlElement>("Description");

                // Explicitly save critical information
                descElement->setAttribute("name", desc.name);
                descElement->setAttribute("pluginFormatName", desc.pluginFormatName);
                descElement->setAttribute("fileOrIdentifier", desc.fileOrIdentifier);
                descElement->setAttribute("manufacturerName", desc.manufacturerName);
                descElement->setAttribute("version", desc.version);
                descElement->setAttribute("isInstrument", desc.isInstrument);
                descElement->setAttribute("numInputChannels", desc.numInputChannels);
                descElement->setAttribute("numOutputChannels", desc.numOutputChannels);

                // Copy remaining attributes from the original XML
                for (int j = 0; j < descXml->getNumAttributes(); ++j)
                {
                    auto attrName = descXml->getAttributeName(j);
                    auto attrValue = descXml->getAttributeValue(j);
                    if (!descElement->hasAttribute(attrName)) // Don't overwrite our explicit attributes
                    {
                        descElement->setAttribute(attrName, attrValue);
                    }
                }

                // Copy any child elements if they exist
                for (auto* child : descXml->getChildIterator())
                {
                    descElement->addChildElement(new juce::XmlElement(*child));
                }

                DBG("Saved description XML with " << descElement->getNumAttributes() << " attributes");
                pluginElement->addChildElement(descElement.release());
            }

            // Save plugin's internal state
            juce::MemoryBlock stateData;
            plugin->processor->getStateInformation(stateData);
            if (stateData.getSize() > 0)
            {
                auto stateElement = std::make_unique<juce::XmlElement>("State");
                stateElement->setAttribute("data", stateData.toBase64Encoding());
                pluginElement->addChildElement(stateElement.release());
                DBG("Saved plugin state data: " << stateData.getSize() << " bytes");
            }

            rootElement.addChildElement(pluginElement.release());
            DBG("Successfully saved plugin " << i << ": " << plugin->processor->getName());
        }
    }

    bool success = rootElement.writeTo(stateFile);
    if (success)
    {
        DBG("Successfully saved plugin state file to: " << stateFile.getFullPathName());
    }
    else
    {
        DBG("Failed to save plugin state file!");
    }
    return success;
}

bool Settings::loadPluginState(std::vector<std::unique_ptr<PluginInstance>>& plugins,
    juce::AudioPluginFormatManager& formatManager,
    double sampleRate,
    int bufferSize)
{
    auto stateFile = getPluginStateFile();
    DBG("Loading plugin state from: " << stateFile.getFullPathName());

    if (!stateFile.existsAsFile())
    {
        DBG("No plugin state file found");
        return false;
    }

    std::unique_ptr<juce::XmlElement> rootXml(juce::parseXML(stateFile));
    if (rootXml == nullptr)
    {
        DBG("Failed to parse plugin state XML");
        return false;
    }

    DBG("Clearing existing plugins");
    plugins.clear();

    // Debug available formats
    DBG("Available formats:");
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        auto* f = formatManager.getFormat(i);
        DBG(" - " << f->getName());
    }

    int pluginIndex = 0;
    for (auto* pluginXml : rootXml->getChildIterator())
    {
        if (pluginXml->hasTagName("Plugin"))
        {
            juce::String pluginName = pluginXml->getStringAttribute("name");
            DBG("\nAttempting to load plugin " << pluginIndex << ": " << pluginName);

            if (auto* descXml = pluginXml->getChildByName("Description"))
            {
                // Debug: Print all available attributes in XML
                DBG("XML attributes available:");
                for (int i = 0; i < descXml->getNumAttributes(); ++i)
                {
                    DBG(" - " << descXml->getAttributeName(i) << ": " << descXml->getAttributeValue(i));
                }

                // Create plugin description with explicit attribute mapping
                juce::PluginDescription desc;

                // Use the explicit attributes from XML first
                desc.name = descXml->getStringAttribute("name");
                desc.pluginFormatName = descXml->getStringAttribute("format", "VST3");
                desc.fileOrIdentifier = descXml->getStringAttribute("file", descXml->getStringAttribute("fileOrIdentifier"));
                desc.manufacturerName = descXml->getStringAttribute("manufacturer", descXml->getStringAttribute("manufacturerName"));
                desc.version = descXml->getStringAttribute("version");
                desc.isInstrument = descXml->getBoolAttribute("isInstrument", false);
                desc.numInputChannels = descXml->getIntAttribute("numInputs", descXml->getIntAttribute("numInputChannels", 2));
                desc.numOutputChannels = descXml->getIntAttribute("numOutputs", descXml->getIntAttribute("numOutputChannels", 2));

                DBG("Loaded plugin description:");
                DBG(" - Name: " << desc.name);
                DBG(" - Format: " << desc.pluginFormatName);
                DBG(" - File: " << desc.fileOrIdentifier);
                DBG(" - Manufacturer: " << desc.manufacturerName);
                DBG(" - Version: " << desc.version);
                DBG(" - Is Instrument: " << (desc.isInstrument ? "Yes" : "No"));
                DBG(" - Channels: " << desc.numInputChannels << " in, " << desc.numOutputChannels << " out");

                // Verify the plugin file exists
                juce::File pluginFile(desc.fileOrIdentifier);
                if (!pluginFile.exists())
                {
                    DBG("WARNING: Plugin file not found at: " << desc.fileOrIdentifier);
                    continue;
                }
                else
                {
                    DBG("Plugin file verified at: " << pluginFile.getFullPathName());
                }

                // Find the right format for this plugin
                juce::AudioPluginFormat* format = nullptr;
                DBG("Looking for format that matches: " << desc.pluginFormatName);

                for (int i = 0; i < formatManager.getNumFormats(); ++i)
                {
                    auto* f = formatManager.getFormat(i);
                    DBG("Checking format: " << f->getName());

                    if (desc.pluginFormatName == f->getName())
                    {
                        DBG("Found matching format: " << f->getName());
                        format = f;
                        break;
                    }
                }

                if (format != nullptr)
                {
                    auto instance = std::make_unique<PluginInstance>();

                    DBG("Creating plugin instance...");
                    juce::String error;
                    auto pluginInstance = format->createInstanceFromDescription(desc, sampleRate, bufferSize, error);

                    if (pluginInstance == nullptr)
                    {
                        DBG("Failed to create plugin instance: " << error);

                        // Try rescanning the plugin
                        DBG("Attempting to rescan plugin file...");
                        juce::OwnedArray<juce::PluginDescription> descriptions;
                        format->findAllTypesForFile(descriptions, desc.fileOrIdentifier);

                        if (descriptions.size() > 0)
                        {
                            DBG("Found " << descriptions.size() << " plugin descriptions from rescan");
                            pluginInstance = format->createInstanceFromDescription(*descriptions[0], sampleRate, bufferSize);
                            if (pluginInstance != nullptr)
                            {
                                DBG("Successfully created plugin instance from rescan");
                            }
                            else
                            {
                                DBG("Failed to create plugin instance even after rescan");
                            }
                        }
                        else
                        {
                            DBG("No plugin descriptions found during rescan");
                        }
                    }

                    if (pluginInstance != nullptr)
                    {
                        instance->processor = std::move(pluginInstance);
                        DBG("Plugin instance created successfully");

                        // Configure the plugin
                        DBG("Configuring plugin: " << desc.name);
                        instance->processor->setRateAndBufferSizeDetails(sampleRate, bufferSize);

                        // Enable buses
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

                        // Set bus layout
                        auto layout = instance->processor->getBusesLayout();
                        if (instance->processor->setBusesLayout(layout))
                        {
                            // Prepare the plugin
                            instance->processor->prepareToPlay(sampleRate, bufferSize);
                            DBG("Plugin prepared to play");

                            // Restore plugin's state
                            if (auto* stateXml = pluginXml->getChildByName("State"))
                            {
                                DBG("Found saved state data for plugin");
                                juce::MemoryBlock stateData;
                                stateData.fromBase64Encoding(stateXml->getStringAttribute("data"));
                                if (stateData.getSize() > 0)
                                {
                                    instance->processor->setStateInformation(stateData.getData(),
                                        (int)stateData.getSize());
                                    DBG("Restored plugin state (" << stateData.getSize() << " bytes)");
                                }
                                else
                                {
                                    DBG("Warning: State data was empty");
                                }
                            }

                            plugins.push_back(std::move(instance));
                            DBG("Successfully loaded plugin " << pluginIndex << ": " << desc.name);
                            pluginIndex++;
                        }
                        else
                        {
                            DBG("Failed to set plugin bus layout for: " << desc.name);
                        }
                    }
                    else
                    {
                        DBG("Failed to create plugin instance for: " << desc.name);
                    }
                }
                else
                {
                    DBG("Could not find format for plugin: " << desc.name <<
                        " (format name: " << desc.pluginFormatName << ")");
                }
            }
            else
            {
                DBG("No description found for plugin " << pluginName);
            }
        }
    }

    DBG("\nLoaded " << plugins.size() << " plugins");
    return true;
}