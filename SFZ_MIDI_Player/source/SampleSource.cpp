#pragma once
#include <SampleSource.hpp>

double Envelope::level(double noteOnTime, double noteOffTime, double time) const
{
	const double duration = noteOffTime - noteOnTime;
	if (duration < m_attackTime + m_decayTime)
	{
		noteOffTime = noteOnTime + (m_attackTime + m_decayTime);
	}

	if (noteOffTime <= time)
	{
		time -= noteOffTime;

		if (time < m_releaseTime)
		{
			return Math::Lerp(m_sustainLevel, 0.0, time / m_releaseTime);
		}

		return 0;
	}

	time -= noteOnTime;

	if (time < 0)
	{
		return 0;
	}

	if (time < m_attackTime)
	{
		return time / m_attackTime;
	}

	time -= m_attackTime;

	if (time < m_decayTime)
	{
		return Math::Lerp(1.0, m_sustainLevel, time / m_decayTime);
	}

	return m_sustainLevel;
}

struct ChunkHead
{
	char id[4];
	uint32 size;
};

struct RiffChunk
{
	ChunkHead head;
	char format[4];
};

void WaveReader::init()
{
	RiffChunk riffChunk;
	m_waveReader.read(riffChunk);

	if (strncmp(riffChunk.head.id, "RIFF", 4) != 0)
	{
		Console << U"error: not riff format";
		return;
	}

	if (strncmp(riffChunk.format, "WAVE", 4) != 0)
	{
		Console << U"error: not wave format";
		return;
	}

	while (m_waveReader.getPos() < m_waveReader.size())
	{
		ChunkHead chunk;
		m_waveReader.read(chunk);

		if (strncmp(chunk.id, "fmt ", 4) == 0)
		{
			m_waveReader.read(m_format);

			if (m_format.channels != 2)
			{
				Console << U"error: channels != 2";
				return;
			}
			if (m_format.bitsPerSample != 16)
			{
				Console << U"error: bitsPerSample != 16";
				return;
			}

			m_readFormat = true;

			if (m_dataPos != 0)
			{
				break;
			}
			else if (chunk.size < sizeof(WaveFileFormat))
			{
				m_waveReader.skip(chunk.size - sizeof(WaveFileFormat));
			}
		}
		else if (strncmp(chunk.id, "data", 4) == 0)
		{
			m_dataPos = m_waveReader.getPos();
			m_dataSize = chunk.size;

			if (m_readFormat)
			{
				break;
			}
			else
			{
				m_waveReader.skip(chunk.size);
			}
		}
		else
		{
			m_waveReader.skip(chunk.size);
		}
	}

	m_lengthSample = m_dataSize / m_format.blockAlign;
	m_sampleRate = m_format.samplePerSecond;
	m_normalize = 1.f / 32768.0f;
}

void WaveReader::use()
{
	if (m_use)
	{
		return;
	}

	m_mutex.lock();

	{
		m_loadSampleCount = 0;

		if (m_waveReader.getPos() != m_dataPos)
		{
			m_waveReader.setPos(m_dataPos);
		}

		readBlock();

		m_use = true;
	}

	m_mutex.unlock();
}

void WaveReader::unuse()
{
	m_use = false;
}

void WaveReader::update()
{
	m_mutex.lock();

	if (m_use)
	{
		readBlock();
	}
	else
	{
		m_readBuffer.clear();
		m_waveReader.setPos(m_dataPos);
		m_loadSampleCount = 0;
	}

	m_mutex.unlock();
}

WaveSample WaveReader::getSample(int64 index) const
{
	const auto& sample = m_readBuffer[index];

	return WaveSample(sample.left * m_normalize, sample.right * m_normalize);
}

void WaveReader::readBlock()
{
	size_t readCount = 512;
	if (m_dataSize <= m_loadSampleCount + readCount)
	{
		readCount = m_loadSampleCount - m_dataSize;
	}

	if (1 <= readCount)
	{
		m_waveReader.read(m_readBuffer.data() + m_loadSampleCount, readCount * m_format.blockAlign);
		m_loadSampleCount += readCount;
	}
}

void AudioSource::setRtDecay(float rtDecay)
{
	m_rtDecay = rtDecay;
}

size_t AudioSource::sampleRate() const
{
	return getReader().sampleRate();
}

size_t AudioSource::lengthSample() const
{
	const auto& sourceWave = getReader();

	const double speed = std::exp2(m_tune / 1200.0);
	const double scale = 1.0 / speed;
	const auto sampleCount = static_cast<size_t>(Math::Ceil(sourceWave.lengthSample() * scale));

	return sampleCount;
}

