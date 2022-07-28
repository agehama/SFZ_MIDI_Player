﻿#pragma once
#include <SamplePlayer.hpp>
#include <Animation.hpp>
#include <SFZLoader.hpp>
#include <PianoRoll.hpp>
#include <MIDILoader.hpp>
#include <SampleSource.hpp>

namespace
{
	const Array<double> hs = { 1.5, 1.0, 2.0, 1.0, 1.5, 1.5, 1.0, 2.0, 1.0, 2.0, 1.0, 1.5, };
	const Array<double> ys = { 0.0, 1.0, 1.5, 3.0, 3.5, 5.0, 6.0, 6.5, 8.0, 8.5, 10.0, 10.5 };
	const Array<String> noteNames = { U"C",U"C#",U"D",U"D#",U"E",U"F",U"F#",U"G",U"G#",U"A",U"A#",U"B" };

	const std::set<int> whiteIndices = { 0,2,4,5,7,9,11 };
	const std::set<int> blackIndices = { 1,3,6,8,10 };
}

namespace
{
	double GetAnimationValue(double time)
	{
		const Array<double> times = { 0, 0.1025, };
		const Array<KeyFrame> keyframes = {
			KeyFrame(1, 0.56818, 0.47727, 4.77273, InterpolateType::ExpBlend),
			KeyFrame(0, 0.5, 0.5, 0.5, InterpolateType::RationalBezier),
		};

		auto it = std::upper_bound(times.begin(), times.end(), time);
		if (it == times.begin() || it == times.end())
		{
			return 0;
		}

		const auto nextIndex = std::distance(times.begin(), it);
		const auto prevIndex = nextIndex - 1;

		const auto& prevKeyFrame = keyframes[prevIndex];
		const auto& nextKeyFrame = keyframes[nextIndex];

		const double t = Math::InvLerp(times[prevIndex], times[nextIndex], time);
		const double v = prevKeyFrame.interpolate(t);

		return Math::Lerp(prevKeyFrame.value, nextKeyFrame.value, v);
	}
}

Wave SetSpeedWave(const Wave& original, int semitone)
{
	const double speed = std::exp2(semitone / 12.0);
	const double scale = 1.0 / speed;
	const auto sampleCount = static_cast<int64>(Math::Ceil(original.lengthSample() * scale));

	Wave wave(sampleCount);
	for (auto i : step(sampleCount))
	{
		const double readIndex = i * speed;
		const auto prevIndex = static_cast<int64>(Floor(readIndex));
		const auto nextIndex = static_cast<int64>(Ceil(readIndex));
		const double t = Math::Fmod(readIndex, 1.0);
		wave[i] = original[prevIndex].lerp(original[nextIndex], t);

		//const auto originalIndex = static_cast<int64>(Math::Round(i * speed));
		//wave[i] = original[originalIndex];
	}

	return wave;
}

Wave SetTuneWave(const Wave& original, int32 tune)
{
	const double speed = std::exp2(tune / 1200.0);
	const double scale = 1.0 / speed;
	const auto sampleCount = static_cast<int64>(Math::Ceil(original.lengthSample() * scale));

	Wave wave(sampleCount);
	for (auto i : step(sampleCount))
	{
		const double readIndex = i * speed;
		const auto prevIndex = static_cast<int64>(Floor(readIndex));
		const auto nextIndex = Min(static_cast<int64>(Ceil(readIndex)), static_cast<int64>(original.size() - 1));
		const double t = Math::Fmod(readIndex, 1.0);
		wave[i] = original[prevIndex].lerp(original[nextIndex], t);

		//const auto originalIndex = static_cast<int64>(Math::Round(i * speed));
		//wave[i] = original[originalIndex];
	}

	return wave;
}

void SetVolume(Wave& wave, double volume)
{
	const double scale = std::pow(10.0, volume / 20.0) * 0.5;
	for (auto& sample : wave)
	{
		sample *= scale;
	}
}

void SetRtDecay(Wave& wave, double rt_decay)
{
	for (auto [i, sample] : IndexedRef(wave))
	{
		const double seconds = 1.0 * i / wave.sampleRate();
		const double currentVolume = -seconds * rt_decay;
		const double scale = std::pow(10.0, currentVolume / 20.0) * 0.5;
		sample *= scale;
	}
}


