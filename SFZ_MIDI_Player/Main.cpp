#include <Siv3D.hpp> // OpenSiv3D v0.6.4
#include <Config.hpp>
#include <SamplePlayer.hpp>
#include <Utility.hpp>
#include <SFZLoader.hpp>
#include <PianoRoll.hpp>
#include <MIDILoader.hpp>
#include <SampleSource.hpp>
#include <AudioLoadManager.hpp>
#include <MemoryPool.hpp>
#include <AudioStreamRenderer.hpp>
#include <Program.hpp>

// https://zenn.dev/link/comments/8af1a8f60deaa1 からコピー
class MicrophonePlayer : public IAudioStream
{
public:

	explicit MicrophonePlayer(Microphone&& microphone)
		: m_microphone{ std::move(microphone) }
		, m_sampleRate{ m_microphone.getSampleRate() }
		, m_delaySamples{ static_cast<uint32>(m_sampleRate * 0.05) }
		, m_bufferLength{ m_microphone.getBufferLength() } {}

private:

	Microphone m_microphone;

	uint32 m_sampleRate = Wave::DefaultSampleRate;

	uint32 m_delaySamples = 0;

	size_t m_bufferLength = 0;

	size_t m_readPos = 0;

	bool m_initialized = false;

	void getAudio(float* left, float* right, const size_t samplesToWrite) override
	{
		if (not m_initialized)
		{
			// 録音が始まっていない場合は無視
			if (m_microphone.posSample() == 0)
			{
				return;
			}

			// 現在の録音サンプル位置から m_delaySamples サンプルだけ引いた位置を読み取り開始位置に
			m_readPos = (m_microphone.posSample() + (m_bufferLength - m_delaySamples)) % m_bufferLength;

			m_initialized = true;
		}

		const size_t tailLength = Min((m_bufferLength - m_readPos), samplesToWrite);
		const size_t headLength = (samplesToWrite - tailLength);
		const Wave& wave = m_microphone.getBuffer();

		for (size_t i = 0; i < tailLength; ++i)
		{
			const auto& sample = wave[m_readPos + i];
			*left++ = sample.left;
			*right++ = sample.right;
		}

		for (size_t i = 0; i < headLength; ++i)
		{
			const auto& sample = wave[i];
			*left++ = sample.left;
			*right++ = sample.right;
		}

		m_readPos = ((m_readPos + samplesToWrite) % m_bufferLength);
	}

	bool hasEnded() override
	{
		return false;
	}

	void rewind() override {}
};

#ifndef DEBUG_MODE

