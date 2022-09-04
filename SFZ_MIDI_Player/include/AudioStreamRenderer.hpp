#pragma once
#include <Siv3D.hpp>
#include "MemoryBlockList.hpp"

class PianoRoll;
class SamplePlayer;

class AudioStreamRenderer
{
public:

	static AudioStreamRenderer& i()
	{
		static AudioStreamRenderer obj;
		return obj;
	}

	void finish();

	bool isFinish() const;

	bool isPlaying() const;

	void clearBuffer();

	void playRestart();

	void pause();

	int64 bufferBeginSample();

	int64 bufferEndSample();

	void update(SamplePlayer& samplePlayer);

	void freePastSample(int64 sampleIndex);

	WaveSample getSample(int64 index) const;

	void lock() const;

	void unlock() const;


private:

	AudioStreamRenderer();

	MemoryBlockList m_writeBlocks;

	int64 m_bufferBeginSample = 0;
	bool m_isFinished = false;
	bool m_isPlaying = false;

	static std::mutex m_mutex;
};

class SamplerAudioStream : public IAudioStream
{
public:

	SamplerAudioStream(PianoRoll& pianoroll, SamplePlayer& samplePlayer) :
		m_pianoroll(pianoroll),
		m_samplePlayer(samplePlayer)
	{
	}

	void reset()
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
	float volume = 1;

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
