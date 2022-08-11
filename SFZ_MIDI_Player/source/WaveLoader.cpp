#pragma once
#include <WaveLoader.hpp>

struct ChunkHead
{
	char id[4];
	uint32 size;
};

struct RiffChunk
{
	ChunkHead head;
	char format[4];
};

WaveLoader::WaveLoader(FilePathView path) :
	m_waveReader(path),
	m_filePath(path)
{
	init();
	m_waveReader.close();
}

void WaveLoader::init()
{
	RiffChunk riffChunk;
	m_waveReader.read(riffChunk);

	if (strncmp(riffChunk.head.id, "RIFF", 4) != 0)
	{
		Console << U"error: not riff format";
		return;
	}

	if (strncmp(riffChunk.format, "WAVE", 4) != 0)
	{
		Console << U"error: not wave format";
		return;
	}

	while (m_waveReader.getPos() < m_waveReader.size())
	{
		ChunkHead chunk;
		m_waveReader.read(chunk);

		if (strncmp(chunk.id, "fmt ", 4) == 0)
		{
			m_waveReader.read(m_format);

			if (m_format.channels != 1 && m_format.channels != 2)
			{
				Console << U"error: channels != 1 && channels != 2";
				return;
			}
			if (m_format.bitsPerSample != 8 && m_format.bitsPerSample != 16)
			{
				Console << U"error: bitsPerSample != 8 && bitsPerSample != 16";
				return;
			}

			m_readFormat = true;

			if (m_dataBeginPos != 0)
			{
				break;
			}
			else if (chunk.size < sizeof(WaveFileFormat))
			{
				m_waveReader.skip(chunk.size - sizeof(WaveFileFormat));
			}
		}
		else if (strncmp(chunk.id, "data", 4) == 0)
		{
			m_dataBeginPos = m_waveReader.getPos();
			m_dataSizeOfBytes = chunk.size;

			if (m_readFormat)
			{
				break;
			}
			else
			{
				m_waveReader.skip(chunk.size);
			}
		}
		else
		{
			m_waveReader.skip(chunk.size);
		}
	}

	m_lengthSample = m_dataSizeOfBytes / m_format.blockAlign;
	m_sampleRate = m_format.samplePerSecond;
	m_normalize = 1.f / 32768.0f;
}

void WaveLoader::use()
{
	m_unuseCount = 0;

	if (m_use)
	{
		return;
	}

	m_mutex.lock();

	{
		m_loadSampleCount = 0;
		m_readBuffer.resize(m_dataSizeOfBytes);

		if (!m_waveReader.isOpen())
		{
			m_waveReader.open(m_filePath);
		}

		if (m_waveReader.getPos() != m_dataBeginPos)
		{
			m_waveReader.setPos(m_dataBeginPos);
		}

		readBlock();

		m_use = true;
	}

	m_mutex.unlock();
}

void WaveLoader::unuse()
{
	m_use = false;
	m_waveReader.close();
}

void WaveLoader::update()
{
	m_mutex.lock();

	++m_unuseCount;
	if (m_use && 60 < m_unuseCount)
	{
		unuse();
	}

	if (m_use)
	{
		readBlock();
	}
	else if (!m_readBuffer.empty())
	{
		m_readBuffer.clear();
		m_readBuffer.shrink_to_fit();
		m_loadSampleCount = 0;
	}

	m_mutex.unlock();
}

struct Sample16bit2ch
{
	int16 left;
	int16 right;
};

struct Sample8bit2ch
{
	int8 left;
	int8 right;
};

WaveSample WaveLoader::getSample(int64 index) const
{
	if (m_format.channels == 1)
	{
		if (m_format.bitsPerSample == 8)
		{
			const auto& sample = m_readBuffer[index];
			return WaveSample(sample * m_normalize, sample * m_normalize);
		}
		else// if (m_format.bitsPerSample == 16)
		{
			auto buffer = std::bit_cast<int16*>(m_readBuffer.data());
			const auto& sample = buffer[index];
			return WaveSample(sample * m_normalize, sample * m_normalize);
		}
	}
	else
	{
		if (m_format.bitsPerSample == 8)
		{
			auto buffer = std::bit_cast<Sample8bit2ch*>(m_readBuffer.data());
			const auto& sample = buffer[index];
			return WaveSample(sample.left * m_normalize, sample.right * m_normalize);
		}
		else// if (m_format.bitsPerSample == 16)
		{
			auto buffer = std::bit_cast<Sample16bit2ch*>(m_readBuffer.data());
			const auto& sample = buffer[index];
			return WaveSample(sample.left * m_normalize, sample.right * m_normalize);
		}
	}
}

void WaveLoader::readBlock()
{
	size_t readCount = 1024;
	if (m_lengthSample <= m_loadSampleCount + readCount)
	{
		readCount = m_loadSampleCount - m_lengthSample;
	}

	if (1 <= readCount)
	{
		m_waveReader.read(m_readBuffer.data() + m_loadSampleCount * m_format.blockAlign, readCount * m_format.blockAlign);
		m_loadSampleCount += readCount;
	}
}

std::mutex WaveLoader::m_mutex;
