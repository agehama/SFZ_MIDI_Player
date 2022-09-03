#pragma once
#include <Siv3D.hpp>
#include "AudioLoaderBase.hpp"

class FlacDecoder;

class FlacLoader : public AudioLoaderBase
{
public:

	FlacLoader(FilePathView path, size_t debugId);

	virtual ~FlacLoader() = default;

	size_t size() const override;

	size_t sampleRate() const override;

	float sampleRateInv() const override;

	size_t lengthSample() const override;

	void use(size_t beginSampleIndex, size_t sampleCount) override;

	void markUnused() override;

	void freeUnusedBlocks() override;

	WaveSample getSample(int64 index) const override;

private:

	void init();

	std::unique_ptr<FlacDecoder> m_flacDecoder;
};
