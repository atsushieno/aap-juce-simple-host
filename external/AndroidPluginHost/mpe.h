#ifndef ANDROIDPLUGINHOST_MPE_H
#define ANDROIDPLUGINHOST_MPE_H

#include <juce_audio_basics/juce_audio_basics.h>

using namespace juce;

// TODO
class MPEDispatchingListener : public MPEInstrument::Listener
{
public:
    explicit MPEDispatchingListener(MidiMessageCollector& collector) : midiCollector(collector) {}

    void noteAdded(MPENote note) override {
        createNoteOnMessages(note);
    }

private:
    MidiMessageCollector& midiCollector;

    void createNoteOnMessages(const MPENote& note)
    {
        Array<MidiMessage> messages;

        int noteNumber = note.initialNote;
        int channel = note.midiChannel;
        auto pressure = note.pressure;
        float timbre = note.timbre.asSignedFloat();
        float pitchBend = note.pitchbend.asSignedFloat();

        // Add note on message
        MidiMessage noteOnMessage = MidiMessage::noteOn(channel, noteNumber, pressure.asUnsignedFloat());
        noteOnMessage.setTimeStamp(Time::getMillisecondCounterHiRes() * 0.001);
        midiCollector.addMessageToQueue(noteOnMessage);

        // Add channel pressure message
        MidiMessage channelPressureMessage = MidiMessage::channelPressureChange(channel, pressure.as7BitInt());
        channelPressureMessage.setTimeStamp(Time::getMillisecondCounterHiRes() * 0.001);
        midiCollector.addMessageToQueue(channelPressureMessage);

        // Add timbre message FIXME: find correct CC number
        MidiMessage timbreMessage = MidiMessage::controllerEvent(channel, 74, (int)timbre);
        timbreMessage.setTimeStamp(Time::getMillisecondCounterHiRes() * 0.001);
        midiCollector.addMessageToQueue(timbreMessage);

        // Add pitch bend message
        int pitchBendValue = (int)(pitchBend * 16383.0f);
        MidiMessage pitchBendMessage = MidiMessage::pitchWheel(channel, pitchBendValue);
        pitchBendMessage.setTimeStamp(Time::getMillisecondCounterHiRes() * 0.001);
        midiCollector.addMessageToQueue(pitchBendMessage);
    }

    void noteKeyStateChanged(MPENote changedNote) override
    {
        // Get the MIDI channel and note number
        int channel = changedNote.midiChannel;

        // Create pitch bend message
        float pitchBend = changedNote.pitchbend.asSignedFloat();
        int pitchBendValue = (int)(pitchBend * 16383.0f);
        MidiMessage pitchBendMessage = MidiMessage::pitchWheel(channel, pitchBendValue);
        pitchBendMessage.setTimeStamp(Time::getMillisecondCounterHiRes() * 0.001);

        // Create channel pressure message
        float pressure = changedNote.pressure.asSignedFloat();
        MidiMessage channelPressureMessage = MidiMessage::channelPressureChange(channel, pressure);
        channelPressureMessage.setTimeStamp(Time::getMillisecondCounterHiRes() * 0.001);

        midiCollector.addMessageToQueue(pitchBendMessage);
        midiCollector.addMessageToQueue(channelPressureMessage);
    }

    void noteReleased(MPENote releasedNote) override
    {
        // Get the MIDI channel and note number
        int channel = releasedNote.midiChannel;
        int noteNumber = releasedNote.initialNote;

        // Create note-off message
        MidiMessage noteOffMessage = MidiMessage::noteOff(channel, noteNumber, releasedNote.noteOffVelocity.asUnsignedFloat());
        noteOffMessage.setTimeStamp(Time::getMillisecondCounterHiRes() * 0.001);

        midiCollector.addMessageToQueue(noteOffMessage);
    }
};

#endif //ANDROIDPLUGINHOST_MPE_H
