#pragma once
#include <FlacLoader.hpp>
#include <MemoryBlockList.hpp>

#define FLAC__NO_DLL
#include <FLAC++/decoder.h>
#include <share/compat.h>

class FlacDecoder : public FLAC::Decoder::Stream
{
public:

	FlacDecoder(FilePathView path, size_t debugId) :
		FLAC::Decoder::Stream(),
		m_filePath(path),
		m_fileReader(path),
		m_readBlocks(debugId, MemoryPool::ReadFile)
	{}

	FLAC__uint64 m_lengthSample = 0;
	FLAC__uint64 m_dataSize = 0;
	uint32_t m_sampleRate = 0;
	uint32_t m_channels = 0;
	uint32_t m_bitsPerSample = 0;

	float m_normalizeRead = 0;
	float m_normalizeWrite = 0;
	bool m_initialized = false;
	float m_sampleRateInv = 0;

	MemoryBlockList m_readBlocks;
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

	WaveSample getSample(int64 index) const
	{
		const size_t blockAlign = sizeof(uint16) * 2;
		auto [ptr, actualReadBytes] = m_readBlocks.getWriteBuffer(index * blockAlign, sizeof(Sample16bit2ch));
		const auto pSample = std::bit_cast<Sample16bit2ch*>(ptr);
		return WaveSample(pSample->left * m_normalizeWrite, pSample->right * m_normalizeWrite);
	}

	void releaseBuffer()
	{
		m_readBlocks.deallocate();
		m_loadSampleCount = 0;
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
		// とりあえず16bit2ch固定
		const size_t blockAlign = sizeof(uint16) * 2;

		size_t readHead = m_loadSampleCount * blockAlign;
		size_t requiredReadBytes = frame->header.blocksize * blockAlign;
		{
			const size_t allocateBegin = (readHead / MemoryPool::UnitBlockSizeOfBytes) * MemoryPool::UnitBlockSizeOfBytes;
			const size_t allocateEnd = readHead + requiredReadBytes;

			m_readBlocks.allocate(allocateBegin, allocateEnd - allocateBegin);
		}

		if (m_channels == 1)
		{
			auto mono = buffer[0];

			while (1 <= requiredReadBytes)
			{
				auto [ptr, actualReadBytes] = m_readBlocks.getWriteBuffer(readHead, requiredReadBytes);
				readHead += actualReadBytes;
				requiredReadBytes -= actualReadBytes;

				auto pSample = std::bit_cast<Sample16bit2ch*>(ptr);

				const size_t readCount = actualReadBytes / blockAlign;

				//if (m_bitsPerSample == 16)
				{
					for (size_t i = 0; i < readCount; i++)
					{
						pSample[i].left = pSample[i].right = static_cast<int16>(mono[i] * m_normalizeRead * 32767);
					}
				}

				mono += readCount;
			}
		}
		else
		{
			auto left = buffer[0];
			auto right = buffer[1];

			while (1 <= requiredReadBytes)
			{
				auto [ptr, actualReadBytes] = m_readBlocks.getWriteBuffer(readHead, requiredReadBytes);
				readHead += actualReadBytes;
				requiredReadBytes -= actualReadBytes;

				auto pSample = std::bit_cast<Sample16bit2ch*>(ptr);

				const size_t readCount = actualReadBytes / blockAlign;

				//if (m_bitsPerSample == 16)
				{
					for (size_t i = 0; i < readCount; i++)
					{
						pSample[i].left = static_cast<int16>(left[i] * m_normalizeRead * 32767);
						pSample[i].right = static_cast<int16>(right[i] * m_normalizeRead * 32767);
					}
				}

				left += readCount;
				right += readCount;
			}
		}

		m_loadSampleCount += frame->header.blocksize;

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
			m_normalizeRead = 1.f / std::powf(2.0f, static_cast<float>(m_bitsPerSample) - 1);
			m_normalizeWrite = 1.f / 32767.0f;
			m_sampleRateInv = 1.f / m_sampleRate;
			m_initialized = true;
			m_loadSampleCount = 0;
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

FlacLoader::FlacLoader(FilePathView path, size_t debugId) :
	m_flacDecoder(std::make_unique<FlacDecoder>(path, debugId))
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

float FlacLoader::sampleRateInv() const
{
	return m_flacDecoder->m_sampleRateInv;
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

void FlacLoader::use(size_t beginSampleIndex, size_t sampleCount)
{
	m_unuseCount = 0;

	if (m_use)
	{
		return;
	}

	m_mutex.lock();

	{
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
	if (m_use && 5 < m_unuseCount)
	{
		unuse();
	}

	if (m_use)
	{
		readBlock();
	}
	else if (1 <= m_flacDecoder->m_loadSampleCount)
	{
		m_flacDecoder->releaseBuffer();
		m_flacDecoder->reset();
	}

	m_mutex.unlock();
}

void FlacLoader::markUnused()
{

}

void FlacLoader::freeUnusedBlocks()
{

}

WaveSample FlacLoader::getSample(int64 index) const
{
	//const bool isValidIndex = 0 <= index && index < m_flacDecoder->m_lengthSample;
	//if (!isValidIndex)
	//{
	//	Console << U"error: FlacLoader::getSample() invalid sample index: " << index;
	//}

	return m_flacDecoder->getSample(index);
}

void FlacLoader::readBlock()
{
	if (!m_flacDecoder->isOpen())
	{
		m_flacDecoder->restore();
	}

	if (auto state = static_cast<FLAC__StreamDecoderState>(m_flacDecoder->get_state());
		state == FLAC__STREAM_DECODER_SEARCH_FOR_METADATA || state == FLAC__STREAM_DECODER_READ_METADATA)
	{
		m_flacDecoder->process_until_end_of_metadata();
	}

	m_flacDecoder->process_single();
}

std::mutex FlacLoader::m_mutex;
