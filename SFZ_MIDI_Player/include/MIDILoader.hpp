#pragma once
#include <Siv3D.hpp>

//https://www.midi.org/specifications-old/item/table-3-control-change-messages-data-bytes-2
enum class ControlChangeType : uint8
{
	Bank_Select,
	Modulation_Wheel_or_Lever,
	Breath_Controller,
	Control_3_Undefined,
	Foot_Controller,
	Portamento_Time,
	Data_Entry_MSB,
	Channel_Volume,
	Balance,
	Control_9_Undefined,
	Pan,
	Expression_Controller,
	Effect_Control_1,
	Effect_Control_2,
	Control_14_Undefined,
	Control_15_Undefined,
	General_Purpose_Controller_1,
	General_Purpose_Controller_2,
	General_Purpose_Controller_3,
	General_Purpose_Controller_4,
	Control_20_Undefined,
	Control_21_Undefined,
	Control_22_Undefined,
	Control_23_Undefined,
	Control_24_Undefined,
	Control_25_Undefined,
	Control_26_Undefined,
	Control_27_Undefined,
	Control_28_Undefined,
	Control_29_Undefined,
	Control_30_Undefined,
	Control_31_Undefined,
	LSB_for_Control_0_Bank_Select,
	LSB_for_Control_1_Modulation_Wheel_or_Lever,
	LSB_for_Control_2_Breath_Controller,
	LSB_for_Control_3_Undefined,
	LSB_for_Control_4_Foot_Controller,
	LSB_for_Control_5_Portamento_Time,
	LSB_for_Control_6_Data_Entry,
	LSB_for_Control_7_Channel_Volume,
	LSB_for_Control_8_Balance,
	LSB_for_Control_9_Undefined,
	LSB_for_Control_10_Pan,
	LSB_for_Control_11_Expression_Controller,
	LSB_for_Control_12_Effect_control_1,
	LSB_for_Control_13_Effect_control_2,
	LSB_for_Control_14_Undefined,
	LSB_for_Control_15_Undefined,
	LSB_for_Control_16_General_Purpose_Controller_1,
	LSB_for_Control_17_General_Purpose_Controller_2,
	LSB_for_Control_18_General_Purpose_Controller_3,
	LSB_for_Control_19_General_Purpose_Controller_4,
	LSB_for_Control_20_Undefined,
	LSB_for_Control_21_Undefined,
	LSB_for_Control_22_Undefined,
	LSB_for_Control_23_Undefined,
	LSB_for_Control_24_Undefined,
	LSB_for_Control_25_Undefined,
	LSB_for_Control_26_Undefined,
	LSB_for_Control_27_Undefined,
	LSB_for_Control_28_Undefined,
	LSB_for_Control_29_Undefined,
	LSB_for_Control_30_Undefined,
	LSB_for_Control_31_Undefined,
	Damper_Pedal_On_Off,
	Portamento_On_Off,
	Sostenuto_On_Off,
	Soft_Pedal_On_Off,
	Legato_Footswitch,
	Hold_2,
	Sound_Controller_1,
	Sound_Controller_2,
	Sound_Controller_3,
	Sound_Controller_4,
	Sound_Controller_5,
	Sound_Controller_6,
	Sound_Controller_7,
	Sound_Controller_8,
	Sound_Controller_9,
	Sound_Controller_10,
	General_Purpose_Controller_5,
	General_Purpose_Controller_6,
	General_Purpose_Controller_7,
	General_Purpose_Controller_8,
	Portamento_Control,
	Control_85_Undefined,
	Control_86_Undefined,
	Control_87_Undefined,
	High_Resolution_Velocity_Prefix,
	Control_89_Undefined,
	Control_90_Undefined,
	Effects_1_Depth,
	Effects_2_Depth,
	Effects_3_Depth,
	Effects_4_Depth,
	Effects_5_Depth,
	Data_Increment,
	Data_Decrement,
	Non_Registered_Parameter_Number_LSB,
	Non_Registered_Parameter_Number_MSB,
	Registered_Parameter_Number_LSB,
	Registered_Parameter_Number_MSB,
	Control_102_Undefined,
	Control_103_Undefined,
	Control_104_Undefined,
	Control_105_Undefined,
	Control_106_Undefined,
	Control_107_Undefined,
	Control_108_Undefined,
	Control_109_Undefined,
	Control_110_Undefined,
	Control_111_Undefined,
	Control_112_Undefined,
	Control_113_Undefined,
	Control_114_Undefined,
	Control_115_Undefined,
	Control_116_Undefined,
	Control_117_Undefined,
	Control_118_Undefined,
	Control_119_Undefined,
	All_Sound_Off,
	Reset_All_Controllers,
	Local_Control_On_Off,
	All_Notes_Off,
	Omni_Mode_Off,
	Omni_Mode_On,
	Mono_Mode_On,
	Poly_Mode_On,
};

