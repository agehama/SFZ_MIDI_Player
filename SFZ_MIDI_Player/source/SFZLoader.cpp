#pragma once
#include <SFZLoader.hpp>

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

SfzData LoadSfz(FilePathView sfzPath)
{
	assert(FileSystem::Exists(sfzPath));

	assert(U"sfz" == FileSystem::Extension(sfzPath));

	TextReader sfzReader(sfzPath);

	const String keyGroup = U"<group>";
	const String keyRegion = U"<region>";
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
	const String keyComment = U"//";

	const auto text = sfzReader.readAll();

	const auto directory = FileSystem::ParentPath(sfzPath);;

	Array<RegionSetting> settings;
	RegionSetting group;
	Optional<RegionSetting> region;

	size_t pos = 0;
	for (;;)
	{
		size_t nextPos = text.indexOfAny(U" \t\n", pos);

		StringView token = text.substrView(pos, nextPos == String::npos ? nextPos : nextPos - pos);

		if (token.empty()) {}
		else if (token.starts_with(keyComment))
		{
			nextPos = text.indexOfAny(U"\n", pos);
		}
		else if (token.starts_with(keyRegion))
		{
			if (region)
			{
				settings.push_back(region.value());
				region = none;
			}
			region = group;
		}
		else if (token.starts_with(keySample))
		{
			token = token.substr(keySample.length());
			if (!FileSystem::Exists(directory + token))
			{
				pos += keySample.length();
				// sampleの場合は空白文字を含める
				nextPos = text.indexOfAny(U"\t\n", pos);
				token = text.substrView(pos, nextPos == String::npos ? nextPos : nextPos - pos);
			}

			(region ? region.value() : group).sample = token;
		}
		else if (token.starts_with(keyLovel))
		{
			(region ? region.value() : group).lovel = ParseInt<uint8>(token.substr(keyLovel.length()));
		}
		else if (token.starts_with(keyHivel))
		{
			(region ? region.value() : group).hivel = ParseInt<uint8>(token.substr(keyHivel.length()));
		}
		else if (token.starts_with(keyKey))
		{
			const auto keyStr = token.substr(keyLokey.length());
			if (auto optKey = ParseIntOpt<uint8>(keyStr))
			{
				(region ? region.value() : group).lokey = optKey.value();
				(region ? region.value() : group).hikey = optKey.value();
				(region ? region.value() : group).pitch_keycenter = optKey.value();
			}
			else if (auto optKeyName = ParseMidiKey(keyStr))
			{
				(region ? region.value() : group).lokey = optKeyName.value();
				(region ? region.value() : group).hikey = optKeyName.value();
				(region ? region.value() : group).pitch_keycenter = optKeyName.value();
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
				(region ? region.value() : group).lokey = optKey.value();
			}
			else if (auto optKeyName = ParseMidiKey(keyStr))
			{
				(region ? region.value() : group).lokey = optKeyName.value();
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
				(region ? region.value() : group).hikey = optKey.value();
			}
			else if (auto optKeyName = ParseMidiKey(keyStr))
			{
				(region ? region.value() : group).hikey = optKeyName.value();
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
				(region ? region.value() : group).pitch_keycenter = optKey.value();
			}
			else if (auto optKeyName = ParseMidiKey(keyStr))
			{
				(region ? region.value() : group).pitch_keycenter = optKeyName.value();
			}
			else
			{
				assert(false);
			}
		}
		else if (token.starts_with(keyOffset))
		{
			(region ? region.value() : group).offset = ParseInt<uint32>(token.substr(keyOffset.length()));
		}
		else if (token.starts_with(keyTune))
		{
			(region ? region.value() : group).tune = ParseInt<int16>(token.substr(keyTune.length()));
		}
		else if (token.starts_with(keyVolume))
		{
			(region ? region.value() : group).volume = ParseFloat<float>(token.substr(keyVolume.length()));
		}
		else if (token.starts_with(keyRtDecay))
		{
			(region ? region.value() : group).rt_decay = ParseFloat<float>(token.substr(keyRtDecay.length()));
		}
		else if (token.starts_with(keyGroup))
		{
			if (region)
			{
				settings.push_back(region.value());
				region = none;
			}

			group = RegionSetting();
		}
		else if (token.starts_with(keyTrigger))
		{
			(region ? region.value() : group).trigger = ParseTrigger(token.substr(keyTrigger.length()));
		}
		else if (token.starts_with(keyAmpegAttack))
		{
			(region ? region.value() : group).ampeg_attack = ParseFloat<float>(token.substr(keyAmpegAttack.length()));
		}
		else if (token.starts_with(keyAmpegDecay))
		{
			(region ? region.value() : group).ampeg_decay = ParseFloat<float>(token.substr(keyAmpegDecay.length()));
		}
		else if (token.starts_with(keyAmpegSustain))
		{
			(region ? region.value() : group).ampeg_sustain = ParseFloat<float>(token.substr(keyAmpegSustain.length()));
		}
		else if (token.starts_with(keyAmpegRelease))
		{
			(region ? region.value() : group).ampeg_release = ParseFloat<float>(token.substr(keyAmpegRelease.length()));
		}
		else
		{
			Console << U"(" << pos << U":" << nextPos << U") unsupported opcode: \"" << token << U"\"";
		}

		if (nextPos == String::npos)
		{
			if (region)
			{
				settings.push_back(region.value());
				region = none;
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
	sfzData.dir = directory;
	sfzData.data = std::move(settings);

	return sfzData;
}
