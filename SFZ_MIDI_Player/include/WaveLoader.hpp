#pragma once
#include <Siv3D.hpp>
#include "AudioLoaderBase.hpp"

class WaveLoader : public AudioLoaderBase
{
public:

	WaveLoader(FilePathView path);

	virtual ~WaveLoader() = default;

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
