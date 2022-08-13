#pragma once
#include <MemoryPool.hpp>

// 16bit * 2ch * 512 サンプルをブロックサイズとする
const size_t MemoryPool::UnitBlockSampleLength = 512;
const size_t MemoryPool::UnitBlockSizeOfBytes = sizeof(uint16) * 2 * UnitBlockSampleLength;

void MemoryPool::setCapacity(size_t sizeOfBytes)
{
	const uint32 blockCount = static_cast<uint32>((sizeOfBytes + UnitBlockSizeOfBytes - 1) / UnitBlockSizeOfBytes);
	m_buffer.resize(blockCount * UnitBlockSizeOfBytes);
	m_buffer.shrink_to_fit();

	m_freeBlocks.reserve(blockCount);
	for (uint32 i = 0; i < blockCount; ++i)
	{
		m_freeBlocks.emplace(i);
	}
}

size_t MemoryPool::blockCount() const
{
	return m_buffer.size() / UnitBlockSizeOfBytes;
}

std::pair<void*, uint32> MemoryPool::allocateBlock()
{
	assert(!m_freeBlocks.empty());

	const auto freeBlockIndex = *m_freeBlocks.begin();

	m_freeBlocks.erase(freeBlockIndex);

	auto ptr = m_buffer.data() + freeBlockIndex * UnitBlockSizeOfBytes;
	return std::make_pair(ptr, freeBlockIndex);
}

void MemoryPool::deallocateBlock(uint32 blockIndex)
{
	m_freeBlocks.emplace(blockIndex);
}