WaveSample AudioSource::getSample(int64 index) const
{
	const auto& sourceWave = getReader();

	float amplitude = m_amplitude;
	if (m_rtDecay)
	{
		const double seconds = 1.0 * index / sourceWave.sampleRate();
		const double currentVolume = -seconds * m_rtDecay.value();
		const float scale = static_cast<float>(std::pow(10.0, currentVolume / 20.0) * 0.5);
		amplitude *= scale;
	}

	if (m_tune == 0)
	{
		return sourceWave.getSample(index) * amplitude;
	}

	const double speed = std::exp2(m_tune / 1200.0);

	const double readIndex = index * speed;
	const auto prevIndex = static_cast<int64>(Floor(readIndex));
	const auto nextIndex = Min(static_cast<int64>(Ceil(readIndex)), static_cast<int64>(sourceWave.size() - 1));
	const double t = Math::Fmod(readIndex, 1.0);

	return sourceWave.getSample(prevIndex).lerp(sourceWave.getSample(nextIndex), t) * amplitude;
}

void AudioSource::use()
{
	getReader().use();
}

void AudioSource::unuse()
{
	getReader().unuse();
}

void AudioKey::init(int8 key)
{
	noteKey = key;
	attackKeys.clear();
	releaseKeys.clear();
}

void AudioKey::addAttackKey(const AudioSource& source)
{
	attackKeys.push_back(source);
}

void AudioKey::addReleaseKey(const AudioSource& source)
{
	releaseKeys.push_back(source);
}

bool AudioKey::hasAttackKey() const
{
	return !attackKeys.empty();
}

void AudioLoadManager::update()
{
	for (auto& reader : m_waveReaders)
	{
		reader.update();
	}
}

std::mutex WaveReader::m_mutex;

const NoteEvent& AudioKey::addEvent(uint8 velocity, int64 pressTimePos, int64 releaseTimePos)
{
	const auto attackIndex = getAttackIndex(velocity);
	const auto releaseIndex = getReleaseIndex(velocity);
	NoteEvent note(attackIndex, releaseIndex, pressTimePos, releaseTimePos, velocity);
	m_noteEvents.push_back(note);
	return m_noteEvents.back();
}

void AudioKey::clearEvent()
{
	m_noteEvents.clear();
}

int64 AudioKey::getAttackIndex(uint8 velocity) const
{
	for (auto [i, key] : Indexed(attackKeys))
	{
		if (key.isValidVelocity(velocity))
		{
			return static_cast<int64>(i);
		}
	}

	return -1;
}

int64 AudioKey::getReleaseIndex(uint8 velocity) const
{
	for (auto [i, key] : Indexed(releaseKeys))
	{
		if (key.isValidVelocity(velocity))
		{
			return static_cast<int64>(i);
		}
	}

	return -1;
}

void AudioKey::debugPrint() const
{
	Console << U"-----------------------------";
	Console << U"key: " << noteKey;
	Console << U"attack keys: " << attackKeys.size();
	for (const auto& [i, source] : Indexed(attackKeys))
	{
		//Console << U"  " << i << U"->[" << source.lovel << U", " << source.hivel << U"]: " << source.filePath << U" " << source.semitone;
	}
	Console << U"release keys: " << releaseKeys.size();
	for (const auto& [i, source] : Indexed(releaseKeys))
	{
		//Console << U"  " << i << U"->[" << source.lovel << U", " << source.hivel << U"]: " << source.filePath << U" " << source.semitone;
	}
}

void AudioKey::sortEvent()
{
	m_noteEvents.sort_by([](const NoteEvent& a, const NoteEvent& b) { return a.pressTimePos < b.pressTimePos; });
}

void AudioKey::deleteDuplicate()
{
	// 直前のノートオンからサンプル数がdeleteRange以内で始まるノートを削除する
	const int64 deleteRange = 50;
	int64 prevPos = -deleteRange;
	for (int64 i = 0; i < static_cast<int64>(m_noteEvents.size());)
	{
		const int64 currentPos = m_noteEvents[i].pressTimePos;
		if (currentPos - prevPos < deleteRange)
		{
			m_noteEvents.erase(m_noteEvents.begin() + i);
		}
		else
		{
			prevPos = currentPos;
			++i;
		}
	}
}