void SamplePlayer::loadData(const SfzData& sfzData)
{
	if (m_audioKeys.size() != 255)
	{
		m_audioKeys = Array<AudioKey>(255);
	}

	for (auto [i, audioKey] : IndexedRef(m_audioKeys))
	{
		audioKey.noteKey = static_cast<int8>(i - 127);
		audioKey.attackKeys.clear();
		audioKey.releaseKeys.clear();
	}

	for (const auto& data : sfzData.data)
	{
		const auto samplePath = sfzData.dir + data.sample;
		if (!FileSystem::Exists(samplePath))
		{
			Console << U"file does not exist: \"" << samplePath << U"\"";
		}
		const Wave wave(samplePath);

		const int loIndex = data.lokey + 127;
		const int hiIndex = data.hikey + 127;

		for (int index = loIndex; index <= hiIndex; ++index)
		{
			const int key = index - 127;

			AudioSource source;
			source.lovel = data.lovel;
			source.hivel = data.hivel;
			source.tune = (key - data.pitch_keycenter) * 100 + data.tune;
			source.wave = wave;

			source.envelope.attackTime = data.ampeg_attack;
			source.envelope.decayTime = data.ampeg_decay;
			source.envelope.sustainLevel = data.ampeg_sustain / 100.0;
			source.envelope.releaseTime = data.ampeg_release;

			if (source.tune == 0)
			{
				source.wave = wave;
			}
			else
			{
				source.wave = SetTuneWave(wave, source.tune);
			}

			SetVolume(source.wave, data.volume);

			if (data.trigger == Trigger::Attack)
			{
				m_audioKeys[index].attackKeys.push_back(source);
			}
			else if (data.trigger == Trigger::Release)
			{
				SetRtDecay(source.wave, data.rt_decay);
				m_audioKeys[index].releaseKeys.push_back(source);
			}
		}
	}

	//for (const auto& key : m_audioKeys)
	//{
	//	key.debugPrint();
	//}
}

int SamplePlayer::octaveCount() const
{
	return m_octaveMax - m_octaveMin + 1;
}
double SamplePlayer::octaveHeight() const
{
	return m_area.h / octaveCount();
}
double SamplePlayer::unitHeight() const
{
	return octaveHeight() / 12.0;
}

double SamplePlayer::octaveWidth() const
{
	return m_area.w / octaveCount();
}
double SamplePlayer::unitWidth() const
{
	return octaveWidth() / 12.0;
}

int SamplePlayer::keyMin() const
{
	return (m_octaveMin + 1) * 12;
}
int SamplePlayer::keyMax() const
{
	return (m_octaveMax + 1) * 12 + 11;
}

RectF SamplePlayer::getRect(int octaveIndex, int noteIndex, bool isWhiteKey) const
{
	const double octaveBottomY = m_area.y + m_area.h - octaveHeight() * octaveIndex;
	const double noteBottomY = octaveBottomY - unitHeight() * ys[noteIndex];
	const double noteHeight = unitHeight() * hs[noteIndex];

	return RectF(m_area.x, noteBottomY - noteHeight, m_area.w * (isWhiteKey ? 1. : 2. / 3.), noteHeight);
}

RectF SamplePlayer::getHorizontalRect(int octaveIndex, int noteIndex, bool isWhiteKey) const
{
	const double octaveLeftX = m_area.x + octaveWidth() * octaveIndex;
	const double noteLeftX = octaveLeftX + unitWidth() * ys[noteIndex];
	const double noteWidth = unitWidth() * hs[noteIndex];

	return RectF(noteLeftX, m_area.y, noteWidth, m_area.h * (isWhiteKey ? 1. : 2. / 3.));
}

