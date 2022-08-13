#pragma once
#include <Siv3D.hpp>

class MemoryPool
{
public:

	static MemoryPool& i()
	{
		static MemoryPool obj;
		return obj;
	}

	void setCapacity(size_t sizeOfBytes);

	std::pair<void*, uint32> allocateBlock();

	void deallocateBlock(uint32 blockIndex);

	size_t blockCount() const;

	static const size_t UnitBlockSampleLength;
	static const size_t UnitBlockSizeOfBytes;

private:

	MemoryPool() = default;

	Array<uint8> m_buffer;
	HashSet<uint32> m_freeBlocks;
};
