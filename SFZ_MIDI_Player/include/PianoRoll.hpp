#pragma once
#include <Siv3D.hpp>

class MidiData;

class PianoRoll
{
public:

	PianoRoll() = default;

	PianoRoll(const Rect& area) :
		m_area(area)
	{}

	void drawVertical(int keyMin, int keyMax, const Optional<MidiData>& midiData) const;

	void drawHorizontal(int keyMin, int keyMax, const Optional<MidiData>& midiData) const;

	void updateTick(const MidiData& midiData);

	void playRestart();

	void pause();

	void resume();

	void progress(size_t sampleCount);

	bool isPlaying() const { return mIsPlaying; }

	double currentTick() const { return m_currentTick; }

	double currentSeconds() const { return m_watch.sF(); }

private:
	std::atomic<bool> mIsPlaying = false;
	std::atomic<double> m_currentTime = 0.0f;

	Font m_font = Font(24);

	RectF m_area;
	double m_drawScale = 0.4;

	double m_lastTick = 0;
	double m_currentTick = 0;
	Stopwatch m_watch;

	double m_unitLength = 8;
	double m_interval = 8;
};
