#pragma once
#include <Siv3D.hpp>

template<class RectType>
inline std::pair<RectType, RectType> SplitUpDown(const RectType& rect, double t)
{
	const auto upHeight = static_cast<RectType::value_type>(rect.h * t);
	return std::make_pair(
		RectType{ rect.x, rect.y, rect.w, upHeight },
		RectType{ rect.x, rect.y + upHeight, rect.w, rect.h - upHeight }
	);
}

template<class RectType>
inline std::pair<RectType, RectType> SplitLeftRight(const RectType& rect, double t)
{
	const auto leftWidth = static_cast<RectType::value_type>(rect.w * t);
	return std::make_pair(
		RectType{ rect.x, rect.y, leftWidth, rect.h },
		RectType{ rect.x + leftWidth, rect.y, rect.w - leftWidth, rect.h }
	);
}

inline uint8 ReadByte(BinaryReader& reader)
{
	uint8 byte[1] = {};
	reader.read(byte, 1);
	return byte[0];
}

template<typename T>
inline T ReadBytes(BinaryReader& reader)
{
	uint8 bytes[sizeof(T)] = {};
	reader.read(bytes, sizeof(T));
	T value = 0;
	for (size_t i = 0; i < sizeof(T); i++)
	{
		value += (bytes[i] << ((sizeof(T) - i - 1) * 8));
	}
	return value;
}

inline std::string ReadText(BinaryReader& reader)
{
	const uint8 length = ReadBytes<uint8>(reader);
	Array<char> chars(length + 1ull, '\0');
	reader.read(chars.data(), length);
	return std::string(chars.data());
}

inline uint32 GetTick(const Array<uint8>& tickBytes)
{
	uint32 value = 0;
	for (size_t i = 0; i < tickBytes.size(); i++)
	{
		value += ((tickBytes[i] & 0x7F) << ((tickBytes.size() - i - 1) * 7));
	}
	return value;
}

inline void DrawDotLine(const Line& line, double unitLength, double interval, double thickness, const Color color)
{
	const double length = line.length();
	const Vec2 dir = line.vector() / length;
	const int itCount = static_cast<int>(Math::Round(length / (unitLength + interval)));
	for (int i = 0; i < itCount; i++)
	{
		const Vec2 begin = line.begin + dir * (unitLength + interval) * i;
		const Vec2 end = begin + dir * unitLength;
		Line(begin, end).draw(thickness, color);
	}
}
