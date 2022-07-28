#pragma once
#include <SampleSource.hpp>

double Envelope::level(double noteOnTime, double noteOffTime, double time) const
{
	const double duration = noteOffTime - noteOnTime;
	if (duration < attackTime + decayTime)
	{
		noteOffTime = noteOnTime + (attackTime + decayTime);
	}

	if (noteOffTime <= time)
	{
		time -= noteOffTime;

		if (time < releaseTime)
		{
			return Math::Lerp(sustainLevel, 0.0, time / releaseTime);
		}

		return 0;
	}

	time -= noteOnTime;

	if (time < 0)
	{
		return 0;
	}

	if (time < attackTime)
	{
		return time / attackTime;
	}

	time -= attackTime;

	if (time < decayTime)
	{
		return Math::Lerp(1.0, sustainLevel, time / decayTime);
	}

	return sustainLevel;
}

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

void AudioKey::getSamples3(float* left, float* right, int64 startPos, int64 sampleCount) const
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
		render3(left, right, startPos, sampleCount, noteIndex);
		renderRelease(left, right, startPos, sampleCount, noteIndex);
	}
}

void AudioKey::render3(float* left, float* right, int64 startPos, int64 sampleCount, int64 noteIndex) const
{
	const auto& targetEvent = m_noteEvents[noteIndex];

	if (targetEvent.attackIndex == -1)
	{
		return;
	}

	const auto& sourceWave = attackKeys[targetEvent.attackIndex].wave;
	const auto& envelope = attackKeys[targetEvent.attackIndex].envelope;

	const auto waveStartPos = Max(0ll, startPos - targetEvent.pressTimePos);
	const auto maxGateSamples = noteIndex + 1 < m_noteEvents.size() ? m_noteEvents[noteIndex + 1].pressTimePos - targetEvent.pressTimePos : sourceWave.lengthSample();

	//const auto startIndex = static_cast<int64>(Math::Round(waveStartTime * sourceWave.sampleRate()));
	//const auto maxReadCount = static_cast<int64>(fetchTime * m_source.sampleRate());
	//const auto maxReadCount = static_cast<int64>(Min({ m_source.lengthSec(), maxGateTime }) * m_source.sampleRate());

	//const auto overReadCount = static_cast<int64>(Math::Round(maxGateTime * sourceWave.sampleRate()));
	//const auto maxReadCount = Min(static_cast<int64>(sourceWave.lengthSample()), overReadCount);
	//const auto endIndex = startIndex + maxReadCount;

	const int64 prevWriteIndexHead = getWriteIndexHead(startPos, noteIndex - 1);
	const int64 writeIndexHead = getWriteIndexHead(startPos, noteIndex);

	const auto skipReadIndex = Max(0ll, startPos - targetEvent.pressTimePos);

	const auto maxWriteCount = sampleCount - writeIndexHead;
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
		const double time = 1.0 * (startPos + writeIndex) / sourceWave.sampleRate();
		const double currentLevel = envelope.level(targetEvent, time) * currentVolume;

		if (1.0 * targetEvent.releaseTimePos / sourceWave.sampleRate() + envelope.noteTime(targetEvent) < time)
		{
			break;
		}

		const auto blendIndex = (startPos + writeIndex) - targetEvent.pressTimePos;
		if (1 <= noteIndex && blendIndex < BlendSampleCount)
		{
			const auto& prevEvent = m_noteEvents[noteIndex - 1];
			const auto& prevWave = attackKeys[prevEvent.attackIndex].wave;
			const auto [prevReadCount, prevEmptyCount] = readEmptyCount(startPos, sampleCount, noteIndex - 1);

			const double prevVolume = prevEvent.velocity / 127.0;

			//const double prevLevel = envelope.level(prevEvent.pressTime, prevEvent.releaseTime, time);
			const double prevLevel = envelope.level(prevEvent, time) * prevVolume;

			//Console << U"  " << Vec3(prevEvent.pressTime, prevEvent.releaseTime, time) << U"" << U" -> " << prevLevel;

			const int64 prevReadIndex = prevWriteCount + i;
			const auto sample0 = (prevReadIndex < prevWave.size() ? prevWave[prevReadIndex] : WaveSample::Zero()) * prevLevel;
			const auto sample1 = sourceWave[readIndex] * currentLevel;
			const double t = 1.0 * blendIndex / BlendSampleCount;
			const auto blendSample = sample0.lerp(sample1, t);
			left[writeIndex] += blendSample.left;
			right[writeIndex] += blendSample.right;
		}
		else
		{
			const auto sample1 = sourceWave[readIndex] * currentLevel;
			left[writeIndex] += sample1.left;
			right[writeIndex] += sample1.right;
		}
	}
}

