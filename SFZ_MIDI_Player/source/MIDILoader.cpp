#pragma once
#include <MIDILoader.hpp>
#include <Utility.hpp>

MetaEventData MetaEventData::Error()
{
	MetaEventData data;
	//data.isError = true;
	data.type = MetaEventType::Error;
	return data;
}

MetaEventData MetaEventData::EndOfTrack()
{
	MetaEventData data;
	//data.isEndOfTrack = true;
	data.type = MetaEventType::EndOfTrack;
	return data;
}

MetaEventData MetaEventData::SetMetre(uint32 numerator, uint32 denominator)
{
	MetaEventData data;
	data.type = MetaEventType::SetMetre;
	data.eventData = MetreData{ numerator,denominator };
	return data;
}

MetaEventData MetaEventData::SetTempo(double bpm)
{
	MetaEventData data;
	data.type = MetaEventType::Tempo;
	data.tempo = bpm;
	return data;
}

MidiEventData MidiEventData::NoteOff(uint8 ch, uint8 key)
{
	MidiEventData d;
	d.type = MidiEventType::NoteOff;
	d.channel = ch;
	d.key = key;
	return d;
}

MidiEventData MidiEventData::NoteOn(uint8 ch, uint8 key, uint8 velocity)
{
	MidiEventData d;
	d.channel = ch;
	d.key = key;
	if (velocity == 0)
	{
		d.type = MidiEventType::NoteOff;
	}
	else
	{
		d.type = MidiEventType::NoteOn;
		d.velocity = velocity;
	}
	return d;
}

MidiEventData MidiEventData::PolyphonicKeyPressure(uint8 ch, uint8 key, uint8 velocity)
{
	MidiEventData d;
	d.type = MidiEventType::PolyphonicKeyPressure;
	d.channel = ch;
	d.key = key;
	d.velocity = velocity;
	return d;
}

MidiEventData MidiEventData::ControlChange(uint8 ch, uint8 type, uint8 value)
{
	MidiEventData d;
	d.type = MidiEventType::ControlChange;
	d.channel = ch;
	d.changeType = type;
	d.value = value;
	return d;
}

MidiEventData MidiEventData::ProgramChange(uint8 ch, uint8 type)
{
	MidiEventData d;
	d.type = MidiEventType::ProgramChange;
	d.channel = ch;
	d.changeType = type;
	return d;
}

MidiEventData MidiEventData::ChannelPressure(uint8 ch, uint8 velocity)
{
	MidiEventData d;
	d.type = MidiEventType::ChannelPressure;
	d.channel = ch;
	d.velocity = velocity;
	return d;
}

MidiEventData MidiEventData::PitchBend(uint8 ch, uint16 value)
{
	MidiEventData d;
	d.type = MidiEventType::ChannelPressure;
	d.channel = ch;
	d.value = value;
	return d;
}

void TrackData::init()
{
	HashTable<int, Note> onNotes;

	for (const auto& code : m_operations)
	{
		if (code.type == EventType::MidiEvent)
		{
			const auto& midiEvent = std::get<MidiEventData>(code.data);

			switch (midiEvent.type)
			{
			case MidiEventType::NoteOff:
			{
				auto it = onNotes.find(midiEvent.key);
				if (it != onNotes.end())
				{
					it->second.gate = code.tick - it->second.tick;
					m_notes.push_back(it->second);
					onNotes.erase(midiEvent.key);
				}
				break;
			}
			case MidiEventType::NoteOn:
			{
				auto it = onNotes.find(midiEvent.key);
				if (it != onNotes.end())
				{
					it->second.gate = code.tick - it->second.tick;
					m_notes.push_back(it->second);
					onNotes.erase(midiEvent.key);
				}
				auto& newNote = onNotes[midiEvent.key];
				newNote.tick = code.tick;
				newNote.key = midiEvent.key;
				newNote.velocity = midiEvent.velocity;
				newNote.ch = midiEvent.channel;
				break;
			}
			case MidiEventType::PolyphonicKeyPressure:
				break;
			case MidiEventType::ControlChange:
				break;
			case MidiEventType::ProgramChange:
				break;
			case MidiEventType::ChannelPressure:
				break;
			case MidiEventType::PitchBend:
				break;
			default: break;
			}
		}
	}
}