void Main()
{
	double yRate = 0.8025;
#ifdef LAYOUT_HORIZONTAL
	Window::Resize(1280, 720);
	Scene::Resize(3840, 2160);
	const auto [pianorollArea, keyboardArea] = SplitUpDown(Scene::Rect(), 0.8);
#else
	Window::Resize(1280, 1280);
	const auto [keyboardArea, pianorollArea] = SplitLeftRight(Scene::Rect(), 0.1);
#endif

#ifdef DEVELOPMENT
	int32 debugDraw = MemoryPool::Size;
	Console << U"";
#endif

	Scene::SetBackground(Palette::Black);

	MemoryPool::i(MemoryPool::ReadFile).setCapacity(16ull << 20);
	MemoryPool::i(MemoryPool::RenderAudio).setCapacity(4ull << 20);

	SamplePlayer player{ keyboardArea };
	player.loadSoundSet(U"default.toml");

	PianoRoll pianoRoll{ pianorollArea };

	std::shared_ptr<SamplerAudioStream> audioStream = std::make_shared<SamplerAudioStream>(pianoRoll, player);
	Audio audio{ audioStream };
	audio.play();

	Optional<MidiData> midiData;

	DragDrop::AcceptFilePaths(true);
	Window::SetTitle(U"MIDIファイルをドラッグドロップして再生");

	Graphics::SetVSyncEnabled(false);
	bool isMute = false;

	auto& renderer = AudioStreamRenderer::i();

	auto renderUpdate = [&]()
	{
		const size_t bufferSampleCount = Wave::DefaultSampleRate;

		while (!renderer.isFinish())
		{
			while (renderer.isPlaying() && !(renderer.bufferBeginSample() <= static_cast<int64>(audioStream->m_pos) && static_cast<int64>(audioStream->m_pos + bufferSampleCount) < renderer.bufferEndSample()))
			{
				//Console << U"bufferBeginSample: " << renderer.bufferBeginSample() << U", currentPosSample: " << pianoRoll.currentPosSample() << U", bufferEndSample: " << renderer.bufferEndSample();
				renderer.update(player);
				renderer.freePastSample(audioStream->m_pos);
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	};

	std::thread audioRenderThread(renderUpdate);

	bool isFullScreen = false;
	double leftPadding = -0.15;
	double rightPadding = -0.1375;
	bool isDebug = false;

	std::shared_ptr<MicrophonePlayer> micStream;
	uint32 sampleRate = Wave::DefaultSampleRate;
	{
		Microphone mic{ 5s, Loop::Yes, StartImmediately::Yes };

		if (not mic.isRecording())
		{
			throw Error{ U"Failed to start recording" };
		}

		sampleRate = mic.getSampleRate();
		micStream = std::make_shared<MicrophonePlayer>(std::move(mic));
	}

	Audio micaudio{ micStream, Arg::sampleRate = sampleRate };
	micaudio.play();

	while (System::Update())
	{
		Window::SetTitle(U"yRate: {:.5f}, stretch: {:.5f}, blackHeight: {:.5f}, left: {:.5f}, right: {:.5f}"_fmt(yRate, player.stretch(), player.blackHeight(), leftPadding, rightPadding));
		if (KeyR.down())
		{
			isDebug = !isDebug;
		}

		bool updateScne = false;
		if (KeyF.down())
		{
			isFullScreen = !isFullScreen;
			Window::SetFullscreen(isFullScreen);
			updateScne = true;
		}
		if (KeyUp.down())
		{
			yRate -= 0.0025;
			updateScne = true;
		}
		if (KeyDown.down())
		{
			yRate += 0.0025;
			updateScne = true;
		}
		if (KeyZ.down())
		{
			leftPadding += 0.0025;
			updateScne = true;
		}
		if (KeyX.down())
		{
			leftPadding -= 0.0025;
			updateScne = true;
		}
		if (KeyN.down())
		{
			rightPadding -= 0.0025;
			updateScne = true;
		}
		if (KeyM.down())
		{
			rightPadding += 0.0025;
			updateScne = true;
		}

		if (updateScne)
		{
			const double sceneWidth = Scene::Rect().w;
			const auto sceneRect = Scene::Rect().stretched(0, sceneWidth * rightPadding, 0, sceneWidth * leftPadding);
			const auto [pianorollArea, keyboardArea] = SplitUpDown(sceneRect, yRate);
			player.setArea(keyboardArea);
			pianoRoll.setArea(pianorollArea);
		}

#ifdef DEVELOPMENT
		if (KeyD.down())
		{
			debugDraw = (debugDraw + 1) % (MemoryPool::Size + 1);
		}
#endif

		if (KeyM.down())
		{
			isMute = !isMute;
			audioStream->volume = isMute ? 0.0f : 1.0f;
		}

		if (DragDrop::HasNewFilePaths())
		{
			const auto filepath = DragDrop::GetDroppedFilePaths().front();

			if (U"mid" == FileSystem::Extension(filepath.path))
			{
				audio.pause();
				pianoRoll.pause();
				renderer.pause();
				audioStream->reset();

				midiData = LoadMidi(filepath.path);
				player.loadMidiData(midiData.value());
				renderer.playRestart();
				std::this_thread::sleep_for(std::chrono::milliseconds(100));

				pianoRoll.playRestart();
				audio.play();
			}
			else if (U"toml" == FileSystem::Extension(filepath.path))
			{
				const bool isPlaying = pianoRoll.isPlaying();
				if (isPlaying)
				{
					audio.pause();
					pianoRoll.pause();
				}

				player.loadSoundSet(filepath.path);
				if (midiData)
				{
					player.loadMidiData(midiData.value());
				}
				renderer.clearBuffer();
				std::this_thread::sleep_for(std::chrono::milliseconds(100));

				if (isPlaying)
				{
					pianoRoll.resume();
					audio.play();
				}
			}
		}

		if (KeySpace.down())
		{
			if (pianoRoll.isPlaying())
			{
				pianoRoll.pause();
				audio.pause();
			}
			else
			{
				pianoRoll.resume();
				audio.play();
			}
		}

		if (KeyHome.down() || player.isReady())
		{
			audio.pause();
			audioStream->reset();
			renderer.playRestart();
			AudioLoadManager::i().debugLog(U"---------------------");
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			pianoRoll.playRestart();
			audio.play();

			player.setReady(false);
			player.reload();
		}

		if (midiData)
		{
			pianoRoll.updateTick(midiData.value());
			player.updateInput();
		}

#ifdef LAYOUT_HORIZONTAL
		pianoRoll.drawHorizontal(player.keyMin(), player.keyMax(), midiData);
		player.drawHorizontal(pianoRoll, midiData, isDebug);
#else
		pianoRoll.drawVertical(player.keyMin(), player.keyMax(), midiData);
		player.drawVertical(pianoRoll, midiData);
#endif

#ifdef DEVELOPMENT
		if (debugDraw < MemoryPool::Size)
		{
			auto& memoryPool = MemoryPool::i(static_cast<MemoryPool::Type>(debugDraw));

			memoryPool.debugUpdate();

			memoryPool.debugDraw();
		}
#endif

	}

	player.finish();

	AudioStreamRenderer::i().finish();
	audioRenderThread.join();
}

#else

void Main()
{
	Window::Resize(1600, 1600);

	auto& memoryPool = MemoryPool::i();
	memoryPool.setCapacity(16ull << 20);

	const auto data = LoadSfz(U"sound/Grand Piano, Kawai.sfz");

	SamplePlayer player{ Rect() };
	player.loadData(data);

	PianoRoll pianoRoll{ Rect() };

	AudioRenderer renderer(pianoRoll, player);

	RectF rect = Scene::Rect().stretched(-50);

	int32 focusSample = 0;
	int32 drawSample = 100000;
	const int scrollSpeed = 50;

	Audio audio;

	Font font(18, Typeface::Heavy);
	Font font2(64, Typeface::Bold);

	bool drawNotes = true;
	bool followPlayPos = true;

	Array<std::pair<uint8, NoteEvent>> eventList;
	Wave renderWave1;
	Wave renderWave2;

	Optional<MidiData> midiData;

	DragDrop::AcceptFilePaths(true);
	Window::SetTitle(U"MIDIファイルをドラッグドロップして再生");

	bool debugDraw = false;
	Graphics::SetVSyncEnabled(false);

	while (System::Update())
	{
		if (DragDrop::HasNewFilePaths())
		{
			//const auto filepath = filePaths.front();

			for (const auto& filepath : DragDrop::GetDroppedFilePaths())
			{
				if (U"mid" == FileSystem::Extension(filepath.path))
				{
					SamplerAudioStream::time1 = 0;
					SamplerAudioStream::time2 = 0;
					SamplerAudioStream::time3 = 0;
					SamplerAudioStream::time4 = 0;

					midiData = LoadMidi(filepath.path);

					eventList = player.addEvents(midiData.value());

					const size_t sampleCount = Wave::DefaultSampleRate * 60;
					Array<float> leftSamples(sampleCount);
					Array<float> rightSamples(sampleCount);

					/*{
						renderer.getAudio(leftSamples.data(), rightSamples.data(), 0, sampleCount);

						renderWave1 = Wave(sampleCount);
						for (auto i : step(renderWave1.size()))
						{
							renderWave1[i].left = leftSamples[i];
							renderWave1[i].right = rightSamples[i];
						}

						renderWave1.saveWAVE(U"debug/render1.wav");
					}*/

					{
						const int windowSize = 512;
						const int itCount = sampleCount / windowSize;

						Stopwatch watch(StartImmediately::Yes);
						for (int i = 0; i < itCount; ++i)
						{
							const int startIndex = i * windowSize;
							renderer.getAudio(&leftSamples[startIndex], &rightSamples[startIndex], startIndex, windowSize);

							/*memoryPool.debugUpdate();

							if (debugDraw)
							{
								memoryPool.debugDraw();
							}

							if (!System::Update())
							{
								return;
							}*/
						}
						Console <<
							SamplerAudioStream::time1 * 1.e-6 << U", " <<
							SamplerAudioStream::time2 * 1.e-6 << U", " <<
							SamplerAudioStream::time3 * 1.e-6 << U", " <<
							SamplerAudioStream::time4 * 1.e-6 << U", " <<
							watch.sF();

						renderWave2 = Wave(sampleCount);
						for (auto i : step(renderWave2.size()))
						{
							renderWave2[i].left = leftSamples[i];
							renderWave2[i].right = rightSamples[i];
						}

						renderWave2.saveWAVE(U"debug/render2.wav");
					}

					audio = Audio(renderWave2);
				}
			}

			Console << U"complete";
		}

		if (KeyD.down())
		{
			debugDraw = !debugDraw;
		}

		if (KeyN.down())
		{
			drawNotes = !drawNotes;
		}

		if (KeyUp.pressed())
		{
			drawSample += scrollSpeed * (KeyShift.pressed() ? 20 : 1);
		}
		if (KeyDown.pressed())
		{
			drawSample -= scrollSpeed * (KeyShift.pressed() ? 20 : 1);
		}
		drawSample = Max(0, drawSample);

		if (KeyRight.pressed())
		{
			focusSample += scrollSpeed * (KeyShift.pressed() ? 20 : 1);
		}
		if (KeyLeft.pressed())
		{
			focusSample -= scrollSpeed * (KeyShift.pressed() ? 20 : 1);
		}
		focusSample = Max(0, focusSample);

		if (followPlayPos && audio.isPlaying())
		{
			focusSample = Min(static_cast<int32>(audio.posSample()), static_cast<int32>(audio.samples()));
		}

		rect.drawFrame();

		if (midiData)
		{
			if (KeySpace.down())
			{
				if (audio.isPlaying())
				{
					audio.pause();
				}
				else
				{
					audio.seekTime(1.0 * focusSample / Wave::DefaultSampleRate);
					audio.play();
				}
			}

			if (KeyHome.down())
			{
				audio.stop();
				audio.play();
			}

			const int32 itCount = Min(static_cast<int32>(renderWave1.size()) - focusSample, drawSample);

			{
				const double deltaTime = 1.0;
				const int32 measureCount = static_cast<int32>(audio.lengthSec() / deltaTime);
				for (int32 i = 0; i < measureCount; ++i)
				{
					const double time = i * deltaTime;
					const auto currentIndex = static_cast<int32>(Math::Round(time * Wave::DefaultSampleRate));
					const double x = Math::Map(currentIndex, focusSample, focusSample + itCount, rect.tl().x, rect.br().x);
					const Line line(x, rect.tl().y, x, rect.br().y);
					line.draw(1.0, Palette::Gray);
					font2(time).draw(line.begin, Color(55, 56, 57));
				}
			}

			for (int32 i = 0; i + 1 < itCount; i++)
			{
				const auto color = Color(0, 255, 0);

				const double x0 = Math::Map(i, 0, itCount, rect.tl().x, rect.br().x);
				const double y0 = Math::Map(renderWave1[focusSample + i].left, -0.5, 0.5, rect.tl().y, rect.br().y);

				const double x1 = Math::Map(i + 1, 0, itCount, rect.tl().x, rect.br().x);
				const double y1 = Math::Map(renderWave1[focusSample + i + 1].left, -0.5, 0.5, rect.tl().y, rect.br().y);

				Line(x0, y0, x1, y1).draw(color);
			}
			for (int32 i = 0; i + 1 < itCount; i++)
			{
				const auto color = Color(255, 0, 0);

				const double x0 = Math::Map(i, 0, itCount, rect.tl().x, rect.br().x);
				const double y0 = Math::Map(renderWave2[focusSample + i].left, -0.5, 0.5, rect.tl().y, rect.br().y);

				const double x1 = Math::Map(i + 1, 0, itCount, rect.tl().x, rect.br().x);
				const double y1 = Math::Map(renderWave2[focusSample + i + 1].left, -0.5, 0.5, rect.tl().y, rect.br().y);

				Line(x0, y0, x1, y1).draw(color);
			}

			if (drawNotes)
			{
				for (const auto& [key, eventNote] : eventList)
				{
					const auto pressIndex = static_cast<int32>(eventNote.pressTimePos);
					const auto releaseIndex = static_cast<int32>(eventNote.releaseTimePos);

					const double x0 = Math::Map(pressIndex, focusSample, focusSample + itCount, rect.tl().x, rect.br().x);
					const double x1 = Math::Map(releaseIndex, focusSample, focusSample + itCount, rect.tl().x, rect.br().x);

					const double y = Math::Map(key, 24, 120, rect.bl().y, rect.tr().y);

					RectF noteRect(x0, y, x1 - x0, 30);
					noteRect.drawFrame(2.0, Palette::Orange);
					font(key).draw(noteRect.pos, Palette::White);
				}
			}
		}

		memoryPool.debugUpdate();

		if (debugDraw)
		{
			memoryPool.debugDraw();
		}
	}
}

#endif
