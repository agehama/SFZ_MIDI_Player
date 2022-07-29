#pragma once
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

struct Envelope
{
	double attackTime = 0;
	double decayTime = 0;
	double sustainLevel = 1;
	double releaseTime = 0;

	// attackやdecayの途中でノートオフが来た場合、途中でフェードアウトすると鳴り方が変わってしまうのでdecayが終わるまで遅延させる
	double noteTime(double noteOnTime, double noteOffTime) const
	{
		const double duration = noteOffTime - noteOnTime;
		return Max(duration, attackTime + decayTime) + releaseTime;
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
};

struct AudioSource
{
	bool isValidVelocity(uint8 velocity) const
	{
		return lovel <= velocity && velocity <= hivel;
	}

	uint8 lovel;
	uint8 hivel;

	Wave wave;
	int32 tune;// 100 == 1 semitone
	Envelope envelope;
};

struct AudioKey
{
	int8 noteKey;
	Array<AudioSource> attackKeys;
	Array<AudioSource> releaseKeys;
	Array<NoteEvent> m_noteEvents;

	const NoteEvent& addEvent(uint8 velocity, int64 pressTimePos, int64 releaseTimePos);

	void clearEvent();

	int64 getAttackIndex(uint8 velocity) const;

	int64 getReleaseIndex(uint8 velocity) const;

	void debugPrint() const;

	void sortEvent();

	void deleteDuplicate();

	const static int64 BlendSampleCount = 100;

	void getSamples(float* left, float* right, int64 startPos, int64 sampleCount) const;

	void render(float* left, float* right, int64 startPos, int64 sampleCount, int64 noteIndex) const;

	void renderRelease(float* left, float* right, int64 startPos, int64 sampleCount, int64 noteIndex) const;

	int64 getWriteIndexHead(int64 startPos, int64 noteIndex) const;

	std::pair<int64, int64> readEmptyCount(int64 startPos, int64 sampleCount, int64 noteIndex) const;

	int64 getWriteIndexHeadRelease(int64 startPos, int64 noteIndex) const;

	std::pair<int64, int64> readEmptyCountRelease(int64 startPos, int64 sampleCount, int64 noteIndex) const;
};
