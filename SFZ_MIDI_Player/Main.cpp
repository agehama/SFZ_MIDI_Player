#include <Siv3D.hpp> // OpenSiv3D v0.6.4
#include <SamplePlayer.hpp>
#include <Utility.hpp>
#include <SFZLoader.hpp>
#include <PianoRoll.hpp>
#include <MIDILoader.hpp>
#include <SampleSource.hpp>
#include <AudioLoadManager.hpp>
#include <MemoryPool.hpp>

//#define DEBUG_MODE

#define LAYOUT_HORIZONTAL

#ifndef DEBUG_MODE

void Main()
{
#ifdef LAYOUT_HORIZONTAL
	Window::Resize(1280, 720);
	const auto [pianorollArea, keyboardArea] = SplitUpDown(Scene::Rect(), 0.8);
#else
	Window::Resize(1280, 1280);
	const auto [keyboardArea, pianorollArea] = SplitLeftRight(Scene::Rect(), 0.1);
#endif

	MemoryPool::i(MemoryPool::ReadFile).setCapacity(16ull << 20);
	MemoryPool::i(MemoryPool::RenderAudio).setCapacity(16ull << 20);

	const auto data = LoadSfz(U"sound/Grand Piano, Kawai.sfz");

	SamplePlayer player{ keyboardArea };
	player.loadData(data);

	PianoRoll pianoRoll{ pianorollArea };

	std::shared_ptr<SamplerAudioStream> audioStream = std::make_shared<SamplerAudioStream>(pianoRoll, player);
	Audio audio{ audioStream };
	audio.play();

	Optional<MidiData> midiData;

	DragDrop::AcceptFilePaths(true);
	Window::SetTitle(U"MIDIファイルをドラッグドロップして再生");

	Graphics::SetVSyncEnabled(false);
	int32 debugDraw = MemoryPool::Size;
	bool isMute = false;

	while (System::Update())
	{
		if (KeyD.down())
		{
			debugDraw = (debugDraw + 1) % (MemoryPool::Size + 1);
		}
		if (KeyM.down())
		{
			isMute = !isMute;
			audioStream->volume = isMute ? 0.0 : 1.0;
		}

		if (DragDrop::HasNewFilePaths())
		{
			const auto filepath = DragDrop::GetDroppedFilePaths().front();

			if (U"mid" == FileSystem::Extension(filepath.path))
			{
				pianoRoll.pause();
				audio.pause();

				midiData = LoadMidi(filepath.path);
				player.addEvents(midiData.value());
				pianoRoll.playRestart();
				audioStream->restart();
				audio.play();
			}
			else if (U"sfz" == FileSystem::Extension(filepath.path))
			{
				pianoRoll.pause();
				audio.pause();

				//AudioLoadManager::i().pause();
				player.loadData(LoadSfz(filepath.path));
				//AudioLoadManager::i().resume();

				pianoRoll.resume();
				audio.play();
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

		if (KeyHome.down())
		{
			pianoRoll.playRestart();
			audioStream->restart();
			audio.play();
		}

		if (midiData)
		{
			pianoRoll.updateTick(midiData.value());
		}

#ifdef LAYOUT_HORIZONTAL
		pianoRoll.drawHorizontal(player.keyMin(), player.keyMax(), midiData);
		player.drawHorizontal(pianoRoll, midiData);
#else
		pianoRoll.drawVertical(player.keyMin(), player.keyMax(), midiData);
		player.drawVertical(pianoRoll, midiData);
#endif

		if (debugDraw < MemoryPool::Size)
		{
			auto& memoryPool = MemoryPool::i(static_cast<MemoryPool::Type>(debugDraw));

			memoryPool.debugUpdate();

			memoryPool.debugDraw();
		}
	}
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