void TrackData::outputLog(TextWriter& writer) const
{
	for (const auto& note : m_notes)
	{
		writer << U"tick: " << note.tick << U", key: " << note.key << U", gate: " << note.gate;
	}
}

void Measure::outputLog(TextWriter& writer) const
{
	writer << U"measure: " << measureIndex;
	writer << U"tick: " << globalTick;

	for (const auto& beat : beats)
	{
		writer << U"  beat: " << beat.localTick;
	}
}

void MidiData::init()
{
	m_measures.clear();

	for (const auto& track : m_tracks)
	{
		for (const auto& code : track.m_operations)
		{
			if (code.type == EventType::MetaEvent)
			{
				const auto& metaEvent = std::get<MetaEventData>(code.data);
				if (metaEvent.type == MetaEventType::SetMetre)
				{
					MeasureInfo info;
					info.metre = metaEvent.eventData;
					info.globalTick = code.tick;
					m_measures.push_back(info);
				}
			}
		}
	}

	m_measures.sort_by([](const MeasureInfo& a, const MeasureInfo& b) { return a.globalTick < b.globalTick; });

	m_bpmSetEvents = BPMSetEvents();

	for (auto& track : m_tracks)
	{
		for (auto& note : track.m_notes)
		{
			note.beginSec = ticksToSeconds(note.tick);
			note.endSec = ticksToSeconds(note.tick + note.gate);
		}
	}
}

Array<Measure> MidiData::getMeasures() const
{
	Array<Measure> result;

	//uint32 currentMeasure = 0;
	uint32 prevEventTick = 0;
	uint32 currentNumerator = 4;
	uint32 currentDenominator = 4;

	const auto addMeasures = [&](uint32 nextTick)
	{
		const uint32 measureWidthOfTick = m_resolution * 4 * currentNumerator / currentDenominator;
		for (uint32 tick = prevEventTick; tick < nextTick; tick += measureWidthOfTick)
		{
			Measure newMeasure;
			newMeasure.globalTick = tick;
			newMeasure.measureIndex = static_cast<uint32>(result.size());
			newMeasure.beatStep = measureWidthOfTick / currentNumerator;

			for (uint32 beatIndex = 0; beatIndex < currentNumerator; ++beatIndex)
			{
				Beat beat;
				beat.localTick = measureWidthOfTick * beatIndex / currentNumerator;
				newMeasure.beats.push_back(beat);
			}

			result.push_back(newMeasure);
		}
	};

	for (const auto& measureData : m_measures)
	{
		// 拍子イベントは必ず小節の先頭にある前提
		const uint32 currentTick = measureData.globalTick;
		addMeasures(currentTick);

		currentNumerator = measureData.metre.numerator;
		currentDenominator = measureData.metre.denominator;
		prevEventTick = measureData.globalTick;
	}

	addMeasures(endTick());

	return result;
}

uint32 MidiData::endTick() const
{
	uint32 maxTick = 0;
	for (const auto& track : m_tracks)
	{
		maxTick = Max(maxTick, track.m_operations.back().tick);
	}
	return maxTick;
}

double MidiData::getBPM() const
{
	for (const auto& track : m_tracks)
	{
		for (const auto& code : track.m_operations)
		{
			if (code.type == EventType::MetaEvent)
			{
				const auto& metaEvent = std::get<MetaEventData>(code.data);
				if (metaEvent.type == MetaEventType::Tempo)
				{
					return metaEvent.tempo;
				}
			}
		}
	}

	return 120.0;
}

