#pragma once
#include <Siv3D.hpp>

class StreamingReader
{
public:

	virtual ~StreamingReader() = default;

	virtual size_t size() const = 0;

	virtual size_t sampleRate() const = 0;

	virtual size_t lengthSample() const = 0;

	virtual void use() = 0;

	virtual void unuse() = 0;

	virtual void update() = 0;

	virtual WaveSample getSample(int64 index) const = 0;
};

class WaveReader : public StreamingReader
{
public:

	WaveReader(FilePathView path);

	virtual ~WaveReader() = default;

	size_t size() const override { return m_dataSize; }

	size_t sampleRate() const override { return m_sampleRate; }

	size_t lengthSample() const override { return m_lengthSample; }

	void use() override;

	void unuse() override;

	void update() override;

	WaveSample getSample(int64 index) const override;

private:

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
		if (FileSystem::Extension(path) == U"wav")
		{
			m_waveReaders.push_back(std::make_unique<WaveReader>(path));
		}
		else
		{
			Console << U"error 未対応の拡張子です\"" << path << U"\"";

			return std::numeric_limits<size_t>::max();
		}

		m_paths.emplace_back(path);

		return i;
	}

	void update();

	const StreamingReader& reader(size_t index) const
	{
		return *m_waveReaders[index];
	}

	StreamingReader& reader(size_t index)
	{
		return *m_waveReaders[index];
	}

private:

	AudioLoadManager() = default;

	Array<std::unique_ptr<StreamingReader>> m_waveReaders;
	Array<String> m_paths;
};
