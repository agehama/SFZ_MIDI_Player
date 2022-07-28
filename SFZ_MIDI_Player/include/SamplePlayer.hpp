#pragma once
#include <Siv3D.hpp>

Wave SetSpeedWave(const Wave& original, int semitone);

Wave SetTuneWave(const Wave& original, int32 tune);

void SetVolume(Wave& wave, double volume);

void SetRtDecay(Wave& wave, double rt_decay);

class SfzData;
class PianoRoll;
class MidiData;
class NoteEvent;
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

	void drawVertical2(const PianoRoll& pianoroll, const MidiData& midiData) const;

	void drawHorizontal(const PianoRoll& pianoroll, const MidiData& midiData) const;

	const NoteEvent& addEvent(uint8 key, uint8 velocity, int64 pressTimePos, int64 releaseTimePos);

	void sortEvent();

	void deleteDuplicate();

	void getSamples3(float* left, float* right, int64 startPos, int64 sampleCount) const;

	void clearEvent();

	Array<NoteEvent> addEvents(const MidiData& midiData);

private:

	Optional<int> m_pressedKey;

	Array<AudioKey> m_audioKeys;

	RectF m_area;

	Font m_font = Font(12);

	//Envelope m_envelope;

	// [-1, 9]
	/*int m_octaveMin = 3;
	int m_octaveMax = 6;*/
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