struct ControlChangeData
{
	ControlChangeType type;
	uint8 data;
};

enum class MetaEventType
{
	TrackName,		// トラック名
	Tempo,			// 一拍あたりの時間（マイクロ秒）
	SetMetre,		// 拍子
	EndOfTrack,
	Error,
};

struct MetreData
{
	uint32 numerator;
	uint32 denominator;
};

struct MetaEventData
{
	MetaEventType type;
	MetreData eventData;
	double tempo = 0;

	bool isError() const { return type == MetaEventType::Error; }
	bool isEndOfTrack() const { return type == MetaEventType::EndOfTrack; }

	static MetaEventData Error();
	static MetaEventData EndOfTrack();
	static MetaEventData SetMetre(uint32 numerator, uint32 denominator);
	static MetaEventData SetTempo(double bpm);
};

enum class MidiEventType
{
	NoteOff,
	NoteOn,
	PolyphonicKeyPressure,
	ControlChange,
	ProgramChange,
	ChannelPressure,
	PitchBend,
};

struct MidiEventData
{
	MidiEventType type;
	uint8 channel;
	uint8 key;
	uint8 velocity;
	uint8 changeType;
	uint16 value;//controlchangeとpitchbend用

	static MidiEventData NoteOff(uint8 ch, uint8 key);
	static MidiEventData NoteOn(uint8 ch, uint8 key, uint8 velocity);
	static MidiEventData PolyphonicKeyPressure(uint8 ch, uint8 key, uint8 velocity);
	static MidiEventData ControlChange(uint8 ch, uint8 type, uint8 value);
	static MidiEventData ProgramChange(uint8 ch, uint8 type);
	static MidiEventData ChannelPressure(uint8 ch, uint8 velocity);
	static MidiEventData PitchBend(uint8 ch, uint16 value);
};

enum class EventType
{
	MidiEvent,
	SysExEvent,
	MetaEvent,
};

struct MidiCode
{
	uint32 tick;
	EventType type;
	std::variant<MidiEventData, MetaEventData> data;
};

struct Note
{
	double beginSec;
	double endSec;
	uint32 tick;
	uint32 gate;
	uint8 key;
	uint8 velocity;
	uint8 ch;
};

class TrackData
{
public:

	TrackData(const Array<MidiCode>& operations) : m_operations(operations)
	{
		init();
	}

	void init();

	const Array<Note>& notes() const { return m_notes; }

	void outputLog(TextWriter& writer) const;

	uint8 channel() const { return m_channel; }

	uint8 program() const { return m_program; }

	bool isPercussionTrack() const { return m_channel == 9; }

private:

	friend class MidiData;

	Array<Note> m_notes;

	Array<MidiCode> m_operations;

	uint8 m_channel = 0;
	uint8 m_program = 0;
};

struct Beat
{
	uint32 localTick;
};

struct Measure
{
	uint32 measureIndex;
	uint32 globalTick;
	uint32 beatStep;
	Array<Beat> beats;

	// 小節のtick数=1拍あたりのtick数×拍数
	uint32 widthOfTicks() const	{ return static_cast<uint32>(beats.size()) * beatStep; }

	void outputLog(TextWriter& writer) const;
};

class MidiData
{
public:

	MidiData(const Array<TrackData>& tracks, uint16 resolution) :
		m_tracks(tracks),
		m_resolution(resolution)
	{
		init();
	}

	void init();

	const Array<TrackData>& notes() const { return m_tracks; }

	Array<Measure> getMeasures() const;

	uint32 endTick() const;

	uint16 resolution() const { return m_resolution; }

	double getBPM() const;

	double ticksToSeconds(int64 currentTick) const;

	int64 secondsToTicks(double seconds) const;

	double secondsToTicks2(double seconds) const;

private:

	// tick -> BPM
	std::map<int64, double> BPMSetEvents() const;

	bool intersects(uint32 range0begin, uint32 range0end, uint32 range1begin, uint32 range1end) const;

	struct MeasureInfo
	{
		MetreData metre;
		uint32 globalTick;
	};

	uint16 m_resolution;

	Array<TrackData> m_tracks;

	Array<MeasureInfo> m_measures;

	std::map<int64, double> m_bpmSetEvents;
};

Optional<MidiData> LoadMidi(FilePathView path);
