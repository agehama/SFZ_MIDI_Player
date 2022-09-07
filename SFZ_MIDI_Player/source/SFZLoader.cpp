#pragma once
#include <Config.hpp>
#include <SFZLoader.hpp>

namespace
{
	Trigger ParseTrigger(StringView trigger)
	{
		if (trigger == U"attack")
		{
			return Trigger::Attack;
		}
		else if (trigger == U"release")
		{
			return Trigger::Release;
		}
		else if (trigger == U"first")
		{
			return Trigger::First;
		}
		else if (trigger == U"legato")
		{
			return Trigger::Legato;
		}

		assert(false);
		return Trigger::Attack;
	}

	String TriggerToStr(Trigger trigger)
	{
		switch (trigger)
		{
		case Trigger::Attack:
			return U"attack";
		case Trigger::Release:
			return U"release";
		case Trigger::First:
			return U"first";
		case Trigger::Legato:
			return U"legato";
		default:
			assert(false);
			return U"";
		}
	}

	OffMode ParseOffMode(StringView trigger)
	{
		if (trigger == U"fast")
		{
			return OffMode::Fast;
		}
		else if (trigger == U"normal")
		{
			return OffMode::Normal;
		}
		else if (trigger == U"time")
		{
			return OffMode::Time;
		}

		assert(false);
		return OffMode::Fast;
	}

	std::pair<PolyphonyType, uint8> ParsePolyphony(StringView polyphony)
	{
		if (auto opt = ParseIntOpt<uint8>(polyphony))
		{
			return std::make_pair(PolyphonyType::Count, opt.value());
		}

		if (polyphony == U"legato_high")
		{
			return std::make_pair<PolyphonyType, uint8>(PolyphonyType::LegatoHigh, 1);
		}
		else if (polyphony == U"legato_last")
		{
			return std::make_pair<PolyphonyType, uint8>(PolyphonyType::LegatoLast, 1);
		}
		else if (polyphony == U"legato_low")
		{
			return std::make_pair<PolyphonyType, uint8>(PolyphonyType::LegatoLow, 1);
		}

		assert(false);
		return std::make_pair<PolyphonyType, uint8>(PolyphonyType::None, 0);
	}

	LoopMode ParseLoopMode(StringView loopModeStr)
	{
		if (loopModeStr == U"no_loop")
		{
			return LoopMode::NoLoop;
		}
		else if (loopModeStr == U"one_shot")
		{
			return LoopMode::OneShot;
		}
		else if (loopModeStr == U"loop_continuous")
		{
			return LoopMode::LoopContinuous;
		}
		else if (loopModeStr == U"loop_sustain")
		{
			return LoopMode::LoopSustain;
		}

		assert(false);
		return LoopMode::Unspecified;
	}

	Optional<uint8> ParseMidiKey(StringView str)
	{
		std::map<String, uint8> keys;
		keys[U"c"] = 0;
		keys[U"c#"] = 1;
		keys[U"db"] = 1;
		keys[U"d"] = 2;
		keys[U"d#"] = 3;
		keys[U"eb"] = 3;
		keys[U"e"] = 4;
		keys[U"f"] = 5;
		keys[U"f#"] = 6;
		keys[U"gb"] = 6;
		keys[U"g"] = 7;
		keys[U"g#"] = 8;
		keys[U"ab"] = 8;
		keys[U"a"] = 9;
		keys[U"a#"] = 10;
		keys[U"bb"] = 10;
		keys[U"b"] = 11;

		const int8 octaveMin = -1;
		const int8 octaveMax = 9;

		for (const auto& [name, localKey] : keys)
		{
			if (str.starts_with(name))
			{
				const auto opt = ParseIntOpt<int8>(str.substr(name.length()));
				if (opt && octaveMin <= opt.value() && opt.value() <= octaveMax)
				{
					return static_cast<uint8>((opt.value() + 1) * 12 + localKey);
				}
			}
		}

		return none;
	}

	template<typename T>
	const Optional<T>& CombineOpt(const Optional<T>& left, const Optional<T>& right)
	{
		return left ? left : right;
	}

