#pragma once
#include <Siv3D.hpp>
#include <MemoryBlockList.hpp>
#include <MemoryPool.hpp>

MemoryBlockList::MemoryBlockList(size_t id, MemoryPool::Type memoryType) :
	m_id(id), m_memoryType(memoryType)
{}

void MemoryBlockList::allocate(size_t beginDataPos, size_t sizeOfBytes)
{
	assert(beginDataPos % MemoryPool::UnitBlockSizeOfBytes == 0);

	auto& memoryPool = MemoryPool::i(m_memoryType);

	const uint32 beginBlockIndex = static_cast<uint32>(beginDataPos / MemoryPool::UnitBlockSizeOfBytes);
	const uint32 blockCount = static_cast<uint32>((sizeOfBytes + MemoryPool::UnitBlockSizeOfBytes - 1) / MemoryPool::UnitBlockSizeOfBytes);
	for (uint32 i = beginBlockIndex; i < beginBlockIndex + blockCount; ++i)
	{
		if (!m_blocks.contains(i))
		{
			auto [buffer, poolId] = memoryPool.allocateBlock(m_id);
			auto ptr = static_cast<uint8*>(buffer);
			m_blocks[i] = BlockInfo{ ptr, poolId, 0 };
		}
		else
		{
			m_blocks[i].unusedCount = 0;
		}
	}
}

void MemoryBlockList::deallocate(size_t beginDataPos, size_t sizeOfBytes)
{
	assert(beginDataPos % MemoryPool::UnitBlockSizeOfBytes == 0);

	auto& memoryPool = MemoryPool::i(m_memoryType);

	const uint32 beginBlockIndex = static_cast<uint32>(beginDataPos / MemoryPool::UnitBlockSizeOfBytes);
	const uint32 blockCount = static_cast<uint32>((sizeOfBytes + MemoryPool::UnitBlockSizeOfBytes - 1) / MemoryPool::UnitBlockSizeOfBytes);
	for (uint32 i = beginBlockIndex; i < beginBlockIndex + blockCount; ++i)
	{
		auto it = m_blocks.find(i);
		if (it != m_blocks.end())
		{
			memoryPool.deallocateBlock(it->second.poolId);
			m_blocks.erase(it);
		}
	}
}

void MemoryBlockList::deallocate()
{
	auto& memoryPool = MemoryPool::i(m_memoryType);

	for (const auto& block : m_blocks)
	{
		memoryPool.deallocateBlock(block.second.poolId);
	}

	m_blocks.clear();
}

std::pair<uint8*, size_t> MemoryBlockList::getWriteBuffer(size_t beginDataPos, size_t expectSizeOfBytes) const
{
	const uint32 blockIndex = static_cast<uint32>(beginDataPos / MemoryPool::UnitBlockSizeOfBytes);

	const uint32 dataOffset = static_cast<uint32>(beginDataPos - blockIndex * MemoryPool::UnitBlockSizeOfBytes);
	const size_t dataSize = Min(MemoryPool::UnitBlockSizeOfBytes - dataOffset, expectSizeOfBytes);

	assert(m_blocks.contains(blockIndex));

	const auto& block = m_blocks.at(blockIndex);

	return std::make_pair(block.buffer + dataOffset, dataSize);
}

bool MemoryBlockList::isAllocatedBlock(uint32 blockIndex)
{
	//const size_t blockIndex = beginDataPos / MemoryPool::UnitBlockSizeOfBytes;

	return m_blocks.contains(blockIndex);
}

Array<std::pair<uint32, bool>> MemoryBlockList::blockIndices(size_t beginDataPos, size_t sizeOfBytes)
{
	const uint32 beginBlockIndex = static_cast<uint32>(beginDataPos / MemoryPool::UnitBlockSizeOfBytes);
	const uint32 blockCount = static_cast<uint32>((sizeOfBytes + MemoryPool::UnitBlockSizeOfBytes - 1) / MemoryPool::UnitBlockSizeOfBytes);

	Array<std::pair<uint32, bool>> result(blockCount);
	for (uint32 i = 0; i < blockCount; ++i)
	{
		const auto blockIndex = beginBlockIndex + i;
		result[i] = std::make_pair(blockIndex, m_blocks.contains(blockIndex));
	}

	return result;
}

std::pair<uint32, uint32> MemoryBlockList::blockIndexRange(size_t beginDataPos, size_t sizeOfBytes)
{
	const uint32 beginBlockIndex = static_cast<uint32>(beginDataPos / MemoryPool::UnitBlockSizeOfBytes);
	const uint32 blockCount = static_cast<uint32>((sizeOfBytes + MemoryPool::UnitBlockSizeOfBytes - 1) / MemoryPool::UnitBlockSizeOfBytes);
	return std::make_pair(beginBlockIndex, beginBlockIndex + blockCount);
}

uint8* MemoryBlockList::allocateSingleBlock(uint32 blockIndex)
{
	assert(!m_blocks.contains(blockIndex));

	auto& memoryPool = MemoryPool::i(m_memoryType);

	auto [buffer, poolId] = memoryPool.allocateBlock(m_id);
	auto ptr = static_cast<uint8*>(buffer);
	m_blocks[blockIndex] = BlockInfo{ ptr, poolId, 0 };
	return ptr;
}

uint8* MemoryBlockList::getBlock(uint32 blockIndex) const
{
	assert(m_blocks.contains(blockIndex));

	const auto& block = m_blocks.at(blockIndex);
	return block.buffer;
}

void MemoryBlockList::markUnused()
{
	for (auto& block : m_blocks)
	{
		++block.second.unusedCount;
	}
}

void MemoryBlockList::use(uint32 blockIndex)
{
	m_blocks[blockIndex].unusedCount = 0;
}

void MemoryBlockList::freeUnusedBlocks()
{
	auto& memoryPool = MemoryPool::i(m_memoryType);

	for (auto it = m_blocks.begin(); it != m_blocks.end();)
	{
		if (it->second.unusedCount < 100)
		{
			++it;
		}
		else
		{
			memoryPool.deallocateBlock(it->second.poolId);
			it = m_blocks.erase(it);
		}
	}
}

size_t MemoryBlockList::numOfBlocks() const
{
	return m_blocks.size();
}

std::tuple<uint32, uint32, uint32> MemoryBlockList::freePreviousBlockIndex(uint32 blockIndex)
{
	auto& memoryPool = MemoryPool::i(m_memoryType);

	uint32 eraseCount = 0;
	uint32 minBlockIndex = std::numeric_limits<uint32>::max();
	uint32 maxBlockIndex = 0;
	for (auto it = m_blocks.begin(); it != m_blocks.end();)
	{
		if (blockIndex <= it->first)
		{
			minBlockIndex = Min(it->first, minBlockIndex);
			maxBlockIndex = Max(it->first, maxBlockIndex);
			++it;
		}
		else
		{
			memoryPool.deallocateBlock(it->second.poolId);
			it = m_blocks.erase(it);
			++eraseCount;
		}
	}

	return std::make_tuple(minBlockIndex, maxBlockIndex, eraseCount);
}
