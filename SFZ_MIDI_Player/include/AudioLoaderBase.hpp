#pragma once
#include <Siv3D.hpp>

class AudioLoaderBase
{
public:

	virtual ~AudioLoaderBase() = default;

	virtual size_t size() const = 0;

	virtual size_t sampleRate() const = 0;

	virtual size_t lengthSample() const = 0;

	virtual void use(size_t beginSampleIndex, size_t sampleCount) = 0;

	virtual void unuse() = 0;

	virtual void update() = 0;

	virtual WaveSample getSample(int64 index) const = 0;
};

struct Sample16bit2ch
{
	int16 left;
	int16 right;
};

struct Sample8bit2ch
{
	int8 left;
	int8 right;
};
