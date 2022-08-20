#pragma once
#include <Siv3D.hpp>
#include <MemoryBlockList.hpp>
#include <MemoryPool.hpp>

MemoryBlockList::MemoryBlockList(size_t id) :
	m_id(id)
{}

void MemoryBlockList::allocate(size_t beginDataPos, size_t sizeOfBytes)
{
	assert(beginDataPos % MemoryPool::UnitBlockSizeOfBytes == 0);

	auto& memoryPool = MemoryPool::i();

	const size_t beginBlockIndex = beginDataPos / MemoryPool::UnitBlockSizeOfBytes;
	const size_t blockCount = (sizeOfBytes + MemoryPool::UnitBlockSizeOfBytes - 1) / MemoryPool::UnitBlockSizeOfBytes;
	for (size_t i = beginBlockIndex; i < beginBlockIndex + blockCount; ++i)
	{
		if (!m_blocks.contains(i))
		{
			auto [buffer, poolId] = memoryPool.allocateBlock(m_id);
			m_blocks[i] = BlockInfo{ static_cast<uint8*>(buffer), poolId };
		}
	}
}

void MemoryBlockList::deallocate(size_t beginDataPos, size_t sizeOfBytes)
{
	assert(beginDataPos % MemoryPool::UnitBlockSizeOfBytes == 0);

	auto& memoryPool = MemoryPool::i();

	const size_t beginBlockIndex = beginDataPos / MemoryPool::UnitBlockSizeOfBytes;
	const size_t blockCount = (sizeOfBytes + MemoryPool::UnitBlockSizeOfBytes - 1) / MemoryPool::UnitBlockSizeOfBytes;
	for (size_t i = beginBlockIndex; i < beginBlockIndex + blockCount; ++i)
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
	auto& memoryPool = MemoryPool::i();

	for (const auto& block : m_blocks)
	{
		memoryPool.deallocateBlock(block.second.poolId);
	}

	m_blocks.clear();
}

std::pair<uint8*, size_t> MemoryBlockList::getWriteBuffer(size_t beginDataPos, size_t expectSizeOfBytes) const
{
	const uint32 blockIndex = static_cast<uint32>(beginDataPos / MemoryPool::UnitBlockSizeOfBytes);

	const uint32 dataOffset = static_cast<uint32>(beginDataPos) - blockIndex * MemoryPool::UnitBlockSizeOfBytes;
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
	const size_t beginBlockIndex = beginDataPos / MemoryPool::UnitBlockSizeOfBytes;
	const size_t blockCount = (sizeOfBytes + MemoryPool::UnitBlockSizeOfBytes - 1) / MemoryPool::UnitBlockSizeOfBytes;

	Array<std::pair<uint32, bool>> result(blockCount);
	for (size_t i = 0; i < blockCount; ++i)
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

void MemoryBlockList::allocateSingleBlock(uint32 blockIndex)
{
	assert(!m_blocks.contains(blockIndex));

	auto& memoryPool = MemoryPool::i();

	auto [buffer, poolId] = memoryPool.allocateBlock(m_id);
	m_blocks[blockIndex] = BlockInfo{ static_cast<uint8*>(buffer), poolId };
}

uint8* MemoryBlockList::getBlock(uint32 blockIndex) const
{
	assert(m_blocks.contains(blockIndex));

	const auto& block = m_blocks.at(blockIndex);
	return block.buffer;
}