void AudioKey::getSamples(float* left, float* right, int64 startPos, int64 sampleCount)
{
	//const double lengthOfTime = 1.0 * sampleCount / Wave::DefaultSampleRate;

	//const double startTime = 1.0 * startPos / Wave::DefaultSampleRate;
	//const double endTime = 1.0 * (startPos + sampleCount) / Wave::DefaultSampleRate;

	NoteEvent note0(0, 0, 0, 0, 0);
	note0.pressTimePos = startPos;
	auto itNextStart = std::upper_bound(m_noteEvents.begin(), m_noteEvents.end(), note0, [](const NoteEvent& a, const NoteEvent& b) { return a.pressTimePos < b.pressTimePos; });
	const int64 nextStartIndex = std::distance(m_noteEvents.begin(), itNextStart);
	int64 startIndex = nextStartIndex - 1;

	if (m_noteEvents.empty())
	{
		return;
	}

	NoteEvent note1(0, 0, 0, 0, 0);
	note1.pressTimePos = startPos + sampleCount;
	auto itNextEnd = std::upper_bound(m_noteEvents.begin(), m_noteEvents.end(), note1, [](const NoteEvent& a, const NoteEvent& b) { return a.pressTimePos < b.pressTimePos; });
	const int64 nextEndIndex = std::distance(m_noteEvents.begin(), itNextEnd);

	if (startIndex < 0)
	{
		startIndex = 0;
	}

	for (int64 noteIndex = startIndex; noteIndex < nextEndIndex; ++noteIndex)
	{
		render(left, right, startPos, sampleCount, noteIndex);
		renderRelease(left, right, startPos, sampleCount, noteIndex);
	}
}

void AudioKey::render(float* left, float* right, int64 startPos, int64 sampleCount, int64 noteIndex)
{
	const auto& targetEvent = m_noteEvents[noteIndex];

	if (targetEvent.attackIndex == -1)
	{
		return;
	}

	auto& attackKey = attackKeys[targetEvent.attackIndex];
	attackKey.use();

	const auto& envelope = attackKey.envelope();

	//const auto waveStartPos = Max(0ll, startPos - targetEvent.pressTimePos);
	//const auto maxGateSamples = noteIndex + 1 < m_noteEvents.size() ? m_noteEvents[noteIndex + 1].pressTimePos - targetEvent.pressTimePos : static_cast<int64>(sourceWave.lengthSample());

	//const auto startIndex = static_cast<int64>(Math::Round(waveStartTime * sourceWave.sampleRate()));
	//const auto maxReadCount = static_cast<int64>(fetchTime * m_source.sampleRate());
	//const auto maxReadCount = static_cast<int64>(Min({ m_source.lengthSec(), maxGateTime }) * m_source.sampleRate());

	//const auto overReadCount = static_cast<int64>(Math::Round(maxGateTime * sourceWave.sampleRate()));
	//const auto maxReadCount = Min(static_cast<int64>(sourceWave.lengthSample()), overReadCount);
	//const auto endIndex = startIndex + maxReadCount;

	const int64 prevWriteIndexHead = getWriteIndexHead(startPos, noteIndex - 1);
	const int64 writeIndexHead = getWriteIndexHead(startPos, noteIndex);

	//const auto skipReadIndex = Max(0ll, startPos - targetEvent.pressTimePos);

	//const auto maxWriteCount = sampleCount - writeIndexHead;
	//const auto sampleReadCount = Min(maxWriteCount, maxReadCount);
	//const auto sampleEmptyCount = Min(maxWriteCount, overReadCount);
	const auto [sampleReadCount, sampleEmptyCount] = readEmptyCount(startPos, sampleCount, noteIndex);

	/*Console << noteIndex << U"[" << noteKey << U"]v" << targetEvent.index << U" -> "
		<< Point(targetEvent.pressTimePos, targetEvent.releaseTimePos)
		<< U", readStart: " << waveStartPos
		<< U", maxReadCount: " << maxGateSamples
		<< U", writeStart: " << writeIndexHead
		<< U", maxWriteCount: " << maxWriteCount
		<< U", sampleReadCount: " << sampleReadCount
		<< U", sampleEmptyCount: " << sampleEmptyCount;*/

		//Console << Vec2(targetEvent.pressTimePos, targetEvent.releaseTimePos);

	const double currentVolume = targetEvent.velocity / 127.0;

	const int64 prevWriteCount = writeIndexHead - prevWriteIndexHead;
	for (int64 i = Max(0ll, -writeIndexHead), j = 0; i < sampleReadCount; ++i, ++j)
	{
		//const int64 readIndex = startIndex + i;
		const int64 readIndex = i;
		//const int64 readIndex = startIndex + j;
		const int64 writeIndex = writeIndexHead + i;
		const double time = 1.0 * (startPos + writeIndex) / attackKey.sampleRate();
		const double currentLevel = envelope.level(targetEvent, time) * currentVolume;

		if (1.0 * targetEvent.releaseTimePos / attackKey.sampleRate() + envelope.noteTime(targetEvent) < time)
		{
			break;
		}

		const auto blendIndex = (startPos + writeIndex) - targetEvent.pressTimePos;
		if (1 <= noteIndex && blendIndex < BlendSampleCount)
		{
			const auto& prevEvent = m_noteEvents[noteIndex - 1];
			auto& prevAttackKey = attackKeys[prevEvent.attackIndex];
			prevAttackKey.use();
			const auto [prevReadCount, prevEmptyCount] = readEmptyCount(startPos, sampleCount, noteIndex - 1);

			const double prevVolume = prevEvent.velocity / 127.0;

			//const double prevLevel = envelope.level(prevEvent.pressTime, prevEvent.releaseTime, time);
			const double prevLevel = envelope.level(prevEvent, time) * prevVolume;

			//Console << U"  " << Vec3(prevEvent.pressTime, prevEvent.releaseTime, time) << U"" << U" -> " << prevLevel;

			const int64 prevReadIndex = prevWriteCount + i;
			const auto sample0 = (prevReadIndex < static_cast<int64>(prevAttackKey.lengthSample()) ? prevAttackKey.getSample(prevReadIndex) : WaveSample::Zero()) * static_cast<float>(prevLevel);
			const auto sample1 = attackKey.getSample(readIndex) * static_cast<float>(currentLevel);
			const double t = 1.0 * blendIndex / BlendSampleCount;
			const auto blendSample = sample0.lerp(sample1, t);
			left[writeIndex] += blendSample.left;
			right[writeIndex] += blendSample.right;
		}
		else
		{
			const auto sample1 = attackKey.getSample(readIndex) * static_cast<float>(currentLevel);
			left[writeIndex] += sample1.left;
			right[writeIndex] += sample1.right;
		}
	}
}

