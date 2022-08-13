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

	std::pair<void*, uint32> allocateBlock(size_t oenerId);

	void deallocateBlock(uint32 blockIndex);

	size_t blockCount() const;

	size_t freeBlockCount() const;

	void sortFreeBlock(size_t beginIndex, size_t endIndex);

	void debugUpdate();

	void debugDraw() const;

	static const size_t UnitBlockSampleLength;
	static const size_t UnitBlockSizeOfBytes;

private:

	MemoryPool() = default;

	Array<uint8> m_buffer;
	std::deque<uint32> m_freeBlocks;

	Image m_debugImage;
	DynamicTexture m_debugTexture;
};