	struct RegionSettingOpt
	{
		Optional<String> sample;

		// Input Controls
		Optional<uint8> lovel;
		Optional<uint8> hivel;
		Optional<uint8> lokey;
		Optional<uint8> hikey;
		Optional<Trigger> trigger;

		// group番号は1以上（0はグループに所属しない）
		Optional<int32> group;
		Optional<int32> off_by;
		Optional<OffMode> off_mode;
		Optional<float> off_time;

		Optional<PolyphonyType> polyphony;
		Optional<uint8> polyphony_count;

		// Sample Player
		Optional<uint32> offset;
		Optional<LoopMode> loopMode;

		// Pitch
		Optional<int16> tune;
		Optional<int8> pitch_keycenter;

		// Amplifier
		Optional<float> volume;
		Optional<float> rt_decay;

		// Amplifier EG
		Optional<float> ampeg_attack;
		Optional<float> ampeg_decay;
		Optional<float> ampeg_sustain;
		Optional<float> ampeg_release;

		Optional<int8> sw_lokey;
		Optional<int8> sw_hikey;
		Optional<int8> sw_last;
		Optional<int8> sw_default;

		RegionSettingOpt combined(const RegionSettingOpt& weaker) const
		{
			RegionSettingOpt result;

			result.sample = CombineOpt(sample, weaker.sample);

			result.lovel = CombineOpt(lovel, weaker.lovel);
			result.hivel = CombineOpt(hivel, weaker.hivel);
			result.lokey = CombineOpt(lokey, weaker.lokey);
			result.hikey = CombineOpt(hikey, weaker.hikey);
			result.trigger = CombineOpt(trigger, weaker.trigger);

			result.group = CombineOpt(group, weaker.group);
			result.off_by = CombineOpt(off_by, weaker.off_by);
			result.off_mode = CombineOpt(off_mode, weaker.off_mode);
			result.off_time = CombineOpt(off_time, weaker.off_time);

			result.polyphony = CombineOpt(polyphony, weaker.polyphony);
			result.polyphony_count = CombineOpt(polyphony_count, weaker.polyphony_count);

			result.offset = CombineOpt(offset, weaker.offset);
			result.loopMode = CombineOpt(loopMode, weaker.loopMode);

			result.tune = CombineOpt(tune, weaker.tune);
			result.pitch_keycenter = CombineOpt(pitch_keycenter, weaker.pitch_keycenter);

			result.volume = CombineOpt(volume, weaker.volume);
			result.rt_decay = CombineOpt(rt_decay, weaker.rt_decay);

			result.ampeg_attack = CombineOpt(ampeg_attack, weaker.ampeg_attack);
			result.ampeg_decay = CombineOpt(ampeg_decay, weaker.ampeg_decay);
			result.ampeg_sustain = CombineOpt(ampeg_sustain, weaker.ampeg_sustain);
			result.ampeg_release = CombineOpt(ampeg_release, weaker.ampeg_release);

			result.sw_lokey = CombineOpt(sw_lokey, weaker.sw_lokey);
			result.sw_hikey = CombineOpt(sw_hikey, weaker.sw_hikey);
			result.sw_last = CombineOpt(sw_last, weaker.sw_last);
			result.sw_default = CombineOpt(sw_default, weaker.sw_default);

			return result;
		}

