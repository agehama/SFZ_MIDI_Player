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

WaveLoader::WaveLoader(FilePathView path, size_t debugId) :
	m_waveReader(path),
	m_filePath(path),
	m_readBlocks(debugId, MemoryPool::ReadFile)
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
	m_sampleRateInv = 1.f / m_sampleRate;
	m_normalize = 1.f / 32767.0f;
}

void WaveLoader::use(size_t beginSampleIndex, size_t sampleCount)
{
	if (!m_waveReader.isOpen())
	{
		m_waveReader.open(m_filePath);
	}

	readBlock(beginSampleIndex, sampleCount);
}

void WaveLoader::markUnused()
{
	m_readBlocks.markUnused();
	m_indexCache.clear();
}

void WaveLoader::freeUnusedBlocks()
{
	m_readBlocks.freeUnusedBlocks();
}

WaveSample WaveLoader::getSample(int64 index) const
{
	for (const auto& cache : m_indexCache)
	{
		const auto relativeIndex = index - cache.sampleIndexBegin;
		if (0 <= relativeIndex && relativeIndex < MemoryPool::UnitBlockSampleLength)
		{
			const auto pSample = std::bit_cast<Sample16bit2ch*>(cache.ptr + relativeIndex * m_format.blockAlign);
			return WaveSample(pSample->left * m_normalize, pSample->right * m_normalize);
		}
	}

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
			//*
			const auto blockIndex = static_cast<uint32>((index * m_format.blockAlign / MemoryPool::UnitBlockSizeOfBytes));
			const auto beginSampleIndex = blockIndex * MemoryPool::UnitBlockSizeOfBytes / m_format.blockAlign;

			auto ptr = m_readBlocks.getBlock(blockIndex);

			m_indexCache.push_back(BlockIndexCache{ beginSampleIndex, ptr });

			const auto pSample = std::bit_cast<Sample16bit2ch*>(ptr + (index - beginSampleIndex) * m_format.blockAlign);
			return WaveSample(pSample->left * m_normalize, pSample->right * m_normalize);

			/*/

			auto [ptr, actualReadBytes] = m_readBlocks.getWriteBuffer(index * m_format.blockAlign, sizeof(Sample16bit2ch));
			const auto pSample = std::bit_cast<Sample16bit2ch*>(ptr);
			return WaveSample(pSample->left * m_normalize, pSample->right * m_normalize);

			//*/
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

			const auto [beginBlock, endBlock] = m_readBlocks.blockIndexRange(allocateBegin, allocateEnd - allocateBegin);
			for (uint32 blockIndex = beginBlock; blockIndex <= endBlock; ++blockIndex)
			{
				if (m_readBlocks.isAllocatedBlock(blockIndex))
				{
					m_readBlocks.use(blockIndex);
				}
				else
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
