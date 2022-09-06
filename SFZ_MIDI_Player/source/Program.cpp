#pragma once
#include <Config.hpp>
#include <Program.hpp>
#include <SFZLoader.hpp>
#include <MIDILoader.hpp>
#include <SampleSource.hpp>
#include <AudioLoadManager.hpp>
#include <AudioStreamRenderer.hpp>

namespace
{
	// 戻り値：[beginIndex, endIndex)
	Optional<std::pair<uint32, uint32>> GetRangeEventIndex(const Array<KeyDownEvent>& keyDownEvents, int64 rangeBegin, int64 rangeEnd)
	{
		// 同時刻も有効範囲に含めたいので-1のupper_boundを取る
		KeyDownEvent e0(0, rangeBegin - 1, 0);
		auto beginIt = std::upper_bound(keyDownEvents.begin(), keyDownEvents.end(), e0,
			[](const KeyDownEvent& a, const KeyDownEvent& b) { return a.pressTimePos < b.pressTimePos; });

		if (beginIt == keyDownEvents.end())
		{
			return none;
		}

		// todo: ノートオフ以降のoff_byは無視しているが、これで正しいのか？
		KeyDownEvent e1(0, rangeEnd, 0);
		auto nextEndIt = std::upper_bound(keyDownEvents.begin(), keyDownEvents.end(), e1,
			[](const KeyDownEvent& a, const KeyDownEvent& b) { return a.pressTimePos < b.pressTimePos; });

		if (nextEndIt == keyDownEvents.begin())
		{
			return none;
		}

		const auto index0 = static_cast<uint32>(std::distance(keyDownEvents.begin(), beginIt));
		const auto index1 = static_cast<uint32>(std::distance(keyDownEvents.begin(), nextEndIt));

		return std::make_pair(index0, index1);
	}
}

void Program::loadProgram(const SfzData& sfzData)
{
	if (m_audioKeys.size() != 255)
	{
		m_audioKeys = Array<AudioKey>(255);
	}

	for (auto [i, audioKey] : IndexedRef(m_audioKeys))
	{
		audioKey.init(static_cast<int8>(i - 127));
	}

	HashTable<String, OscillatorType> oscTypes;
	oscTypes[U"*sine"] = OscillatorType::Sine;
	oscTypes[U"*tri"] = OscillatorType::Tri;
	oscTypes[U"*triangle"] = OscillatorType::Tri;
	oscTypes[U"*saw"] = OscillatorType::Saw;
	oscTypes[U"*square"] = OscillatorType::Square;
	oscTypes[U"*noise"] = OscillatorType::Noise;
	oscTypes[U"*silence"] = OscillatorType::Silence;

	Window::SetTitle(U"音源読み込み中：0 %");
	for (const auto& [i, data] : Indexed(sfzData.data))
	{
		const auto progress = 1.0 * i / sfzData.data.size();
		Window::SetTitle(Format(U"音源読み込み中：", Math::Round(progress * 100), U" %"));

		const auto samplePath = sfzData.dir + data.sample;

		Optional<size_t> waveIndexOpt;
		if (!data.sample.starts_with(U"*"))
		{
			if (!FileSystem::IsFile(samplePath))
			{
#ifdef DEVELOPMENT
				Console << U"error: file does not exist: \"" << samplePath << U"\"";
#endif
				continue;
			}

			waveIndexOpt = AudioLoadManager::i().load(samplePath);
		}

		const Envelope envelope(data.ampeg_attack, data.ampeg_decay, data.ampeg_sustain / 100.0, data.ampeg_release);

		const float volume = data.volume;
		const float amplitude = static_cast<float>(std::pow(10.0, volume / 20.0) * 0.5);

		const int32 loIndex = data.lokey + 127;
		const int32 hiIndex = data.hikey + 127;

		for (int32 index = loIndex; index <= hiIndex; ++index)
		{
			const int32 key = index - 127;
			const int32 tune = (key - data.pitch_keycenter) * 100 + data.tune;

			AudioSource source(amplitude, envelope, data.lovel, data.hivel, tune);

			if (waveIndexOpt)
			{
				source.setWaveIndex(waveIndexOpt.value());
			}
			else
			{
				const auto frequency = static_cast<float>(440.0 * pow(2.0, (key - 69) / 12.0));
				const auto oscType = oscTypes.at(data.sample);
				source.setOscillator(oscType, frequency);
			}

			if (data.loopMode != LoopMode::Unspecified)
			{
				source.setLoopMode(data.loopMode);
			}

			source.setSwitch(data.sw_lokey, data.sw_hikey, data.sw_last, data.sw_default);

			float offTime = 0.006f;
			if (data.off_mode == OffMode::Time)
			{
				offTime = data.off_time;
			}
			else if (data.off_mode == OffMode::Normal)
			{
				offTime = data.ampeg_release;
			}
			source.setGroup(data.group, data.off_by, offTime);

			if (data.trigger == Trigger::Attack)
			{
				m_audioKeys[index].addAttackKey(source);
			}
			else if (data.trigger == Trigger::Release)
			{
				source.setRtDecay(data.rt_decay);
				m_audioKeys[index].addReleaseKey(source);
			}
		}
	}
}

