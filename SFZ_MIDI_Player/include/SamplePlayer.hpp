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
struct Note;

struct KeyData
{
	uint8 key = 0;
	bool note_on = true;
};

enum class NoteGrade
{
	Undetermined,
	Miss,
	Bad,
	Good,
	Perfect,
};

struct KeyNoteState
{
	uint32 keyDownCount = 0;
	NoteGrade noteGrade = NoteGrade::Undetermined;
	Stopwatch keyEffectWatch;
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

	int octaveMin() const;
	int octaveMax() const;

	int keyMin() const { return m_keyMin; }
	int keyMax() const { return m_keyMax; }

	RectF getRect(int octaveIndex, int noteIndex, bool isWhiteKey) const;

	//RectF getHorizontalRect(int octaveIndex, int noteIndex, bool isWhiteKey) const;

	void drawVertical(const PianoRoll& pianoroll, const Optional<MidiData>& midiData) const;

	void drawHorizontal(const PianoRoll& pianoroll, const Optional<MidiData>& midiData, bool isDebug) const;

	Array<std::pair<uint8, NoteEvent>> loadMidiData(const MidiData& midiData);

	void getSamples(float* left, float* right, int64 startPos, int64 sampleCount);

	void updateInput();

	void setArea(const Rect& area);

	double stretch() const { return m_stretch; }

	double blackHeight() const { return m_blackHeight; }

	void finish()
	{
		if (m_midiin)
		{
			m_midiin->close_port();
		}
	}

	void setReady(bool isReady)
	{
		m_isReady = isReady;
	}

	bool isReady() const
	{
		return m_isReady;
	}

	void reload()
	{
		m_keyData.clear();

		m_keyState = Array<KeyNoteState>(128);
		m_noteGrade.clear();
	}

private:

	Program* refProgram(const TrackData& trackData);

	Array<Program> m_soundSet;
	Array<Program> m_drumKit;

	Array<uint8> m_programChangeNumberToSoundSetIndex;

	RectF m_area;

	Font m_font = Font(12);

	int m_keyMin = 36;
	int m_keyMax = 96;
	double m_stretch = -0.11;
	double m_blackHeight = 0.63667;

	size_t m_activeTrack = 1;

	libremidi::observer m_observer;
	std::unique_ptr<libremidi::midi_in> m_midiin;
	std::mutex m_mutex;
	std::deque<KeyData> m_keyData;

	mutable Array<KeyNoteState> m_keyState = Array<KeyNoteState>(128);
	mutable HashTable<const Note*, NoteGrade> m_noteGrade;
	Font m_gradeFont = Font(24, Typeface::Black);
	bool m_isReady = false;
};