double MidiData::ticksToSeconds(int64 currentTick) const
{
	const double resolution = m_resolution;
	double sumOfTime = 0;
	int64 lastBPMSetTick = 0;
	double lastTickToSec = 60.0 / (resolution * 120.0);
	for (const auto [tick, bpm] : m_bpmSetEvents)
	{
		if (currentTick <= tick)
		{
			return sumOfTime + lastTickToSec * (currentTick - lastBPMSetTick);
		}

		sumOfTime += lastTickToSec * (tick - lastBPMSetTick);
		lastBPMSetTick = tick;
		lastTickToSec = 60.0 / (resolution * bpm);
	}

	return sumOfTime + lastTickToSec * (currentTick - lastBPMSetTick);
}

int64 MidiData::secondsToTicks(double seconds) const
{
	const double resolution = m_resolution;
	double sumOfTime = 0;
	int64 lastBPMSetTick = 0;
	double lastBPM = 120;
	//double lastTickToSec = (60.0 / (resolution * lastBPM));
	for (const auto [tick, bpm] : m_bpmSetEvents)
	{
		const double nextSumOfTime = sumOfTime + (60.0 / (resolution * lastBPM)) * (tick - lastBPMSetTick);
		if (sumOfTime <= seconds && seconds < nextSumOfTime)
		{
			const double secToTicks = (resolution * lastBPM) / 60.0;
			return lastBPMSetTick + static_cast<int64>(Math::Round((seconds - sumOfTime) * secToTicks));
		}

		sumOfTime = nextSumOfTime;
		lastBPMSetTick = tick;
		lastBPM = bpm;
		//lastTickToSec = 60.0 / (resolution * bpm);
	}

	const double secToTicks = (resolution * lastBPM) / 60.0;
	return lastBPMSetTick + static_cast<int64>(Math::Round((seconds - sumOfTime) * secToTicks));
}
double MidiData::secondsToTicks2(double seconds) const
{
	const double resolution = m_resolution;
	double sumOfTime = 0;
	int64 lastBPMSetTick = 0;
	double lastBPM = 120;
	//double lastTickToSec = (60.0 / (resolution * lastBPM));
	for (const auto [tick, bpm] : m_bpmSetEvents)
	{
		const double nextSumOfTime = sumOfTime + (60.0 / (resolution * lastBPM)) * (tick - lastBPMSetTick);
		if (sumOfTime <= seconds && seconds < nextSumOfTime)
		{
			const double secToTicks = (resolution * lastBPM) / 60.0;
			return lastBPMSetTick + (seconds - sumOfTime) * secToTicks;
		}

		sumOfTime = nextSumOfTime;
		lastBPMSetTick = tick;
		lastBPM = bpm;
		//lastTickToSec = 60.0 / (resolution * bpm);
	}

	const double secToTicks = (resolution * lastBPM) / 60.0;
	return lastBPMSetTick + (seconds - sumOfTime) * secToTicks;
}

// tick -> BPM
std::map<int64, double> MidiData::BPMSetEvents() const
{
	std::map<int64, double> result;
	for (const auto& track : m_tracks)
	{
		for (const auto& code : track.m_operations)
		{
			if (code.type == EventType::MetaEvent)
			{
				const auto& metaEvent = std::get<MetaEventData>(code.data);
				if (metaEvent.type == MetaEventType::Tempo)
				{
					result[code.tick] = metaEvent.tempo;
				}
			}
		}
	}

	return result;
}

bool MidiData::intersects(uint32 range0begin, uint32 range0end, uint32 range1begin, uint32 range1end) const
{
	const bool notIntersects = range0end < range1begin || range1end < range0begin;
	return !notIntersects;
}

