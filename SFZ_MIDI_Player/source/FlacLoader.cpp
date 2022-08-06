#pragma once
#include <FlacLoader.hpp>

#define FLAC__NO_DLL
#include <FLAC++/decoder.h>
#include <share/compat.h>

class FlacDecoder : public FLAC::Decoder::Stream
{
public:

	FlacDecoder(FILE* f_) : FLAC::Decoder::Stream(), f(f_) {}

	FLAC__uint64 total_samples = 0;
	FLAC__uint64 m_dataSize = 0;
	size_t m_loadSampleCount = 0;
	uint32_t sample_rate = 0;
	uint32_t channels = 0;
	uint32_t bps = 0;

	float m_normalize = 1.f / 32768.0f;
	bool m_initialized = false;
	Array<WaveSample> m_readBuffer;

protected:
	FILE* f;

	::FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame* frame, const FLAC__int32* const buffer[]) override
	{
		if (m_loadSampleCount < m_readBuffer.size())
		{
			for (size_t i = 0; i < frame->header.blocksize; i++)
			{
				m_readBuffer[m_loadSampleCount].left = static_cast<FLAC__int16>(buffer[0][i]) * m_normalize;
				m_readBuffer[m_loadSampleCount].right = static_cast<FLAC__int16>(buffer[1][i]) * m_normalize;
				++m_loadSampleCount;
			}
		}

		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
	}

	::FLAC__StreamDecoderReadStatus read_callback(FLAC__byte buffer[], size_t* bytes) override
	{
		const size_t size = *bytes;
		if (feof(f))
		{
			*bytes = 0;
			return FLAC__StreamDecoderReadStatus::FLAC__STREAM_DECODER_READ_STATUS_ABORT;
		}
		else if (size > 0)
		{
			*bytes = ::fread(buffer, 1, size, f);
			if (*bytes == 0)
			{
				if (feof(f))
				{
					return FLAC__StreamDecoderReadStatus::FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
				}
				else
				{
					return FLAC__StreamDecoderReadStatus::FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
				}
			}
			else
			{
				return FLAC__StreamDecoderReadStatus::FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
			}
		}
		else
		{
			return FLAC__StreamDecoderReadStatus::FLAC__STREAM_DECODER_READ_STATUS_ABORT;
		}
	}

	void metadata_callback(const ::FLAC__StreamMetadata* metadata) override
	{
		if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
		{
			total_samples = metadata->data.stream_info.total_samples;
			sample_rate = metadata->data.stream_info.sample_rate;
			channels = metadata->data.stream_info.channels;
			bps = metadata->data.stream_info.bits_per_sample;
			m_dataSize = total_samples * channels * (bps / 8);
			m_initialized = true;
		}
	}

	void error_callback(::FLAC__StreamDecoderErrorStatus) override
	{
		Console << U"error_callback";
	}

private:
	FlacDecoder(const FlacDecoder&) = default;
	FlacDecoder& operator=(const FlacDecoder&) = default;
};

FlacLoader::FlacLoader(FilePathView path)
{
	const auto str = Unicode::Narrow(String(path));
	FILE* stream = nullptr;
	{
		errno_t err = fopen_s(&stream, str.c_str(), "rb");
		if (err == 0)
		{
			m_flacDecoder = std::make_unique<FlacDecoder>(stream);
		}
		else
		{
			Console << U"error: not opened";
			return;
		}
	}

	init();
}

size_t FlacLoader::size() const
{
	return m_flacDecoder->m_dataSize;
}

size_t FlacLoader::sampleRate() const
{
	return m_flacDecoder->sample_rate;
}

size_t FlacLoader::lengthSample() const
{
	return m_flacDecoder->total_samples;
}

void FlacLoader::init()
{
	auto initResult = m_flacDecoder->init();

	if (initResult != FLAC__STREAM_DECODER_INIT_STATUS_OK)
	{
		Console << U"error: initResult != FLAC__STREAM_DECODER_INIT_STATUS_OK";
		m_flacDecoder->m_initialized = false;
		return;
	}

	m_flacDecoder->process_until_end_of_metadata();

	if (m_flacDecoder->total_samples == 0) {
		Console << U"error: m_flacDecoder->total_samples == 0";
		m_flacDecoder->m_initialized = false;
		return;
	}
	if (m_flacDecoder->channels != 2 || m_flacDecoder->bps != 16)
	{
		Console << U"error: m_flacDecoder->channels != 2 || m_flacDecoder->bps != 16";
		m_flacDecoder->m_initialized = false;
		return;
	}

	m_flacDecoder->m_initialized = true;
}

void FlacLoader::use()
{
	m_unuseCount = 0;

	if (m_use)
	{
		return;
	}

	m_mutex.lock();

	{
		m_flacDecoder->m_loadSampleCount = 0;
		m_flacDecoder->m_readBuffer.resize(m_flacDecoder->m_dataSize);

		readBlock();

		m_use = true;
	}

	m_mutex.unlock();
}

void FlacLoader::unuse()
{
	m_use = false;
}

void FlacLoader::update()
{
	m_mutex.lock();

	++m_unuseCount;
	if (m_use && 60 < m_unuseCount)
	{
		unuse();
	}

	if (m_use)
	{
		readBlock();
	}
	else if(!m_flacDecoder->m_readBuffer.empty())
	{
		m_flacDecoder->m_readBuffer.clear();
		m_flacDecoder->m_readBuffer.shrink_to_fit();
		//m_waveReader.setPos(m_dataPos);
		m_flacDecoder->m_loadSampleCount = 0;
	}

	m_mutex.unlock();
}

WaveSample FlacLoader::getSample(int64 index) const
{
	return m_flacDecoder->m_readBuffer[index];
}

void FlacLoader::readBlock()
{
	m_flacDecoder->process_single();

	/*size_t readCount = 4096;
	if (m_dataSize <= m_loadSampleCount + readCount)
	{
		readCount = m_loadSampleCount - m_dataSize;
	}

	if (1 <= readCount)
	{
		m_waveReader.read(m_readBuffer.data() + m_loadSampleCount, readCount * m_format.blockAlign);
		m_loadSampleCount += readCount;
	}*/
}

std::mutex FlacLoader::m_mutex;