		RegionSetting value() const
		{
			RegionSetting result;

			if (sample) { result.sample = sample.value(); }

			if (lovel) { result.lovel = lovel.value(); }
			if (hivel) { result.hivel = hivel.value(); }
			if (lokey) { result.lokey = lokey.value(); }
			if (hikey) { result.hikey = hikey.value(); }
			if (trigger) { result.trigger = trigger.value(); }

			if (group) { result.group = group.value(); }
			if (off_by) { result.off_by = off_by.value(); }
			if (off_mode) { result.off_mode = off_mode.value(); }
			if (off_time) { result.off_time = off_time.value(); }

			if (polyphony) { result.polyphony = polyphony.value(); }
			if (polyphony_count) { result.polyphony_count = polyphony_count.value(); }

			if (offset) { result.offset = offset.value(); }
			if (loopMode) { result.loopMode = loopMode.value(); }

			if (tune) { result.tune = tune.value(); }
			if (pitch_keycenter) { result.pitch_keycenter = pitch_keycenter.value(); }

			if (volume) { result.volume = volume.value(); }
			if (rt_decay) { result.rt_decay = rt_decay.value(); }

			if (ampeg_attack) { result.ampeg_attack = ampeg_attack.value(); }
			if (ampeg_decay) { result.ampeg_decay = ampeg_decay.value(); }
			if (ampeg_sustain) { result.ampeg_sustain = ampeg_sustain.value(); }
			if (ampeg_release) { result.ampeg_release = ampeg_release.value(); }

			if (sw_lokey) { result.sw_lokey = sw_lokey.value(); }
			if (sw_hikey) { result.sw_hikey = sw_hikey.value(); }
			if (sw_last) { result.sw_last = sw_last.value(); }
			if (sw_default) { result.sw_default = sw_default.value(); }

			return result;
		}
	};

}

void RegionSetting::debugPrint() const
{
	Console << U"-----------------------------";
	Console << U"sample: " << sample;
	Console << U"vel: [" << lovel << U", " << hivel << U"]";
	Console << U"key: [" << lokey << U", " << hikey << U"]";
	Console << U"trigger: " << TriggerToStr(trigger);

	Console << U"offset: " << offset;

	Console << U"tune: " << tune;
	Console << U"pitch_keycenter: " << pitch_keycenter;

	Console << U"volume: " << volume;
	Console << U"rt_decay: " << rt_decay;

	Console << U"ampeg: " << Vec4(ampeg_attack, ampeg_decay, ampeg_sustain, ampeg_release);
}

String RemoveComment(const String& text)
{
	auto lines = text.split_lines();

	for (auto& line : lines)
	{
		if (line.starts_with(U'/'))
		{
			line.clear();
		}
	}

	return lines.join(U"\n", U"", U"");
}

std::pair<String, String> splitDefine(StringView defineLine, size_t& endPos)
{
	const String keyDefine = U"#define";

	const size_t nameBegin = defineLine.indexOf(U'$');
	const size_t nameEnd = defineLine.indexOfAny(U" \t", nameBegin);
	const size_t valueBegin = defineLine.indexNotOfAny(U" \t", nameEnd);
	const size_t valueEnd = Min(defineLine.indexOfAny(U" \t\n", valueBegin), defineLine.length());

	const String name(defineLine.substr(nameBegin, nameEnd - nameBegin));
	const String value(defineLine.substr(valueBegin, valueEnd - valueBegin));

	endPos = valueEnd;

	//Console << U"(" << defineLine << U") => {" << name << U": " << value << U"}";

	return std::make_pair(name, value.ltrimmed());
}

String Preprocess(const String& text, const String& currentDirectory, HashTable<String, String>& macroDefinitions)
{
	const String keyInclude = U"#include";
	const String keyDefine = U"#define";

	String result;

	size_t pos = 0;
	for (;;)
	{
		size_t beginPos = text.indexOfAny(U"#$", pos);

		if (beginPos == String::npos)
		{
			result += text.substrView(pos);
			break;
		}

		result += text.substrView(pos, beginPos - pos);
		pos = beginPos;

		StringView token = text.substrView(beginPos);

		if (text[beginPos] == U'#')
		{
			if (token.starts_with(keyInclude))
			{
				const size_t pathBegin = token.indexOf(U'\"', keyInclude.length());
				const size_t pathEnd = token.indexOf(U'\"', pathBegin + 1);

				const auto pathStr = token.substr(pathBegin + 1, pathEnd - pathBegin - 1);

				const auto includePath = currentDirectory + pathStr;
				//Console << U"include \"" << includePath << U"\"";

				assert(FileSystem::Exists(includePath));
				if (!FileSystem::Exists(includePath))
				{
					Console << U"error: include file \"" << includePath << U"\" does not exist";
					return U"";
				}

				TextReader textReader(includePath);

				result += Preprocess(RemoveComment(textReader.readAll()), currentDirectory, macroDefinitions);

				pos += (pathEnd + 1);
			}
			else if (token.starts_with(keyDefine))
			{
				size_t endPos = 0;
				const auto [name, value] = splitDefine(token, endPos);
				pos += endPos;

				//Console << U"define " << name << U"=>" << value;
				macroDefinitions[name] = value;
			}
			else
			{
				result += U"#";
				++pos;
			}
		}
		else if (text[beginPos] == U'$')
		{
			const size_t nameBegin = 1;
			const size_t nameEnd = Min(token.indexNotOfAny(U"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_", nameBegin), token.length());

			StringView varName = token.substr(0, nameEnd);

			if (!macroDefinitions.contains(varName))
			{
				Console << U"token: " << token;
				Console << U"error: (" << varName << U") not defined ";
				return U"";

				result += Format(U"($", varName, U": not defined)");
			}
			else
			{
				result += macroDefinitions[varName];
			}

			pos += nameEnd;
		}
	}

	return result;
}