void AudioKey::renderRelease(float* left, float* right, int64 startPos, int64 sampleCount, int64 noteIndex)
{
	const auto& targetEvent = m_noteEvents[noteIndex];

	//todo: ノイズ対策が不十分なため一旦外す
	return;
	if (targetEvent.releaseIndex == -1)
	{
		return;
	}

	const auto& releaseKey = releaseKeys[targetEvent.releaseIndex];

	//const auto waveStartPos = Max(0ll, startPos - targetEvent.releaseTimePos);
	//const auto maxGateSamples = noteIndex + 1 < static_cast<int64>(m_noteEvents.size()) ? m_noteEvents[noteIndex + 1].pressTimePos - targetEvent.releaseTimePos : static_cast<int64>(sourceWave.lengthSample());

	const int64 prevWriteIndexHead = getWriteIndexHeadRelease(startPos, noteIndex - 1);
	const int64 writeIndexHead = getWriteIndexHeadRelease(startPos, noteIndex);

	//const auto skipReadIndex = Max(0ll, startPos - targetEvent.releaseTimePos);

	//const auto maxWriteCount = sampleCount - writeIndexHead;
	//const auto sampleReadCount = Min(maxWriteCount, maxReadCount);
	//const auto sampleEmptyCount = Min(maxWriteCount, overReadCount);
	const auto [sampleReadCount, sampleEmptyCount] = readEmptyCountRelease(startPos, sampleCount, noteIndex);

	/*Console << noteIndex << U"[" << noteKey << U"]v" << targetEvent.index << U" -> "
		<< Point(targetEvent.pressTimePos, targetEvent.releaseTimePos)
		<< U", readStart: " << waveStartPos
		<< U", maxReadCount: " << maxGateSamples
		<< U", writeStart: " << writeIndexHead
		<< U", maxWriteCount: " << maxWriteCount
		<< U", sampleReadCount: " << sampleReadCount
		<< U", sampleEmptyCount: " << sampleEmptyCount;*/

	//const double currentVolume = 1;

	const int64 prevWriteCount = writeIndexHead - prevWriteIndexHead;
	for (int64 i = Max(0ll, -writeIndexHead), j = 0; i < sampleReadCount; ++i, ++j)
	{
		//const int64 readIndex = startIndex + i;
		const int64 readIndex = i;
		//const int64 readIndex = startIndex + j;
		const int64 writeIndex = writeIndexHead + i;
		//const double time = 1.0 * (startPos + writeIndex) / sourceWave.sampleRate();
		//const double currentLevel = currentVolume;

		//if (1.0 * targetEvent.releaseTimePos / sourceWave.sampleRate() + envelope.releaseTime < time)
		//{
		//	break;
		//}

		const auto blendIndex = (startPos + writeIndex) - targetEvent.releaseTimePos;
		if (1 <= noteIndex && m_noteEvents[noteIndex - 1].releaseIndex != -1 && blendIndex < BlendSampleCount)
		{
			const auto& prevEvent = m_noteEvents[noteIndex - 1];

			const auto& prevReleaseKey = releaseKeys[prevEvent.releaseIndex];
			const auto [prevReadCount, prevEmptyCount] = readEmptyCountRelease(startPos, sampleCount, noteIndex - 1);

			//const double prevVolume = 1;

			//const double prevLevel = envelope.level(prevEvent.pressTime, prevEvent.releaseTime, time);
			//const double prevLevel = envelope.level(prevEvent, time) * prevVolume;

			//Console << U"  " << Vec3(prevEvent.pressTime, prevEvent.releaseTime, time) << U"" << U" -> " << prevLevel;

			const int64 prevReadIndex = prevWriteCount + i;
			const auto sample0 = (prevReadIndex < static_cast<int64>(prevReleaseKey.lengthSample()) ? prevReleaseKey.getSample(prevReadIndex) : WaveSample::Zero());
			const auto sample1 = releaseKey.getSample(readIndex);
			const double t = 1.0 * blendIndex / BlendSampleCount;
			const auto blendSample = sample0.lerp(sample1, t);
			left[writeIndex] += blendSample.left;
			right[writeIndex] += blendSample.right;
		}
		else
		{
			const auto sample1 = releaseKey.getSample(readIndex);
			left[writeIndex] += sample1.left;
			right[writeIndex] += sample1.right;
		}
	}
}

