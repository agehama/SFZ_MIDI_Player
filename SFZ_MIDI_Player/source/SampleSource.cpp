#pragma once
#include <AudioLoadManager.hpp>
#include <SampleSource.hpp>
#include <SamplePlayer.hpp>

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

AudioSource::AudioSource(float amplitude, const Envelope& envelope, uint8 lovel, uint8 hivel, int32 tune) :
	m_amplitude(amplitude),
	m_lovel(lovel),
	m_hivel(hivel),
	m_tune(tune),
	m_speed(static_cast<float>(std::exp2(m_tune / 1200.0))),
	m_envelope(envelope)
{}

void AudioSource::setOscillator(OscillatorType oscillatorType, float frequency)
{
	m_oscillatorType = oscillatorType;
	m_frequency = frequency;
}

bool AudioSource::isValidSwLast(int64 pressTimePos, const Array<KeyDownEvent>& history) const
{
	// 未設定の場合
	if (m_swLokey == m_swHikey)
	{
		return true;
	}

	for (int64 i = static_cast<int64>(history.size()) - 1; 0 <= i; --i)
	{
		if (pressTimePos <= history[i].pressTimePos)
		{
			continue;
		}

		const auto key = history[i].key;
		if (m_swLokey <= key && key <= m_swHikey)
		{
			return m_swLast == key;
		}
	}

	return m_swLast == m_swDefault;
}

void AudioSource::setRtDecay(float rtDecay)
{
	m_rtDecay = rtDecay;
}

size_t AudioSource::sampleRate() const
{
	if (isOscillator())
	{
		return Wave::DefaultSampleRate;
	}
	else
	{
		return getReader().sampleRate();
	}
}

size_t AudioSource::lengthSample() const
{
	if (isOscillator())
	{
		return std::numeric_limits<uint32>::max();
	}

	const auto& sourceWave = getReader();

	const float scale = 1.0f / m_speed;
	const auto sampleCount = static_cast<size_t>(Math::Ceil(sourceWave.lengthSample() * scale));

	return sampleCount;
}

inline double TriangleWave(double t, int n)
{
	double f = 0;
	for (int i = 1; i < n; ++i)
	{
		const double a = i * 2 - 1;
		const double coeff = pow(-1.0, i + 1) / (a * a);
		f += coeff * sin(a * t);
	}

	return 8.0 * f / 1_pi;
}

inline double SawtoothWave(double t, int n)
{
	double f = 0;
	for (int i = 1; i < n; ++i)
	{
		const double coeff = 2.0 * pow(-1.0, i + 1) / i;
		f += coeff * sin(i * t) / i;
	}

	return 2.0 * f / 1_pi;
}

inline double SquareWave(double t, int n)
{
	double f = 0;
	for (int i = 1; i < n; ++i)
	{
		const double a = i * 2 - 1;
		f += sin(a * t) / a;
	}

	return 4.0 * f / 1_pi;
}

// https://www.dsprelated.com/showcode/60.php
inline float PowerOf10(float x)
{
	return std::bit_cast<float>(static_cast<int32>(x * 27866352.6f + 1064866808.0f));
}

float AudioSource::getSpeed() const
{
	return m_speed;
}

WaveSample AudioSource::getSample(int64 index) const
{
	if (isOscillator())
	{
		const double t = 1.0 * index / Wave::DefaultSampleRate;
		const double x = t * m_frequency * 2_pi;

		switch (m_oscillatorType.value())
		{
		case OscillatorType::Sine:
		{
			const float osc = static_cast<float>(sin(x) * m_amplitude);
			return WaveSample(osc, osc);
		}
		case OscillatorType::Tri:
		{
			const float osc = static_cast<float>(TriangleWave(x, 20) * m_amplitude);
			return WaveSample(osc, osc);
		}
		case OscillatorType::Saw:
		{
			const float osc = static_cast<float>(SawtoothWave(x, 20) * m_amplitude);
			return WaveSample(osc, osc);
		}
		case OscillatorType::Square:
		{
			const float osc = static_cast<float>(SquareWave(x, 20) * m_amplitude);
			return WaveSample(osc, osc);
		}
		case OscillatorType::Noise:
		{
			const float osc = Random(-1.f, 1.f) * m_amplitude;
			return WaveSample(osc, osc);
		}
		default:
			return WaveSample(0, 0);
		}
	}

	const auto& sourceWave = getReader();

	float amplitude = m_amplitude;
	if (m_rtDecay)
	{
		const float seconds = static_cast<float>(index) * sourceWave.sampleRateInv();
		const float currentVolume = -seconds * m_rtDecay.value();
		const float scale = PowerOf10(currentVolume * 0.05f) * 0.5f;
		amplitude *= scale;
	}

	if (m_tune == 0)
	{
		return sourceWave.getSample(index) * amplitude;
	}

	const float readIndex = index * m_speed;
	const auto prevIndex = static_cast<int64>(Floor(readIndex));
	const auto nextIndex = Min(prevIndex + 1, static_cast<int64>(sourceWave.size() - 1));
	const float t = readIndex - prevIndex;

	return sourceWave.getSample(prevIndex).lerp(sourceWave.getSample(nextIndex), t) * amplitude;
}