void Program::clearEvent()
{
	for (uint8 index = 127; index < 255; ++index)
	{
		m_audioKeys[index].clearEvent();
	}

	m_keyDownEvents.clear();
}

void Program::addKeyDownEvents(const MidiData& midiData, const TrackData& trackData)
{
	for (const auto& note : trackData.notes())
	{
		const int64 beginTick = note.tick;
		const double beginSec = midiData.ticksToSeconds(beginTick);
		const int64 pressTimePos = static_cast<int64>(Math::Round(beginSec * Wave::DefaultSampleRate));

		m_keyDownEvents.emplace_back(note.key, pressTimePos, note.velocity);
	}
}

void Program::sortKeyDownEvents()
{
	m_keyDownEvents.sort_by([](const KeyDownEvent& a, const KeyDownEvent& b) { return a.pressTimePos < b.pressTimePos; });
}

Array<std::pair<uint8, NoteEvent>> Program::addEvents(const MidiData& midiData, const TrackData& trackData)
{
	Array<std::pair<uint8, NoteEvent>> results;

	for (const auto& note : trackData.notes())
	{
		const int64 beginTick = note.tick;
		const int64 endTick = note.tick + note.gate;

		const double beginSec = midiData.ticksToSeconds(beginTick);
		const double endSec = midiData.ticksToSeconds(endTick);

		const int64 pressTimePos = static_cast<int64>(Math::Round(beginSec * Wave::DefaultSampleRate));
		const int64 releaseTimePos = static_cast<int64>(Math::Round(endSec * Wave::DefaultSampleRate));

		const NoteEvent noteEvent = addEvent(note.key, note.velocity, pressTimePos, releaseTimePos, m_keyDownEvents);
		results.push_back(std::make_pair(note.key, noteEvent));
	}

	return results;
}

void Program::sortEvent()
{
	for (uint8 index = 127; index < 255; ++index)
	{
		if (m_audioKeys[index].hasAttackKey())
		{
			m_audioKeys[index].sortEvent();
		}
	}
}

void Program::deleteDuplicate()
{
	for (uint8 index = 127; index < 255; ++index)
	{
		if (m_audioKeys[index].hasAttackKey())
		{
			m_audioKeys[index].deleteDuplicate();
		}
	}
}

void Program::calculateOffTime()
{
	// off_byによるdisableTimeが決まるのは、sw_*などを考慮して各イベントに対応するAudioSourceが決まった後
	for (uint8 index = 127; index < 255; ++index)
	{
		const auto key = index - 127;
		if (m_audioKeys[index].hasAttackKey())
		{
			//m_audioKeys[index].sortEvent();
			auto& events = m_audioKeys[index].noteEvents();

			for (auto& noteEvent : events)
			{
				if (noteEvent.attackIndex == -1)
				{
					continue;
				}

				const auto& audioKey = m_audioKeys[key + 127];
				const auto& attackKey = audioKey.getAttackKey(noteEvent.attackIndex);
				const auto off_by = attackKey.offBy();
				if (off_by == 0)
				{
					continue;
				}

				if (auto rangeOpt = GetRangeEventIndex(m_keyDownEvents, noteEvent.pressTimePos, noteEvent.releaseTimePos))
				{
					const auto [beginIndex, endIndex] = rangeOpt.value();

					for (uint32 i = beginIndex; i < endIndex; ++i)
					{
						const auto& followKeyDown = m_keyDownEvents[i];

						// 自分自身だったら無視
						if (key == followKeyDown.key && noteEvent.pressTimePos == followKeyDown.pressTimePos)
						{
							continue;
						}

						const auto& followAudioKey = m_audioKeys[followKeyDown.key + 127];
						const auto followAttackIndex = followAudioKey.getAttackIndex(followKeyDown.velocity, followKeyDown.pressTimePos, m_keyDownEvents);
						if (followAttackIndex != -1)
						{
							const auto& followAttackKey = followAudioKey.getAttackKey(followAttackIndex);

							if (off_by == followAttackKey.group())
							{
								noteEvent.disableTimePos = followKeyDown.pressTimePos;
								break;
							}
						}
					}
				}
			}
		}
	}
}

void Program::getSamples(float* left, float* right, int64 startPos, int64 sampleCount)
{
	for (uint8 index = 127; index < 255; ++index)
	{
		if (m_audioKeys[index].hasAttackKey())
		{
			m_audioKeys[index].getSamples(left, right, startPos, sampleCount);
		}
	}
}

const NoteEvent& Program::addEvent(uint8 key, uint8 velocity, int64 pressTimePos, int64 releaseTimePos, const Array<KeyDownEvent>& history)
{
	return m_audioKeys[key + 127].addEvent(velocity, pressTimePos, releaseTimePos, history);
}
