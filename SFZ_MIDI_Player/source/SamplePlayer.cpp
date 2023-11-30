#pragma once
#include <Config.hpp>
#include <SamplePlayer.hpp>
#include <Animation.hpp>
#include <SFZLoader.hpp>
#include <PianoRoll.hpp>
#include <MIDILoader.hpp>
#include <SampleSource.hpp>
#include <AudioLoadManager.hpp>
#include <AudioStreamRenderer.hpp>
#include <Program.hpp>

namespace
{
	const Array<double> hs = { 1.5, 1.0, 2.0, 1.0, 1.5, 1.5, 1.0, 2.0, 1.0, 2.0, 1.0, 1.5, };
	const Array<double> ys = { 0.0, 1.0, 1.5, 3.0, 3.5, 5.0, 6.0, 6.5, 8.0, 8.5, 10.0, 10.5 };
	const Array<String> noteNames = { U"C",U"C#",U"D",U"D#",U"E",U"F",U"F#",U"G",U"G#",U"A",U"A#",U"B" };

	const std::set<int> whiteIndices = { 0,2,4,5,7,9,11 };
	const std::set<int> blackIndices = { 1,3,6,8,10 };

	enum class InstrumentType
	{
		Melody,
		Rhythm,
		Unknown,
	};

	InstrumentType ParseInstrumentType(const String& instrumentTypeStr)
	{
		if (instrumentTypeStr == U"melody")
		{
			return InstrumentType::Melody;
		}
		else if (instrumentTypeStr == U"rhythm")
		{
			return InstrumentType::Rhythm;
		}

		return InstrumentType::Unknown;
	}

