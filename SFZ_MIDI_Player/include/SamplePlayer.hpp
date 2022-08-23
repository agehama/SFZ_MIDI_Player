#pragma once
#include <Siv3D.hpp>

struct SfzData;
class PianoRoll;
class MidiData;
struct KeyDownEvent;
struct NoteEvent;
class AudioKey;

class SamplePlayer
{
public:

	SamplePlayer() = default;

	SamplePlayer(const Rect& area) :
		m_area(area)
	{}

	void loadData(const SfzData& sfzData);

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

	const NoteEvent& addEvent(uint8 key, uint8 velocity, int64 pressTimePos, int64 releaseTimePos, const Array<KeyDownEvent>& history);

	void sortEvent();

	void deleteDuplicate();

	void getSamples(float* left, float* right, int64 startPos, int64 sampleCount);

	void clearEvent();

	Array<std::pair<uint8, NoteEvent>> addEvents(const MidiData& midiData);

private:

	Array<AudioKey> m_audioKeys;

	RectF m_area;

	Font m_font = Font(12);

	// [-1, 9]
	int m_octaveMin = 1;
	int m_octaveMax = 7;
};

class SamplerAudioStream : public IAudioStream
{
public:

	SamplerAudioStream(PianoRoll& pianoroll, SamplePlayer& samplePlayer) :
		m_pianoroll(pianoroll),
		m_samplePlayer(samplePlayer)
	{
	}

	void restart()
	{
		m_pos = 0;
	}

	std::reference_wrapper<PianoRoll> m_pianoroll;

	std::reference_wrapper<SamplePlayer> m_samplePlayer;

	std::atomic<size_t> m_pos = 0;

	static double time1;
	static double time2;
	static double time3;
	static double time4;
	double volume = 1;

private:

	void getAudio(float* left, float* right, const size_t samplesToWrite) override;

	bool hasEnded() override
	{
		return false;
	}

	void rewind() override
	{
		m_pos = 0;
	}

	Wave m_wave;
};

class AudioRenderer
{
public:

	AudioRenderer(PianoRoll& pianoroll, SamplePlayer& samplePlayer) :
		m_pianoroll(pianoroll),
		m_samplePlayer(samplePlayer)
	{
	}

	std::reference_wrapper<PianoRoll> m_pianoroll;

	std::reference_wrapper<SamplePlayer> m_samplePlayer;

	size_t m_pos = 0;

	void getAudio(float* left, float* right, int64 startPos, int64 sampleCount);
};
