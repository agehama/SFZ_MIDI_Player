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

	void seekPos(int64 index)
	{
		//Console << index;
		seek_absolute(index);
		m_loadSampleCount = index;
	}

	int64 getPos() const
	{
		return m_loadSampleCount;
	}

	bool isEof()
	{
		return eof_callback();
	}

	void readBlock(size_t beginSample, size_t sampleCount)
	{
		size_t readCount = sampleCount;

		if (m_lengthSample <= beginSample)
		{
			readCount = 0;
		}
		else if (m_lengthSample <= beginSample + readCount)
		{
			readCount = m_lengthSample - beginSample;
		}

		if (1 <= readCount)
		{
			//Console << U"read " << beginSample << U" + " << readCount;

			const size_t blockAlign = sizeof(uint16) * 2;
			size_t readHead = beginSample * blockAlign;
			size_t requiredReadBytes = readCount * blockAlign;

			{
				const size_t allocateBegin = (readHead / MemoryPool::UnitBlockSizeOfBytes) * MemoryPool::UnitBlockSizeOfBytes;
				const size_t allocateEnd = readHead + requiredReadBytes;

				m_tempBeginSample = allocateBegin / blockAlign;
				m_tempSampleCount = (allocateEnd - allocateBegin) / blockAlign;

				m_readBlocks.allocate(allocateBegin, allocateEnd - allocateBegin);

				if (!isOpen())
				{
					restore();
				}

				if (auto state = static_cast<FLAC__StreamDecoderState>(get_state());
					state == FLAC__STREAM_DECODER_SEARCH_FOR_METADATA || state == FLAC__STREAM_DECODER_READ_METADATA)
				{
					process_until_end_of_metadata();
				}

				seekPos(m_tempBeginSample);

				for (;;)
				{
					if (beginSample + sampleCount <= m_tempBeginSample || isEof())
					{
						break;
					}

					if (!process_single())
					{
						break;
					}
				}
			}
		}
	}

protected:
	FilePath m_filePath;
	BinaryReader m_fileReader;

	size_t m_tempBeginSample = 0;
	size_t m_tempSampleCount = 0;

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
		//Console<< U"seek " << absolute_byte_offset;
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
		return m_fileReader.size() <= m_fileReader.getPos() + 1;
		//return m_fileReader.getPos() == m_fileReader.size();
	}

	::FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame* frame, const FLAC__int32* const buffer[]) override
	{
		// とりあえず16bit2ch固定
		const size_t blockAlign = sizeof(uint16) * 2;

		if (frame->header.number_type != FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER)
		{
			Console << U"error frame->header.number_type != FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER";
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}

		//if (m_tempBeginSample != frame->header.number.sample_number)
		//{
		//	Console << U"error m_tempBeginSample = " << m_tempBeginSample << U", sample_number = " << frame->header.number.sample_number;
		//	return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		//}

		//size_t readHead = m_loadSampleCount * blockAlign;
		size_t readHead = frame->header.number.sample_number * blockAlign;
		size_t requiredReadBytes = frame->header.blocksize * blockAlign;
		//{
		//	const size_t allocateBegin = (readHead / MemoryPool::UnitBlockSizeOfBytes) * MemoryPool::UnitBlockSizeOfBytes;
		//	const size_t allocateEnd = readHead + requiredReadBytes;

		//	m_readBlocks.allocate(allocateBegin, allocateEnd - allocateBegin);
		//	//Console << U"write " << m_loadSampleCount << U", +" << frame->header.blocksize;
		//}

		if (m_channels == 1)
		{
			auto mono = buffer[0];

			while (1 <= requiredReadBytes)
			{
				const auto blockIndex = readHead / MemoryPool::UnitBlockSizeOfBytes;
				if (!m_readBlocks.isAllocatedBlock(blockIndex))
				{
					//Console << U"return at " << m_tempBeginSample;
					// ABORTを返すと以降のprocess_single()が処理されなくなるのでCONTINUEを返しておく
					return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
				}

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
				m_tempBeginSample += readCount;
				m_loadSampleCount += readCount;
			}
		}
		else
		{
			auto left = buffer[0];
			auto right = buffer[1];

			while (1 <= requiredReadBytes)
			{
				const auto blockIndex = readHead / MemoryPool::UnitBlockSizeOfBytes;
				if (!m_readBlocks.isAllocatedBlock(blockIndex))
				{
					return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
				}

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
				m_tempBeginSample += readCount;
				m_loadSampleCount += readCount;
			}
		}

		//m_loadSampleCount += frame->header.blocksize;
		//m_loadSampleCount = frame->header.number.sample_number + frame->header.blocksize;
		//m_tempBeginSample += frame->header.blocksize;

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
	//m_unuseCount = 0;

	//if (m_use)
	//{
	//	return;
	//}

	//m_mutex.lock();

	//{
	//	readBlock();

	//	m_use = true;
	//}

	//m_mutex.unlock();

	m_flacDecoder->readBlock(beginSampleIndex, sampleCount);
}

void FlacLoader::unuse()
{
	//m_use = false;
	//m_flacDecoder->close();
}

void FlacLoader::update()
{
	//++m_unuseCount;
	//if (m_use && 5 < m_unuseCount)
	//{
	//	unuse();
	//}

	//if (m_use)
	//{
	//	readBlock();
	//}
	//else if (1 <= m_flacDecoder->m_loadSampleCount)
	//{
	//	m_flacDecoder->releaseBuffer();
	//	m_flacDecoder->reset();
	//}

	//m_mutex.unlock();
}

void FlacLoader::markUnused()
{
	m_flacDecoder->m_readBlocks.markUnused();
}

void FlacLoader::freeUnusedBlocks()
{
	m_flacDecoder->m_readBlocks.freeUnusedBlocks();
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
