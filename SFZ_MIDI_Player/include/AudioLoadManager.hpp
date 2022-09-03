#pragma once
#include <Siv3D.hpp>
#include "Config.hpp"
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

	void markBlocks();

	void freeUnusedBlocks();

	const AudioLoaderBase& reader(size_t index) const;

	AudioLoaderBase& reader(size_t index);

	bool isFinish() const { return m_isFinish; }

	bool isRunning() const { return !m_isPause && !m_isFinish; }

	void finish() { m_isFinish = true; }

	void pause() { m_isPause = true; }

	void resume() { m_isPause = false; }

	void debugLog(const String& str);

private:

	AudioLoadManager()
	{
#ifdef DEVELOPMENT
		m_debugLog = TextWriter(U"debug/audioDebugLog.txt");
#endif
	}

	Array<std::unique_ptr<AudioLoaderBase>> m_waveReaders;
	Array<String> m_paths;
	bool m_isPause = false;
	bool m_isFinish = false;

#ifdef DEVELOPMENT
	TextWriter m_debugLog;
#endif
};
