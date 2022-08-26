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

void AudioStreamRenderer::playRestart()
{
	m_isPlaying = true;
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

void AudioStreamRenderer::update(PianoRoll& pianoroll, SamplePlayer& samplePlayer)
{
	AudioLoadManager::i().markBlocks();

	const auto leftIndex = bufferEndSample() / MemoryPool::UnitBlockSampleLength;
	const auto rightIndex = leftIndex + 1;
	auto left = m_writeBlocks.allocateSingleBlock(leftIndex);
	auto right = m_writeBlocks.allocateSingleBlock(rightIndex);
	samplePlayer.getSamples(std::bit_cast<float*>(left), std::bit_cast<float*>(right), bufferEndSample(), MemoryPool::UnitBlockSampleLength);

	AudioLoadManager::i().freeUnusedBlocks();
}

void AudioStreamRenderer::freeUntilSample(int64 sampleIndex)
{
	const auto blockIndex = sampleIndex / MemoryPool::UnitBlockSampleLength;
	Console << m_writeBlocks.freePreviousBlockIndex(blockIndex);
	m_bufferBeginSample = sampleIndex;
}

WaveSample AudioStreamRenderer::getSample(int64 index) const
{
	const auto leftBlockIndex = (index / MemoryPool::UnitBlockSampleLength);
	const auto leftBeginSampleIndex = leftBlockIndex * MemoryPool::UnitBlockSampleLength;

	const auto rightBlockIndex = leftBlockIndex + 1;

	const auto offsetSample = index - leftBeginSampleIndex;

	auto leftPtr = m_writeBlocks.getBlock(leftBlockIndex) + offsetSample * sizeof(float);
	auto rightPtr = m_writeBlocks.getBlock(rightBlockIndex) + offsetSample * sizeof(float);
	return WaveSample(*std::bit_cast<float*>(leftPtr), *std::bit_cast<float*>(rightPtr));

	//const auto leftIndex = index / MemoryPool::UnitBlockSampleLength;
	//const auto rightIndex = leftIndex + 1;

	//auto [ptr, actualReadBytes] = m_writeBlocks.getWriteBuffer(index * m_format.blockAlign, sizeof(Sample16bit2ch));
	//const auto pSample = std::bit_cast<Sample16bit2ch*>(ptr);
	//return WaveSample(pSample->left * m_normalize, pSample->right * m_normalize);
}
