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

	void playRestart();

	void pause();

	int64 bufferBeginSample();

	int64 bufferEndSample();

	void update(PianoRoll& pianoroll, SamplePlayer& samplePlayer);

	void freeUntilSample(int64 sampleIndex);

	WaveSample getSample(int64 index) const;

private:

	AudioStreamRenderer();

	MemoryBlockList m_writeBlocks;

	int64 m_bufferBeginSample = 0;
	bool m_isFinished = false;
	bool m_isPlaying = false;
};