void SamplePlayer::drawVertical2(const PianoRoll& pianoroll, const MidiData& midiData) const
{
	const int octaveMin = m_octaveMin + 1;
	const int octaveMax = m_octaveMax + 1;

	const double currentTick = pianoroll.currentTick();
	const double currentSeconds = pianoroll.currentSeconds();

	const auto& tracks = midiData.notes();

	HashTable<int, std::pair<double, int>> pressedKeyTick;
	for (const auto& [i, track] : Indexed(tracks))
	{
		if (i == 10)
		{
			continue;
		}

		for (const auto& note : track.notes())
		{
			if (note.beginSec <= currentSeconds && currentSeconds < note.endSec)
			{
				pressedKeyTick[note.key] = std::make_pair(currentSeconds - note.beginSec, i);
			}
		}
	}

	const Color whiteKeyColor = Color(49, 48, 56);
	const Color blackKeyColor = Color(20, 19, 18);
	const Color frameColor = blackKeyColor;

	for (int key = keyMin(); key <= keyMax(); ++key)
	{
		const int octaveAbs = static_cast<int>(floor(key / 12.0));
		const int noteIndex = key - octaveAbs * 12;
		if (!whiteIndices.contains(noteIndex))
		{
			continue;
		}

		const int octaveIndex = octaveAbs - octaveMin;
		const auto rect = getRect(octaveIndex, noteIndex, true);

		rect.draw(whiteKeyColor);
		auto itKey = pressedKeyTick.find(key);
		if (itKey != pressedKeyTick.end())
		{
			const auto [pressSeconds, trackIndex] = itKey->second;
			const double t = GetAnimationValue(pressSeconds);
			
			const double hue = 360.0 * trackIndex / 16.0;
			ColorF color00 = HSV(hue, 0.6, 0.92);
			ColorF color01 = HSV(hue, 0.81, 0.84);
			ColorF color0 = color00.lerp(color01, t);
			ColorF color1 = Color(180, 180, 180);
			ColorF blendColor(Math::Saturate(color0.toVec4() / (Vec4::One() - color1.toVec4() * Min(0.999, t))).xyz());
			rect.draw(blendColor);
		}

		rect.drawFrame(1, frameColor);

		if (noteIndex == 0)
		{
			m_font(noteNames[noteIndex], octaveAbs - 1, U" ").draw(Arg::bottomRight = rect.br(), Palette::Black);
		}
	}

	for (int key = keyMin(); key <= keyMax(); ++key)
	{
		const int octaveAbs = static_cast<int>(floor(key / 12.0));
		const int noteIndex = key - octaveAbs * 12;
		if (!blackIndices.contains(noteIndex))
		{
			continue;
		}

		const int octaveIndex = octaveAbs - octaveMin;
		const auto rect = getRect(octaveIndex, noteIndex, false);

		rect.draw(blackKeyColor);
		auto itKey = pressedKeyTick.find(key);
		if (itKey != pressedKeyTick.end())
		{
			const auto [pressSeconds, trackIndex] = itKey->second;
			const double t = GetAnimationValue(pressSeconds);

			const double hue = 360.0 * trackIndex / 16.0;
			ColorF color00 = HSV(hue, 0.6, 0.92);
			ColorF color01 = HSV(hue, 0.81, 0.84);
			ColorF color0 = color00.lerp(color01, t);
			ColorF color1 = Color(180, 180, 180);
			ColorF blendColor(Math::Saturate(color0.toVec4() / (Vec4::One() - color1.toVec4() * Min(0.999, t))).xyz());
			rect.draw(blendColor);
		}

		rect.drawFrame(1, frameColor);
	}
}

void SamplePlayer::drawHorizontal(const PianoRoll& pianoroll, const MidiData& midiData) const
{
	const int octaveMin = m_octaveMin + 1;
	const int octaveMax = m_octaveMax + 1;

	const double currentTick = pianoroll.currentTick();
	const double currentSeconds = pianoroll.currentSeconds();

	const auto& tracks = midiData.notes();

	HashTable<int, std::pair<double, int>> pressedKeyTick;
	for (const auto& [i, track] : Indexed(tracks))
	{
		if (i == 10)
		{
			continue;
		}

		for (const auto& note : track.notes())
		{
			if (note.beginSec <= currentSeconds && currentSeconds < note.endSec)
			{
				pressedKeyTick[note.key] = std::make_pair(currentSeconds - note.beginSec, i);
			}
		}
	}

	const Color whiteKeyColor = Color(49, 48, 56);
	const Color blackKeyColor = Color(20, 19, 18);
	const Color frameColor = blackKeyColor;

	for (int key = keyMin(); key <= keyMax(); ++key)
	{
		const int octaveAbs = static_cast<int>(floor(key / 12.0));
		const int noteIndex = key - octaveAbs * 12;
		if (!whiteIndices.contains(noteIndex))
		{
			continue;
		}

		const int octaveIndex = octaveAbs - octaveMin;
		const auto rect = getHorizontalRect(octaveIndex, noteIndex, true);

		rect.draw(whiteKeyColor);
		auto itKey = pressedKeyTick.find(key);
		if (itKey != pressedKeyTick.end())
		{
			const auto [pressSeconds, trackIndex] = itKey->second;
			const double t = GetAnimationValue(pressSeconds);

			const double hue = 360.0 * trackIndex / 16.0;
			ColorF color00 = HSV(hue, 0.6, 0.92);
			ColorF color01 = HSV(hue, 0.81, 0.84);
			ColorF color0 = color00.lerp(color01, t);
			ColorF color1 = Color(180, 180, 180);
			ColorF blendColor(Math::Saturate(color0.toVec4() / (Vec4::One() - color1.toVec4() * Min(0.999, t))).xyz());
			rect.draw(blendColor);
		}

		rect.drawFrame(1, frameColor);

		if (noteIndex == 0)
		{
			m_font(noteNames[noteIndex], octaveAbs - 1, U" ").draw(Arg::bottomRight = rect.br(), Palette::Black);
		}
	}

	for (int key = keyMin(); key <= keyMax(); ++key)
	{
		const int octaveAbs = static_cast<int>(floor(key / 12.0));
		const int noteIndex = key - octaveAbs * 12;
		if (!blackIndices.contains(noteIndex))
		{
			continue;
		}

		const int octaveIndex = octaveAbs - octaveMin;
		const auto rect = getHorizontalRect(octaveIndex, noteIndex, false);

		rect.draw(blackKeyColor);
		auto itKey = pressedKeyTick.find(key);
		if (itKey != pressedKeyTick.end())
		{
			const auto [pressSeconds, trackIndex] = itKey->second;
			const double t = GetAnimationValue(pressSeconds);

			const double hue = 360.0 * trackIndex / 16.0;
			ColorF color00 = HSV(hue, 0.6, 0.92);
			ColorF color01 = HSV(hue, 0.81, 0.84);
			ColorF color0 = color00.lerp(color01, t);
			ColorF color1 = Color(180, 180, 180);
			ColorF blendColor(Math::Saturate(color0.toVec4() / (Vec4::One() - color1.toVec4() * Min(0.999, t))).xyz());
			rect.draw(blendColor);
		}

		rect.drawFrame(1, frameColor);
	}
}

