#pragma once
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

	bool isFinish() const { return m_isFinish; }

	bool isRunning() const { return !m_isPause && !m_isFinish; }

	void finish() { m_isFinish = true; }

	void pause() { m_isPause = true; }

	void resume() { m_isPause = false; }

private:

	AudioLoadManager() = default;

	Array<std::unique_ptr<AudioLoaderBase>> m_waveReaders;
	Array<String> m_paths;
	bool m_isPause = false;
	bool m_isFinish = false;
};
