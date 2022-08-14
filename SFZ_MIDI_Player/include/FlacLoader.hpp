﻿#pragma once
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

	size_t lengthSample() const override;

	void use() override;

	void unuse() override;

	void update() override;

	WaveSample getSample(int64 index) const override;

private:

	void init();

	void readBlock();

	std::unique_ptr<FlacDecoder> m_flacDecoder;

	uint32 m_unuseCount = 0;
	bool m_use = false;
	static std::mutex m_mutex;
};
