#pragma once
#include <Siv3D.hpp>
#include "AudioLoaderBase.hpp"
#include "MemoryBlockList.hpp"

class WaveLoader : public AudioLoaderBase
{
public:

	WaveLoader(FilePathView path, size_t debugId);

	virtual ~WaveLoader() = default;

	size_t size() const override { return m_dataSizeOfBytes; }

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

	void init();

	void readBlock();

	BinaryReader m_waveReader;
	FilePath m_filePath;

	WaveFileFormat m_format = {};
	int64 m_dataBeginPos = 0;
	size_t m_dataSizeOfBytes = 0;
	size_t m_sampleRate = 0;
	size_t m_lengthSample = 0;
	size_t m_bytesPerSample = 0;
	bool m_readFormat = false;
	float m_normalize = 0;

	uint32 m_unuseCount = 0;
	bool m_use = false;
	size_t m_loadSampleCount = 0;
	MemoryBlockList m_readBlocks;

	static std::mutex m_mutex;
};
