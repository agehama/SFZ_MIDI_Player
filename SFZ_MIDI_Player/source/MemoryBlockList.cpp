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
			auto [buffer, blockIndex] = memoryPool.allocateBlock(m_id);
			m_blocks[i] = BlockInfo{ static_cast<uint8*>(buffer), blockIndex};
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
			memoryPool.deallocateBlock(it->second.blockIndex);
			m_blocks.erase(it);
		}
	}
}

void MemoryBlockList::deallocate()
{
	auto& memoryPool = MemoryPool::i();

	for (const auto& block : m_blocks)
	{
		memoryPool.deallocateBlock(block.second.blockIndex);
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