// https://sites.google.com/site/yyagisite/material/smfspec
// http://quelque.sakura.ne.jp/midi_meta.html
MetaEventData ReadMetaEvent(BinaryReader& reader, TextWriter& debugLog)
{
	const uint8 metaEventType = ReadBytes<uint8>(reader);
	switch (metaEventType)
	{
	case 0x0:
	{
		debugLog << U"error: シーケンス番号（非対応フォーマット）";
		return MetaEventData::Error();
	}
	case 0x1:
	{
		debugLog << U"テキストイベント";
		const auto text = ReadText(reader);
		debugLog << Unicode::FromUTF8(text);
		return MetaEventData();
	}
	case 0x2:
	{
		debugLog << U"著作権表示";
		const uint8 length = ReadBytes<uint8>(reader);
		Array<char> chars(length + 1ull, '\0');
		reader.read(chars.data(), length);
		debugLog << Unicode::FromUTF8(std::string(chars.data()));
		return MetaEventData();
	}
	case 0x3:
	{
		debugLog << U"シーケンス名/トラック名";
		const auto text = ReadText(reader);
		debugLog << Unicode::FromUTF8(text);
		return MetaEventData();
	}
	case 0x4:
	{
		debugLog << U"楽器名";
		const auto text = ReadText(reader);
		debugLog << Unicode::FromUTF8(text);
		return MetaEventData();
	}
	case 0x5:
	{
		debugLog << U"歌詞";
		const auto text = ReadText(reader);
		debugLog << Unicode::FromUTF8(text);
		return MetaEventData();
	}
	case 0x6:
	{
		debugLog << U"マーカー";
		const auto text = ReadText(reader);
		debugLog << Unicode::FromUTF8(text);
		return MetaEventData();
	}
	case 0x7:
	{
		debugLog << U"キューポイント";
		const auto text = ReadText(reader);
		debugLog << Unicode::FromUTF8(text);
		return MetaEventData();
	}
	case 0x8:
	{
		debugLog << U"プログラム名";
		const auto text = ReadText(reader);
		debugLog << Unicode::FromUTF8(text);
		return MetaEventData();
	}
	case 0x9:
	{
		debugLog << U"デバイス名";
		const auto text = ReadText(reader);
		debugLog << Unicode::FromUTF8(text);
		return MetaEventData();
	}
	case 0x20:
	{
		debugLog << U"MIDIチャンネルプリフィクス";
		ReadBytes<uint8>(reader);
		const uint8 data = ReadBytes<uint8>(reader);
		return MetaEventData();
	}
	case 0x21:
	{
		debugLog << U"ポート指定";
		ReadBytes<uint8>(reader);
		const uint8 data = ReadBytes<uint8>(reader);
		return MetaEventData();
	}
	case 0x2f:
	{
		debugLog << U"end of track";
		ReadBytes<uint8>(reader);
		return MetaEventData::EndOfTrack();
	}
	case 0x51:
	{
		//debugLog << U"テンポ指定 at " << reader.getPos();
		ReadBytes<uint8>(reader);//==3

		const auto a = ReadBytes<uint8>(reader);
		const auto b = ReadBytes<uint8>(reader);
		const auto c = ReadBytes<uint8>(reader);
		const auto microSecPerBeat = 1.0 * ((a << 16) + (b << 8) + c);

		const double bpm = 1.e6 * 60.0 / microSecPerBeat;
		debugLog << U"テンポ: " << bpm;
		return MetaEventData::SetTempo(bpm);
	}
	case 0x54:
	{
		debugLog << U"SMPTEオフセット";
		ReadBytes<uint8>(reader);
		ReadBytes<uint8>(reader);
		ReadBytes<uint8>(reader);
		ReadBytes<uint8>(reader);
		ReadBytes<uint8>(reader);
		ReadBytes<uint8>(reader);
		return MetaEventData();
	}
	case 0x58:
	{
		//https://nekonenene.hatenablog.com/entry/2017/02/26/001351
		//debugLog << U"拍子設定";
		ReadBytes<uint8>(reader);
		const uint8 numerator = ReadBytes<uint8>(reader);
		const uint8 denominator = ReadBytes<uint8>(reader);
		debugLog << U"拍子: " << numerator << U"/" << (1 << denominator);
		ReadBytes<uint8>(reader);
		ReadBytes<uint8>(reader);
		return MetaEventData::SetMetre(numerator, (1 << denominator));
	}
	case 0x59:
	{
		debugLog << U"調号";
		ReadBytes<uint8>(reader);
		ReadBytes<uint8>(reader);
		ReadBytes<uint8>(reader);
		return MetaEventData();
	}
	case 0x7f:
	{
		debugLog << U"シーケンサ固有メタイベント";
		const uint8 length = ReadBytes<uint8>(reader);
		Array<uint8> data(length);
		reader.read(data.data(), length);
		return MetaEventData();
	}
	default:
		debugLog << U" unknown metaEvent: " << metaEventType;
		return MetaEventData::Error();
	}
}

