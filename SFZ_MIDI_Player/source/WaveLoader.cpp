﻿#pragma once
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

WaveLoader::WaveLoader(FilePathView path, size_t debugId) :
	m_waveReader(path),
	m_filePath(path),
	m_readBlocks(debugId)
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
	m_normalize = 1.f / 32767.0f;
}

void WaveLoader::use(size_t beginSampleIndex, size_t sampleCount)
{
	//m_unuseCount = 0;

	//if (m_use)
	//{
	//	return;
	//}

	//m_mutex.lock();

	{
		//m_loadSampleCount = 0;

		if (!m_waveReader.isOpen())
		{
			m_waveReader.open(m_filePath);
		}

		//if (m_waveReader.getPos() != m_dataBeginPos)
		//{
		//	m_waveReader.setPos(m_dataBeginPos);
		//}

		readBlock(beginSampleIndex, sampleCount);

		m_use = true;
	}

	//m_mutex.unlock();
}

void WaveLoader::unuse()
{
	//m_use = false;
	//m_waveReader.close();
}

void WaveLoader::update()
{
	++m_unuseCount;
	if (m_use && 5 < m_unuseCount)
	{
		//unuse();
	}

	if (m_use)
	{
		//readBlock();
	}
	//else
	//{
	//	m_readBlocks.deallocate();
	//	m_loadSampleCount = 0;
	//}
}

WaveSample WaveLoader::getSample(int64 index) const
{
	if (m_format.channels == 1)
	{
		if (m_format.bitsPerSample == 8)
		{
			auto [ptr, actualReadBytes] = m_readBlocks.getWriteBuffer(index * m_format.blockAlign, sizeof(int8));
			const auto pSample = std::bit_cast<int8*>(ptr);
			return WaveSample(*pSample * m_normalize, *pSample * m_normalize);
		}
		else// if (m_format.bitsPerSample == 16)
		{
			auto [ptr, actualReadBytes] = m_readBlocks.getWriteBuffer(index * m_format.blockAlign, sizeof(int16));
			const auto pSample = std::bit_cast<int16*>(ptr);
			return WaveSample(*pSample * m_normalize, *pSample * m_normalize);
		}
	}
	else
	{
		if (m_format.bitsPerSample == 8)
		{
			auto [ptr, actualReadBytes] = m_readBlocks.getWriteBuffer(index * m_format.blockAlign, sizeof(Sample8bit2ch));
			const auto pSample = std::bit_cast<Sample8bit2ch*>(ptr);
			return WaveSample(pSample->left * m_normalize, pSample->right * m_normalize);
		}
		else// if (m_format.bitsPerSample == 16)
		{
			auto [ptr, actualReadBytes] = m_readBlocks.getWriteBuffer(index * m_format.blockAlign, sizeof(Sample16bit2ch));
			const auto pSample = std::bit_cast<Sample16bit2ch*>(ptr);
			return WaveSample(pSample->left * m_normalize, pSample->right * m_normalize);
		}
	}
}

void WaveLoader::readBlock(size_t beginSample, size_t sampleCount)
{
	size_t readCount = sampleCount;

	if (m_lengthSample <= beginSample)
	{
		readCount = 0;
	}
	else if (m_lengthSample <= beginSample + readCount)
	{
		readCount = m_lengthSample - beginSample;
	}

	if (1 <= readCount)
	{
		size_t readHead = beginSample * m_format.blockAlign;
		size_t requiredReadBytes = readCount * m_format.blockAlign;

		{
			const size_t allocateBegin = (readHead / MemoryPool::UnitBlockSizeOfBytes) * MemoryPool::UnitBlockSizeOfBytes;
			const size_t allocateEnd = readHead + requiredReadBytes;

			{
				// todo: 判定が雑
				const auto [beginBlock, endBlock] = m_readBlocks.blockIndexRange(allocateBegin, allocateEnd - allocateBegin);
				if (m_readBlocks.isAllocatedBlock(beginBlock) && m_readBlocks.isAllocatedBlock(endBlock))
				{
					return;
				}
			}

			auto blockIndices = m_readBlocks.blockIndices(allocateBegin, allocateEnd - allocateBegin);

			for (const auto& [blockIndex, allocated] : blockIndices)
			{
				if (!allocated)
				{
					const auto currentReadPos = static_cast<int64>(blockIndex * MemoryPool::UnitBlockSizeOfBytes);
					m_readBlocks.allocateSingleBlock(blockIndex);

					auto ptr = m_readBlocks.getBlock(blockIndex);

					if (m_waveReader.getPos() != m_dataBeginPos + currentReadPos)
					{
						m_waveReader.setPos(m_dataBeginPos + currentReadPos);
					}

					const auto readBytes = Min(MemoryPool::UnitBlockSizeOfBytes, m_dataSizeOfBytes - currentReadPos);
					m_waveReader.read(ptr, readBytes);
				}
			}
		}
	}
}
