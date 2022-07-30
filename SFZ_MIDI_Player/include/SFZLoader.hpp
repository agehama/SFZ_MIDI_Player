#pragma once
#include <Siv3D.hpp>

enum class Trigger : uint8
{
	Attack, Release, First, Legato
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

	void debugPrint() const;
};

struct SfzData
{
	String dir;
	Array<RegionSetting> data;
};

SfzData LoadSfz(FilePathView sfzPath);