	Array<uint8> ParseProgramNumber(const String& programNumberStr)
	{
		Array<uint8> programNumbers;

		for (auto str : programNumberStr.split(U','))
		{
			if (auto rangePos = str.indexOf(U".."); rangePos != String::npos)
			{
				const auto rangeBeginStr = str.substrView(0, rangePos);
				const auto rangeEndStr = str.substrView(rangePos + 2);

				const auto rangeBegin = ParseInt<uint8>(rangeBeginStr);
				const auto rangeEnd = ParseInt<uint8>(rangeEndStr);

				for (uint8 i = rangeBegin; i <= rangeEnd; ++i)
				{
					programNumbers.push_back(i);
				}
			}
			else
			{
				const auto programNumber = ParseInt<uint8>(str);
				programNumbers.push_back(programNumber);
			}
		}

		return programNumbers;
	}
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

void SamplePlayer::loadSoundSet(FilePathView soundSetTomlPath)
{
	TOMLReader soundSetReader(soundSetTomlPath);
	if (!soundSetReader)
	{
		Print << U"TOMLファイルの読み込みに失敗しました: " << soundSetTomlPath;
		return;
	}

	m_programChangeNumberToSoundSetIndex.resize(128);
	for (auto& soundSetIndex : m_programChangeNumberToSoundSetIndex)
	{
		soundSetIndex = 0;
	}

	m_soundSet.clear();
	m_drumKit.clear();

	for (const auto& instrument : soundSetReader[U"Instrument"].tableArrayView())
	{
		const auto sourcePath = instrument[U"source"].getString();
		if (!FileSystem::Exists(sourcePath))
		{
			Print << U"\"{}\" ファイルが見つかりません。サウンドセットの読み込みに失敗しました: "_fmt(sourcePath) << soundSetTomlPath;
			continue;
		}

		if (U"sfz" != FileSystem::Extension(sourcePath))
		{
			Print << U"\"{}\" sfzでないファイルが指定されました。サウンドセットの読み込みに失敗しました: "_fmt(sourcePath) << soundSetTomlPath;
			continue;
		}

		const auto volumeVal = instrument[U"volume"];
		float volume = 0.0f;
		if (!volumeVal.isEmpty())
		{
			if (auto opt = volumeVal.getOpt<float>())
			{
				volume = opt.value();
			}
		}

		Program soundProgram;
		soundProgram.loadProgram(LoadSfz(sourcePath), volume);

		const auto typeStr = instrument[U"type"].getString();
		const auto type = ParseInstrumentType(typeStr);

		if (type == InstrumentType::Melody)
		{
			const auto soundSetIndex = static_cast<uint8>(m_soundSet.size());
			const auto programStr = instrument[U"program"].getString();
			const auto programNumberList = ParseProgramNumber(programStr);
			for (auto num : programNumberList)
			{
				const auto programIndex = static_cast<int32>(num) - 1;
				m_programChangeNumberToSoundSetIndex[programIndex] = soundSetIndex;
			}

			m_soundSet.push_back(soundProgram);
		}
		else if (type == InstrumentType::Rhythm)
		{
			m_drumKit.push_back(soundProgram);
		}
		else
		{
			Print << U"\"{}\" 不明なインストゥルメントタイプです。サウンドセットの読み込みに失敗しました: "_fmt(typeStr) << soundSetTomlPath;
			continue;
		}
	}
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

void SamplePlayer::drawVertical(const PianoRoll& pianoroll, const Optional<MidiData>& midiDataOpt) const
{
	const int octaveMin = m_octaveMin + 1;
	//const int octaveMax = m_octaveMax + 1;

	//const double currentTick = pianoroll.currentTick();
	const double currentSeconds = pianoroll.currentSeconds();

	HashTable<int, std::pair<double, int>> pressedKeyTick;
	if (midiDataOpt)
	{
		const auto& tracks = midiDataOpt.value().notes();
		for (const auto& [i, track] : Indexed(tracks))
		{
			if (track.isPercussionTrack())
			{
				continue;
			}

			for (const auto& note : track.notes())
			{
				if (note.beginSec <= currentSeconds && currentSeconds < note.endSec)
				{
					pressedKeyTick[note.key] = std::make_pair(currentSeconds - note.beginSec, static_cast<int>(i));
				}
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

void SamplePlayer::drawHorizontal(const PianoRoll& pianoroll, const Optional<MidiData>& midiDataOpt) const
{
	const int octaveMin = m_octaveMin + 1;
	//const int octaveMax = m_octaveMax + 1;

	//const double currentTick = pianoroll.currentTick();
	const double currentSeconds = pianoroll.currentSeconds();

	HashTable<int, std::pair<double, int>> pressedKeyTick;
	if (midiDataOpt)
	{
		const auto& tracks = midiDataOpt.value().notes();
		for (const auto& [i, track] : Indexed(tracks))
		{
			if (track.isPercussionTrack())
			{
				continue;
			}

			for (const auto& note : track.notes())
			{
				if (note.beginSec <= currentSeconds && currentSeconds < note.endSec)
				{
					pressedKeyTick[note.key] = std::make_pair(currentSeconds - note.beginSec, static_cast<int>(i));
				}
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

Array<NoteEvent> SamplePlayer::loadMidiData(const MidiData& midiData)
{
	for (auto& program: m_soundSet)
	{
		program.clearEvent();
	}

	for (auto& program : m_drumKit)
	{
		program.clearEvent();
	}

	//for (const auto& track : midiData.notes())
	//{
	//	if (auto programPtr = refProgram(track))
	//	{
	//		programPtr->addKeyDownEvents(midiData, track);
	//	}
	//}

	//for (auto& program : m_soundSet)
	//{
	//	program.sortKeyDownEvents();
	//}

	//for (auto& program : m_drumKit)
	//{
	//	program.sortKeyDownEvents();
	//}

	Array<NoteEvent> results;

	for (const auto& track : midiData.notes())
	{
		if (auto programPtr = refProgram(track))
		{
			const auto eventsData = programPtr->addEvents(midiData, track);
			results.append(eventsData);
		}
	}

	for (auto& program : m_soundSet)
	{
		program.sortEvent();
		program.resolveKeyIndex();
		program.deleteDuplicate();
		program.calculateOffTime();
		program.setEachKeyEvent();
	}

	for (auto& program : m_drumKit)
	{
		program.sortEvent();
		program.resolveKeyIndex();
		program.deleteDuplicate();
		program.calculateOffTime();
		program.setEachKeyEvent();
	}

	return results;
}

void SamplePlayer::getSamples(float* left, float* right, int64 startPos, int64 sampleCount)
{
	for (int i = 0; i < sampleCount; ++i)
	{
		left[i] = right[i] = 0;
	}

	for (auto& program : m_soundSet)
	{
		program.getSamples(left, right, startPos, sampleCount);
	}

	for (auto& program : m_drumKit)
	{
		program.getSamples(left, right, startPos, sampleCount);
	}
}

Program* SamplePlayer::refProgram(const TrackData& trackData)
{
	if (trackData.isPercussionTrack())
	{
		if (!m_drumKit.empty())
		{
			return &m_drumKit[0];
		}
	}
	else
	{
		const auto programNumer = trackData.program();
		const auto soundSetIndex = m_programChangeNumberToSoundSetIndex[programNumer];
		return &m_soundSet[soundSetIndex];
	}

	return nullptr;
}