Optional<MidiData> LoadMidi(FilePathView path)
{
	TextWriter debugLog(U"debugLog.txt");
	TextWriter debugLog2(U"debugLog2.txt");
	debugLog << U"open \"" << path << U"\"";
	BinaryReader reader(path);

	if (!reader.isOpen())
	{
		debugLog << U"couldn't open file";
		return none;
	}

	char mthd[5] = {};
	reader.read(mthd, 4);
	if (std::string(mthd) != "MThd")
	{
		debugLog << U"error: std::string(mthd) != \"MThd\"";
		return none;
	}

	const uint32 headerLength = ReadBytes<uint32>(reader);
	if (headerLength != 6)
	{
		debugLog << U"error: headerLength != 6";
		return none;
	}

	const uint16 format = ReadBytes<uint16>(reader);
	if (format != 1)
	{
		debugLog << U"error: format != 1";
		return none;
	}

	uint16 trackCount = ReadBytes<uint16>(reader);
	debugLog << U"tracks: " << trackCount;
	//midiData.tracks.resize(trackCount);

	const uint16 resolution = ReadBytes<uint16>(reader);
	debugLog << U"resolution: " << resolution;
	//midiData.resolution = resolution;
	/*{
		uint8 bytes[2] = {};
		reader.read(bytes, 2);
		resolution = (bytes[0] << 8) + (bytes[1] << 0);
		debugLog << U"resolution : " << resolution;
	}*/

	Array<TrackData> tracks;

	for (uint32 trackIndex = 0; trackIndex < trackCount; ++trackIndex)
	{
		char mtrk[5] = {};
		reader.read(mtrk, 4);
		if (std::string(mtrk) != "MTrk")
		{
			debugLog << U"error: std::string(str) != \"MTrk\"";
			return none;
		}

		uint32 trackBytesLength = ReadBytes<uint32>(reader);
		debugLog << U"trackLength: " << trackBytesLength;

		Array<MidiCode> trackData;
		//TrackData trackData;

		//debugLog2 << U"track " << trackIndex;

		uint32 currentTick = 0;

		for (;;)
		{
			MidiCode codeData;

			Array<uint8> nextTick;
			for (;;)
			{
				uint8 byte = ReadBytes<uint8>(reader);
				if (byte < 0x80)
				{
					nextTick.push_back(byte);
					break;
				}
				else
				{
					nextTick.push_back(byte);
				}
			}

			if (5 <= nextTick.size())
			{
				debugLog << U"error: 5 <= nextTick.size()";
				return none;
			}
			const uint32 step = GetTick(nextTick);
			currentTick += step;
			codeData.tick = currentTick;
			//debugLog2 << U"measure: " << (tick / resolution + 1) << U", tick: " << tick % resolution << U" | " << tick;

			const uint8 opcode = ReadBytes<uint8>(reader);

			//if (0xFF != opcode)
			{
				//debugLog2 << U"measure: " << (currentTick / resolution + 1) << U", tick: " << currentTick % resolution << U" | " << step;
			}

			// https://sites.google.com/site/yyagisite/material/smfspec
			if (0x80 <= opcode && opcode <= 0x8F)
			{
				debugLog << U"ノートオフ";
				const uint8 channelIndex = opcode - 0x80;
				const uint8 key = ReadBytes<uint8>(reader);
				ReadBytes<uint8>(reader);
				codeData.type = EventType::MidiEvent;
				codeData.data = MidiEventData::NoteOff(channelIndex, key);
			}
			else if (0x90 <= opcode && opcode <= 0x9F)
			{
				debugLog << U"ノートオン";
				const uint8 channelIndex = opcode - 0x90;
				const uint8 key = ReadBytes<uint8>(reader);
				const uint8 velocity = ReadBytes<uint8>(reader);
				codeData.type = EventType::MidiEvent;
				codeData.data = MidiEventData::NoteOn(channelIndex, key, velocity);
			}
			else if (0xA0 <= opcode && opcode <= 0xAF)
			{
				debugLog << U"ポリフォニックキープレッシャー";
				const uint8 channelIndex = opcode - 0xA0;
				const uint8 key = ReadBytes<uint8>(reader);
				const uint8 velocity = ReadBytes<uint8>(reader);
				codeData.type = EventType::MidiEvent;
				codeData.data = MidiEventData::PolyphonicKeyPressure(channelIndex, key, velocity);
			}
			else if (0xB0 <= opcode && opcode <= 0xBF)
			{
				const uint8 channelIndex = opcode - 0xB0;
				const uint8 changeType = ReadBytes<uint8>(reader);
				const uint8 controlChangeData = ReadBytes<uint8>(reader);
				debugLog << U"コントロールチェンジ " << changeType;
				codeData.type = EventType::MidiEvent;
				codeData.data = MidiEventData::ControlChange(channelIndex, changeType, controlChangeData);
			}
			else if (0xC0 <= opcode && opcode <= 0xCF)
			{
				debugLog << U"プログラムチェンジ";
				const uint8 channelIndex = opcode - 0xC0;
				const uint8 programNumber = ReadBytes<uint8>(reader);
				codeData.type = EventType::MidiEvent;
				codeData.data = MidiEventData::ProgramChange(channelIndex, programNumber);
			}
			else if (0xD0 <= opcode && opcode <= 0xDF)
			{
				debugLog << U"チャンネルプレッシャー";
				const uint8 channelIndex = opcode - 0xD0;
				const uint8 velocity = ReadBytes<uint8>(reader);
				codeData.type = EventType::MidiEvent;
				codeData.data = MidiEventData::ChannelPressure(channelIndex, velocity);
			}
			else if (0xE0 <= opcode && opcode <= 0xEF)
			{
				debugLog << U"ピッチベンド";
				const uint8 channelIndex = opcode - 0xE0;
				const uint8 m = ReadBytes<uint8>(reader);
				const uint8 l = ReadBytes<uint8>(reader);
				const uint16 value = ((l & 0x7F) << 7) + (m & 0x7F);
				codeData.type = EventType::MidiEvent;
				codeData.data = MidiEventData::PitchBend(channelIndex, value);
			}
			else if (0xF0 == opcode)
			{
				debugLog << U"SysEx イベント";
				codeData.type = EventType::SysExEvent;
				while (ReadBytes<uint8>(reader) != 0xF7) {}
			}
			else if (0xF7 == opcode)
			{
				debugLog << U"error: SysEx イベント（非対応フォーマット）";
				return none;
			}
			else if (0xFF == opcode)
			{
				const auto result = ReadMetaEvent(reader, debugLog);
				codeData.type = EventType::MetaEvent;
				codeData.data = result;

				if (result.isEndOfTrack())
				{
					trackData.push_back(codeData);
					break;
				}
				else if (result.isError())
				{
					return none;
				}
			}
			else
			{
				debugLog << U" unknown opcode: " << opcode;
				return none;
			}

			trackData.push_back(codeData);
		}

		tracks.emplace_back(trackData);
	}

	for (const auto& [i, track] : Indexed(tracks))
	{
		debugLog2 << U"-------------" << U" Track " << i << U" " << U"-------------";
		track.outputLog(debugLog2);
	}

	MidiData midiData(tracks, resolution);
	debugLog << U"read succeeded";

	return midiData;
}
