#pragma once
#include <JuceHeader.h>

// A simple AudioSource that stores incoming audio in a lock-free FIFO.
// We'll read from it on the monitor device side.
class MonitorAudioSource : public juce::AudioSource
{
public:
    MonitorAudioSource()
        : fifo (32768)  // e.g. 32k samples capacity
    {
        // For safety, 2 channels of stereo, 32768 samples
        buffer.setSize (2, 32768);
    }

    // Writer: called by the main device callback
    void writeToFifo (const float* const* data, int numChans, int numSamples)
    {
        int start1, size1, start2, size2;
        fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

        // First block
        if (size1 > 0)
        {
            for (int c = 0; c < 2; ++c)
            {
                if (c < numChans && data[c] != nullptr)
                    buffer.copyFrom (c, start1, data[c], size1);
                else
                    buffer.clear (c, start1, size1);
            }
        }
        // Second block (if wraps around)
        if (size2 > 0)
        {
            for (int c = 0; c < 2; ++c)
            {
                if (c < numChans && data[c] != nullptr)
                    buffer.copyFrom (c, start2, data[c], size2);
                else
                    buffer.clear (c, start2, size2);
            }
        }

        fifo.finishedWrite (size1 + size2);
    }

    //==============================================================================
    // AudioSource overrides
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        juce::ignoreUnused (samplesPerBlockExpected, sampleRate);
        fifo.reset();
    }

    void releaseResources() override
    {
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        int numSamples = bufferToFill.numSamples;
        int start1, size1, start2, size2;

        fifo.prepareToRead (numSamples, start1, size1, start2, size2);

        if (size1 > 0)
        {
            for (int c = 0; c < bufferToFill.buffer->getNumChannels(); ++c)
            {
                bufferToFill.buffer->copyFrom (c,
                                               bufferToFill.startSample,
                                               buffer,
                                               c,
                                               start1,
                                               size1);
            }
        }

        if (size2 > 0)
        {
            for (int c = 0; c < bufferToFill.buffer->getNumChannels(); ++c)
            {
                bufferToFill.buffer->copyFrom (c,
                                               bufferToFill.startSample + size1,
                                               buffer,
                                               c,
                                               start2,
                                               size2);
            }
        }

        fifo.finishedRead (size1 + size2);
    }

private:
    juce::AudioSampleBuffer buffer;
    juce::AbstractFifo fifo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonitorAudioSource)
};
