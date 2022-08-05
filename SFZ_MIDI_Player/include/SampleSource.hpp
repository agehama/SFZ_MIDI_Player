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

struct WaveFileFormat
{
	uint16 audioFormat;
	uint16 channels;
	uint32 samplePerSecond;
	uint32 bytesPerSecond;
	uint16 blockAlign;
	uint16 bitsPerSample;
};

struct Sample16bit2ch
{
	int16 left;
	int16 right;
};

class WaveReader
{
public:

	WaveReader(FilePathView path) :
		m_waveReader(path)
	{
		init();
	}

	WaveReader(const WaveReader&) = default;

	size_t size() const { return m_dataSize; }

	size_t sampleRate() const { return m_sampleRate; }

	size_t lengthSample() const { return m_lengthSample; }

	void use();

	void unuse();

	void update();

	WaveSample getSample(int64 index) const;

private:

	void init();

	void readBlock();

	BinaryReader m_waveReader;

	WaveFileFormat m_format = {};
	int64 m_dataPos = 0;
	size_t m_dataSize = 0;
	size_t m_sampleRate = 0;
	size_t m_lengthSample = 0;
	bool m_readFormat = false;
	float m_normalize = 0;

	uint32 m_unuseCount = 0;
	bool m_use = false;
	size_t m_loadSampleCount = 0;
	Array<Sample16bit2ch> m_readBuffer;
	static std::mutex m_mutex;
};

class AudioLoadManager
{
public:

	static AudioLoadManager& i()
	{
		static AudioLoadManager obj;
		return obj;
	}

	size_t load(FilePathView path)
	{
		for (auto [i, wavePath]: Indexed(m_paths))
		{
			if (wavePath == path)
			{
				return i;
			}
		}

		const auto i = m_paths.size();
		m_paths.emplace_back(path);
		m_waveReaders.emplace_back(path);

		return i;
	}

	void update();

	const WaveReader& reader(size_t index) const
	{
		return m_waveReaders[index];
	}

	WaveReader& reader(size_t index)
	{
		return m_waveReaders[index];
	}

private:

	AudioLoadManager() = default;

	Array<WaveReader> m_waveReaders;
	Array<String> m_paths;
};

// 1つのソース音源に対応
struct AudioSource
{
public:

	AudioSource(size_t waveIndex, float amplitude, const Envelope& envelope, uint8 lovel, uint8 hivel, int32 tune):
		m_index(waveIndex),
		m_amplitude(amplitude),
		m_lovel(lovel),
		m_hivel(hivel),
		m_tune(tune),
		m_envelope(envelope)
	{
	}

	bool isValidVelocity(uint8 velocity) const
	{
		return m_lovel <= velocity && velocity <= m_hivel;
	}

	void setRtDecay(float rtDecay);

	size_t sampleRate() const;

	size_t lengthSample() const;

	WaveSample getSample(int64 index) const;

	const Envelope& envelope() const { return m_envelope; }

	void use();

	void unuse();

private:

	size_t m_index;

	const WaveReader& getReader() const { return AudioLoadManager::i().reader(m_index); }
	WaveReader& getReader() { return AudioLoadManager::i().reader(m_index); }

	float m_amplitude;

	int32 m_tune;// 100 == 1 semitone
	Optional<float> m_rtDecay;

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

	void getSamples(float* left, float* right, int64 startPos, int64 sampleCount);

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

	std::pair<int64, int64> readEmptyCountRelease(int64 startPos, int64 sampleCount, int64 noteIndex) const;
};
