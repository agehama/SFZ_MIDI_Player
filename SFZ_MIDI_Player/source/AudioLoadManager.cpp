﻿#pragma once
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

void AudioLoadManager::markBlocks()
{
	for (auto& reader : m_waveReaders)
	{
		reader->markUnused();
	}
}

void AudioLoadManager::freeUnusedBlocks()
{
	for (auto& reader : m_waveReaders)
	{
		reader->freeUnusedBlocks();
	}
}

const AudioLoaderBase& AudioLoadManager::reader(size_t index) const
{
	return *m_waveReaders[index];
}

AudioLoaderBase& AudioLoadManager::reader(size_t index)
{
	return *m_waveReaders[index];
}

void AudioLoadManager::debugLog([[maybe_unused]] const String& str)
{

#ifdef DEVELOPMENT
	m_debugLog << str;
#endif

}