const NoteEvent& SamplePlayer::addEvent(uint8 key, uint8 velocity, int64 pressTimePos, int64 releaseTimePos)
{
	return m_audioKeys[key + 127].addEvent(velocity, pressTimePos, releaseTimePos);
}

void SamplePlayer::sortEvent()
{
	for (uint8 index = 127; index < 255; ++index)
	{
		if (!m_audioKeys[index].attackKeys.empty())
		{
			m_audioKeys[index].sortEvent();
		}
	}
}

void SamplePlayer::deleteDuplicate()
{
	for (uint8 index = 127; index < 255; ++index)
	{
		if (!m_audioKeys[index].attackKeys.empty())
		{
			m_audioKeys[index].deleteDuplicate();
		}
	}
}

void SamplePlayer::getSamples3(float* left, float* right, int64 startPos, int64 sampleCount) const
{
	for (int i = 0; i < sampleCount; ++i)
	{
		left[i] = right[i] = 0;
	}
	for (uint8 index = 127; index < 255; ++index)
	{
		if (!m_audioKeys[index].attackKeys.empty())
		{
			m_audioKeys[index].getSamples3(left, right, startPos, sampleCount);
		}
	}
}

void SamplePlayer::clearEvent()
{
	for (uint8 index = 127; index < 255; ++index)
	{
		m_audioKeys[index].clearEvent();
	}
}

Array<NoteEvent> SamplePlayer::addEvents(const MidiData& midiData)
{
	clearEvent();

	const double resolution = midiData.resolution();
	const auto tracks = midiData.notes();

	Array<NoteEvent> results;
	for (const auto& [i, track] : Indexed(tracks))
	{
		if (i == 10)
		{
			continue;
		}

		if (track.notes().empty())
		{
			continue;
		}

		int shift = 0;

		for (const auto& note : track.notes())
		{
			const uint32 beginTick = note.tick + (shift * i);
			const uint32 endTick = note.tick + note.gate + (shift * i);

			const double beginSec = midiData.ticksToSeconds(beginTick);
			const double endSec = midiData.ticksToSeconds(endTick);

			const int64 pressTimePos = static_cast<int64>(Math::Round(beginSec * Wave::DefaultSampleRate));
			const int64 releaseTimePos = static_cast<int64>(Math::Round(endSec * Wave::DefaultSampleRate));

			const NoteEvent noteEvent = addEvent(note.key, note.velocity, pressTimePos, releaseTimePos);
			results.push_back(noteEvent);
		}
	}

	sortEvent();

	deleteDuplicate();

	return results;
}

void SamplerAudioStream::getAudio(float* left, float* right, const size_t samplesToWrite)
{
	if (!m_pianoroll.get().isPlaying())
	{
		return;
	}

	m_samplePlayer.get().getSamples3(left, right, m_pos, samplesToWrite);
	m_pos += samplesToWrite;
}

void AudioRenderer::getAudio(float* left, float* right, int64 startPos, int64 sampleCount)
{
	m_samplePlayer.get().getSamples3(left, right, startPos, sampleCount);
}