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

SamplePlayer::SamplePlayer(const Rect& area) :
	m_area(area)
{
	auto inputs = m_observer.get_input_ports();
	if (1 <= inputs.size())
	{
		const auto callback = [&](const libremidi::message& message)
		{
			m_mutex.lock();

			switch (static_cast<libremidi::message_type>(message.get_message_type()))
			{
			case libremidi::message_type::NOTE_OFF:
				m_keyData.push_back({ message.bytes[1], false });
				break;
			case libremidi::message_type::NOTE_ON:
				m_keyData.push_back({ message.bytes[1], true });
				break;
			case libremidi::message_type::PITCH_BEND:
			{
				setReady(true);
				//const uint8 m = message.bytes[1];
				//const uint8 l = message.bytes[2];
				//const uint16 value = ((l & 0x7F) << 7) + (m & 0x7F);
				//const double t = (value / 8192.0 - 1.0);
				break;
			}
			default:
				break;
			}

			m_mutex.unlock();
		};

		const libremidi::input_configuration input_config{ .on_message = callback };
		m_midiin = std::make_unique<libremidi::midi_in>(input_config, libremidi::midi_in_configuration_for(m_observer));
		m_midiin->open_port(inputs[0]);
		if (!m_midiin->is_port_connected())
		{
			Console << U"Failed to connect to MIDI input port\n";
		}
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
	return octaveMax() - octaveMin() + 1;
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

int SamplePlayer::octaveMin() const
{
	return m_keyMin / 12;
}
int SamplePlayer::octaveMax() const
{
	return (m_keyMax - 11) / 12;
}

RectF SamplePlayer::getRect(int octaveIndex, int noteIndex, bool isWhiteKey) const
{
	const double octaveBottomY = m_area.y + m_area.h - octaveHeight() * octaveIndex;
	const double noteBottomY = octaveBottomY - unitHeight() * ys[noteIndex];
	const double noteHeight = unitHeight() * hs[noteIndex];

	return RectF(m_area.x, noteBottomY - noteHeight, m_area.w * (isWhiteKey ? 1. : 2. / 3.), noteHeight);
}

void SamplePlayer::drawVertical(const PianoRoll& pianoroll, const Optional<MidiData>& midiDataOpt) const
{
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

	for (int key = m_keyMin; key <= m_keyMax; ++key)
	{
		const int octaveAbs = static_cast<int>(floor(key / 12.0));
		const int noteIndex = key - octaveAbs * 12;
		if (!whiteIndices.contains(noteIndex))
		{
			continue;
		}

		const int octaveIndex = octaveAbs - octaveMin();
		const auto rect = getRect(octaveIndex, noteIndex, true);

		rect.draw(whiteKeyColor);
		{
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
		}

		rect.drawFrame(1, frameColor);

		if (noteIndex == 0)
		{
			m_font(noteNames[noteIndex], octaveAbs - 1, U" ").draw(Arg::bottomRight = rect.br(), Palette::Black);
		}
	}

	for (int key = m_keyMin; key <= m_keyMax; ++key)
	{
		const int octaveAbs = static_cast<int>(floor(key / 12.0));
		const int noteIndex = key - octaveAbs * 12;
		if (!blackIndices.contains(noteIndex))
		{
			continue;
		}

		const int octaveIndex = octaveAbs - octaveMin();
		const auto rect = getRect(octaveIndex, noteIndex, false);

		rect.draw(blackKeyColor);
		{
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
		}
		

		rect.drawFrame(1, frameColor);
	}
}

void SamplePlayer::drawHorizontal(const PianoRoll& pianoroll, const Optional<MidiData>& midiDataOpt, bool isDebug) const
{
	const double currentSeconds = pianoroll.currentSeconds();
	double checkTime = 0.4;
	double goodTime = 0.2;
	double perfectTime = 0.1;

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

			if (i == m_activeTrack)
			{
				for (const auto& note : track.notes())
				{
					if (m_keyState[note.key].keyDownCount == 1 && m_noteGrade[&note] == NoteGrade::Undetermined)
					{
						if (note.beginSec - checkTime <= currentSeconds && currentSeconds < note.beginSec + checkTime)
						{
							if (note.beginSec - perfectTime <= currentSeconds && currentSeconds < note.beginSec + perfectTime)
							{
								m_noteGrade[&note] = NoteGrade::Perfect;
								m_keyState[note.key].noteGrade = NoteGrade::Perfect;
							}
							else if (note.beginSec - goodTime <= currentSeconds && currentSeconds < note.beginSec + goodTime)
							{
								m_noteGrade[&note] = NoteGrade::Good;
								m_keyState[note.key].noteGrade = NoteGrade::Good;
							}
							else
							{
								m_noteGrade[&note] = NoteGrade::Bad;
								m_keyState[note.key].noteGrade = NoteGrade::Bad;
							}

							m_keyState[note.key].keyEffectWatch.start();
						}
					}
					else if (m_noteGrade[&note] == NoteGrade::Undetermined && note.beginSec + checkTime < currentSeconds)
					{
						m_noteGrade[&note] = NoteGrade::Miss;
						m_keyState[note.key].noteGrade = NoteGrade::Miss;
						m_keyState[note.key].keyEffectWatch.start();
					}
				}

			}
			else
			{
				for (const auto& note : track.notes())
				{
					if (note.beginSec <= currentSeconds && currentSeconds < note.endSec)
					{
						pressedKeyTick[note.key] = std::make_pair(currentSeconds - note.beginSec, static_cast<int>(i));
					}
				}
			}
		}
	}

	const Color whiteKeyColor = Color(49, 48, 56);
	const Color blackKeyColor = Color(20, 19, 18);
	const Color frameColor = blackKeyColor;

	size_t whiteKeyCount = 0;
	for (int key = m_keyMin; key <= m_keyMax; ++key)
	{
		const int octaveAbs = static_cast<int>(floor(key / 12.0));
		const int noteIndex = key - octaveAbs * 12;
		if (whiteIndices.contains(noteIndex))
		{
			++whiteKeyCount;
		}
	}

	const double effectTime = 0.3;

	const double unitWidth = m_area.w / whiteKeyCount;
	const double blackUnitWidth = unitWidth * 7.0 / 12.0;

	double x = m_area.x;
	for (int key = m_keyMin, i = 0; key <= m_keyMax; ++key, ++i)
	{
		const int octaveAbs = static_cast<int>(floor(key / 12.0));
		const int noteIndex = key - octaveAbs * 12;
		if (!whiteIndices.contains(noteIndex))
		{
			continue;
		}

		const RectF rect(x, m_area.y, unitWidth, m_area.h);
		const auto whiteSmallRect = RectF(m_area.x + blackUnitWidth * i, m_area.y, blackUnitWidth, m_area.h * m_blackHeight).stretched(blackUnitWidth * m_stretch, 0);

		rect.draw(whiteKeyColor);
		{
			auto itKey = pressedKeyTick.find(key);
			const bool isNoteOn = itKey != pressedKeyTick.end();
			const bool isPressed = static_cast<bool>(1 <= m_keyState[key].keyDownCount);

			auto& keyState = m_keyState[key];

			if (1 <= m_keyState[key].keyDownCount)
			{
				rect.draw(Palette::Cyan);
			}

			if (keyState.keyEffectWatch.isRunning())
			{
				if (effectTime < keyState.keyEffectWatch.sF())
				{
					keyState.keyEffectWatch.reset();
				}
				else
				{
					const double t = EaseInCirc(1.0 - keyState.keyEffectWatch.sF() / effectTime);
					
					const auto fontPos = whiteSmallRect.topCenter() + Vec2(0, -30 + 10 * (1.0 - t));

					switch (keyState.noteGrade)
					{
					case NoteGrade::Perfect:
						m_gradeFont(U"Perfect").drawAt(fontPos + Vec2(1, 1), Palette::Black);
						m_gradeFont(U"Perfect").drawAt(fontPos, Palette::Orange);
						break;
					case NoteGrade::Good:
						m_gradeFont(U"Good").drawAt(fontPos + Vec2(1, 1), Palette::Black);
						m_gradeFont(U"Good").drawAt(fontPos, Palette::Blue);
						break;
					case NoteGrade::Bad:
						m_gradeFont(U"Bad").drawAt(fontPos + Vec2(1, 1), Palette::Black);
						m_gradeFont(U"Bad").drawAt(fontPos, Palette::Purple);
						break;
					case NoteGrade::Miss:
						m_gradeFont(U"Miss").drawAt(fontPos + Vec2(1, 1), Palette::Black);
						m_gradeFont(U"Miss").drawAt(fontPos, Palette::Brown);
						break;
					default:
						break;
					}
				}
			}

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
		}

		rect.drawFrame(1, frameColor);

		if (isDebug)
		{
			Line(rect.topCenter(), rect.bottomCenter()).draw(1.0, Palette::Orange);
		}

		if (noteIndex == 0)
		{
			m_font(noteNames[noteIndex], octaveAbs - 1, U" ").draw(Arg::bottomRight = rect.br(), Palette::Black);
		}
		x += unitWidth;
	}

	for (int key = m_keyMin, i = 0; key <= m_keyMax; ++key, ++i)
	{
		const int octaveAbs = static_cast<int>(floor(key / 12.0));
		const int noteIndex = key - octaveAbs * 12;
		if (!blackIndices.contains(noteIndex))
		{
			continue;
		}

		const auto rect = RectF(m_area.x + blackUnitWidth * i, m_area.y, blackUnitWidth, m_area.h * m_blackHeight).stretched(blackUnitWidth * m_stretch, 0);

		rect.draw(blackKeyColor);

		{
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
		}

		if (1 <= m_keyState[key].keyDownCount)
		{
			rect.draw(Palette::Cyan);
		}

		auto& keyState = m_keyState[key];
		if (keyState.keyEffectWatch.isRunning())
		{
			if (effectTime < keyState.keyEffectWatch.sF())
			{
				keyState.keyEffectWatch.reset();
			}
			else
			{
				const double t = EaseInCirc(1.0 - keyState.keyEffectWatch.sF() / effectTime);

				const auto fontPos = rect.topCenter() + Vec2(0, -30 + 10 * (1.0 - t));

				switch (keyState.noteGrade)
				{
				case NoteGrade::Perfect:
					m_gradeFont(U"Perfect").drawAt(fontPos + Vec2(1, 1), Palette::Black);
					m_gradeFont(U"Perfect").drawAt(fontPos, Palette::Orange);
					break;
				case NoteGrade::Good:
					m_gradeFont(U"Good").drawAt(fontPos + Vec2(1, 1), Palette::Black);
					m_gradeFont(U"Good").drawAt(fontPos, Palette::Blue);
					break;
				case NoteGrade::Bad:
					m_gradeFont(U"Bad").drawAt(fontPos + Vec2(1, 1), Palette::Black);
					m_gradeFont(U"Bad").drawAt(fontPos, Palette::Purple);
					break;
				case NoteGrade::Miss:
					m_gradeFont(U"Miss").drawAt(fontPos + Vec2(1, 1), Palette::Black);
					m_gradeFont(U"Miss").drawAt(fontPos, Palette::Brown);
					break;
				default:
					break;
				}
			}
		}

		if (isDebug)
		{
			rect.drawFrame(1, Palette::Cyan);
		}
	}

	if (isDebug)
	{
		m_area.drawFrame(2, Palette::Orange);
	}
}