int64 AudioKey::getWriteIndexHead(int64 startPos, int64 noteIndex) const
{
	if (noteIndex < 0 || static_cast<int64>(m_noteEvents.size()) <= noteIndex)
	{
		return 0;
	}

	const auto& targetEvent = m_noteEvents[noteIndex];
	return targetEvent.pressTimePos - startPos;
}

std::pair<int64, int64> AudioKey::readEmptyCount(int64 startPos, int64 sampleCount, int64 noteIndex) const
{
	const auto& targetEvent = m_noteEvents[noteIndex];
	const auto& attackKey = attackKeys[targetEvent.attackIndex];
	const auto maxGateSamples = noteIndex + 1 < static_cast<int64>(m_noteEvents.size()) ? m_noteEvents[noteIndex + 1].pressTimePos - m_noteEvents[noteIndex].pressTimePos : static_cast<int64>(attackKey.lengthSample());
	const auto maxReadCount = Min(static_cast<int64>(attackKey.lengthSample()), maxGateSamples);
	const int64 writeIndexHead = getWriteIndexHead(startPos, noteIndex);

	const auto maxWriteCount = sampleCount - writeIndexHead;
	const auto sampleReadCount = Min(maxWriteCount, maxReadCount);
	const auto sampleEmptyCount = Min(maxWriteCount, maxGateSamples);

	return std::make_pair(sampleReadCount, sampleEmptyCount);
}

int64 AudioKey::getWriteIndexHeadRelease(int64 startPos, int64 noteIndex) const
{
	if (noteIndex < 0 || static_cast<int64>(m_noteEvents.size()) <= noteIndex)
	{
		return 0;
	}

	const auto& targetEvent = m_noteEvents[noteIndex];
	return targetEvent.releaseTimePos - startPos;
}

std::pair<int64, int64> AudioKey::readEmptyCountRelease(int64 startPos, int64 sampleCount, int64 noteIndex) const
{
	const auto& targetEvent = m_noteEvents[noteIndex];
	const auto& releaseKey = releaseKeys[targetEvent.releaseIndex];
	const auto maxGateSamples = noteIndex + 1 < static_cast<int64>(m_noteEvents.size()) ? m_noteEvents[noteIndex + 1].releaseTimePos - m_noteEvents[noteIndex].pressTimePos : static_cast<int64>(releaseKey.lengthSample());
	const auto maxReadCount = Min(static_cast<int64>(releaseKey.lengthSample()), maxGateSamples);
	const int64 writeIndexHead = getWriteIndexHeadRelease(startPos, noteIndex);

	const auto maxWriteCount = sampleCount - writeIndexHead;
	const auto sampleReadCount = Min(maxWriteCount, maxReadCount);
	const auto sampleEmptyCount = Min(maxWriteCount, maxGateSamples);

	return std::make_pair(sampleReadCount, sampleEmptyCount);
}
