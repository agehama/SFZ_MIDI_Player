﻿#pragma once
#include <Siv3D.hpp>
#include "MemoryPool.hpp"

class MemoryBlockList
{
public:

	MemoryBlockList(size_t id);

	// assert(beginDataPos % MemoryPool::UnitBlockSizeOfBytes == 0)
	void allocate(size_t beginDataPos, size_t sizeOfBytes);

	// assert(beginDataPos % MemoryPool::UnitBlockSizeOfBytes == 0)
	void deallocate(size_t beginDataPos, size_t sizeOfBytes);

	void deallocate();

	// returns [beginDataPtr, actualSizeOfBytes]
	std::pair<uint8*, size_t> getWriteBuffer(size_t beginDataPos, size_t expectSizeOfBytes) const;

	bool isAllocatedBlock(uint32 blockIndex);

	// return [(blockIndex, isAllocated)]
	Array<std::pair<uint32, bool>> blockIndices(size_t beginDataPos, size_t sizeOfBytes);

	std::pair<uint32, uint32> blockIndexRange(size_t beginDataPos, size_t sizeOfBytes);

	void allocateSingleBlock(uint32 blockIndex);

	uint8* getBlock(uint32 blockIndex) const;

private:

	struct BlockInfo
	{
		// 波形データの先頭からのオフセット
		//size_t dataOffset;
		uint8* buffer;
		uint32 poolId;
	};

	//Array<BlockInfo> m_blocks;

	// key: 波形データの先頭からのブロックインデックス
	//std::map<uint32, BlockInfo> m_blocks;
	HashTable<uint32, BlockInfo> m_blocks;

	size_t m_id;
};