SfzData LoadSfz(FilePathView sfzPath)
{
	assert(FileSystem::Exists(sfzPath));

	assert(U"sfz" == FileSystem::Extension(sfzPath));

	TextReader sfzReader(sfzPath);

	const String keyRegionHeader = U"<region>";
	const String keyGroupHeader = U"<group>";
	//const String keyControlHeader = U"<control>";
	const String keyGlobalHeader = U"<global>";
	//const String keyCurveHeader = U"<curve>";
	//const String keyEffectHeader = U"<effect>";
	//const String keyMasterHeader = U"<master>";
	//const String keyMidiHeader = U"<midi>";
	//const String keySampleHeader = U"<sample>";

	const String keySample = U"sample=";
	const String keyLovel = U"lovel=";
	const String keyHivel = U"hivel=";
	const String keyPitchKeycenter = U"pitch_keycenter=";
	const String keyKey = U"key=";
	const String keyLokey = U"lokey=";
	const String keyHikey = U"hikey=";
	const String keyOffset = U"offset=";
	const String keyTune = U"tune=";
	const String keyVolume = U"volume=";
	const String keyTrigger = U"trigger=";
	const String keyAmpegAttack = U"ampeg_attack=";
	const String keyAmpegDecay = U"ampeg_decay=";
	const String keyAmpegSustain = U"ampeg_sustain=";
	const String keyAmpegRelease = U"ampeg_release=";
	const String keyRtDecay = U"rt_decay=";
	const String keyDefaultPath = U"default_path=";
	const String keySwLoKey = U"sw_lokey=";
	const String keySwHiKey = U"sw_hikey=";
	const String keySwDefault = U"sw_default=";
	const String keySwLast = U"sw_last=";
	const String keyGroup = U"group=";
	const String keyOffBy = U"off_by=";
	const String keyOffMode = U"off_mode=";
	const String keyOffTime = U"off_time=";
	const String keyPolyphony = U"polyphony=";
	const String keyLoopMode = U"loop_mode=";

	const auto parentDirectory = FileSystem::ParentPath(sfzPath);
	String defaultPath = parentDirectory;

	HashTable<String, String> macroDefinitions;
	const auto text = Preprocess(RemoveComment(sfzReader.readAll()), parentDirectory, macroDefinitions);

#ifdef DEVELOPMENT
	TextWriter writer(U"debug/preprocessed.txt");
	writer << text;
#endif

	SFZHeader header = SFZHeader::Global;

	Array<RegionSetting> settings;
	RegionSettingOpt globalSetting;
	RegionSettingOpt groupSetting;
	Optional<RegionSettingOpt> regionSetting;

	const auto setting = [&]()->RegionSettingOpt&
	{
		if (header == SFZHeader::Global)
		{
			return globalSetting;
		}
		else if (header == SFZHeader::Group)
		{
			return groupSetting;
		}

		return regionSetting.value();
	};

	HashSet<String> unsupportedOpcodes;

	size_t pos = 0;
	for (;;)
	{
		size_t nextPos = text.indexOfAny(U" \t\n", pos);

		StringView token = text.substrView(pos, nextPos == String::npos ? nextPos : nextPos - pos);

		if (token.empty())
		{
		}
		else if (token.starts_with(keyRegionHeader))
		{
			nextPos = pos + keyRegionHeader.length() - 1;
			if (regionSetting)
			{
				settings.push_back(regionSetting.value().value());
				regionSetting = none;
			}

			regionSetting = groupSetting.combined(globalSetting);
			header = SFZHeader::Region;
		}
		else if (token.starts_with(keyGroupHeader))
		{
			nextPos = pos + keyGroupHeader.length() - 1;
			if (regionSetting)
			{
				settings.push_back(regionSetting.value().value());
				regionSetting = none;
			}

			groupSetting = RegionSettingOpt();
			header = SFZHeader::Group;
		}
		else if (token.starts_with(keyGlobalHeader))
		{
			nextPos = pos + keyGlobalHeader.length() - 1;
			if (regionSetting)
			{
				settings.push_back(regionSetting.value().value());
				regionSetting = none;
			}

			globalSetting = RegionSettingOpt();
			header = SFZHeader::Global;
		}
		else if (token.starts_with(keySample))
		{
			token = token.substr(keySample.length());

			if (token.starts_with(U"*"))
			{
				setting().sample = token;
			}
			else
			{
				if (!FileSystem::IsFile(defaultPath + token))
				{
					pos += keySample.length();
					// sampleの場合は空白文字を含める
					nextPos = text.indexOfAny(U"\t\n", pos);
					token = text.substrView(pos, nextPos == String::npos ? nextPos : nextPos - pos);
				}

				if (FileSystem::IsFile(defaultPath + token))
				{
					setting().sample = token;
				}
				else
				{
					Console << U"warning: not found sample \"" << (defaultPath + token) << U"\"";
					Console << U"this region is skipped";
					regionSetting = none;
				}
			}
		}
		else if (token.starts_with(keyGroup))
		{
			setting().group = ParseInt<int32>(token.substr(keyGroup.length()));
		}
		else if (token.starts_with(keyLoopMode))
		{
			setting().loopMode = ParseLoopMode(token.substr(keyLoopMode.length()));
		}
		else if (token.starts_with(keyPolyphony))
		{
			const auto [polyType, polyCount] = ParsePolyphony(token.substr(keyPolyphony.length()));
			setting().polyphony = polyType;
			setting().polyphony_count = polyCount;
		}
		else if (token.starts_with(keyOffBy))
		{
			setting().off_by = ParseInt<int32>(token.substr(keyOffBy.length()));
		}
		else if (token.starts_with(keyOffMode))
		{
			setting().off_mode = ParseOffMode(token.substr(keyOffMode.length()));
		}
		else if (token.starts_with(keyOffTime))
		{
			setting().off_time = ParseFloat<float>(token.substr(keyOffTime.length()));
		}
		else if (token.starts_with(keySwLoKey))
		{
			setting().sw_lokey = ParseInt<int8>(token.substr(keySwLoKey.length()));
		}
		else if (token.starts_with(keySwHiKey))
		{
			setting().sw_hikey = ParseInt<int8>(token.substr(keySwHiKey.length()));
		}
		else if (token.starts_with(keySwDefault))
		{
			setting().sw_default = ParseInt<int8>(token.substr(keySwDefault.length()));
		}
		else if (token.starts_with(keySwLast))
		{
			setting().sw_last = ParseInt<int8>(token.substr(keySwLast.length()));
		}
		else if (token.starts_with(keyLovel))
		{
			setting().lovel = ParseInt<uint8>(token.substr(keyLovel.length()));
		}
		else if (token.starts_with(keyHivel))
		{
			setting().hivel = ParseInt<uint8>(token.substr(keyHivel.length()));
		}
		else if (token.starts_with(keyDefaultPath))
		{
			defaultPath = parentDirectory + token.substr(keyDefaultPath.length());
		}
		else if (token.starts_with(keyKey))
		{
			const auto keyStr = token.substr(keyKey.length());
			if (auto optKey = ParseIntOpt<uint8>(keyStr))
			{
				setting().lokey = optKey.value();
				setting().hikey = optKey.value();
				setting().pitch_keycenter = optKey.value();
			}
			else if (auto optKeyName = ParseMidiKey(keyStr))
			{
				setting().lokey = optKeyName.value();
				setting().hikey = optKeyName.value();
				setting().pitch_keycenter = optKeyName.value();
			}
			else
			{
				assert(false);
			}
		}
		else if (token.starts_with(keyLokey))
		{
			const auto keyStr = token.substr(keyLokey.length());
			if (auto optKey = ParseIntOpt<uint8>(keyStr))
			{
				setting().lokey = optKey.value();
			}
			else if (auto optKeyName = ParseMidiKey(keyStr))
			{
				setting().lokey = optKeyName.value();
			}
			else
			{
				assert(false);
			}
		}
		else if (token.starts_with(keyHikey))
		{
			const auto keyStr = token.substr(keyHikey.length());
			if (auto optKey = ParseIntOpt<uint8>(keyStr))
			{
				setting().hikey = optKey.value();
			}
			else if (auto optKeyName = ParseMidiKey(keyStr))
			{
				setting().hikey = optKeyName.value();
			}
			else
			{
				assert(false);
			}
		}
		else if (token.starts_with(keyPitchKeycenter))
		{
			const auto keyStr = token.substr(keyPitchKeycenter.length());
			if (auto optKey = ParseIntOpt<uint8>(keyStr))
			{
				setting().pitch_keycenter = optKey.value();
			}
			else if (auto optKeyName = ParseMidiKey(keyStr))
			{
				setting().pitch_keycenter = optKeyName.value();
			}
			else
			{
				assert(false);
			}
		}
		else if (token.starts_with(keyOffset))
		{
			setting().offset = ParseInt<uint32>(token.substr(keyOffset.length()));
		}
		else if (token.starts_with(keyTune))
		{
			setting().tune = ParseInt<int16>(token.substr(keyTune.length()));
		}
		else if (token.starts_with(keyVolume))
		{
			setting().volume = ParseFloat<float>(token.substr(keyVolume.length()));
		}
		else if (token.starts_with(keyRtDecay))
		{
			setting().rt_decay = ParseFloat<float>(token.substr(keyRtDecay.length()));
		}
		else if (token.starts_with(keyTrigger))
		{
			setting().trigger = ParseTrigger(token.substr(keyTrigger.length()));
		}
		else if (token.starts_with(keyAmpegAttack))
		{
			setting().ampeg_attack = ParseFloat<float>(token.substr(keyAmpegAttack.length()));
		}
		else if (token.starts_with(keyAmpegDecay))
		{
			setting().ampeg_decay = ParseFloat<float>(token.substr(keyAmpegDecay.length()));
		}
		else if (token.starts_with(keyAmpegSustain))
		{
			setting().ampeg_sustain = ParseFloat<float>(token.substr(keyAmpegSustain.length()));
		}
		else if (token.starts_with(keyAmpegRelease))
		{
			setting().ampeg_release = ParseFloat<float>(token.substr(keyAmpegRelease.length()));
		}
		else
		{
#ifdef DEVELOPMENT
			if (!token.includes(U"label"))
			{
				const auto equalPos = token.indexOf(U'=');
				if (equalPos != String::npos)
				{
					const auto opcodeStr = token.substr(0, equalPos);
					if (!unsupportedOpcodes.contains(opcodeStr))
					{
						unsupportedOpcodes.emplace(opcodeStr);
						Console << U"(" << pos << U":" << nextPos << U") unsupported opcode: \"" << opcodeStr << U"\"";
					}
				}
				else
				{
					Console << U"(" << pos << U":" << nextPos << U") unknown token: \"" << token << U"\"";
				}
			}
#endif
		}

		if (nextPos == String::npos)
		{
			if (regionSetting)
			{
				settings.push_back(regionSetting.value().value());
				regionSetting = none;
			}

			break;
		}

		pos = nextPos + 1;
	}

	//for (const auto& setting : settings)
	//{
	//	setting.debugPrint();
	//}

	SfzData sfzData;
	sfzData.dir = defaultPath;
	sfzData.data = std::move(settings);

	return sfzData;
}
