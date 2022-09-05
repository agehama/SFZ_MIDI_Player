#pragma once
#include <Siv3D.hpp>

struct SfzData;
class PianoRoll;
class TrackData;
class MidiData;
struct KeyDownEvent;
struct NoteEvent;
class AudioKey;

class Program
{
public:

	Program() = default;

	void loadProgram(const SfzData& sfzData);

	void clearEvent();

	void addKeyDownEvents(const MidiData& midiData, const TrackData& trackData);

	void sortKeyDownEvents();

	Array<std::pair<uint8, NoteEvent>> addEvents(const MidiData& midiData, const TrackData& trackData);

	void sortEvent();

	void deleteDuplicate();

	void calculateOffTime();

	void getSamples(float* left, float* right, int64 startPos, int64 sampleCount);

private:

	const NoteEvent& addEvent(uint8 key, uint8 velocity, int64 pressTimePos, int64 releaseTimePos, const Array<KeyDownEvent>& history);

	Optional<std::pair<uint32, uint32>> GetRangeEventIndex(const Array<KeyDownEvent>& keyDownEvents, int64 rangeBegin, int64 rangeEnd) const;

	Array<AudioKey> m_audioKeys;

	Array<KeyDownEvent> m_keyDownEvents;
};
