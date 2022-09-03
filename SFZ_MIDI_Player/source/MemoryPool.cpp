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

	for (uint32 i = 0; i < blockCount; ++i)
	{
		m_freeBlocks.push_back(i);
	}

	{
		const int32 width = static_cast<int32>(Math::Round(Sqrt(blockCount)));
		const int32 height = static_cast<int32>(Math::Ceil(1.0 * blockCount / width));

		m_debugImage = Image(width, height, Palette::Black);
		m_debugTexture = DynamicTexture(m_debugImage);
	}
}

size_t MemoryPool::blockCount() const
{
	return m_buffer.size() / UnitBlockSizeOfBytes;
}

size_t MemoryPool::freeBlockCount() const
{
	return m_freeBlocks.size();
}

void MemoryPool::sortFreeBlock(size_t beginIndex, size_t endIndex)
{
	std::sort(m_freeBlocks.begin() + beginIndex, m_freeBlocks.begin() + endIndex);
}

std::pair<void*, uint32> MemoryPool::allocateBlock(size_t ownerId)
{
	assert(!m_freeBlocks.empty());

	const auto freeBlockIndex = *m_freeBlocks.begin();
	m_freeBlocks.pop_front();

	{
		const auto y = static_cast<int32>(freeBlockIndex / m_debugImage.width());
		const auto x = static_cast<int32>(freeBlockIndex % m_debugImage.width());
		m_debugImage[y][x] = HSV(ownerId * 10.0, 1.0, 1.0 - 0.1 * ((ownerId / 36) % 5));
	}

	auto ptr = m_buffer.data() + freeBlockIndex * UnitBlockSizeOfBytes;
	return std::make_pair(ptr, freeBlockIndex);
}

void MemoryPool::deallocateBlock(uint32 poolId)
{
	{
		const auto y = static_cast<int32>(poolId / m_debugImage.width());
		const auto x = static_cast<int32>(poolId % m_debugImage.width());
		m_debugImage[y][x] = Palette::Black;
	}

	m_freeBlocks.push_front(poolId);
}

void MemoryPool::debugUpdate()
{
	m_debugTexture.fillIfNotBusy(m_debugImage);
}

void MemoryPool::debugDraw() const
{
	const double scale = 0.8 * Min(1.0 * Scene::Height() / m_debugTexture.height(), 1.0 * Scene::Width() / m_debugTexture.width());

	ScopedRenderStates2D rs(SamplerState::ClampNearest);
	m_debugTexture.scaled(scale).drawAt(Scene::Center());
}
