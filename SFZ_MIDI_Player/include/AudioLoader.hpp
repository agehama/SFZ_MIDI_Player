#pragma once
#include <Siv3D.hpp>

struct WaveFileFormat
{
	uint16 audioFormat;
	uint16 channels;
	uint32 samplePerSecond;
	uint32 bytesPerSecond;
	uint16 blockAlign;
	uint16 bitsPerSample;
};

class WaveReader
{
public:

	WaveReader(FilePathView path);

	size_t size() const { return m_dataSize; }

	size_t sampleRate() const { return m_sampleRate; }

	size_t lengthSample() const { return m_lengthSample; }

	void use();

	void unuse();

	void update();

	WaveSample getSample(int64 index) const;

private:

	struct Sample16bit2ch
	{
		int16 left;
		int16 right;
	};

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
