#pragma once
#include <Siv3D.hpp>
#include <AudioStreamRenderer.hpp>
#include <PianoRoll.hpp>
#include <SamplePlayer.hpp>
#include <AudioLoadManager.hpp>
#include <SampleSource.hpp>

AudioStreamRenderer::AudioStreamRenderer() :
	m_writeBlocks(0, MemoryPool::RenderAudio)
{
}

void AudioStreamRenderer::finish()
{
	m_isFinished = true;
}

bool AudioStreamRenderer::isFinish() const
{
	return m_isFinished;
}

bool AudioStreamRenderer::isPlaying() const
{
	return m_isPlaying;
}

int64 AudioStreamRenderer::bufferBeginSample()
{
	return m_bufferBeginSample;
}

void AudioStreamRenderer::clearBuffer()
{
	lock();
	m_writeBlocks.deallocate();
	unlock();
}

void AudioStreamRenderer::playRestart()
{
	lock();
	m_writeBlocks.deallocate();
	m_isPlaying = true;
	m_bufferBeginSample = 0;
	unlock();
}

void AudioStreamRenderer::pause()
{
	m_isPlaying = false;
}

int64 AudioStreamRenderer::bufferEndSample()
{
	// ブロックごとに left, right, left, と交互に並べる
	return m_bufferBeginSample + (m_writeBlocks.numOfBlocks() / 2) * MemoryPool::UnitBlockSampleLength;
}

void AudioStreamRenderer::update(SamplePlayer& samplePlayer)
{
	AudioLoadManager::i().markBlocks();

	lock();
	const auto block = static_cast<uint32>(bufferEndSample() / MemoryPool::UnitBlockSampleLength);

	const auto leftIndex = 2 * block;
	const auto rightIndex = leftIndex + 1;
	//Console << U"allocate " << leftIndex << U", " << rightIndex;
	auto left = m_writeBlocks.allocateSingleBlock(leftIndex);
	auto right = m_writeBlocks.allocateSingleBlock(rightIndex);
	samplePlayer.getSamples(std::bit_cast<float*>(left), std::bit_cast<float*>(right), bufferEndSample(), MemoryPool::UnitBlockSampleLength);

	unlock();

	AudioLoadManager::i().freeUnusedBlocks();
}

void AudioStreamRenderer::freePastSample(int64 sampleIndex)
{
	const auto block = static_cast<uint32>(sampleIndex / MemoryPool::UnitBlockSampleLength);
	const auto blockIndex = 2 * block;

	lock();
	const auto [minBlockIndex, maxBlockIndex, eraseCount] = m_writeBlocks.freePreviousBlockIndex(blockIndex);
	if (1 <= eraseCount)
	{
		m_bufferBeginSample = (minBlockIndex / 2) * MemoryPool::UnitBlockSampleLength;
	}

	//Console << blockIndex << U" | [" << minBlockIndex << U", " << maxBlockIndex << U"] " << eraseCount << U" " << m_writeBlocks.numOfBlocks();
	//Console << U"remove " << eraseCount << U", bufferBeginSample: " << m_bufferBeginSample;
	unlock();
}

WaveSample AudioStreamRenderer::getSample(int64 index) const
{
	const auto block = static_cast<uint32>(index / MemoryPool::UnitBlockSampleLength);
	const auto blockBeginSampleIndex = block * MemoryPool::UnitBlockSampleLength;
	const auto offsetSample = index - blockBeginSampleIndex;

	const auto leftBlockIndex = 2 * block;
	const auto rightBlockIndex = leftBlockIndex + 1;

	lock();
	auto leftBlock = std::bit_cast<float*>(m_writeBlocks.getBlock(leftBlockIndex));
	auto rightBlock = std::bit_cast<float*>(m_writeBlocks.getBlock(rightBlockIndex));
	unlock();

	return WaveSample(leftBlock[offsetSample], rightBlock[offsetSample]);
}

void AudioStreamRenderer::lock() const
{
	m_mutex.lock();
}

void AudioStreamRenderer::unlock() const
{
	m_mutex.unlock();
}

std::mutex AudioStreamRenderer::m_mutex;