Array<std::pair<uint8, NoteEvent>> SamplePlayer::loadMidiData(const MidiData& midiData)
{
	for (auto& program: m_soundSet)
	{
		program.clearEvent();
	}

	for (auto& program : m_drumKit)
	{
		program.clearEvent();
	}

	for (const auto& track : midiData.notes())
	{
		if (auto programPtr = refProgram(track))
		{
			programPtr->addKeyDownEvents(midiData, track);
		}
	}

	for (auto& program : m_soundSet)
	{
		program.sortKeyDownEvents();
	}

	for (auto& program : m_drumKit)
	{
		program.sortKeyDownEvents();
	}

	Array<std::pair<uint8, NoteEvent>> results;

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
		program.deleteDuplicate();
		program.calculateOffTime();
	}

	for (auto& program : m_drumKit)
	{
		program.sortEvent();
		program.deleteDuplicate();
		program.calculateOffTime();
	}

	m_noteGrade.clear();
	for (const auto& track : midiData.notes())
	{
		for (const auto& note : track.notes())
		{
			m_noteGrade[&note] = NoteGrade::Undetermined;
		}
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

void SamplePlayer::updateInput()
{
	if (KeyShift.pressed())
	{
		if (KeyLeft.down())
		{
			m_blackHeight -= 0.01;
		}
		if (KeyRight.down())
		{
			m_blackHeight += 0.01;
		}
	}
	else
	{
		if (KeyLeft.down())
		{
			m_stretch -= 0.01;
		}
		if (KeyRight.down())
		{
			m_stretch += 0.01;
		}
	}

	for (auto& keyState : m_keyState)
	{
		if (1 <= keyState.keyDownCount)
		{
			++keyState.keyDownCount;
		}
	}

	m_mutex.lock();
	if (!m_keyData.empty())
	{
		const auto& note = m_keyData.front();

		m_keyState[note.key].keyDownCount = note.note_on ? 1 : 0;

		m_keyData.pop_front();
	}
	m_mutex.unlock();
}

void SamplePlayer::setArea(const Rect& area)
{
	m_area = area;
}
