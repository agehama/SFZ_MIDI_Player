#pragma once
#include <FlacLoader.hpp>

#define FLAC__NO_DLL
#include <FLAC++/decoder.h>
#include <share/compat.h>

class FlacDecoder : public FLAC::Decoder::Stream
{
public:

	FlacDecoder(FilePathView path) :
		FLAC::Decoder::Stream(),
		m_filePath(path),
		m_fileReader(path)
	{}

	FLAC__uint64 m_lengthSample = 0;
	FLAC__uint64 m_dataSize = 0;
	uint32_t m_sampleRate = 0;
	uint32_t m_channels = 0;
	uint32_t m_bitsPerSample = 0;

	float m_normalize = 0;
	bool m_initialized = false;

	Array<WaveSample> m_readBuffer;
	size_t m_loadSampleCount = 0;
	size_t m_readPos = 0;

	bool isOpen() const
	{
		return m_fileReader.isOpen();
	}

	void restore()
	{
		m_fileReader.open(m_filePath);
		m_fileReader.setPos(m_readPos);
	}

	void close()
	{
		m_fileReader.close();
	}

protected:
	FilePath m_filePath;
	BinaryReader m_fileReader;

	::FLAC__StreamDecoderReadStatus read_callback(FLAC__byte buffer[], size_t* bytes) override
	{
		if (m_fileReader.getPos() == m_fileReader.size())
		{
			*bytes = 0;
			return FLAC__StreamDecoderReadStatus::FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		}

		const size_t size = *bytes;
		*bytes = m_fileReader.read(buffer, size);
		m_readPos = m_fileReader.getPos();

		return FLAC__StreamDecoderReadStatus::FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
	}

	::FLAC__StreamDecoderSeekStatus seek_callback(FLAC__uint64 absolute_byte_offset) override
	{
		m_fileReader.setPos(static_cast<int64>(absolute_byte_offset));
		m_readPos = m_fileReader.getPos();
		return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
	}

	::FLAC__StreamDecoderTellStatus tell_callback(FLAC__uint64* absolute_byte_offset) override
	{
		*absolute_byte_offset = static_cast<FLAC__uint64>(m_fileReader.getPos());
		return FLAC__STREAM_DECODER_TELL_STATUS_OK;
	}

	::FLAC__StreamDecoderLengthStatus length_callback(FLAC__uint64* stream_length) override
	{
		*stream_length = static_cast<FLAC__uint64>(m_fileReader.size());
		return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
	}

	bool eof_callback() override
	{
		return m_fileReader.getPos() == m_fileReader.size();
	}

	::FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame* frame, const FLAC__int32* const buffer[]) override
	{
		if (m_loadSampleCount < m_readBuffer.size())
		{
			if (m_channels == 1)
			{
				if (m_bitsPerSample == 16)
				{
					for (size_t i = 0; i < frame->header.blocksize; i++)
					{
						m_readBuffer[m_loadSampleCount].left = m_readBuffer[m_loadSampleCount].right =
							static_cast<FLAC__int16>(buffer[0][i]) * m_normalize;
						++m_loadSampleCount;
					}
				}
				else
				{
					for (size_t i = 0; i < frame->header.blocksize; i++)
					{
						m_readBuffer[m_loadSampleCount].left = m_readBuffer[m_loadSampleCount].right =
							buffer[0][i] * m_normalize;
						++m_loadSampleCount;
					}
				}
			}
			else
			{
				if (m_bitsPerSample == 16)
				{
					for (size_t i = 0; i < frame->header.blocksize; i++)
					{
						m_readBuffer[m_loadSampleCount].left = static_cast<FLAC__int16>(buffer[0][i]) * m_normalize;
						m_readBuffer[m_loadSampleCount].right = static_cast<FLAC__int16>(buffer[1][i]) * m_normalize;
						++m_loadSampleCount;
					}
				}
				else
				{
					for (size_t i = 0; i < frame->header.blocksize; i++)
					{
						m_readBuffer[m_loadSampleCount].left = buffer[0][i] * m_normalize;
						m_readBuffer[m_loadSampleCount].right = buffer[1][i] * m_normalize;
						++m_loadSampleCount;
					}
				}
			}
		}

		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
	}

	void metadata_callback(const ::FLAC__StreamMetadata* metadata) override
	{
		if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
		{
			m_lengthSample = metadata->data.stream_info.total_samples;
			m_sampleRate = metadata->data.stream_info.sample_rate;
			m_channels = metadata->data.stream_info.channels;
			m_bitsPerSample = metadata->data.stream_info.bits_per_sample;
			m_dataSize = m_lengthSample * m_channels * (m_bitsPerSample / 8);
			m_normalize = 1.f / std::powf(2.0f, static_cast<float>(m_bitsPerSample) - 1);
			m_initialized = true;
		}
	}

	void error_callback(::FLAC__StreamDecoderErrorStatus status) override
	{
		Console << U"error: FlacDecoder::error_callback >" << Unicode::Widen(FLAC__StreamDecoderErrorStatusString[status]);
	}

private:
	FlacDecoder(const FlacDecoder&) = default;
	FlacDecoder& operator=(const FlacDecoder&) = default;
};

FlacLoader::FlacLoader(FilePathView path) :
	m_flacDecoder(std::make_unique<FlacDecoder>(path))
{
	init();
	m_flacDecoder->close();
}

size_t FlacLoader::size() const
{
	return m_flacDecoder->m_dataSize;
}

size_t FlacLoader::sampleRate() const
{
	return m_flacDecoder->m_sampleRate;
}

size_t FlacLoader::lengthSample() const
{
	return m_flacDecoder->m_lengthSample;
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

	if (m_flacDecoder->m_lengthSample == 0) {
		Console << U"error: m_flacDecoder->m_lengthSample == 0";
		m_flacDecoder->m_initialized = false;
		return;
	}
	if (m_flacDecoder->m_channels != 1 && m_flacDecoder->m_channels != 2)
	{
		Console << U"error: m_flacDecoder->m_channels != 1 && m_flacDecoder->m_channels != 2";
		Console << U"channels: " << m_flacDecoder->m_channels;
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
	m_flacDecoder->close();
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
		m_flacDecoder->m_readPos = 0;
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
	if (!m_flacDecoder->isOpen())
	{
		m_flacDecoder->restore();
	}

	m_flacDecoder->process_single();
}

std::mutex FlacLoader::m_mutex;
