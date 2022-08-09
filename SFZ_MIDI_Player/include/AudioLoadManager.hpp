﻿#pragma once
#include <Siv3D.hpp>
#include "AudioLoaderBase.hpp"

class AudioLoadManager
{
public:

	static AudioLoadManager& i()
	{
		static AudioLoadManager obj;
		return obj;
	}

	size_t load(FilePathView path);

	void update();

	const AudioLoaderBase& reader(size_t index) const;

	AudioLoaderBase& reader(size_t index);

	bool isRunning() const { return m_isRunning; }

	void finish() { m_isRunning = false; }

private:

	AudioLoadManager() = default;

	Array<std::unique_ptr<AudioLoaderBase>> m_waveReaders;
	Array<String> m_paths;
	bool m_isRunning = true;
};