#pragma once
#include <Siv3D.hpp>
#include <libremidi/libremidi.hpp>

struct SfzData;
class PianoRoll;
class TrackData;
class MidiData;
struct KeyDownEvent;
struct NoteEvent;
class AudioKey;
class Program;

struct KeyData
{
	uint8 key = 0;
	bool note_on = true;
};

class SamplePlayer
{
public:

	SamplePlayer() = default;

	SamplePlayer(const Rect& area);

	void loadSoundSet(FilePathView soundSetTomlPath);

	int octaveCount() const;
	double octaveHeight() const;
	double unitHeight() const;

	double octaveWidth() const;
	double unitWidth() const;

	int keyMin() const;
	int keyMax() const;

	RectF getRect(int octaveIndex, int noteIndex, bool isWhiteKey) const;

	RectF getHorizontalRect(int octaveIndex, int noteIndex, bool isWhiteKey) const;

	void drawVertical(const PianoRoll& pianoroll, const Optional<MidiData>& midiData) const;

	void drawHorizontal(const PianoRoll& pianoroll, const Optional<MidiData>& midiData) const;

	Array<std::pair<uint8, NoteEvent>> loadMidiData(const MidiData& midiData);

	void getSamples(float* left, float* right, int64 startPos, int64 sampleCount);

	void updateInput();

private:

	Program* refProgram(const TrackData& trackData);

	Array<Program> m_soundSet;
	Array<Program> m_drumKit;

	Array<uint8> m_programChangeNumberToSoundSetIndex;

	RectF m_area;

	Font m_font = Font(12);

	// [-1, 9]
	int m_octaveMin = 1;
	int m_octaveMax = 7;

	libremidi::observer m_observer;
	std::unique_ptr<libremidi::midi_in> m_midiin;
	std::mutex m_mutex;
	std::deque<KeyData> m_key_data;
	Array<uint8> m_key_on = Array<uint8>{ 128 };
};
