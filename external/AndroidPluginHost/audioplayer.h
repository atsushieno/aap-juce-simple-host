//
// Created by Atsushi Eno on 2023/03/06.
//

#ifndef ANDROIDPLUGINHOST_AUDIOPLAYER_H
#define ANDROIDPLUGINHOST_AUDIOPLAYER_H

#include <juce_audio_processors/juce_audio_processors.h>

using namespace juce;

class AudioFilePlayerProcessor : public AudioProcessor
{
    AudioTransportSource transportSource;
    std::unique_ptr<AudioFormatReaderSource> readerSource;
    AudioParameterFloat* playbackSpeed;

public:
    AudioFilePlayerProcessor() {}
    AudioTransportSource& getTransportSource() { return transportSource; }

    void releaseResources() override { transportSource.releaseResources(); }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        transportSource.prepareToPlay(samplesPerBlock, sampleRate);
    }
    void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiBuffer) override {
        transportSource.getNextAudioBlock(AudioSourceChannelInfo{buffer});
    }
    AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const String getName() const override { return "AudioFilePlayerProcessor"; }
    void getStateInformation(juce::MemoryBlock& destData) override {}
    void setStateInformation(const void* data, int sizeInBytes) override {}
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override {}
    const String getProgramName(int index) override { return String(); }
    void changeProgramName(int index, const String &newName) override {}
};

#endif //ANDROIDPLUGINHOST_AUDIOPLAYER_H
