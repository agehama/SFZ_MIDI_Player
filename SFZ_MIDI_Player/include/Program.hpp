#pragma once
#include <Siv3D.hpp>

struct SfzData;
class PianoRoll;
class TrackData;
class MidiData;
struct NoteEvent;
class AudioKey;

class Program
{
public:

	Program() = default;

	void loadProgram(const SfzData& sfzData, float volume);

	void clearEvent();

	//void addKeyDownEvents(const MidiData& midiData, const TrackData& trackData);

	//void sortKeyDownEvents();

	const Array<NoteEvent>& addEvents(const MidiData& midiData, const TrackData& trackData);

	void sortEvent();

	void resolveKeyIndex();

	void deleteDuplicate();

	void calculateOffTime();

	void setEachKeyEvent();

	void getSamples(float* left, float* right, int64 startPos, int64 sampleCount);

private:

	//const NoteEvent& addEvent(uint8 key, uint8 velocity, int64 pressTimePos, int64 releaseTimePos, const Array<KeyDownEvent>& history);

	Optional<std::pair<uint32, uint32>> pressEventIndexRange(int64 rangeBegin, int64 rangeEnd) const;

	Optional<std::pair<uint32, uint32>> bothEventIndexRange(int64 rangeBegin, int64 rangeEnd) const;

	Array<NoteEvent> m_noteEvents;

	Array<AudioKey> m_audioKeys;

	//Array<KeyDownEvent> m_keyDownEvents;
};