void AudioSource::use(size_t beginSampleIndex, size_t sampleCount)
{
	if (!isOscillator())
	{
		getReader().use(beginSampleIndex, sampleCount);
	}
}

void AudioSource::unuse()
{
	if (!isOscillator())
	{
		getReader().unuse();
	}
}

const AudioLoaderBase& AudioSource::getReader() const
{
	return AudioLoadManager::i().reader(m_index);
}

AudioLoaderBase& AudioSource::getReader()
{
	return AudioLoadManager::i().reader(m_index);
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

const NoteEvent& AudioKey::addEvent(uint8 velocity, int64 pressTimePos, int64 releaseTimePos, const Array<KeyDownEvent>& history)
{
	const auto attackIndex = getAttackIndex(velocity, pressTimePos, history);
	const auto releaseIndex = getReleaseIndex(velocity);
	NoteEvent note(attackIndex, releaseIndex, pressTimePos, releaseTimePos, velocity);
	m_noteEvents.push_back(note);
	return m_noteEvents.back();
}

void AudioKey::clearEvent()
{
	m_noteEvents.clear();
}

int64 AudioKey::getAttackIndex(uint8 velocity, int64 pressTimePos, const Array<KeyDownEvent>& history) const
{
	for (auto [i, key] : Indexed(attackKeys))
	{
		if (key.isValidVelocity(velocity) && key.isValidSwLast(pressTimePos, history))
		{
			return static_cast<int64>(i);
		}
	}

	return -1;
}

const AudioSource& AudioKey::getAttackKey(int64 attackIndex) const
{
	return attackKeys[attackIndex];
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
	{
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
		}
	}

	if (!releaseKeys.empty())
	{
		auto maxReleaseTimeIt = std::max_element(releaseKeys.begin(), releaseKeys.end(),
			[](const AudioSource& a, const AudioSource& b) { return a.envelope().releaseTime() < b.envelope().releaseTime(); });

		const auto maxReleaseCount = static_cast<int64>(maxReleaseTimeIt->envelope().releaseTime() * Wave::DefaultSampleRate);

		NoteEvent note0(0, 0, 0, 0, 0);
		note0.pressTimePos = startPos - maxReleaseCount;
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
			renderRelease(left, right, startPos, sampleCount, noteIndex);
		}
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

	const auto& envelope = attackKey.envelope();

	const int64 prevWriteIndexHead = getWriteIndexHead(startPos, noteIndex - 1);
	const int64 writeIndexHead = getWriteIndexHead(startPos, noteIndex);

	const auto [sampleReadCount, sampleEmptyCount] = readEmptyCount(startPos, sampleCount, noteIndex);

	/*Console << noteIndex << U"[" << noteKey << U"]v" << targetEvent.index << U" -> "
		<< Point(targetEvent.pressTimePos, targetEvent.releaseTimePos)
		<< U", readStart: " << waveStartPos
		<< U", maxReadCount: " << maxGateSamples
		<< U", writeStart: " << writeIndexHead
		<< U", maxWriteCount: " << maxWriteCount
		<< U", sampleReadCount: " << sampleReadCount
		<< U", sampleEmptyCount: " << sampleEmptyCount;*/

	const double currentVolume = targetEvent.velocity / 127.0;
	const int64 prevWriteCount = writeIndexHead - prevWriteIndexHead;

	Stopwatch watch(StartImmediately::Yes);

	if (Max(0ll, -writeIndexHead) < sampleReadCount)
	{
		const double startTime = 1.0 * startPos / attackKey.sampleRate();
		if (startTime < 1.0 * targetEvent.pressTimePos / attackKey.sampleRate() + envelope.noteTime(targetEvent))
		{
			const auto speed = attackKey.getSpeed();
			const auto samples = static_cast<size_t>((sampleCount + 10) * speed);
			const auto sampleBegin = static_cast<size_t>(Max(0ll, -writeIndexHead) * speed);
			attackKey.use(sampleBegin, samples);

			const auto blendIndex = startPos - targetEvent.pressTimePos;
			if (1 <= noteIndex && blendIndex < BlendSampleCount)
			{
				const auto& prevEvent = m_noteEvents[noteIndex - 1];
				auto& prevAttackKey = attackKeys[prevEvent.attackIndex];

				if (startTime < 1.0 * prevEvent.pressTimePos / attackKey.sampleRate() + prevAttackKey.envelope().noteTime(prevEvent))
				{
					const auto [prevReadCount, prevEmptyCount] = readEmptyCount(startPos, sampleCount, noteIndex - 1);

					{
						const auto prevSpeed = prevAttackKey.getSpeed();
						const auto prevSamples = static_cast<size_t>((BlendSampleCount + 10) * prevSpeed);
						const auto prevSampleBegin = static_cast<size_t>(Max(0ll, prevWriteCount - writeIndexHead) * prevSpeed);
						prevAttackKey.use(prevSampleBegin, prevSamples);
					}
				}
			}
		}
	}

	SamplerAudioStream::time1 += watch.usF();
	watch.restart();

	for (int64 i = Max(0ll, -writeIndexHead), j = 0; i < sampleReadCount && j < sampleCount; ++i, ++j)
	{
		//const int64 readIndex = startIndex + i;
		const int64 readIndex = i;
		//const int64 readIndex = startIndex + j;
		const int64 writeIndex = writeIndexHead + i;
		const double time = 1.0 * (startPos + writeIndex) / attackKey.sampleRate();

		double disableCoeff = 1.0;
		if (targetEvent.disableTimePos)
		{
			const auto currentTimePos = startPos + writeIndex;
			const auto disableTimePos = targetEvent.disableTimePos.value();
			if (disableTimePos < currentTimePos)
			{
				const auto disableTime = 1.0 * (currentTimePos - disableTimePos) / attackKey.sampleRate();

				disableCoeff = 1.0 - Saturate(disableTime / attackKey.disableFadeSeconds());
			}
		}

		const double currentLevel = envelope.level(targetEvent, time) * currentVolume * disableCoeff;

		if (1.0 * targetEvent.pressTimePos / attackKey.sampleRate() + envelope.noteTime(targetEvent) < time)
		{
			break;
		}

		bool isBlendSample = false;

		const auto blendIndex = (startPos + writeIndex) - targetEvent.pressTimePos;
		if (1 <= noteIndex && blendIndex < BlendSampleCount)
		{
			const auto& prevEvent = m_noteEvents[noteIndex - 1];
			auto& prevAttackKey = attackKeys[prevEvent.attackIndex];

			if (time < 1.0 * prevEvent.pressTimePos / attackKey.sampleRate() + prevAttackKey.envelope().noteTime(prevEvent))
			{
				const auto [prevReadCount, prevEmptyCount] = readEmptyCount(startPos, sampleCount, noteIndex - 1);

				const double prevVolume = prevEvent.velocity / 127.0;
				const double prevLevel = envelope.level(prevEvent, time) * prevVolume;
				const int64 prevReadIndex = prevWriteCount + i;
				if (prevReadIndex < static_cast<int64>(prevAttackKey.lengthSample()))
				{
					const auto sample0 = prevAttackKey.getSample(prevReadIndex) * static_cast<float>(prevLevel);
					const auto sample1 = attackKey.getSample(readIndex) * static_cast<float>(currentLevel);
					const double t = 1.0 * blendIndex / BlendSampleCount;
					const auto blendSample = sample0.lerp(sample1, t);
					left[writeIndex] += blendSample.left;
					right[writeIndex] += blendSample.right;
					isBlendSample = true;
				}
			}
		}

		if (!isBlendSample)
		{
			const auto sample1 = attackKey.getSample(readIndex) * static_cast<float>(currentLevel);
			left[writeIndex] += sample1.left;
			right[writeIndex] += sample1.right;
		}
	}

	SamplerAudioStream::time2 += watch.usF();
}

