﻿#pragma once
#include <Siv3D.hpp>

struct NoteEvent
{
	int64 attackIndex;
	int64 releaseIndex;
	int64 pressTimePos;
	int64 releaseTimePos;
	uint8 velocity;

	NoteEvent() = delete;
	NoteEvent(int64 attackIndex, int64 releaseIndex, int64 pressTimePos, int64 releaseTimePos, uint8 velocity) :
		attackIndex(attackIndex),
		releaseIndex(releaseIndex),
		pressTimePos(pressTimePos),
		releaseTimePos(releaseTimePos),
		velocity(velocity)
	{}
};

class Envelope
{
public:

	Envelope(double attackTime, double decayTime, double sustainLevel, double releaseTime):
		m_attackTime(attackTime),
		m_decayTime(decayTime),
		m_sustainLevel(sustainLevel),
		m_releaseTime(releaseTime)
	{}

	// attackやdecayの途中でノートオフが来た場合、途中でフェードアウトすると鳴り方が変わってしまうのでdecayが終わるまで遅延させる
	double noteTime(double noteOnTime, double noteOffTime) const
	{
		const double duration = noteOffTime - noteOnTime;
		return Max(duration, m_attackTime + m_decayTime) + m_releaseTime;
	}

	double noteTime(const NoteEvent& noteEvent) const
	{
		return noteTime(1.0 * noteEvent.pressTimePos / Wave::DefaultSampleRate, 1.0 * noteEvent.releaseTimePos / Wave::DefaultSampleRate);
	}

	double level(double noteOnTime, double noteOffTime, double time) const;

	double level(const NoteEvent& noteEvent, double time) const
	{
		return level(1.0 * noteEvent.pressTimePos / Wave::DefaultSampleRate, 1.0 * noteEvent.releaseTimePos / Wave::DefaultSampleRate, time);
	}

private:

	double m_attackTime = 0;
	double m_decayTime = 0;
	double m_sustainLevel = 1;
	double m_releaseTime = 0;
};

// 1つのソース音源に対応
struct AudioSource
{
public:

	AudioSource(const Wave& wave, const Envelope& envelope, uint8 lovel, uint8 hivel, int32 tune):
		m_lovel(lovel),
		m_hivel(hivel),
		m_tune(tune),
		m_envelope(envelope)
	{
		initTuneWave(wave);
	}

	bool isValidVelocity(uint8 velocity) const
	{
		return m_lovel <= velocity && velocity <= m_hivel;
	}

	void setRtDecay(float rtDecay);

	const Wave& wave() const { return m_wave; }

	const Envelope& envelope() const { return m_envelope; }

private:

	void initTuneWave(const Wave& originalWave);

	int32 m_tune;// 100 == 1 semitone
	float m_rtDecay = 0;

	Wave m_wave;
	uint8 m_lovel;
	uint8 m_hivel;
	Envelope m_envelope;
};

// 1つのキーから鳴らされるAudioSourceをまとめたもの
class AudioKey
{
public:

	void init(int8 key);

	void addAttackKey(const AudioSource& source);

	void addReleaseKey(const AudioSource& source);

	bool hasAttackKey() const;

	const NoteEvent& addEvent(uint8 velocity, int64 pressTimePos, int64 releaseTimePos);

	void clearEvent();

	int64 getAttackIndex(uint8 velocity) const;

	int64 getReleaseIndex(uint8 velocity) const;

	void debugPrint() const;

	void sortEvent();

	void deleteDuplicate();

	void getSamples(float* left, float* right, int64 startPos, int64 sampleCount) const;

private:

	const static int64 BlendSampleCount = 100;
	int8 noteKey;
	Array<AudioSource> attackKeys;
	Array<AudioSource> releaseKeys;
	Array<NoteEvent> m_noteEvents;

	void render(float* left, float* right, int64 startPos, int64 sampleCount, int64 noteIndex) const;

	void renderRelease(float* left, float* right, int64 startPos, int64 sampleCount, int64 noteIndex) const;

	int64 getWriteIndexHead(int64 startPos, int64 noteIndex) const;

	std::pair<int64, int64> readEmptyCount(int64 startPos, int64 sampleCount, int64 noteIndex) const;

	int64 getWriteIndexHeadRelease(int64 startPos, int64 noteIndex) const;

	std::pair<int64, int64> readEmptyCountRelease(int64 startPos, int64 sampleCount, int64 noteIndex) const;
};
