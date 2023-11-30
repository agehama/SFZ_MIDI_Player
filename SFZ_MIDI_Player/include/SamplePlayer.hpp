#pragma once
#include <Siv3D.hpp>

struct SfzData;
class PianoRoll;
class TrackData;
class MidiData;
struct NoteEvent;
class AudioKey;
class Program;

class SamplePlayer
{
public:

	SamplePlayer() = default;

	SamplePlayer(const Rect& area) :
		m_area(area)
	{}

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

	Array<NoteEvent> loadMidiData(const MidiData& midiData);

	void getSamples(float* left, float* right, int64 startPos, int64 sampleCount);

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
};
