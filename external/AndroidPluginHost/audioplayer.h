#ifndef ANDROIDPLUGINHOST_AUDIOPLAYER_H
#define ANDROIDPLUGINHOST_AUDIOPLAYER_H

#include <juce_audio_processors/juce_audio_processors.h>
#include "sample_wav.h"

using namespace juce;

class AudioFilePlayerProcessor : public AudioProcessor
{
    AudioTransportSource transportSource{};

public:
    AudioFilePlayerProcessor()
    : AudioProcessor(BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                             .withOutput ("Output", juce::AudioChannelSet::stereo(), true)) {
    }
    ~AudioFilePlayerProcessor() {
        transportSource.stop();
        transportSource.releaseResources();
    }

    AudioTransportSource& getTransportSource() { return transportSource; }

    void playLoadedFile() {
        AudioFormatManager audioFormatManager;
        WavAudioFormat format;
        auto stream = new MemoryInputStream(resources_sample_wav, resources_sample_wav_len, false);
        auto audioFormatReader = format.createReaderFor(stream, true);
        auto audioFormatReaderSource = new AudioFormatReaderSource(audioFormatReader, true);
        transportSource.setSource(audioFormatReaderSource);
        transportSource.start();
    }

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
