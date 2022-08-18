#pragma once
#include <Siv3D.hpp>
#include "SFZLoader.hpp"

struct KeyDownEvent
{
	int8 key;
	int64 pressTimePos;
	uint8 velocity;

	KeyDownEvent() = delete;
	KeyDownEvent(int8 key, int64 pressTimePos, uint8 velocity) :
		key(key),
		pressTimePos(pressTimePos),
		velocity(velocity)
	{}
};

struct NoteEvent
{
	int64 attackIndex;
	int64 releaseIndex;
	int64 pressTimePos;
	int64 releaseTimePos;
	uint8 velocity;

	Optional<int64> disableTimePos;

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

class AudioLoaderBase;

enum class OscillatorType
{
	Sine,
	Tri,
	Saw,
	Square,
	Noise,
	Silence,
};

// 1つのソース音源に対応
struct AudioSource
{
public:

	AudioSource(float amplitude, const Envelope& envelope, uint8 lovel, uint8 hivel, int32 tune):
		m_amplitude(amplitude),
		m_lovel(lovel),
		m_hivel(hivel),
		m_tune(tune),
		m_envelope(envelope)
	{}

	void setWaveIndex(size_t index)
	{
		m_index = index;
	}

	void setOscillator(OscillatorType oscillatorType, float frequency);

	void setSwitch(int8 swLokey, int8 swHikey, int8 swLast, int8 swDefault)
	{
		m_swLokey = swLokey;
		m_swHikey = swHikey;
		m_swLast = swLast;
		m_swDefault = swDefault;
	}

	void setGroup(uint32 group, uint32 offBy, float disableFadeSeconds)
	{
		m_group = group;
		m_offBy = offBy;
		m_disableFadeSeconds = disableFadeSeconds;
	}

	bool isValidVelocity(uint8 velocity) const
	{
		return m_lovel <= velocity && velocity <= m_hivel;
	}

	bool isValidSwLast(int64 pressTimePos, const Array<KeyDownEvent>& history) const;

	void setRtDecay(float rtDecay);

	size_t sampleRate() const;

	size_t lengthSample() const;

	WaveSample getSample(int64 index) const;

	const Envelope& envelope() const { return m_envelope; }

	void use();

	void unuse();

	bool isOscillator() const { return m_oscillatorType.has_value(); }

	uint32 group() const { return m_group; }
	uint32 offBy() const { return m_offBy; }
	float disableFadeSeconds() const { return m_disableFadeSeconds; }

private:

	Optional<OscillatorType> m_oscillatorType;
	float m_frequency;
	size_t m_index;

	const AudioLoaderBase& getReader() const;
	AudioLoaderBase& getReader();

	float m_amplitude;

	int32 m_tune;// 100 == 1 semitone
	Optional<float> m_rtDecay;

	uint8 m_lovel;
	uint8 m_hivel;
	Envelope m_envelope;

	int8 m_swLokey = 0;
	int8 m_swHikey = 0;
	int8 m_swLast = 0;
	int8 m_swDefault = 0;

	uint32 m_group = 0;
	uint32 m_offBy = 0;
	float m_disableFadeSeconds = 0;
};

// 1つのキーから鳴らされるAudioSourceをまとめたもの
class AudioKey
{
public:

	void init(int8 key);

	void addAttackKey(const AudioSource& source);

	void addReleaseKey(const AudioSource& source);

	bool hasAttackKey() const;

	const NoteEvent& addEvent(uint8 velocity, int64 pressTimePos, int64 releaseTimePos, const Array<KeyDownEvent>& history);

	void clearEvent();

	int64 getAttackIndex(uint8 velocity, int64 pressTimePos, const Array<KeyDownEvent>& history) const;

	const AudioSource& getAttackKey(int64 attackIndex) const;

	int64 getReleaseIndex(uint8 velocity) const;

	void debugPrint() const;

	void sortEvent();

	void deleteDuplicate();

	void getSamples(float* left, float* right, int64 startPos, int64 sampleCount);

	Array<NoteEvent>& noteEvents() { return m_noteEvents; }

private:

	const static int64 BlendSampleCount = 100;
	int8 noteKey;
	Array<AudioSource> attackKeys;
	Array<AudioSource> releaseKeys;
	Array<NoteEvent> m_noteEvents;

	void render(float* left, float* right, int64 startPos, int64 sampleCount, int64 noteIndex);

	void renderRelease(float* left, float* right, int64 startPos, int64 sampleCount, int64 noteIndex);

	int64 getWriteIndexHead(int64 startPos, int64 noteIndex) const;

	std::pair<int64, int64> readEmptyCount(int64 startPos, int64 sampleCount, int64 noteIndex) const;

	int64 getWriteIndexHeadRelease(int64 startPos, int64 noteIndex) const;

	int64 readCountRelease(int64 startPos, int64 sampleCount, int64 noteIndex) const;
};
