#pragma once
#include <AudioLoadManager.hpp>
#include <WaveLoader.hpp>
#include <FlacLoader.hpp>

size_t AudioLoadManager::load(FilePathView path)
{
	for (auto [i, wavePath] : Indexed(m_paths))
	{
		if (wavePath == path)
		{
			return i;
		}
	}

	const auto i = m_paths.size();
	if (FileSystem::Extension(path) == U"wav")
	{
		m_waveReaders.push_back(std::make_unique<WaveLoader>(path, i));
	}
	else if (FileSystem::Extension(path) == U"flac")
	{
		m_waveReaders.push_back(std::make_unique<FlacLoader>(path, i));
	}
	else
	{
		Console << U"error 未対応の拡張子です\"" << path << U"\"";

		return std::numeric_limits<size_t>::max();
	}

	m_paths.emplace_back(path);

	return i;
}

void AudioLoadManager::update()
{
	for (auto& reader : m_waveReaders)
	{
		reader->update();
	}

	// 長時間起動したときのキャッシュ効率の低下を防ぐために空きブロックを少しずつソートする
	//{
	//	auto& memoryPool = MemoryPool::i();
	//	const size_t unitSortCount = 8192;//400[us]くらい
	//	const size_t blockCount = memoryPool.freeBlockCount();
	//	auto sortBegin = Random(0ull, blockCount - unitSortCount);
	//	memoryPool.sortFreeBlock(sortBegin, sortBegin + unitSortCount);
	//}
}

const AudioLoaderBase& AudioLoadManager::reader(size_t index) const
{
	return *m_waveReaders[index];
}

AudioLoaderBase& AudioLoadManager::reader(size_t index)
{
	return *m_waveReaders[index];
}