void AudioKey::renderRelease(float* left, float* right, int64 startPos, int64 sampleCount, int64 noteIndex)
{
	const auto& targetEvent = m_noteEvents[noteIndex];

	if (targetEvent.releaseIndex == -1)
	{
		return;
	}

	Stopwatch watch(StartImmediately::Yes);

	auto& releaseKey = releaseKeys[targetEvent.releaseIndex];

	const int64 writeIndexHead = getWriteIndexHeadRelease(startPos, noteIndex);
	const int64 sampleReadCount = readCountRelease(startPos, sampleCount, noteIndex);

	if (Max(0ll, -writeIndexHead) < sampleReadCount)
	{
		const auto speed = releaseKey.getSpeed();
		const auto samples = static_cast<size_t>((sampleCount + 10) * speed);
		const auto sampleBegin = static_cast<size_t>(Max(0ll, -writeIndexHead) * speed);
		releaseKey.use(sampleBegin, samples);
	}

	SamplerAudioStream::time3 += watch.usF();
	watch.restart();

	for (int64 i = Max(0ll, -writeIndexHead), j = 0; i < sampleReadCount && j < sampleCount; ++i, ++j)
	{
		const int64 readIndex = i;
		const int64 writeIndex = writeIndexHead + i;

		const auto sample1 = releaseKey.getSample(readIndex);
		left[writeIndex] += sample1.left;
		right[writeIndex] += sample1.right;
	}

	SamplerAudioStream::time4 += watch.usF();
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

int64 AudioKey::readCountRelease(int64 startPos, int64 sampleCount, int64 noteIndex) const
{
	const auto& targetEvent = m_noteEvents[noteIndex];
	const auto& releaseKey = releaseKeys[targetEvent.releaseIndex];
	const auto maxReadCount = static_cast<int64>(releaseKey.lengthSample());
	const int64 writeIndexHead = getWriteIndexHeadRelease(startPos, noteIndex);

	const auto maxWriteCount = sampleCount - writeIndexHead;
	const auto sampleReadCount = Min(maxWriteCount, maxReadCount);

	return sampleReadCount;
}
