#pragma once
#include <Siv3D.hpp>

enum class Trigger : uint8
{
	Attack, Release, First, Legato
};

enum class OffMode : uint8
{
	Fast, Normal, Time
};

// https://musf.ifdef.jp/sfz/sfz_File_Format.html
struct RegionSetting
{
	String sample;

	// Input Controls
	uint8 lovel = 0;
	uint8 hivel = 127;
	uint8 lokey = 0;
	uint8 hikey = 127;
	Trigger trigger = Trigger::Attack;

	// group番号は1以上（0はグループに所属しない）
	uint32 group = 0;
	uint32 off_by = 0;
	OffMode off_mode = OffMode::Fast;
	float off_time = 0.006f;

	// Sample Player
	uint32 offset = 0;

	// Pitch
	int16 tune = 0;
	int8 pitch_keycenter = 60;

	// Amplifier
	float volume = 0.0f;
	float rt_decay = 0.0f;

	// Amplifier EG
	float ampeg_attack = 0.0f;
	float ampeg_decay = 0.0f;
	float ampeg_sustain = 100.0f;
	float ampeg_release = 0.0f;

	int8 sw_lokey = 0;
	int8 sw_hikey = 0;
	int8 sw_last = 0;
	int8 sw_default = 0;

	void debugPrint() const;
};

struct SfzData
{
	String dir;
	Array<RegionSetting> data;
};

SfzData LoadSfz(FilePathView sfzPath);