void AudioKey::renderRelease(float* left, float* right, int64 startPos, int64 sampleCount, int64 noteIndex) const
{
	const auto& targetEvent = m_noteEvents[noteIndex];

	return;
	if (targetEvent.releaseIndex == -1)
	{
		return;
	}

	const auto& sourceWave = releaseKeys[targetEvent.releaseIndex].wave;

	const auto waveStartPos = Max(0ll, startPos - targetEvent.releaseTimePos);
	const auto maxGateSamples = noteIndex + 1 < m_noteEvents.size() ? m_noteEvents[noteIndex + 1].pressTimePos - targetEvent.releaseTimePos : sourceWave.lengthSample();

	const int64 prevWriteIndexHead = getWriteIndexHeadRelease(startPos, noteIndex - 1);
	const int64 writeIndexHead = getWriteIndexHeadRelease(startPos, noteIndex);

	const auto skipReadIndex = Max(0ll, startPos - targetEvent.releaseTimePos);

	const auto maxWriteCount = sampleCount - writeIndexHead;
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

	const double currentVolume = 1;

	const int64 prevWriteCount = writeIndexHead - prevWriteIndexHead;
	for (int64 i = Max(0ll, -writeIndexHead), j = 0; i < sampleReadCount; ++i, ++j)
	{
		//const int64 readIndex = startIndex + i;
		const int64 readIndex = i;
		//const int64 readIndex = startIndex + j;
		const int64 writeIndex = writeIndexHead + i;
		const double time = 1.0 * (startPos + writeIndex) / sourceWave.sampleRate();
		//const double currentLevel = currentVolume;

		//if (1.0 * targetEvent.releaseTimePos / sourceWave.sampleRate() + envelope.releaseTime < time)
		//{
		//	break;
		//}

		const auto blendIndex = (startPos + writeIndex) - targetEvent.releaseTimePos;
		if (1 <= noteIndex && m_noteEvents[noteIndex - 1].releaseIndex != -1 && blendIndex < BlendSampleCount)
		{
			const auto& prevEvent = m_noteEvents[noteIndex - 1];

			const auto& prevWave = releaseKeys[prevEvent.releaseIndex].wave;
			const auto [prevReadCount, prevEmptyCount] = readEmptyCountRelease(startPos, sampleCount, noteIndex - 1);

			const double prevVolume = 1;

			//const double prevLevel = envelope.level(prevEvent.pressTime, prevEvent.releaseTime, time);
			//const double prevLevel = envelope.level(prevEvent, time) * prevVolume;

			//Console << U"  " << Vec3(prevEvent.pressTime, prevEvent.releaseTime, time) << U"" << U" -> " << prevLevel;

			const int64 prevReadIndex = prevWriteCount + i;
			const auto sample0 = (prevReadIndex < prevWave.size() ? prevWave[prevReadIndex] : WaveSample::Zero());
			const auto sample1 = sourceWave[readIndex];
			const double t = 1.0 * blendIndex / BlendSampleCount;
			const auto blendSample = sample0.lerp(sample1, t);
			left[writeIndex] += blendSample.left;
			right[writeIndex] += blendSample.right;
		}
		else
		{
			const auto sample1 = sourceWave[readIndex];
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
	const auto& sourceWave = attackKeys[targetEvent.attackIndex].wave;
	const auto maxGateSamples = noteIndex + 1 < m_noteEvents.size() ? m_noteEvents[noteIndex + 1].pressTimePos - m_noteEvents[noteIndex].pressTimePos : static_cast<int64>(sourceWave.lengthSample());
	const auto maxReadCount = Min(static_cast<int64>(sourceWave.lengthSample()), maxGateSamples);
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
	const auto& sourceWave = releaseKeys[targetEvent.releaseIndex].wave;
	const auto maxGateSamples = noteIndex + 1 < m_noteEvents.size() ? m_noteEvents[noteIndex + 1].releaseTimePos - m_noteEvents[noteIndex].pressTimePos : static_cast<int64>(sourceWave.lengthSample());
	const auto maxReadCount = Min(static_cast<int64>(sourceWave.lengthSample()), maxGateSamples);
	const int64 writeIndexHead = getWriteIndexHeadRelease(startPos, noteIndex);

	const auto maxWriteCount = sampleCount - writeIndexHead;
	const auto sampleReadCount = Min(maxWriteCount, maxReadCount);
	const auto sampleEmptyCount = Min(maxWriteCount, maxGateSamples);

	return std::make_pair(sampleReadCount, sampleEmptyCount);
}
