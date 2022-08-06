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
	m_waveReader(path)
{
	init();
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

			if (m_format.channels != 2)
			{
				Console << U"error: channels != 2";
				return;
			}
			if (m_format.bitsPerSample != 16)
			{
				Console << U"error: bitsPerSample != 16";
				return;
			}

			m_readFormat = true;

			if (m_dataPos != 0)
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
			m_dataPos = m_waveReader.getPos();
			m_dataSize = chunk.size;

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

	m_lengthSample = m_dataSize / m_format.blockAlign;
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
		m_readBuffer.resize(m_dataSize);
		if (m_waveReader.getPos() != m_dataPos)
		{
			m_waveReader.setPos(m_dataPos);
		}

		readBlock();

		m_use = true;
	}

	m_mutex.unlock();
}

void WaveLoader::unuse()
{
	m_use = false;
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
	else if(!m_readBuffer.empty())
	{
		m_readBuffer.clear();
		m_readBuffer.shrink_to_fit();
		m_waveReader.setPos(m_dataPos);
		m_loadSampleCount = 0;
	}

	m_mutex.unlock();
}

WaveSample WaveLoader::getSample(int64 index) const
{
	const auto& sample = m_readBuffer[index];

	return WaveSample(sample.left * m_normalize, sample.right * m_normalize);
}

void WaveLoader::readBlock()
{
	size_t readCount = 4096;
	if (m_dataSize <= m_loadSampleCount + readCount)
	{
		readCount = m_loadSampleCount - m_dataSize;
	}

	if (1 <= readCount)
	{
		m_waveReader.read(m_readBuffer.data() + m_loadSampleCount, readCount * m_format.blockAlign);
		m_loadSampleCount += readCount;
	}
}

std::mutex WaveLoader::m_mutex;
